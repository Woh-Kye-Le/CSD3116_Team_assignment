/**
 * @file    client_net.c
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Client-side networking and DDS communication.
 *
 * @version 1.0
 */
#include <stdio.h>
#include <string.h>
#include "client_net.h"
#include "protocol.h"
#include "shared.h"

#define JOIN_RETRY   DDS_SECS(1)
#define AUTH_TIMEOUT DDS_SECS(2)

typedef struct
{
    uint64_t tag;
    uint8_t  client_nonce[SEC_NONCE_LEN];
    uint8_t  server_nonce[SEC_NONCE_LEN];
    uint8_t  session_key[SEC_KEY_LEN];
    int      player_id;
    uint64_t tx_seq;
    uint64_t last_rx_seq;
} Session;

typedef struct
{
    ClientNet  *net;
    Session     s;
    int         conn;
    dds_time_t  last_join_sent;
    dds_time_t  auth_started;
    dds_time_t  last_state_rx;
    dds_time_t  last_input_sent;
    int8_t      last_dir_sent;
    uint8_t     last_presses_sent;
} Worker;

/**
 * @brief Generate a new session attempt.
 * @param s Pointer to the Session structure to reset.
 */
static void session_new_attempt(Session *s)
{
    sec_wipe(s, sizeof(*s));
    sec_random(&s->tag, sizeof(s->tag));
    sec_random(s->client_nonce, SEC_NONCE_LEN);
}

/**
 * @brief Set the current connection state and update the shared view.
 * @param w Pointer to the Worker structure.
 * @param conn The new connection state.
 * @param player_id The player ID.
 * @param status A string describing the status.
 */
static void set_conn(Worker *w, int conn, int player_id, const char *status)
{
    w->conn = conn;
    shared_view_set_conn(conn, player_id, status);
}

/**
 * @brief Send a join request to the server.
 * @param w Pointer to the Worker structure.
 */
static void send_join_request(Worker *w)
{
    JoinRequest req;
    memset(&req, 0, sizeof(req));
    req.client_tag = w->s.tag;
    snprintf(req.username, sizeof(req.username), "%s", w->net->creds->username);
    memcpy(req.client_nonce, w->s.client_nonce, SEC_NONCE_LEN);
    dds_write(w->net->join_writer, &req);
}

/**
 * @brief Send player input to the server.
 * @param w Pointer to the Worker structure.
 * @param dir The paddle direction (-1, 0, or 1).
 * @param flags The input flags (e.g., leave game).
 * @param pause_presses The number of pause button presses.
 */
static void send_input(Worker *w, int8_t dir, uint8_t flags, uint8_t pause_presses)
{
    InputPayload payload = { .dir = dir, .flags = flags, .pause_presses = pause_presses };
    uint8_t plain[INPUT_PLAIN_LEN];
    pack_input(&payload, plain);

    Input in;
    in.int_player_id = w->s.player_id;
    in.seq = ++w->s.tx_seq;
    sec_seal(in.data, plain, INPUT_PLAIN_LEN, SEC_DIR_C2S, w->s.player_id, in.seq, w->s.session_key);
    dds_write(w->net->input_writer, &in);
}

/**
 * @brief Send a leave request to the server.
 * @param w Pointer to the Worker structure.
 */
static void send_leave(Worker *w)
{
    uint8_t presses = shared_pause_presses();
    for (int i = 0; i < 3; i++)
    {
        send_input(w, 0, INPUT_FLAG_LEAVE, presses);
        dds_sleepfor(DDS_MSECS(20));
    }
    printf("Left the game\n");
}

/**
 * @brief Restart the login process.
 * @param w Pointer to the Worker structure.
 */
static void restart_login(Worker *w)
{
    session_new_attempt(&w->s);
    w->conn = CONN_CONNECTING;
}

/**
 * @brief Handle UI requests like play and leave.
 * @param w Pointer to the Worker structure.
 */
static void handle_ui_request(Worker *w)
{
    int req = shared_take_request();
    if (req == REQ_PLAY && (w->conn == CONN_IDLE || w->conn == CONN_REJECTED))
    {
        session_new_attempt(&w->s);
        w->last_join_sent = 0;
        set_conn(w, CONN_CONNECTING, -1, "Connecting to server...");
    }
    else if (req == REQ_LEAVE && w->conn != CONN_IDLE)
    {
        if (w->conn == CONN_JOINED)
            send_leave(w);
        session_new_attempt(&w->s);
        set_conn(w, CONN_IDLE, -1, "");
    }
}

/**
 * @brief Process an incoming challenge from the server.
 * @param w Pointer to the Worker structure.
 * @param ch Pointer to the JoinChallenge message.
 * @param now The current monotonic time.
 */
static void on_challenge(Worker *w, const JoinChallenge *ch, dds_time_t now)
{
    if (w->conn != CONN_CONNECTING || ch->client_tag != w->s.tag)
        return;
    memcpy(w->s.server_nonce, ch->server_nonce, SEC_NONCE_LEN);

    const Credentials *c = w->net->creds;
    JoinProof proof;
    proof.client_tag = w->s.tag;
    sec_transcript_mac(c->user_key, SEC_LABEL_PROOF, w->s.tag, c->username,
                       w->s.client_nonce, w->s.server_nonce, 0, proof.mac);
    dds_write(w->net->proof_writer, &proof);

    w->auth_started = now;
    set_conn(w, CONN_AUTHENTICATING, -1, "Authenticating...");
}

/**
 * @brief Process an incoming response from the server.
 * @param w Pointer to the Worker structure.
 * @param r Pointer to the JoinResponse message.
 * @param now The current monotonic time.
 */
static void on_response(Worker *w, const JoinResponse *r, dds_time_t now)
{
    if (w->conn != CONN_AUTHENTICATING || r->client_tag != w->s.tag)
        return;

    const Credentials *c = w->net->creds;
    uint8_t expected[SEC_MAC_LEN];
    sec_transcript_mac(c->user_key, SEC_LABEL_ACCEPT, w->s.tag, c->username,
                       w->s.client_nonce, w->s.server_nonce, r->int_player_id, expected);
    bool server_genuine = sec_equal(expected, r->mac, SEC_MAC_LEN);
    sec_wipe(expected, sizeof(expected));

    if (r->int_player_id > 0 && server_genuine)
    {
        w->s.player_id = r->int_player_id;
        sec_transcript_mac(c->user_key, SEC_LABEL_SESSION, w->s.tag, c->username,
                           w->s.client_nonce, w->s.server_nonce, w->s.player_id, w->s.session_key);
        w->s.tx_seq = w->s.last_rx_seq = 0;
        w->last_state_rx = now;
        w->last_input_sent = 0;
        set_conn(w, CONN_JOINED, w->s.player_id, "");
        printf("Logged in as Player %d (%s paddle)\n", w->s.player_id, w->s.player_id == 1 ? "left" : "right");
    }
    else if (r->int_player_id > 0)
    {
        printf("WARNING: server failed to prove its identity - ignoring it\n");
        restart_login(w);
        set_conn(w, CONN_CONNECTING, -1, "Server could not be verified - retrying...");
    }
    else if (r->int_player_id == JOIN_BAD_LOGIN)
    {
        set_conn(w, CONN_REJECTED, -1, "Login failed: wrong username or password");
        printf("Login failed: wrong username or password\n");
    }
    else
    {
        restart_login(w);
        w->last_join_sent = now;
        set_conn(w, CONN_CONNECTING, -1, r->int_player_id == JOIN_FULL
                 ? "Server full (2 players). Retrying..."
                 : "This user is already in the game. Retrying...");
    }
}

/**
 * @brief Process an incoming game state update from the server.
 * @param w Pointer to the Worker structure.
 * @param gs Pointer to the GameState message.
 * @param out Pointer to store the unpacked game snapshot.
 * @return true if the state was successfully unpacked and valid, false otherwise.
 */
static bool on_state(Worker *w, const GameState *gs, GameSnapshot *out)
{
    if (w->conn != CONN_JOINED || gs->int_player_id != w->s.player_id || gs->seq <= w->s.last_rx_seq)
        return false;

    uint8_t plain[STATE_PLAIN_LEN];
    if (sec_open(plain, gs->data, sizeof(gs->data), SEC_DIR_S2C, w->s.player_id, gs->seq, w->s.session_key) != 0)
        return false;
    w->s.last_rx_seq = gs->seq;
    unpack_state(plain, out);
    return true;
}

/**
 * @brief Drain incoming DDS readers and process their samples.
 * @param w Pointer to the Worker structure.
 * @param now The current monotonic time.
 */
static void drain_readers(Worker *w, dds_time_t now)
{
    JoinChallenge buf_chal[MAX_TAKE];
    JoinResponse  buf_resp[MAX_TAKE];
    GameState     buf_state[MAX_TAKE];
    void         *samples[MAX_TAKE];
    dds_sample_info_t infos[MAX_TAKE];
    dds_return_t rc;

    for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_chal[i];
    rc = dds_take(w->net->challenge_reader, samples, infos, MAX_TAKE, MAX_TAKE);
    for (int i = 0; i < rc; i++)
        if (infos[i].valid_data)
            on_challenge(w, &buf_chal[i], now);

    for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_resp[i];
    rc = dds_take(w->net->response_reader, samples, infos, MAX_TAKE, MAX_TAKE);
    for (int i = 0; i < rc; i++)
        if (infos[i].valid_data)
            on_response(w, &buf_resp[i], now);

    bool got_state = false;
    GameSnapshot snap;
    for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_state[i];
    rc = dds_take(w->net->state_reader, samples, infos, MAX_TAKE, MAX_TAKE);
    for (int i = 0; i < rc; i++)
        if (infos[i].valid_data && on_state(w, &buf_state[i], &snap))
            got_state = true;

    if (got_state)
    {
        w->last_state_rx = now;
        shared_view_set_snapshot(&snap);
    }
}

/**
 * @brief Send input to the server if the state changed or heartbeat is needed.
 * @param w Pointer to the Worker structure.
 * @param now The current monotonic time.
 */
static void send_input_if_needed(Worker *w, dds_time_t now)
{
    int8_t dir = shared_input_dir();
    uint8_t presses = shared_pause_presses();
    if (dir != w->last_dir_sent || presses != w->last_presses_sent || now - w->last_input_sent > HEARTBEAT)
    {
        send_input(w, dir, 0, presses);
        w->last_dir_sent = dir;
        w->last_presses_sent = presses;
        w->last_input_sent = now;
    }
}

/**
 * @brief Run network timers (retries, timeouts, heartbeats).
 * @param w Pointer to the Worker structure.
 * @param now The current monotonic time.
 */
static void run_timers(Worker *w, dds_time_t now)
{
    switch (w->conn)
    {
    case CONN_CONNECTING:
        if (now - w->last_join_sent > JOIN_RETRY)
        {
            send_join_request(w);
            w->last_join_sent = now;
        }
        break;

    case CONN_AUTHENTICATING:
        if (now - w->auth_started > AUTH_TIMEOUT)
        {
            restart_login(w);
            w->last_join_sent = 0;
        }
        break;

    case CONN_JOINED:
        send_input_if_needed(w, now);
        if (now - w->last_state_rx > SERVER_TIMEOUT)
        {
            printf("Lost connection to server - reconnecting\n");
            restart_login(w);
            set_conn(w, CONN_CONNECTING, -1, "Connection lost. Reconnecting...");
        }
        break;

    default:
        break;
    }
}

/**
 * @brief The worker thread loop for client networking.
 * @param arg Pointer to the ClientNet structure.
 * @return NULL.
 */
static void *worker_thread(void *arg)
{
    Worker w;
    memset(&w, 0, sizeof(w));
    w.net = (ClientNet *)arg;
    w.conn = CONN_IDLE;
    session_new_attempt(&w.s);

    while (!shared_quit_requested())
    {
        dds_waitset_wait(w.net->waitset, NULL, 0, DDS_MSECS(5));
        dds_time_t now = mono_now();

        handle_ui_request(&w);
        drain_readers(&w, now);
        run_timers(&w, now);
    }

    if (w.conn == CONN_JOINED)
        send_leave(&w);
    sec_wipe(&w.s, sizeof(w.s));
    return NULL;
}

/**
 * @brief Initialize the client network state.
 * @param n Pointer to the ClientNet structure to initialize.
 * @param dds Pointer to the DDS setup structure.
 * @param creds Pointer to the user credentials.
 */
void client_net_init(ClientNet *n, PongDds *dds, const Credentials *creds)
{
    n->creds = creds;
    n->join_writer      = pong_writer(dds, dds->t_join,      dds->qos_handshake);
    n->proof_writer     = pong_writer(dds, dds->t_proof,     dds->qos_handshake);
    n->input_writer     = pong_writer(dds, dds->t_input,     dds->qos_input);
    n->challenge_reader = pong_reader(dds, dds->t_challenge, dds->qos_handshake);
    n->response_reader  = pong_reader(dds, dds->t_response,  dds->qos_handshake);
    n->state_reader     = pong_reader(dds, dds->t_state,     dds->qos_state);
    n->waitset = dds_create_waitset(dds->participant);
    pong_attach(n->waitset, n->challenge_reader);
    pong_attach(n->waitset, n->response_reader);
    pong_attach(n->waitset, n->state_reader);
}

/**
 * @brief Start the client network worker thread.
 * @param n Pointer to the initialized ClientNet structure.
 * @return 0 on success, or a non-zero error code on failure.
 */
int client_net_start(ClientNet *n)
{
    if (pthread_create(&n->thread, NULL, worker_thread, n) != 0)
    {
        perror("pthread_create");
        return -1;
    }
    return 0;
}

/**
 * @brief Stop the client network worker thread and clean up.
 * @param n Pointer to the ClientNet structure.
 */
void client_net_stop(ClientNet *n)
{
    shared_request_quit();
    pthread_join(n->thread, NULL);
}
