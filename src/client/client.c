#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>

#include "pong_common.h"
#include "stb_easy_font.h"

#define MAX_TAKE        8
#define JOIN_RETRY      DDS_SECS(1)
#define AUTH_TIMEOUT    DDS_SECS(2)

enum { CONN_IDLE, CONN_CONNECTING, CONN_AUTHENTICATING, CONN_JOINED, CONN_REJECTED };
enum { REQ_NONE, REQ_PLAY, REQ_LEAVE };
enum { SCREEN_MENU, SCREEN_INSTRUCTIONS, SCREEN_GAME };

typedef struct
{
    int          conn;
    int          player_id;
    bool         have_snap;
    GameSnapshot snap;
    char         status[96];
} ClientView;

static ClientView      g_view = { .conn = CONN_IDLE, .player_id = -1, .status = "" };
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static atomic_int  g_input_dir = 0;
static atomic_bool g_quit = false;
static atomic_int  g_request = REQ_NONE;
static atomic_uint g_pause_presses = 0;

static char    g_username[32];
static uint8_t g_user_key[SEC_KEY_LEN];

static void view_set(int conn, int player_id, const char *status)
{
    pthread_mutex_lock(&g_mutex);
    g_view.conn = conn;
    g_view.player_id = player_id;
    if (player_id < 0)
        g_view.have_snap = false;
    snprintf(g_view.status, sizeof(g_view.status), "%s", status);
    pthread_mutex_unlock(&g_mutex);
}

typedef struct
{
    dds_entity_t join_writer, proof_writer, input_writer;
    dds_entity_t challenge_reader, response_reader, state_reader;
    dds_entity_t waitset;
} NetCtx;

typedef struct
{
    uint64_t tag;
    uint8_t  client_nonce[SEC_NONCE_LEN];
    uint8_t  server_nonce[SEC_NONCE_LEN];
    uint8_t  session_key[SEC_KEY_LEN];
    int      player_id;
    uint64_t tx_seq, last_rx_seq;
} Session;

static void session_new_attempt(Session *s)
{
    sec_wipe(s, sizeof(*s));
    sec_random(&s->tag, sizeof(s->tag));
    sec_random(s->client_nonce, SEC_NONCE_LEN);
}

static void send_join_request(NetCtx *n, const Session *s)
{
    JoinRequest req;
    memset(&req, 0, sizeof(req));
    req.client_tag = s->tag;
    snprintf(req.username, sizeof(req.username), "%s", g_username);
    memcpy(req.client_nonce, s->client_nonce, SEC_NONCE_LEN);
    dds_write(n->join_writer, &req);
}

static void send_input(NetCtx *n, Session *s, int8_t dir, uint8_t flags, uint8_t pause_presses)
{
    uint8_t plain[INPUT_PLAIN_LEN] = { (uint8_t)dir, flags, pause_presses, 0 };
    Input in;
    in.int_player_id = s->player_id;
    in.seq = ++s->tx_seq;
    sec_seal(in.data, plain, INPUT_PLAIN_LEN, SEC_DIR_C2S, s->player_id, in.seq, s->session_key);
    dds_write(n->input_writer, &in);
}

static void send_leave(NetCtx *n, Session *s)
{
    uint8_t presses = (uint8_t)atomic_load(&g_pause_presses);
    for (int i = 0; i < 3; i++)
    {
        send_input(n, s, 0, INPUT_FLAG_LEAVE, presses);
        dds_sleepfor(DDS_MSECS(20));
    }
    printf("Left the game\n");
}

static void *worker(void *arg)
{
    NetCtx *n = (NetCtx *)arg;

    JoinChallenge buf_chal[MAX_TAKE];
    JoinResponse  buf_resp[MAX_TAKE];
    GameState     buf_state[MAX_TAKE];
    void         *samples[MAX_TAKE];
    dds_sample_info_t infos[MAX_TAKE];
    dds_return_t rc;

    Session s;
    session_new_attempt(&s);
    int conn = CONN_IDLE;
    dds_time_t last_join_sent = 0, auth_started = 0, last_state_rx = 0, last_input_sent = 0;
    int8_t last_dir_sent = 0;
    uint8_t last_presses_sent = 0;

    while (!atomic_load(&g_quit))
    {
        dds_waitset_wait(n->waitset, NULL, 0, DDS_MSECS(5));
        dds_time_t now = mono_now();

        int req = atomic_exchange(&g_request, REQ_NONE);
        if (req == REQ_PLAY && (conn == CONN_IDLE || conn == CONN_REJECTED))
        {
            session_new_attempt(&s);
            conn = CONN_CONNECTING;
            last_join_sent = 0;
            view_set(conn, -1, "Connecting to server...");
        }
        else if (req == REQ_LEAVE && conn != CONN_IDLE)
        {
            if (conn == CONN_JOINED)
                send_leave(n, &s);
            session_new_attempt(&s);
            conn = CONN_IDLE;
            view_set(conn, -1, "");
        }

        for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_chal[i];
        rc = dds_take(n->challenge_reader, samples, infos, MAX_TAKE, MAX_TAKE);
        for (int i = 0; i < rc; i++)
        {
            if (!infos[i].valid_data || conn != CONN_CONNECTING || buf_chal[i].client_tag != s.tag)
                continue;
            memcpy(s.server_nonce, buf_chal[i].server_nonce, SEC_NONCE_LEN);

            JoinProof proof;
            proof.client_tag = s.tag;
            sec_transcript_mac(g_user_key, SEC_LABEL_PROOF, s.tag, g_username,
                               s.client_nonce, s.server_nonce, 0, proof.mac);
            dds_write(n->proof_writer, &proof);
            conn = CONN_AUTHENTICATING;
            auth_started = now;
            view_set(conn, -1, "Authenticating...");
        }

        for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_resp[i];
        rc = dds_take(n->response_reader, samples, infos, MAX_TAKE, MAX_TAKE);
        for (int i = 0; i < rc; i++)
        {
            const JoinResponse *r = &buf_resp[i];
            if (!infos[i].valid_data || conn != CONN_AUTHENTICATING || r->client_tag != s.tag)
                continue;

            uint8_t expected[SEC_MAC_LEN];
            sec_transcript_mac(g_user_key, SEC_LABEL_ACCEPT, s.tag, g_username,
                               s.client_nonce, s.server_nonce, r->int_player_id, expected);
            bool server_genuine = sec_equal(expected, r->mac, SEC_MAC_LEN);

            if (r->int_player_id > 0 && server_genuine)
            {
                s.player_id = r->int_player_id;
                sec_transcript_mac(g_user_key, SEC_LABEL_SESSION, s.tag, g_username,
                                   s.client_nonce, s.server_nonce, s.player_id, s.session_key);
                s.tx_seq = s.last_rx_seq = 0;
                conn = CONN_JOINED;
                last_state_rx = now;
                last_input_sent = 0;
                view_set(conn, s.player_id, "");
                printf("Logged in as Player %d (%s paddle)\n", s.player_id, s.player_id == 1 ? "left" : "right");
            }
            else if (r->int_player_id > 0)
            {
                printf("WARNING: server failed to prove its identity - ignoring it\n");
                view_set(CONN_CONNECTING, -1, "Server could not be verified - retrying...");
                conn = CONN_CONNECTING;
                session_new_attempt(&s);
            }
            else if (r->int_player_id == JOIN_BAD_LOGIN)
            {
                conn = CONN_REJECTED;
                view_set(conn, -1, "Login failed: wrong username or password");
                printf("Login failed: wrong username or password\n");
            }
            else
            {
                conn = CONN_CONNECTING;
                view_set(conn, -1, r->int_player_id == JOIN_FULL
                         ? "Server full (2 players). Retrying..."
                         : "This user is already in the game. Retrying...");
                session_new_attempt(&s);
                last_join_sent = now;
            }
        }

        bool got_state = false;
        GameSnapshot snap;
        for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_state[i];
        rc = dds_take(n->state_reader, samples, infos, MAX_TAKE, MAX_TAKE);
        for (int i = 0; i < rc; i++)
        {
            const GameState *gs = &buf_state[i];
            if (!infos[i].valid_data || conn != CONN_JOINED ||
                gs->int_player_id != s.player_id || gs->seq <= s.last_rx_seq)
                continue;

            uint8_t plain[STATE_PLAIN_LEN];
            if (sec_open(plain, gs->data, sizeof(gs->data), SEC_DIR_S2C, s.player_id, gs->seq, s.session_key) != 0)
                continue;
            s.last_rx_seq = gs->seq;
            unpack_state(plain, &snap);
            got_state = true;
        }
        if (got_state)
        {
            last_state_rx = now;
            pthread_mutex_lock(&g_mutex);
            g_view.snap = snap;
            g_view.have_snap = true;
            pthread_mutex_unlock(&g_mutex);
        }

        switch (conn)
        {
        case CONN_CONNECTING:
            if (now - last_join_sent > JOIN_RETRY)
            {
                send_join_request(n, &s);
                last_join_sent = now;
            }
            break;

        case CONN_AUTHENTICATING:
            if (now - auth_started > AUTH_TIMEOUT)
            {
                conn = CONN_CONNECTING;
                session_new_attempt(&s);
                last_join_sent = 0;
            }
            break;

        case CONN_JOINED:
        {
            int8_t dir = (int8_t)atomic_load(&g_input_dir);
            uint8_t presses = (uint8_t)atomic_load(&g_pause_presses);
            if (dir != last_dir_sent || presses != last_presses_sent || now - last_input_sent > HEARTBEAT)
            {
                send_input(n, &s, dir, 0, presses);
                last_dir_sent = dir;
                last_presses_sent = presses;
                last_input_sent = now;
            }
            if (now - last_state_rx > SERVER_TIMEOUT)
            {
                printf("Lost connection to server - reconnecting\n");
                conn = CONN_CONNECTING;
                session_new_attempt(&s);
                view_set(conn, -1, "Connection lost. Reconnecting...");
            }
            break;
        }

        case CONN_IDLE:
        case CONN_REJECTED:
            break;
        }
    }

    if (conn == CONN_JOINED)
        send_leave(n, &s);
    sec_wipe(&s, sizeof(s));
    return NULL;
}

static void draw_text(float x, float y, float scale, const char *text)
{
    static char buffer[65536];
    int num_quads = stb_easy_font_print(0, 0, (char *)text, NULL, buffer, sizeof(buffer));

    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buffer);
    glDrawArrays(GL_QUADS, 0, num_quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
}

static void draw_text_centered(float cx, float y, float scale, const char *text)
{
    float w = (float)stb_easy_font_width((char *)text) * scale;
    draw_text(cx - w / 2.0f, y, scale, text);
}

static void draw_rect(float x, float y, float w, float h)
{
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void set_player_color(int idx, float brightness)
{
    if (idx == 0) glColor3f(0.30f * brightness, 0.90f * brightness, 0.95f * brightness);
    else          glColor3f(0.85f * brightness, 0.30f * brightness, 0.90f * brightness);
}

#define MENU_ITEMS   3
#define MENU_ITEM_W  320.0f
#define MENU_ITEM_H  54.0f
#define MENU_TOP     270.0f
#define MENU_GAP     72.0f
#define BACK_X       (WIDTH / 2.0f - 110.0f)
#define BACK_Y       538.0f
#define BACK_W       220.0f
#define BACK_H       44.0f

static const char *k_menu_items[MENU_ITEMS] = { "PLAY", "INSTRUCTIONS", "QUIT" };

static atomic_bool g_need_redraw = true;
static int  g_screen = SCREEN_MENU;
static int  g_menu_sel = 0;
static bool g_back_hover = false;

static void draw_background(bool with_center_line)
{
    glClearColor(0.13f, 0.14f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(0.10f, 0.11f, 0.12f);
    draw_rect(0, 0, PADDLE_MARGIN - 8, HEIGHT);
    draw_rect(WIDTH - PADDLE_MARGIN + 8, 0, PADDLE_MARGIN - 8, HEIGHT);

    if (with_center_line)
    {
        glColor3f(0.42f, 0.42f, 0.45f);
        for (int y = 0; y < HEIGHT; y += 20)
            draw_rect(WIDTH / 2.0f - 2, (float)y + 4, 4, 12);
    }
}

static void draw_dim_overlay(float alpha)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, alpha);
    draw_rect(0, 0, WIDTH, HEIGHT);
    glDisable(GL_BLEND);
}

static void draw_button(float x, float y, float w, float h, const char *label, bool selected)
{
    if (selected)
    {
        glColor3f(0.30f, 0.90f, 0.95f);
        draw_rect(x, y, w, h);
        glColor3f(0.08f, 0.09f, 0.10f);
    }
    else
    {
        glColor3f(0.42f, 0.42f, 0.45f);
        draw_rect(x, y, w, h);
        glColor3f(0.13f, 0.14f, 0.16f);
        draw_rect(x + 2, y + 2, w - 4, h - 4);
        glColor3f(0.90f, 0.90f, 0.90f);
    }
    draw_text_centered(x + w / 2.0f, y + h / 2.0f - 10.0f, 3.0f, label);
}

static float menu_item_y(int i)
{
    return MENU_TOP + (float)i * MENU_GAP;
}

static int menu_item_at(double mx, double my)
{
    float x = WIDTH / 2.0f - MENU_ITEM_W / 2.0f;
    for (int i = 0; i < MENU_ITEMS; i++)
    {
        float y = menu_item_y(i);
        if (mx >= x && mx <= x + MENU_ITEM_W && my >= y && my <= y + MENU_ITEM_H)
            return i;
    }
    return -1;
}

static bool back_button_at(double mx, double my)
{
    return mx >= BACK_X && mx <= BACK_X + BACK_W && my >= BACK_Y && my <= BACK_Y + BACK_H;
}

static void render_menu(void)
{
    draw_background(false);

    glColor3f(0.30f, 0.90f, 0.95f);
    draw_rect(70, 110, PADDLE_W, PADDLE_H);
    glColor3f(0.85f, 0.30f, 0.90f);
    draw_rect(WIDTH - 70 - PADDLE_W, 70, PADDLE_W, PADDLE_H);
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_rect(600, 225, BALL_SIZE, BALL_SIZE);

    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text_centered(WIDTH / 2.0f, 70, 10.0f, "PONG");
    glColor3f(0.55f, 0.55f, 0.60f);
    draw_text_centered(WIDTH / 2.0f, 190, 2.5f, "SECURE DDS EDITION");

    for (int i = 0; i < MENU_ITEMS; i++)
        draw_button(WIDTH / 2.0f - MENU_ITEM_W / 2.0f, menu_item_y(i), MENU_ITEM_W, MENU_ITEM_H,
                    k_menu_items[i], i == g_menu_sel);

    char buf[96];
    glColor3f(0.55f, 0.55f, 0.60f);
    draw_text_centered(WIDTH / 2.0f, 500, 1.8f, "Up/Down + Enter, or click with the mouse");
    snprintf(buf, sizeof(buf), "User: %s", g_username);
    draw_text_centered(WIDTH / 2.0f, 560, 2.0f, buf);
}

enum { LINE_HEADER, LINE_TEXT, LINE_KEY };

typedef struct
{
    int         kind;
    const char *a;
    const char *b;
} InstructionLine;

static const InstructionLine k_instructions[] =
{
    { LINE_HEADER, "CONNECTING", NULL },
    { LINE_TEXT,   "1. Start the server first:   ./bin/server", NULL },
    { LINE_TEXT,   "2. Each player runs:   ./bin/client <username>", NULL },
    { LINE_TEXT,   "3. Type your password in the terminal.", NULL },
    { LINE_TEXT,   "4. Press PLAY. The match starts when 2 players join.", NULL },
    { LINE_TEXT,   "Two laptops: same Wi-Fi, then in every terminal run", NULL },
    { LINE_TEXT,   "   source scripts/lan.sh <other laptop IP>   (see README)", NULL },
    { LINE_HEADER, "CONTROLS", NULL },
    { LINE_KEY,    "W / S  or  Up / Down", "move your paddle" },
    { LINE_KEY,    "Esc", "pause / resume for BOTH players" },
    { LINE_KEY,    "Q", "leave the match, back to this menu" },
    { LINE_HEADER, "RULES", NULL },
    { LINE_TEXT,   "Get the ball past your opponent's paddle to score.", NULL },
    { LINE_TEXT,   "Paddle edges give steep angles; the ball speeds up.", NULL },
    { LINE_TEXT,   "First to 5 points wins, then a new match starts.", NULL },
};

static void render_instructions(void)
{
    draw_background(false);

    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text_centered(WIDTH / 2.0f, 22, 4.0f, "INSTRUCTIONS");

    float y = 82.0f;
    for (size_t i = 0; i < sizeof(k_instructions) / sizeof(k_instructions[0]); i++)
    {
        const InstructionLine *l = &k_instructions[i];
        switch (l->kind)
        {
        case LINE_HEADER:
            y += 8.0f;
            glColor3f(0.30f, 0.90f, 0.95f);
            draw_text(60, y, 2.4f, l->a);
            y += 28.0f;
            break;
        case LINE_TEXT:
            glColor3f(0.88f, 0.88f, 0.90f);
            draw_text(80, y, 1.9f, l->a);
            y += 22.0f;
            break;
        case LINE_KEY:
            glColor3f(0.85f, 0.30f, 0.90f);
            draw_text(80, y, 1.9f, l->a);
            glColor3f(0.88f, 0.88f, 0.90f);
            draw_text(330, y, 1.9f, l->b);
            y += 22.0f;
            break;
        }
    }

    draw_button(BACK_X, BACK_Y, BACK_W, BACK_H, "BACK", g_back_hover);
}

static void render_game(const ClientView *v)
{
    draw_background(true);

    if (!v->have_snap)
    {
        glColor3f(0.9f, 0.9f, 0.9f);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 60, 3.0f, v->status);
        glColor3f(0.55f, 0.55f, 0.6f);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f + 10, 2.0f, "Press Q or Esc to go back to the menu");
        char who[64];
        snprintf(who, sizeof(who), "User: %s", g_username);
        draw_text_centered(WIDTH / 2.0f, HEIGHT - 40, 2.0f, who);
        return;
    }

    const GameSnapshot *s = &v->snap;
    int me = v->player_id - 1;

    char buf[128];
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        set_player_color(i, 1.0f);
        snprintf(buf, sizeof(buf), "%d", s->score[i]);
        draw_text_centered(WIDTH / 2.0f + (i == 0 ? -70.0f : 70.0f), 25, 6.0f, buf);
    }

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        float x = (i == 0) ? PADDLE_MARGIN : WIDTH - PADDLE_MARGIN - PADDLE_W;
        bool active = (s->active_mask >> i) & 1;
        set_player_color(i, active ? 1.0f : 0.35f);
        draw_rect(x, s->paddle_y[i], PADDLE_W, PADDLE_H);
        if (i == me)
        {
            glColor3f(1.0f, 1.0f, 1.0f);
            draw_text_centered(x + PADDLE_W / 2.0f, s->paddle_y[i] - 18, 2.0f, "YOU");
        }
    }

    if (s->phase == PHASE_PLAYING || s->phase == PHASE_COUNTDOWN)
    {
        glColor3f(1.0f, 1.0f, 1.0f);
        draw_rect(s->ball_x - BALL_SIZE / 2.0f, s->ball_y - BALL_SIZE / 2.0f, BALL_SIZE, BALL_SIZE);
    }

    if (!s->paused)
    {
        glColor3f(1.0f, 1.0f, 1.0f);
        switch (s->phase)
        {
        case PHASE_WAITING:
            draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 70, 3.0f, "Waiting for an opponent...");
            break;
        case PHASE_COUNTDOWN:
            snprintf(buf, sizeof(buf), "%d", s->countdown);
            draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 110, 7.0f, buf);
            break;
        case PHASE_GAMEOVER:
            snprintf(buf, sizeof(buf), "%s", (s->winner == v->player_id) ? "YOU WIN!" : "YOU LOSE");
            draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 80, 6.0f, buf);
            snprintf(buf, sizeof(buf), "Player %d wins %d - %d", s->winner, s->score[0], s->score[1]);
            draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f, 2.5f, buf);
            break;
        default:
            break;
        }
    }

    glColor3f(0.55f, 0.55f, 0.6f);
    snprintf(buf, sizeof(buf), "%s - Player %d (%s)    W/S or Up/Down: move    Esc: pause    Q: leave",
             g_username, v->player_id, me == 0 ? "left" : "right");
    draw_text_centered(WIDTH / 2.0f, HEIGHT - 22, 1.6f, buf);

    if (s->paused)
    {
        draw_dim_overlay(0.6f);
        glColor3f(1.0f, 1.0f, 1.0f);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 70, 8.0f, "PAUSED");
        if (s->paused_by == v->player_id)
            snprintf(buf, sizeof(buf), "Paused by you");
        else
            snprintf(buf, sizeof(buf), "Paused by Player %d", s->paused_by);
        glColor3f(0.75f, 0.75f, 0.80f);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f + 20, 2.5f, buf);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f + 60, 2.0f, "Esc: resume      Q: leave to menu");
    }
}

static void on_window_refresh(GLFWwindow *window)
{
    (void)window;
    atomic_store(&g_need_redraw, true);
}

static void open_game(void)
{
    g_screen = SCREEN_GAME;
    atomic_store(&g_request, REQ_PLAY);
}

static void leave_to_menu(void)
{
    atomic_store(&g_request, REQ_LEAVE);
    g_screen = SCREEN_MENU;
}

static void activate_menu_item(GLFWwindow *window, int item)
{
    switch (item)
    {
    case 0: open_game(); break;
    case 1: g_screen = SCREEN_INSTRUCTIONS; g_back_hover = false; break;
    case 2: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
    default: break;
    }
}

static void on_key(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if (action != GLFW_PRESS)
        return;

    switch (g_screen)
    {
    case SCREEN_MENU:
        if (key == GLFW_KEY_UP || key == GLFW_KEY_W)
            g_menu_sel = (g_menu_sel + MENU_ITEMS - 1) % MENU_ITEMS;
        else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S)
            g_menu_sel = (g_menu_sel + 1) % MENU_ITEMS;
        else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE)
            activate_menu_item(window, g_menu_sel);
        break;

    case SCREEN_INSTRUCTIONS:
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER ||
            key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_SPACE)
            g_screen = SCREEN_MENU;
        break;

    case SCREEN_GAME:
    {
        pthread_mutex_lock(&g_mutex);
        bool in_match = g_view.conn == CONN_JOINED && g_view.have_snap;
        pthread_mutex_unlock(&g_mutex);

        if (key == GLFW_KEY_Q)
            leave_to_menu();
        else if (key == GLFW_KEY_ESCAPE)
        {
            if (in_match)
                atomic_fetch_add(&g_pause_presses, 1);
            else
                leave_to_menu();
        }
        break;
    }
    }
}

static void on_cursor(GLFWwindow *window, double mx, double my)
{
    (void)window;
    if (g_screen == SCREEN_MENU)
    {
        int item = menu_item_at(mx, my);
        if (item >= 0)
            g_menu_sel = item;
    }
    else if (g_screen == SCREEN_INSTRUCTIONS)
        g_back_hover = back_button_at(mx, my);
}

static void on_mouse_button(GLFWwindow *window, int button, int action, int mods)
{
    (void)mods;
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    if (g_screen == SCREEN_MENU)
    {
        int item = menu_item_at(mx, my);
        if (item >= 0)
            activate_menu_item(window, item);
    }
    else if (g_screen == SCREEN_INSTRUCTIONS && back_button_at(mx, my))
        g_screen = SCREEN_MENU;
}

static void read_hidden(const char *prompt, char *buf, size_t len)
{
    struct termios old_t, new_t;
    bool tty = isatty(STDIN_FILENO);

    printf("%s", prompt);
    fflush(stdout);
    if (tty)
    {
        tcgetattr(STDIN_FILENO, &old_t);
        new_t = old_t;
        new_t.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_t);
    }
    if (!fgets(buf, (int)len, stdin))
        buf[0] = '\0';
    if (tty)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
        printf("\n");
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (sec_init() != 0)
    {
        fprintf(stderr, "libsodium failed to initialise\n");
        return 1;
    }

    char password[128];
    if (argc >= 2)
        snprintf(g_username, sizeof(g_username), "%s", argv[1]);
    else
    {
        printf("Username: ");
        fflush(stdout);
        if (!fgets(g_username, sizeof(g_username), stdin))
            return 1;
        g_username[strcspn(g_username, "\r\n")] = '\0';
    }
    if (argc >= 3)
    {
        snprintf(password, sizeof(password), "%s", argv[2]);
        fprintf(stderr, "Note: passing the password on the command line is insecure; omit it to be prompted.\n");
    }
    else
        read_hidden("Password: ", password, sizeof(password));

    if (g_username[0] == '\0')
    {
        fprintf(stderr, "Username required\n");
        return 1;
    }

    printf("Deriving login key...\n");
    if (sec_derive_user_key(g_username, password, g_user_key) != 0)
    {
        fprintf(stderr, "Key derivation failed\n");
        return 1;
    }
    sec_wipe(password, sizeof(password));

    PongDds dds;
    if (pong_dds_create(&dds) != 0)
        return 1;

    NetCtx net;
    net.join_writer      = pong_writer(&dds, dds.t_join,      dds.qos_handshake);
    net.proof_writer     = pong_writer(&dds, dds.t_proof,     dds.qos_handshake);
    net.input_writer     = pong_writer(&dds, dds.t_input,     dds.qos_input);
    net.challenge_reader = pong_reader(&dds, dds.t_challenge, dds.qos_handshake);
    net.response_reader  = pong_reader(&dds, dds.t_response,  dds.qos_handshake);
    net.state_reader     = pong_reader(&dds, dds.t_state,     dds.qos_state);
    net.waitset = dds_create_waitset(dds.participant);
    pong_attach(net.waitset, net.challenge_reader);
    pong_attach(net.waitset, net.response_reader);
    pong_attach(net.waitset, net.state_reader);

    pthread_t thread;
    if (pthread_create(&thread, NULL, worker, &net) != 0)
    {
        perror("pthread_create");
        return 1;
    }

    if (!glfwInit())
    {
        atomic_store(&g_quit, true);
        pthread_join(thread, NULL);
        return 1;
    }
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    char title[96];
    snprintf(title, sizeof(title), "Secure DDS Pong - %s", g_username);
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, title, NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        atomic_store(&g_quit, true);
        pthread_join(thread, NULL);
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetKeyCallback(window, on_key);
    glfwSetCursorPosCallback(window, on_cursor);
    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetWindowRefreshCallback(window, on_window_refresh);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIDTH, HEIGHT, 0, -1, 1);

    typedef struct
    {
        int        screen;
        int        menu_sel;
        bool       back_hover;
        ClientView view;
    } FrameKey;

    const dds_duration_t frame_time = DDS_NSECS_IN_SEC / TICK_HZ;
    dds_time_t next_frame = mono_now();
    FrameKey last_frame;
    memset(&last_frame, 0, sizeof(last_frame));
    last_frame.screen = -1;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int dir = 0;
        if (g_screen == SCREEN_GAME)
        {
            bool up   = glfwGetKey(window, GLFW_KEY_UP)   == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
            bool down = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
            dir = (up && !down) ? -1 : (down && !up) ? 1 : 0;
        }
        atomic_store(&g_input_dir, dir);

        FrameKey frame;
        memset(&frame, 0, sizeof(frame));
        frame.screen = g_screen;
        frame.menu_sel = g_menu_sel;
        frame.back_hover = g_back_hover;
        if (g_screen == SCREEN_GAME)
        {
            pthread_mutex_lock(&g_mutex);
            frame.view = g_view;
            pthread_mutex_unlock(&g_mutex);
        }

        bool changed = atomic_exchange(&g_need_redraw, false) ||
                       memcmp(&frame, &last_frame, sizeof(frame)) != 0;
        if (changed)
        {
            switch (g_screen)
            {
            case SCREEN_MENU:
                render_menu();
                break;
            case SCREEN_INSTRUCTIONS:
                render_instructions();
                break;
            case SCREEN_GAME:
                render_game(&frame.view);
                break;
            }
            glfwSwapBuffers(window);
            last_frame = frame;
        }

        next_frame += frame_time;
        dds_time_t now = mono_now();
        if (now > next_frame + frame_time)
            next_frame = now;
        else if (next_frame > now)
            dds_sleepfor(next_frame - now);
    }

    atomic_store(&g_quit, true);
    pthread_join(thread, NULL);
    glfwDestroyWindow(window);
    glfwTerminate();
    pong_dds_destroy(&dds);
    sec_wipe(g_user_key, sizeof(g_user_key));
    return 0;
}
