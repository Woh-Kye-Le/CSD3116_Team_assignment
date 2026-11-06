/**
 * @file    textures.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Texture loading and management.
 *
 * @version 1.0
 */
#ifndef TEXTURES_H
#define TEXTURES_H

/**
 * @brief Initializes the texture system and generates textures.
 */
void textures_init(void);

/**
 * @brief Destroys the texture system and frees OpenGL resources.
 */
void textures_destroy(void);

/**
 * @brief Draws the background texture over the entire screen.
 */
void tex_draw_background(void);

/**
 * @brief Draws a colored glow effect using the glow texture.
 *
 * @param cx The center X coordinate.
 * @param cy The center Y coordinate.
 * @param w The width of the glow.
 * @param h The height of the glow.
 * @param r The red color component.
 * @param g The green color component.
 * @param b The blue color component.
 * @param a The alpha (opacity) component.
 */
void tex_draw_glow(float cx, float cy, float w, float h, float r, float g, float b, float a);

#endif
