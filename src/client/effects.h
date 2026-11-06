/**
 * @file    effects.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Visual effects and particle systems.
 *
 * @version 1.0
 */
#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdbool.h>
#include "protocol.h"
#include "draw.h"

/**
 * @brief Resets all visual effects and particles.
 */
void effects_reset(void);

/**
 * @brief Observes the game snapshot to trigger effects based on events.
 *
 * @param s Pointer to the current game snapshot.
 */
void effects_observe(const GameSnapshot *s);

/**
 * @brief Updates the visual effects and particles over time.
 *
 * @param dt Delta time in seconds.
 * @param paused True if the game is currently paused.
 */
void effects_update(float dt, bool paused);

/**
 * @brief Checks if any visual effects are currently active.
 *
 * @return True if effects are active, false otherwise.
 */
bool effects_active(void);

/**
 * @brief Draws the trail behind the ball.
 */
void  effects_draw_trail(void);

/**
 * @brief Draws the active particles.
 */
void  effects_draw_particles(void);

/**
 * @brief Gets the current screen shake offset.
 *
 * @param dx Pointer to store the X offset.
 * @param dy Pointer to store the Y offset.
 */
void  effects_shake_offset(float *dx, float *dy);

/**
 * @brief Gets the scaling factor for a player's score display.
 *
 * @param idx The index of the player.
 * @return The current scaling factor for the score.
 */
float effects_score_scale(int idx);

/**
 * @brief Gets the scaling factor for the countdown display.
 *
 * @return The current scaling factor for the countdown.
 */
float effects_countdown_scale(void);

/**
 * @brief Draws a full-screen flash effect if active.
 */
void  effects_draw_flash(void);

/**
 * @brief Updates the visual effects for the main menu.
 *
 * @param dt Delta time in seconds.
 */
void effects_menu_update(float dt);

/**
 * @brief Draws the visual effects for the main menu.
 */
void effects_menu_draw(void);

/**
 * @brief Gets the total time accumulated by the effects system.
 *
 * @return The total elapsed time in seconds.
 */
float effects_time(void);

#endif
