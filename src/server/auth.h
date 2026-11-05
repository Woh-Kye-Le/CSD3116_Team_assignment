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

void auth_init(Auth *a, World *world, dds_entity_t challenge_writer, dds_entity_t response_writer);
void auth_destroy(Auth *a);

void auth_on_join_request(Auth *a, const JoinRequest *req, dds_time_t now);
void auth_on_join_proof(Auth *a, const JoinProof *proof, dds_time_t now);
void auth_expire(Auth *a, dds_time_t now);

#endif
