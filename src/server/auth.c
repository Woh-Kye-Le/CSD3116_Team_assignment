/**
 * @file    auth.c
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Authentication and security for the server.
 *
 * @version 1.0
 */
#include <stdio.h>
#include <string.h>
#include "auth.h"

/**
 * @brief Finds a pending login by client tag.
 *
 * @param a Pointer to the Auth structure.
 * @param tag The client tag to search for.
 * @return Pointer to the PendingLogin if found, or NULL.
 */
static PendingLogin *pending_find(Auth *a, uint64_t tag)
{
    for (int i = 0; i < MAX_PENDING; i++)
        if (a->pending[i].used && a->pending[i].tag == tag)
            return &a->pending[i];
    return NULL;
}

/**
 * @brief Clears a pending login entry securely.
 *
 * @param p Pointer to the PendingLogin entry.
 */
static void pending_clear(PendingLogin *p)
{
    sec_wipe(p, sizeof(*p));
}

/**
 * @brief Allocates a new pending login entry, evicting the oldest if full.
 *
 * @param a Pointer to the Auth structure.
 * @return Pointer to the allocated PendingLogin entry.
 */
static PendingLogin *pending_alloc(Auth *a)
{
    PendingLogin *p = &a->pending[0];
    for (int i = 0; i < MAX_PENDING; i++)
    {
        if (!a->pending[i].used)
            return &a->pending[i];
        if (a->pending[i].created < p->created)
            p = &a->pending[i];
    }
    return p;
}

/**
 * @brief Starts tracking a new pending login from a join request.
 *
 * @param p Pointer to the PendingLogin entry.
 * @param req Pointer to the JoinRequest.
 * @param now The current time.
 */
static void pending_start(PendingLogin *p, const JoinRequest *req, dds_time_t now)
{
    pending_clear(p);
    p->used = true;
    p->tag = req->client_tag;
    p->created = now;
    snprintf(p->username, sizeof(p->username), "%.31s", req->username);
    memcpy(p->client_nonce, req->client_nonce, SEC_NONCE_LEN);
    sec_random(p->server_nonce, SEC_NONCE_LEN);

    bool valid = users_name_valid(p->username);
    const UserRecord *u = valid ? users_find(p->username) : NULL;
    p->known_user = (u != NULL);
    if (u)
        memcpy(p->user_key, u->key, SEC_KEY_LEN);
    else
        sec_random(p->user_key, SEC_KEY_LEN);
    if (!valid)
        strcpy(p->username, "?");
}

/**
 * @brief Sends a join challenge to a client.
 *
 * @param a Pointer to the Auth structure.
 * @param p Pointer to the PendingLogin entry.
 */
static void send_challenge(Auth *a, const PendingLogin *p)
{
    JoinChallenge ch;
    ch.client_tag = p->tag;
    memcpy(ch.server_nonce, p->server_nonce, SEC_NONCE_LEN);
    dds_return_t rc = dds_write(a->challenge_writer, &ch);
    if (rc != DDS_RETCODE_OK)
        fprintf(stderr, "dds_write(challenge): %s\n", dds_strretcode(-rc));
}

/**
 * @brief Validates a join proof received from a client.
 *
 * @param p Pointer to the PendingLogin entry.
 * @param proof Pointer to the JoinProof.
 * @return true if the proof is valid, false otherwise.
 */
static bool proof_is_valid(const PendingLogin *p, const JoinProof *proof)
{
    uint8_t expected[SEC_MAC_LEN];
    sec_transcript_mac(p->user_key, SEC_LABEL_PROOF, p->tag, p->username,
                       p->client_nonce, p->server_nonce, 0, expected);
    bool ok = p->known_user && sec_equal(expected, proof->mac, SEC_MAC_LEN);
    sec_wipe(expected, sizeof(expected));
    return ok;
}

/**
 * @brief Admits an authenticated client into the game world.
 *
 * @param a Pointer to the Auth structure.
 * @param p Pointer to the PendingLogin entry.
 * @param now The current time.
 * @return The assigned player ID, or a negative code on failure.
 */
static int admit(Auth *a, const PendingLogin *p, dds_time_t now)
{
    world_lock(a->world);
    int id = world_admit_locked(a->world, p->username, now);
    if (id > 0)
        sec_transcript_mac(p->user_key, SEC_LABEL_SESSION, p->tag, p->username,
                           p->client_nonce, p->server_nonce, id,
                           world_session_key_locked(a->world, id));
    world_unlock(a->world);

    if (id > 0)
        printf("[auth] '%s' logged in as Player %d (%s)\n", p->username, id, id == 1 ? "left" : "right");
    else
        printf("[auth] '%s' authenticated but not admitted (%s)\n", p->username,
               id == JOIN_FULL ? "server full" : "already in game");
    return id;
}

/**
 * @brief Sends a join response back to the client.
 *
 * @param a Pointer to the Auth structure.
 * @param p Pointer to the PendingLogin entry.
 * @param player_id The player ID assigned (or an error code).
 */
static void send_response(Auth *a, const PendingLogin *p, int player_id)
{
    JoinResponse resp;
    memset(&resp, 0, sizeof(resp));
    resp.client_tag = p->tag;
    resp.int_player_id = player_id;
    if (p->known_user)
        sec_transcript_mac(p->user_key, SEC_LABEL_ACCEPT, p->tag, p->username,
                           p->client_nonce, p->server_nonce, player_id, resp.mac);

    dds_return_t rc = dds_write(a->response_writer, &resp);
    if (rc != DDS_RETCODE_OK)
        fprintf(stderr, "dds_write(response): %s\n", dds_strretcode(-rc));
}

/**
 * @brief Initializes the authentication subsystem.
 *
 * @param a Pointer to the Auth structure to initialize.
 * @param world Pointer to the World instance.
 * @param challenge_writer DDS writer entity for sending join challenges.
 * @param response_writer DDS writer entity for sending join responses.
 */
void auth_init(Auth *a, World *world, dds_entity_t challenge_writer, dds_entity_t response_writer)
{
    memset(a, 0, sizeof(*a));
    a->world = world;
    a->challenge_writer = challenge_writer;
    a->response_writer = response_writer;
}

/**
 * @brief Destroys the authentication subsystem and cleans up resources.
 *
 * @param a Pointer to the Auth structure.
 */
void auth_destroy(Auth *a)
{
    sec_wipe(a->pending, sizeof(a->pending));
}

/**
 * @brief Handles an incoming join request.
 *
 * @param a Pointer to the Auth structure.
 * @param req Pointer to the received JoinRequest.
 * @param now The current time.
 */
void auth_on_join_request(Auth *a, const JoinRequest *req, dds_time_t now)
{
    PendingLogin *p = pending_find(a, req->client_tag);
    if (p == NULL)
    {
        p = pending_alloc(a);
        pending_start(p, req, now);
    }
    send_challenge(a, p);
}

/**
 * @brief Handles an incoming join proof.
 *
 * @param a Pointer to the Auth structure.
 * @param proof Pointer to the received JoinProof.
 * @param now The current time.
 */
void auth_on_join_proof(Auth *a, const JoinProof *proof, dds_time_t now)
{
    PendingLogin *p = pending_find(a, proof->client_tag);
    if (p == NULL)
        return;

    int player_id;
    if (proof_is_valid(p, proof))
        player_id = admit(a, p, now);
    else
    {
        player_id = JOIN_BAD_LOGIN;
        printf("[auth] REJECTED login for '%s' (wrong password or unknown user)\n", p->username);
    }

    send_response(a, p, player_id);
    pending_clear(p);
}

/**
 * @brief Expires pending login requests that have timed out.
 *
 * @param a Pointer to the Auth structure.
 * @param now The current time.
 */
void auth_expire(Auth *a, dds_time_t now)
{
    for (int i = 0; i < MAX_PENDING; i++)
        if (a->pending[i].used && now - a->pending[i].created > PENDING_TTL)
            pending_clear(&a->pending[i]);
}
