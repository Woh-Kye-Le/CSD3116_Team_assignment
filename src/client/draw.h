/**
 * @file    draw.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Rendering and drawing logic.
 *
 * @version 1.0
 */
#ifndef DRAW_H
#define DRAW_H

#include <stdbool.h>

typedef struct
{
    float r, g, b;
} Rgb;

/**
 * @brief Gets the RGB color for a given player.
 *
 * @param idx The index of the player.
 * @return The RGB color of the player.
 */
Rgb draw_player_rgb(int idx);

/**
 * @brief Draws text at the specified coordinates.
 *
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param scale The scaling factor for the text.
 * @param text The string to draw.
 */
void draw_text(float x, float y, float scale, const char *text);

/**
 * @brief Draws horizontally centered text at the specified Y coordinate.
 *
 * @param cx The center X coordinate.
 * @param y The Y coordinate.
 * @param scale The scaling factor for the text.
 * @param text The string to draw.
 */
void draw_text_centered(float cx, float y, float scale, const char *text);

/**
 * @brief Draws a filled rectangle.
 *
 * @param x The X coordinate of the top-left corner.
 * @param y The Y coordinate of the top-left corner.
 * @param w The width of the rectangle.
 * @param h The height of the rectangle.
 */
void draw_rect(float x, float y, float w, float h);

/**
 * @brief Sets the OpenGL drawing color to the specified player's color.
 *
 * @param idx The index of the player.
 * @param brightness The brightness multiplier.
 */
void draw_set_player_color(int idx, float brightness);

/**
 * @brief Draws the game background.
 *
 * @param with_center_line True to draw the center line.
 */
void draw_background(bool with_center_line);

/**
 * @brief Draws a dim overlay over the entire screen.
 *
 * @param alpha The alpha (opacity) of the overlay.
 */
void draw_dim_overlay(float alpha);

/**
 * @brief Draws a colored flash overlay over the entire screen.
 *
 * @param color The color of the flash.
 * @param alpha The alpha (opacity) of the flash.
 */
void draw_flash(Rgb color, float alpha);

/**
 * @brief Draws a UI button.
 *
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param w The width of the button.
 * @param h The height of the button.
 * @param label The text on the button.
 * @param selected True if the button is selected/hovered.
 */
void draw_button(float x, float y, float w, float h, const char *label, bool selected);

/**
 * @brief Draws a player's paddle.
 *
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param idx The index of the player.
 * @param brightness The brightness multiplier.
 */
void draw_paddle(float x, float y, int idx, float brightness);

/**
 * @brief Draws the ball.
 *
 * @param cx The center X coordinate.
 * @param cy The center Y coordinate.
 * @param brightness The brightness multiplier.
 */
void draw_ball(float cx, float cy, float brightness);

#endif
