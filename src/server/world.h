/**
 * @file    world.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   World state and entity management.
 *
 * @version 1.0
 */
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

/**
 * @brief Initializes the world state.
 * @param w Pointer to the World structure.
 */
void world_init(World *w);

/**
 * @brief Destroys the world state and cleans up resources.
 * @param w Pointer to the World structure.
 */
void world_destroy(World *w);

/**
 * @brief Locks the world state mutex for thread-safe access.
 * @param w Pointer to the World structure.
 */
void world_lock(World *w);

/**
 * @brief Unlocks the world state mutex.
 * @param w Pointer to the World structure.
 */
void world_unlock(World *w);

/**
 * @brief Admits a new player into the world.
 * @param w Pointer to the World structure.
 * @param username The username of the new player.
 * @param now The current time.
 * @return The assigned player ID, or an error code if joining failed.
 */
int      world_admit_locked(World *w, const char *username, dds_time_t now);

/**
 * @brief Gets the session key for a specific player.
 * @param w Pointer to the World structure.
 * @param player_id The ID of the player.
 * @return A pointer to the player's session key.
 */
uint8_t *world_session_key_locked(World *w, int player_id);

/**
 * @brief Retrieves the session view for a specific player.
 * @param w Pointer to the World structure.
 * @param player_id The ID of the player.
 * @param out Pointer to the SessionView structure to populate.
 * @return true if the player is active, false otherwise.
 */
bool world_session_locked(World *w, int player_id, SessionView *out);

/**
 * @brief Applies an input payload to a player's state.
 * @param w Pointer to the World structure.
 * @param player_id The ID of the player.
 * @param session_gen The session generation of the player.
 * @param seq The sequence number of the input payload.
 * @param in Pointer to the InputPayload structure.
 * @param now The current time.
 */
void world_apply_input_locked(World *w, int player_id, uint32_t session_gen, uint64_t seq,
                              const InputPayload *in, dds_time_t now);

/**
 * @brief Ticks the world state forward by one frame.
 * @param w Pointer to the World structure.
 * @param dt Time elapsed since the last tick in seconds.
 * @param now The current time.
 * @param snap Pointer to a GameSnapshot structure to populate with the current game state.
 * @param out Pointer to an Outbox structure to populate with outgoing messages.
 */
void world_tick_locked(World *w, float dt, dds_time_t now, GameSnapshot *snap, Outbox *out);

#endif
