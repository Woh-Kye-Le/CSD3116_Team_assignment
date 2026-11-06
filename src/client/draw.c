/**
 * @file    draw.c
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Rendering and drawing logic.
 *
 * @version 1.0
 */
#include <GLFW/glfw3.h>
#include "draw.h"
#include "textures.h"
#include "protocol.h"
#include "stb_easy_font.h"

static const Rgb k_player_rgb[MAX_PLAYERS] =
{
    { 0.30f, 0.90f, 0.95f },
    { 0.85f, 0.30f, 0.90f },
};

/**
 * @brief Gets the RGB color for a given player.
 *
 * @param idx The index of the player.
 * @return The RGB color of the player.
 */
Rgb draw_player_rgb(int idx)
{
    return k_player_rgb[idx == 0 ? 0 : 1];
}

/**
 * @brief Draws text at the specified coordinates.
 *
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param scale The scaling factor for the text.
 * @param text The string to draw.
 */
void draw_text(float x, float y, float scale, const char *text)
{
    static char buffer[65536];
    int num_quads = stb_easy_font_print(0, 0, (char *)text, NULL, buffer, sizeof(buffer));

    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buffer);
    glDrawArrays(GL_QUADS, 0, num_quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
}

/**
 * @brief Draws horizontally centered text at the specified Y coordinate.
 *
 * @param cx The center X coordinate.
 * @param y The Y coordinate.
 * @param scale The scaling factor for the text.
 * @param text The string to draw.
 */
void draw_text_centered(float cx, float y, float scale, const char *text)
{
    float w = (float)stb_easy_font_width((char *)text) * scale;
    draw_text(cx - w / 2.0f, y, scale, text);
}

/**
 * @brief Draws a filled rectangle.
 *
 * @param x The X coordinate of the top-left corner.
 * @param y The Y coordinate of the top-left corner.
 * @param w The width of the rectangle.
 * @param h The height of the rectangle.
 */
void draw_rect(float x, float y, float w, float h)
{
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

/**
 * @brief Sets the OpenGL drawing color to the specified player's color.
 *
 * @param idx The index of the player.
 * @param brightness The brightness multiplier.
 */
void draw_set_player_color(int idx, float brightness)
{
    Rgb c = draw_player_rgb(idx);
    glColor3f(c.r * brightness, c.g * brightness, c.b * brightness);
}

/**
 * @brief Draws the game background.
 *
 * @param with_center_line True to draw the center line.
 */
void draw_background(bool with_center_line)
{
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    tex_draw_background();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.35f);
    draw_rect(0, 0, PADDLE_MARGIN - 8, HEIGHT);
    draw_rect(WIDTH - PADDLE_MARGIN + 8, 0, PADDLE_MARGIN - 8, HEIGHT);
    glDisable(GL_BLEND);

    if (with_center_line)
    {
        glColor3f(0.42f, 0.42f, 0.45f);
        for (int y = 0; y < HEIGHT; y += 20)
            draw_rect(WIDTH / 2.0f - 2, (float)y + 4, 4, 12);
    }
}

/**
 * @brief Draws a dim overlay over the entire screen.
 *
 * @param alpha The alpha (opacity) of the overlay.
 */
void draw_dim_overlay(float alpha)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, alpha);
    draw_rect(0, 0, WIDTH, HEIGHT);
    glDisable(GL_BLEND);
}

/**
 * @brief Draws a colored flash overlay over the entire screen.
 *
 * @param color The color of the flash.
 * @param alpha The alpha (opacity) of the flash.
 */
void draw_flash(Rgb color, float alpha)
{
    if (alpha <= 0.0f)
        return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(color.r, color.g, color.b, alpha);
    draw_rect(0, 0, WIDTH, HEIGHT);
    glDisable(GL_BLEND);
}

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
void draw_button(float x, float y, float w, float h, const char *label, bool selected)
{
    if (selected)
    {
        tex_draw_glow(x + w / 2.0f, y + h / 2.0f, w * 1.5f, h * 2.6f, 0.30f, 0.90f, 0.95f, 0.45f);
        glColor3f(0.30f, 0.90f, 0.95f);
        draw_rect(x, y, w, h);
        glColor3f(0.08f, 0.09f, 0.10f);
    }
    else
    {
        glColor3f(0.42f, 0.42f, 0.45f);
        draw_rect(x, y, w, h);
        glColor3f(0.11f, 0.12f, 0.14f);
        draw_rect(x + 2, y + 2, w - 4, h - 4);
        glColor3f(0.90f, 0.90f, 0.90f);
    }
    draw_text_centered(x + w / 2.0f, y + h / 2.0f - 10.0f, 3.0f, label);
}

/**
 * @brief Draws a player's paddle.
 *
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param idx The index of the player.
 * @param brightness The brightness multiplier.
 */
void draw_paddle(float x, float y, int idx, float brightness)
{
    Rgb c = draw_player_rgb(idx);
    tex_draw_glow(x + PADDLE_W / 2.0f, y + PADDLE_H / 2.0f, PADDLE_W * 6.0f, PADDLE_H * 1.7f,
                  c.r, c.g, c.b, 0.55f * brightness);

    glBegin(GL_QUADS);
    glColor3f(c.r * brightness * 1.15f, c.g * brightness * 1.15f, c.b * brightness * 1.15f);
    glVertex2f(x, y);
    glVertex2f(x + PADDLE_W, y);
    glColor3f(c.r * brightness * 0.65f, c.g * brightness * 0.65f, c.b * brightness * 0.65f);
    glVertex2f(x + PADDLE_W, y + PADDLE_H);
    glVertex2f(x, y + PADDLE_H);
    glEnd();

    glColor3f(brightness, brightness, brightness);
    draw_rect(x + 2, y + 4, 2, PADDLE_H - 8);
}

/**
 * @brief Draws the ball.
 *
 * @param cx The center X coordinate.
 * @param cy The center Y coordinate.
 * @param brightness The brightness multiplier.
 */
void draw_ball(float cx, float cy, float brightness)
{
    tex_draw_glow(cx, cy, BALL_SIZE * 5.0f, BALL_SIZE * 5.0f, 0.85f, 0.95f, 1.0f, 0.75f * brightness);
    glColor3f(brightness, brightness, brightness);
    draw_rect(cx - BALL_SIZE / 2.0f, cy - BALL_SIZE / 2.0f, BALL_SIZE, BALL_SIZE);
}
