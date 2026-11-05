#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "protocol.h"
#include "users.h"
#include "game.h"

typedef struct
{
    bool       active;
    char       username[USERNAME_LEN];
    uint8_t    session_key[SEC_KEY_LEN];
    uint32_t   session_gen;
    uint64_t   last_rx_seq;
    uint64_t   tx_seq;
    dds_time_t last_input_time;
    int8_t     dir;
    bool       pause_seen;
    uint8_t    pause_presses;
} Player;

typedef struct
{
    bool     active;
    uint32_t session_gen;
    uint64_t last_rx_seq;
    uint8_t  session_key[SEC_KEY_LEN];
} SessionView;

typedef struct
{
    int      count;
    int      player_id[MAX_PLAYERS];
    uint64_t seq[MAX_PLAYERS];
    uint8_t  session_key[MAX_PLAYERS][SEC_KEY_LEN];
} Outbox;

typedef struct
{
    pthread_mutex_t mutex;
    Player          players[MAX_PLAYERS];
    Game            game;
    uint32_t        session_counter;
} World;

void world_init(World *w);
void world_destroy(World *w);

void world_lock(World *w);
void world_unlock(World *w);

int      world_admit_locked(World *w, const char *username, dds_time_t now);
uint8_t *world_session_key_locked(World *w, int player_id);

bool world_session_locked(World *w, int player_id, SessionView *out);
void world_apply_input_locked(World *w, int player_id, uint32_t session_gen, uint64_t seq,
                              const InputPayload *in, dds_time_t now);

void world_tick_locked(World *w, float dt, dds_time_t now, GameSnapshot *snap, Outbox *out);

#endif
