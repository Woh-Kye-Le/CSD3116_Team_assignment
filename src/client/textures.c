/**
 * @file    textures.c
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Texture loading and management.
 *
 * @version 1.0
 */
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <GLFW/glfw3.h>
#include "textures.h"
#include "protocol.h"

#define BG_SIZE   512
#define GLOW_SIZE 64

static GLuint g_bg_tex = 0;
static GLuint g_glow_tex = 0;

/**
 * @brief Hashes a 32-bit integer.
 *
 * @param x The input integer.
 * @return The hashed 32-bit integer.
 */
static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/**
 * @brief Converts a float in [0, 1] to an unsigned char in [0, 255].
 *
 * @param v The float value.
 * @return The converted unsigned byte.
 */
static unsigned char to_byte(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return (unsigned char)(v * 255.0f + 0.5f);
}

/**
 * @brief Uploads pixel data to an OpenGL texture.
 *
 * @param w The width of the texture.
 * @param h The height of the texture.
 * @param format The OpenGL format of the pixel data.
 * @param pixels Pointer to the pixel data.
 * @return The generated OpenGL texture ID.
 */
static GLuint upload(int w, int h, GLenum format, const unsigned char *pixels)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)format, w, h, 0, format, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

/**
 * @brief Builds the background texture pixel data.
 *
 * @param px Pointer to the pixel buffer to fill.
 */
static void build_background(unsigned char *px)
{
    for (int y = 0; y < BG_SIZE; y++)
    {
        for (int x = 0; x < BG_SIZE; x++)
        {
            float u = ((float)x + 0.5f) / BG_SIZE * 2.0f - 1.0f;
            float v = ((float)y + 0.5f) / BG_SIZE * 2.0f - 1.0f;
            float vignette = 1.0f - 0.45f * (u * u * 0.9f + v * v);
            if (vignette < 0.45f) vignette = 0.45f;

            float noise = ((float)(hash32((uint32_t)(y * BG_SIZE + x)) & 255) / 255.0f - 0.5f) * 0.03f;
            float grid = (x % 32 == 0 || y % 32 == 0) ? 0.02f : 0.0f;
            float scan = (y % 4 == 0) ? -0.008f : 0.0f;
            float add = grid + noise + scan;

            unsigned char *p = px + (y * BG_SIZE + x) * 3;
            p[0] = to_byte((0.13f + add) * vignette);
            p[1] = to_byte((0.14f + add) * vignette);
            p[2] = to_byte((0.17f + add) * vignette);
        }
    }
}

/**
 * @brief Builds the glow texture pixel data.
 *
 * @param px Pointer to the pixel buffer to fill.
 */
static void build_glow(unsigned char *px)
{
    for (int y = 0; y < GLOW_SIZE; y++)
    {
        for (int x = 0; x < GLOW_SIZE; x++)
        {
            float u = ((float)x + 0.5f) / GLOW_SIZE * 2.0f - 1.0f;
            float v = ((float)y + 0.5f) / GLOW_SIZE * 2.0f - 1.0f;
            float a = 1.0f - sqrtf(u * u + v * v);
            if (a < 0.0f) a = 0.0f;

            unsigned char *p = px + (y * GLOW_SIZE + x) * 4;
            p[0] = p[1] = p[2] = 255;
            p[3] = to_byte(a * a);
        }
    }
}

/**
 * @brief Draws a textured quad.
 *
 * @param tex The OpenGL texture ID.
 * @param x The X coordinate of the quad.
 * @param y The Y coordinate of the quad.
 * @param w The width of the quad.
 * @param h The height of the quad.
 */
static void textured_quad(GLuint tex, float x, float y, float w, float h)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x + w, y);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x + w, y + h);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y + h);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

/**
 * @brief Initializes the texture system and generates textures.
 */
void textures_init(void)
{
    unsigned char *bg = malloc(BG_SIZE * BG_SIZE * 3);
    unsigned char *glow = malloc(GLOW_SIZE * GLOW_SIZE * 4);
    if (bg && glow)
    {
        build_background(bg);
        build_glow(glow);
        g_bg_tex = upload(BG_SIZE, BG_SIZE, GL_RGB, bg);
        g_glow_tex = upload(GLOW_SIZE, GLOW_SIZE, GL_RGBA, glow);
    }
    free(bg);
    free(glow);
}

/**
 * @brief Destroys the texture system and frees OpenGL resources.
 */
void textures_destroy(void)
{
    if (g_bg_tex)
        glDeleteTextures(1, &g_bg_tex);
    if (g_glow_tex)
        glDeleteTextures(1, &g_glow_tex);
    g_bg_tex = g_glow_tex = 0;
}

/**
 * @brief Draws the background texture over the entire screen.
 */
void tex_draw_background(void)
{
    glColor3f(1.0f, 1.0f, 1.0f);
    textured_quad(g_bg_tex, 0.0f, 0.0f, WIDTH, HEIGHT);
}

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
void tex_draw_glow(float cx, float cy, float w, float h, float r, float g, float b, float a)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(r, g, b, a);
    textured_quad(g_glow_tex, cx - w / 2.0f, cy - h / 2.0f, w, h);
    glDisable(GL_BLEND);
}
