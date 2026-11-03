#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include "pong_common.h"

#define MAX_USERS    32
#define MAX_PENDING  8
#define PENDING_TTL  DDS_SECS(5)
#define MAX_TAKE     8
#define STATS_PERIOD DDS_SECS(10)

typedef struct
{
    char    name[32];
    uint8_t key[SEC_KEY_LEN];
} UserRecord;

typedef struct
{
    bool       b_active;
    char       username[32];
    uint8_t    session_key[SEC_KEY_LEN];
    uint32_t   session_gen;
    uint64_t   last_rx_seq;
    uint64_t   tx_seq;
    dds_time_t last_input_time;
    int8_t     dir;
} Player;

typedef struct
{
    float   ball_x, ball_y, ball_vx, ball_vy, ball_speed;
    float   paddle_y[MAX_PLAYERS];
    uint8_t score[MAX_PLAYERS];
    int     phase;
    float   timer;
    int     winner;
    int     serve_dir;
} Game;

typedef struct
{
    bool       used;
    uint64_t   tag;
    char       username[32];
    bool       known_user;
    uint8_t    user_key[SEC_KEY_LEN];
    uint8_t    client_nonce[SEC_NONCE_LEN];
    uint8_t    server_nonce[SEC_NONCE_LEN];
    dds_time_t created;
} Pending;

typedef struct
{
    dds_entity_t waitset;
    dds_entity_t join_reader, proof_reader, input_reader;
    dds_entity_t challenge_writer, response_writer;
} NetCtx;

static UserRecord g_users[MAX_USERS];
static int        g_num_users = 0;

static Player     arr_Players[MAX_PLAYERS];
static Game       g_game;
static uint32_t   g_session_counter = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static atomic_bool g_stop = false;

static void on_sigint(int sig)
{
    (void)sig;
    atomic_store(&g_stop, true);
}

static bool valid_username(const char *name)
{
    size_t len = strlen(name);
    if (len == 0 || len > 31)
        return false;
    for (size_t i = 0; i < len; i++)
        if (!isalnum((unsigned char)name[i]) && name[i] != '_' && name[i] != '-')
            return false;
    return true;
}

static char g_users_path[PATH_MAX + 16] = "data/users.txt";

static void resolve_users_path(void)
{
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len <= 0)
        return;
    exe[len] = '\0';
    char *slash = strrchr(exe, '/');
    if (!slash)
        return;
    *slash = '\0';

    char data_dir[PATH_MAX + 16];
    char resolved[PATH_MAX];
    snprintf(data_dir, sizeof(data_dir), "%s/../data", exe);
    if (realpath(data_dir, resolved))
        snprintf(g_users_path, sizeof(g_users_path), "%s/users.txt", resolved);
}

static const UserRecord *find_user(const char *name)
{
    for (int i = 0; i < g_num_users; i++)
        if (strcmp(g_users[i].name, name) == 0)
            return &g_users[i];
    return NULL;
}

static int load_users(void)
{
    FILE *f = fopen(g_users_path, "r");
    if (!f)
        return -1;

    char line[256];
    g_num_users = 0;
    while (g_num_users < MAX_USERS && fgets(line, sizeof(line), f))
    {
        char name[64], hex[160];
        size_t bin_len = 0;
        if (line[0] == '#' || sscanf(line, "%63s %159s", name, hex) != 2)
            continue;
        if (!valid_username(name))
            continue;
        if (sodium_hex2bin(g_users[g_num_users].key, SEC_KEY_LEN, hex, strlen(hex),
                           NULL, &bin_len, NULL) != 0 || bin_len != SEC_KEY_LEN)
            continue;
        snprintf(g_users[g_num_users].name, sizeof(g_users[0].name), "%.31s", name);
        g_num_users++;
    }
    fclose(f);
    return g_num_users;
}

static int add_user(const char *name, const char *password)
{
    if (!valid_username(name))
    {
        fprintf(stderr, "Username must be 1-31 characters: letters, digits, _ or -\n");
        return 1;
    }
    if (strlen(password) < 6)
    {
        fprintf(stderr, "Password must be at least 6 characters\n");
        return 1;
    }
    load_users();
    if (find_user(name))
    {
        fprintf(stderr, "User '%s' already exists in %s\n", name, g_users_path);
        return 1;
    }

    uint8_t key[SEC_KEY_LEN];
    char hex[SEC_KEY_LEN * 2 + 1];
    if (sec_derive_user_key(name, password, key) != 0)
    {
        fprintf(stderr, "Key derivation failed (out of memory?)\n");
        return 1;
    }
    sodium_bin2hex(hex, sizeof(hex), key, SEC_KEY_LEN);
    sec_wipe(key, sizeof(key));

    FILE *f = fopen(g_users_path, "a");
    if (!f)
    {
        perror(g_users_path);
        return 1;
    }
    fprintf(f, "%s %s\n", name, hex);
    fclose(f);
    chmod(g_users_path, 0600);
    printf("Added user '%s' to %s\n", name, g_users_path);
    return 0;
}

static void remove_player_locked(int idx, const char *reason)
{
    Player *p = &arr_Players[idx];
    printf("[game] Player %d (%s) %s\n", idx + 1, p->username, reason);
    sec_wipe(p->session_key, sizeof(p->session_key));
    memset(p, 0, sizeof(*p));
}

static void game_center_ball(Game *g)
{
    g->ball_x = WIDTH / 2.0f;
    g->ball_y = HEIGHT / 2.0f;
    g->ball_vx = g->ball_vy = 0.0f;
}

static void game_to_waiting(Game *g)
{
    g->phase = PHASE_WAITING;
    g->score[0] = g->score[1] = 0;
    g->winner = 0;
    game_center_ball(g);
}

static void game_start_match(Game *g)
{
    g->score[0] = g->score[1] = 0;
    g->winner = 0;
    g->phase = PHASE_COUNTDOWN;
    g->timer = COUNTDOWN_START;
    g->serve_dir = randombytes_uniform(2) ? 1 : -1;
    game_center_ball(g);
    printf("[game] Match starting\n");
}

static void game_launch_ball(Game *g)
{
    float angle = ((float)randombytes_uniform(1001) / 1000.0f * 2.0f - 1.0f) * 0.5236f;
    g->ball_speed = BALL_SPEED_START;
    g->ball_vx = (float)g->serve_dir * g->ball_speed * cosf(angle);
    g->ball_vy = g->ball_speed * sinf(angle);
}

static bool game_hit_paddle(Game *g, float old_x, float old_y, int side)
{
    const float half = BALL_SIZE / 2.0f;
    float face, old_edge, new_edge;

    if (side == 0)
    {
        if (g->ball_vx >= 0) return false;
        face = PADDLE_MARGIN + PADDLE_W;
        old_edge = old_x - half;
        new_edge = g->ball_x - half;
        if (!(old_edge >= face && new_edge < face)) return false;
    }
    else
    {
        if (g->ball_vx <= 0) return false;
        face = WIDTH - PADDLE_MARGIN - PADDLE_W;
        old_edge = old_x + half;
        new_edge = g->ball_x + half;
        if (!(old_edge <= face && new_edge > face)) return false;
    }

    float t = (old_edge - face) / (old_edge - new_edge);
    float y = old_y + (g->ball_y - old_y) * t;
    float top = g->paddle_y[side];
    if (y + half < top || y - half > top + PADDLE_H)
        return false;

    float rel = (y - (top + PADDLE_H * 0.5f)) / (PADDLE_H * 0.5f);
    if (rel < -1.0f) rel = -1.0f;
    if (rel >  1.0f) rel =  1.0f;
    float angle = rel * BALL_MAX_ANGLE;

    g->ball_speed = fminf(g->ball_speed * BALL_SPEEDUP, BALL_SPEED_MAX);
    float dir = (side == 0) ? 1.0f : -1.0f;
    g->ball_vx = dir * g->ball_speed * cosf(angle);
    g->ball_vy = g->ball_speed * sinf(angle);
    g->ball_x = (side == 0) ? face + half : face - half;
    g->ball_y = y;
    return true;
}

static void game_step_ball(Game *g, float dt)
{
    const float half = BALL_SIZE / 2.0f;
    float old_x = g->ball_x, old_y = g->ball_y;

    g->ball_x += g->ball_vx * dt;
    g->ball_y += g->ball_vy * dt;

    if (g->ball_y - half < 0.0f)
    {
        g->ball_y = 2.0f * half - g->ball_y;
        g->ball_vy = fabsf(g->ball_vy);
    }
    else if (g->ball_y + half > HEIGHT)
    {
        g->ball_y = 2.0f * (HEIGHT - half) - g->ball_y;
        g->ball_vy = -fabsf(g->ball_vy);
    }

    if (!game_hit_paddle(g, old_x, old_y, 0))
        game_hit_paddle(g, old_x, old_y, 1);

    int scorer = 0;
    if (g->ball_x + half < 0.0f)
        scorer = 2;
    else if (g->ball_x - half > WIDTH)
        scorer = 1;
    if (scorer == 0)
        return;

    g->score[scorer - 1]++;
    printf("[game] Player %d scores  (%d - %d)\n", scorer, g->score[0], g->score[1]);
    game_center_ball(g);

    if (g->score[scorer - 1] >= WIN_SCORE)
    {
        g->phase = PHASE_GAMEOVER;
        g->timer = GAMEOVER_TIME;
        g->winner = scorer;
        printf("[game] Player %d wins the match\n", scorer);
    }
    else
    {
        g->phase = PHASE_COUNTDOWN;
        g->timer = COUNTDOWN_POINT;
        g->serve_dir = (scorer == 1) ? 1 : -1;
    }
}

static void game_update(Game *g, float dt)
{
    bool both_present = arr_Players[0].b_active && arr_Players[1].b_active;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!arr_Players[i].b_active)
            continue;
        float y = g->paddle_y[i] + (float)arr_Players[i].dir * PADDLE_SPEED * dt;
        if (y < 0.0f) y = 0.0f;
        if (y > HEIGHT - PADDLE_H) y = HEIGHT - PADDLE_H;
        g->paddle_y[i] = y;
    }

    switch (g->phase)
    {
    case PHASE_WAITING:
        if (both_present)
            game_start_match(g);
        break;

    case PHASE_COUNTDOWN:
        if (!both_present) { game_to_waiting(g); break; }
        g->timer -= dt;
        if (g->timer <= 0.0f)
        {
            g->phase = PHASE_PLAYING;
            game_launch_ball(g);
        }
        break;

    case PHASE_PLAYING:
        if (!both_present) { game_to_waiting(g); break; }
        game_step_ball(g, dt);
        break;

    case PHASE_GAMEOVER:
        g->timer -= dt;
        if (g->timer <= 0.0f)
        {
            if (both_present) game_start_match(g);
            else              game_to_waiting(g);
        }
        break;
    }
}

static void game_snapshot(const Game *g, GameSnapshot *s)
{
    s->ball_x = g->ball_x;
    s->ball_y = g->ball_y;
    s->active_mask = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        s->paddle_y[i] = g->paddle_y[i];
        s->score[i] = g->score[i];
        if (arr_Players[i].b_active)
            s->active_mask |= (uint8_t)(1u << i);
    }
    s->phase = (uint8_t)g->phase;
    s->winner = (uint8_t)g->winner;
    s->countdown = (g->phase == PHASE_COUNTDOWN) ? (uint8_t)ceilf(g->timer) : 0;
}

static Pending g_pending[MAX_PENDING];

static Pending *pending_find(uint64_t tag)
{
    for (int i = 0; i < MAX_PENDING; i++)
        if (g_pending[i].used && g_pending[i].tag == tag)
            return &g_pending[i];
    return NULL;
}

static void pending_clear(Pending *p)
{
    sec_wipe(p, sizeof(*p));
}

static void handle_join_request(NetCtx *n, const JoinRequest *req, dds_time_t now)
{
    Pending *p = pending_find(req->client_tag);

    if (p == NULL)
    {
        p = &g_pending[0];
        for (int i = 0; i < MAX_PENDING; i++)
        {
            if (!g_pending[i].used) { p = &g_pending[i]; break; }
            if (g_pending[i].created < p->created) p = &g_pending[i];
        }
        pending_clear(p);
        p->used = true;
        p->tag = req->client_tag;
        p->created = now;
        snprintf(p->username, sizeof(p->username), "%.31s", req->username);
        memcpy(p->client_nonce, req->client_nonce, SEC_NONCE_LEN);
        sec_random(p->server_nonce, SEC_NONCE_LEN);

        const UserRecord *u = valid_username(p->username) ? find_user(p->username) : NULL;
        p->known_user = (u != NULL);
        if (u)
            memcpy(p->user_key, u->key, SEC_KEY_LEN);
        else
            sec_random(p->user_key, SEC_KEY_LEN);
        if (!valid_username(p->username))
            strcpy(p->username, "?");
    }

    JoinChallenge ch;
    ch.client_tag = p->tag;
    memcpy(ch.server_nonce, p->server_nonce, SEC_NONCE_LEN);
    dds_return_t rc = dds_write(n->challenge_writer, &ch);
    if (rc != DDS_RETCODE_OK)
        fprintf(stderr, "dds_write(challenge): %s\n", dds_strretcode(-rc));
}

static void handle_join_proof(NetCtx *n, const JoinProof *proof, dds_time_t now)
{
    Pending *p = pending_find(proof->client_tag);
    if (p == NULL)
        return;

    uint8_t expected[SEC_MAC_LEN];
    sec_transcript_mac(p->user_key, SEC_LABEL_PROOF, p->tag, p->username,
                       p->client_nonce, p->server_nonce, 0, expected);
    bool ok = p->known_user && sec_equal(expected, proof->mac, SEC_MAC_LEN);

    JoinResponse resp;
    memset(&resp, 0, sizeof(resp));
    resp.client_tag = p->tag;

    if (!ok)
    {
        resp.int_player_id = JOIN_BAD_LOGIN;
        printf("[auth] REJECTED login for '%s' (wrong password or unknown user)\n", p->username);
    }
    else
    {
        pthread_mutex_lock(&g_mutex);
        int free_idx = -1;
        bool already = false;
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (arr_Players[i].b_active && strcmp(arr_Players[i].username, p->username) == 0)
                already = true;
            if (!arr_Players[i].b_active && free_idx < 0)
                free_idx = i;
        }

        if (already)
            resp.int_player_id = JOIN_ALREADY;
        else if (free_idx < 0)
            resp.int_player_id = JOIN_FULL;
        else
        {
            Player *pl = &arr_Players[free_idx];
            int id = free_idx + 1;
            memset(pl, 0, sizeof(*pl));
            pl->b_active = true;
            snprintf(pl->username, sizeof(pl->username), "%s", p->username);
            sec_transcript_mac(p->user_key, SEC_LABEL_SESSION, p->tag, p->username,
                               p->client_nonce, p->server_nonce, id, pl->session_key);
            pl->session_gen = ++g_session_counter;
            pl->last_input_time = now;
            g_game.paddle_y[free_idx] = (HEIGHT - PADDLE_H) / 2.0f;
            resp.int_player_id = id;
        }
        pthread_mutex_unlock(&g_mutex);

        if (resp.int_player_id > 0)
            printf("[auth] '%s' logged in as Player %d (%s)\n", p->username, resp.int_player_id,
                   resp.int_player_id == 1 ? "left" : "right");
        else
            printf("[auth] '%s' authenticated but not admitted (%s)\n", p->username,
                   resp.int_player_id == JOIN_FULL ? "server full" : "already in game");
    }

    if (p->known_user)
        sec_transcript_mac(p->user_key, SEC_LABEL_ACCEPT, p->tag, p->username,
                           p->client_nonce, p->server_nonce, resp.int_player_id, resp.mac);

    dds_return_t rc = dds_write(n->response_writer, &resp);
    if (rc != DDS_RETCODE_OK)
        fprintf(stderr, "dds_write(response): %s\n", dds_strretcode(-rc));

    sec_wipe(expected, sizeof(expected));
    pending_clear(p);
}

static void handle_input(const Input *in, dds_time_t now)
{
    int id = in->int_player_id;
    if (id < 1 || id > MAX_PLAYERS)
        return;
    int idx = id - 1;

    uint8_t key[SEC_KEY_LEN];
    pthread_mutex_lock(&g_mutex);
    bool active = arr_Players[idx].b_active;
    uint32_t gen = arr_Players[idx].session_gen;
    uint64_t last_seq = arr_Players[idx].last_rx_seq;
    memcpy(key, arr_Players[idx].session_key, SEC_KEY_LEN);
    pthread_mutex_unlock(&g_mutex);

    if (!active || in->seq <= last_seq)
    {
        sec_wipe(key, sizeof(key));
        return;
    }

    uint8_t plain[INPUT_PLAIN_LEN];
    if (sec_open(plain, in->data, sizeof(in->data), SEC_DIR_C2S, id, in->seq, key) != 0)
    {
        static dds_time_t last_warn = 0;
        if (now - last_warn > DDS_SECS(1))
        {
            printf("[security] Dropped forged/tampered input claiming to be Player %d\n", id);
            last_warn = now;
        }
        sec_wipe(key, sizeof(key));
        return;
    }
    sec_wipe(key, sizeof(key));

    int8_t dir = (int8_t)plain[0];
    if (dir < -1) dir = -1;
    if (dir > 1)  dir = 1;

    pthread_mutex_lock(&g_mutex);
    Player *p = &arr_Players[idx];
    if (p->b_active && p->session_gen == gen && in->seq > p->last_rx_seq)
    {
        p->last_rx_seq = in->seq;
        p->last_input_time = now;
        p->dir = dir;
        if (plain[1] & INPUT_FLAG_LEAVE)
            remove_player_locked(idx, "left the game");
    }
    pthread_mutex_unlock(&g_mutex);
}

static void *network_worker(void *arg)
{
    NetCtx *n = (NetCtx *)arg;

    JoinRequest  buf_join[MAX_TAKE];
    JoinProof    buf_proof[MAX_TAKE];
    Input        buf_input[MAX_TAKE];
    void        *samples[MAX_TAKE];
    dds_sample_info_t infos[MAX_TAKE];
    dds_return_t rc;

    while (!atomic_load(&g_stop))
    {
        dds_waitset_wait(n->waitset, NULL, 0, DDS_MSECS(100));
        dds_time_t now = dds_time();

        for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_join[i];
        rc = dds_take(n->join_reader, samples, infos, MAX_TAKE, MAX_TAKE);
        for (int i = 0; i < rc; i++)
            if (infos[i].valid_data)
                handle_join_request(n, &buf_join[i], now);

        for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_proof[i];
        rc = dds_take(n->proof_reader, samples, infos, MAX_TAKE, MAX_TAKE);
        for (int i = 0; i < rc; i++)
            if (infos[i].valid_data)
                handle_join_proof(n, &buf_proof[i], now);

        for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_input[i];
        rc = dds_take(n->input_reader, samples, infos, MAX_TAKE, MAX_TAKE);
        for (int i = 0; i < rc; i++)
            if (infos[i].valid_data)
                handle_input(&buf_input[i], now);

        for (int i = 0; i < MAX_PENDING; i++)
            if (g_pending[i].used && now - g_pending[i].created > PENDING_TTL)
                pending_clear(&g_pending[i]);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (sec_init() != 0)
    {
        fprintf(stderr, "libsodium failed to initialise\n");
        return 1;
    }

    resolve_users_path();

    if (argc == 4 && strcmp(argv[1], "adduser") == 0)
        return add_user(argv[2], argv[3]);
    if (argc != 1)
    {
        fprintf(stderr, "Usage: %s                         run the server\n"
                        "       %s adduser <name> <pass>   add a login to %s\n",
                argv[0], argv[0], g_users_path);
        return 1;
    }

    if (load_users() <= 0)
    {
        fprintf(stderr, "No users found in %s. Add one with: %s adduser <name> <password>\n",
                g_users_path, argv[0]);
        return 1;
    }
    printf("Loaded %d user(s) from %s\n", g_num_users, g_users_path);

    signal(SIGINT, on_sigint);

    PongDds dds;
    if (pong_dds_create(&dds) != 0)
        return 1;

    NetCtx net;
    net.join_reader      = pong_reader(&dds, dds.t_join,      dds.qos_handshake);
    net.proof_reader     = pong_reader(&dds, dds.t_proof,     dds.qos_handshake);
    net.input_reader     = pong_reader(&dds, dds.t_input,     dds.qos_input);
    net.challenge_writer = pong_writer(&dds, dds.t_challenge, dds.qos_handshake);
    net.response_writer  = pong_writer(&dds, dds.t_response,  dds.qos_handshake);
    net.waitset = dds_create_waitset(dds.participant);
    pong_attach(net.waitset, net.join_reader);
    pong_attach(net.waitset, net.proof_reader);
    pong_attach(net.waitset, net.input_reader);

    dds_entity_t state_writer = pong_writer(&dds, dds.t_state, dds.qos_state);

    game_to_waiting(&g_game);
    g_game.paddle_y[0] = g_game.paddle_y[1] = (HEIGHT - PADDLE_H) / 2.0f;

    pthread_t thread;
    if (pthread_create(&thread, NULL, network_worker, &net) != 0)
    {
        perror("pthread_create");
        return 1;
    }

    printf("Secure Pong server running. Waiting for players... (Ctrl+C to stop)\n");

    const dds_duration_t tick = DDS_NSECS_IN_SEC / TICK_HZ;
    const float dt = 1.0f / TICK_HZ;
    dds_time_t next_tick = dds_time();

    dds_time_t stats_start = next_tick;
    uint64_t stats_ticks = 0;
    dds_duration_t stats_work_total = 0, stats_work_max = 0;

    while (!atomic_load(&g_stop))
    {
        dds_time_t t0 = dds_time();

        GameSnapshot snap;
        int     send_count = 0;
        int     send_id[MAX_PLAYERS];
        uint64_t send_seq[MAX_PLAYERS];
        uint8_t send_key[MAX_PLAYERS][SEC_KEY_LEN];

        pthread_mutex_lock(&g_mutex);
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (arr_Players[i].b_active && t0 - arr_Players[i].last_input_time > PLAYER_TIMEOUT)
                remove_player_locked(i, "timed out (no messages for 3 s)");

        game_update(&g_game, dt);
        game_snapshot(&g_game, &snap);

        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (!arr_Players[i].b_active)
                continue;
            send_id[send_count] = i + 1;
            send_seq[send_count] = ++arr_Players[i].tx_seq;
            memcpy(send_key[send_count], arr_Players[i].session_key, SEC_KEY_LEN);
            send_count++;
        }
        pthread_mutex_unlock(&g_mutex);

        uint8_t plain[STATE_PLAIN_LEN];
        pack_state(&snap, plain);

        for (int k = 0; k < send_count; k++)
        {
            GameState gs;
            gs.int_player_id = send_id[k];
            gs.seq = send_seq[k];
            sec_seal(gs.data, plain, STATE_PLAIN_LEN, SEC_DIR_S2C, send_id[k], send_seq[k], send_key[k]);
            dds_return_t rc = dds_write(state_writer, &gs);
            if (rc != DDS_RETCODE_OK)
                fprintf(stderr, "dds_write(state): %s\n", dds_strretcode(-rc));
        }
        sec_wipe(send_key, sizeof(send_key));

        dds_time_t t1 = dds_time();
        dds_duration_t work = t1 - t0;
        stats_ticks++;
        stats_work_total += work;
        if (work > stats_work_max) stats_work_max = work;
        if (t1 - stats_start >= STATS_PERIOD)
        {
            double secs = (double)(t1 - stats_start) / DDS_NSECS_IN_SEC;
            printf("[perf] %.1f ticks/s | work per tick: avg %.1f us, max %.1f us | players: %d\n",
                   stats_ticks / secs,
                   stats_work_total / (double)stats_ticks / 1000.0,
                   stats_work_max / 1000.0, send_count);
            stats_start = t1;
            stats_ticks = 0;
            stats_work_total = stats_work_max = 0;
        }

        next_tick += tick;
        if (t1 > next_tick + 5 * tick)
            next_tick = t1;
        dds_duration_t wait = next_tick - dds_time();
        if (wait > 0)
            dds_sleepfor(wait);
    }

    printf("\nShutting down...\n");
    pthread_join(thread, NULL);
    pong_dds_destroy(&dds);
    for (int i = 0; i < MAX_PLAYERS; i++)
        sec_wipe(arr_Players[i].session_key, SEC_KEY_LEN);
    sec_wipe(g_users, sizeof(g_users));
    return 0;
}
