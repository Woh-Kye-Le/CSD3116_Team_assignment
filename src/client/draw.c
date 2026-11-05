#include <GLFW/glfw3.h>
#include "draw.h"
#include "protocol.h"
#include "stb_easy_font.h"

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

void draw_text_centered(float cx, float y, float scale, const char *text)
{
    float w = (float)stb_easy_font_width((char *)text) * scale;
    draw_text(cx - w / 2.0f, y, scale, text);
}

void draw_rect(float x, float y, float w, float h)
{
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void draw_set_player_color(int idx, float brightness)
{
    if (idx == 0) glColor3f(0.30f * brightness, 0.90f * brightness, 0.95f * brightness);
    else          glColor3f(0.85f * brightness, 0.30f * brightness, 0.90f * brightness);
}

void draw_background(bool with_center_line)
{
    glClearColor(0.13f, 0.14f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(0.10f, 0.11f, 0.12f);
    draw_rect(0, 0, PADDLE_MARGIN - 8, HEIGHT);
    draw_rect(WIDTH - PADDLE_MARGIN + 8, 0, PADDLE_MARGIN - 8, HEIGHT);

    if (with_center_line)
    {
        glColor3f(0.42f, 0.42f, 0.45f);
        for (int y = 0; y < HEIGHT; y += 20)
            draw_rect(WIDTH / 2.0f - 2, (float)y + 4, 4, 12);
    }
}

void draw_dim_overlay(float alpha)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, alpha);
    draw_rect(0, 0, WIDTH, HEIGHT);
    glDisable(GL_BLEND);
}

void draw_button(float x, float y, float w, float h, const char *label, bool selected)
{
    if (selected)
    {
        glColor3f(0.30f, 0.90f, 0.95f);
        draw_rect(x, y, w, h);
        glColor3f(0.08f, 0.09f, 0.10f);
    }
    else
    {
        glColor3f(0.42f, 0.42f, 0.45f);
        draw_rect(x, y, w, h);
        glColor3f(0.13f, 0.14f, 0.16f);
        draw_rect(x + 2, y + 2, w - 4, h - 4);
        glColor3f(0.90f, 0.90f, 0.90f);
    }
    draw_text_centered(x + w / 2.0f, y + h / 2.0f - 10.0f, 3.0f, label);
}
