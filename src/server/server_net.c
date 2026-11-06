/**
 * @file    server_net.c
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Server-side networking and DDS communication.
 *
 * @version 1.0
 */
#include <stdio.h>
#include "server_net.h"

/**
 * @brief Handle incoming input from a client.
 * @param n Pointer to the ServerNet structure.
 * @param in Pointer to the Input message.
 * @param now The current monotonic time.
 */
static void handle_input(ServerNet *n, const Input *in, dds_time_t now)
{
    int id = in->int_player_id;
    if (id < 1 || id > MAX_PLAYERS)
        return;

    SessionView session;
    world_lock(n->world);
    world_session_locked(n->world, id, &session);
    world_unlock(n->world);

    if (!session.active || in->seq <= session.last_rx_seq)
    {
        sec_wipe(&session, sizeof(session));
        return;
    }

    uint8_t plain[INPUT_PLAIN_LEN];
    int rc = sec_open(plain, in->data, sizeof(in->data), SEC_DIR_C2S, id, in->seq, session.session_key);
    sec_wipe(&session.session_key, sizeof(session.session_key));
    if (rc != 0)
    {
        static dds_time_t last_warn = 0;
        if (now - last_warn > DDS_SECS(1))
        {
            printf("[security] Dropped forged/tampered input claiming to be Player %d\n", id);
            last_warn = now;
        }
        return;
    }

    InputPayload payload;
    unpack_input(plain, &payload);

    world_lock(n->world);
    world_apply_input_locked(n->world, id, session.session_gen, in->seq, &payload, now);
    world_unlock(n->world);
}

/**
 * @brief Initialize the server network state.
 * @param n Pointer to the ServerNet structure to initialize.
 * @param dds Pointer to the DDS setup structure.
 * @param world Pointer to the game world structure.
 * @param auth Pointer to the authentication structure.
 * @param stop Pointer to the atomic stop flag.
 */
void server_net_init(ServerNet *n, PongDds *dds, World *world, Auth *auth, atomic_bool *stop)
{
    n->world = world;
    n->auth = auth;
    n->stop = stop;
    n->join_reader  = pong_reader(dds, dds->t_join,  dds->qos_handshake);
    n->proof_reader = pong_reader(dds, dds->t_proof, dds->qos_handshake);
    n->input_reader = pong_reader(dds, dds->t_input, dds->qos_input);
    n->waitset = dds_create_waitset(dds->participant);
    pong_attach(n->waitset, n->join_reader);
    pong_attach(n->waitset, n->proof_reader);
    pong_attach(n->waitset, n->input_reader);
}

/**
 * @brief The worker thread loop for server networking.
 * @param arg Pointer to the ServerNet structure.
 * @return NULL.
 */
void *server_net_thread(void *arg)
{
    ServerNet *n = (ServerNet *)arg;

    JoinRequest  buf_join[MAX_TAKE];
    JoinProof    buf_proof[MAX_TAKE];
    Input        buf_input[MAX_TAKE];
    void        *samples[MAX_TAKE];
    dds_sample_info_t infos[MAX_TAKE];
    dds_return_t rc;

    while (!atomic_load(n->stop))
    {
        dds_waitset_wait(n->waitset, NULL, 0, DDS_MSECS(100));
        dds_time_t now = mono_now();

        for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_join[i];
        rc = dds_take(n->join_reader, samples, infos, MAX_TAKE, MAX_TAKE);
        for (int i = 0; i < rc; i++)
            if (infos[i].valid_data)
                auth_on_join_request(n->auth, &buf_join[i], now);

        for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_proof[i];
        rc = dds_take(n->proof_reader, samples, infos, MAX_TAKE, MAX_TAKE);
        for (int i = 0; i < rc; i++)
            if (infos[i].valid_data)
                auth_on_join_proof(n->auth, &buf_proof[i], now);

        for (int i = 0; i < MAX_TAKE; i++) samples[i] = &buf_input[i];
        rc = dds_take(n->input_reader, samples, infos, MAX_TAKE, MAX_TAKE);
        for (int i = 0; i < rc; i++)
            if (infos[i].valid_data)
                handle_input(n, &buf_input[i], now);

        auth_expire(n->auth, now);
    }
    return NULL;
}
