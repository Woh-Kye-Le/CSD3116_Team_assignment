/**
 * @file    game.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Core gameplay logic and rules.
 *
 * @version 1.0
 */
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

typedef struct
{
    float   ball_x, ball_y, ball_vx, ball_vy, ball_speed;
    float   paddle_y[MAX_PLAYERS];
    uint8_t score[MAX_PLAYERS];
    int     phase;
    float   timer;
    int     winner;
    int     serve_dir;
    bool    paused;
    int     paused_by;
} Game;

/**
 * @brief Initializes the game state.
 * @param g Pointer to the Game structure to initialize.
 */
void game_init(Game *g);

/**
 * @brief Resets a player's paddle to its starting position.
 * @param g Pointer to the Game structure.
 * @param idx Index of the player (0 or 1).
 */
void game_reset_paddle(Game *g, int idx);

/**
 * @brief Updates the game state for one frame.
 * @param g Pointer to the Game structure.
 * @param active Array indicating whether each player is currently active.
 * @param dir Array containing the input direction of each player.
 * @param dt Time elapsed since the last update in seconds.
 */
void game_update(Game *g, const bool active[MAX_PLAYERS], const int8_t dir[MAX_PLAYERS], float dt);

/**
 * @brief Toggles the game pause state.
 * @param g Pointer to the Game structure.
 * @param player_id The ID of the player requesting the pause.
 * @param both_present Boolean indicating if both players are present.
 * @return true if the pause state was successfully toggled, false otherwise.
 */
bool game_toggle_pause(Game *g, int player_id, bool both_present);

/**
 * @brief Creates a snapshot of the current game state for network transmission.
 * @param g Pointer to the Game structure.
 * @param active_mask Bitmask of active players.
 * @param s Pointer to the GameSnapshot structure to populate.
 */
void game_snapshot(const Game *g, uint8_t active_mask, GameSnapshot *s);

#endif
