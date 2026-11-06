/**
 * @file    auth.h
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Authentication and security for the server.
 *
 * @version 1.0
 */
#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"
#include "users.h"
#include "world.h"

#define MAX_PENDING 8
#define PENDING_TTL DDS_SECS(5)

typedef struct
{
    bool       used;
    uint64_t   tag;
    char       username[USERNAME_LEN];
    bool       known_user;
    uint8_t    user_key[SEC_KEY_LEN];
    uint8_t    client_nonce[SEC_NONCE_LEN];
    uint8_t    server_nonce[SEC_NONCE_LEN];
    dds_time_t created;
} PendingLogin;

typedef struct
{
    World       *world;
    dds_entity_t challenge_writer;
    dds_entity_t response_writer;
    PendingLogin pending[MAX_PENDING];
} Auth;

/**
 * @brief Initializes the authentication subsystem.
 *
 * @param a Pointer to the Auth structure to initialize.
 * @param world Pointer to the World instance.
 * @param challenge_writer DDS writer entity for sending join challenges.
 * @param response_writer DDS writer entity for sending join responses.
 */
void auth_init(Auth *a, World *world, dds_entity_t challenge_writer, dds_entity_t response_writer);

/**
 * @brief Destroys the authentication subsystem and cleans up resources.
 *
 * @param a Pointer to the Auth structure.
 */
void auth_destroy(Auth *a);

/**
 * @brief Handles an incoming join request.
 *
 * @param a Pointer to the Auth structure.
 * @param req Pointer to the received JoinRequest.
 * @param now The current time.
 */
void auth_on_join_request(Auth *a, const JoinRequest *req, dds_time_t now);

/**
 * @brief Handles an incoming join proof.
 *
 * @param a Pointer to the Auth structure.
 * @param proof Pointer to the received JoinProof.
 * @param now The current time.
 */
void auth_on_join_proof(Auth *a, const JoinProof *proof, dds_time_t now);

/**
 * @brief Expires pending login requests that have timed out.
 *
 * @param a Pointer to the Auth structure.
 * @param now The current time.
 */
void auth_expire(Auth *a, dds_time_t now);

#endif
