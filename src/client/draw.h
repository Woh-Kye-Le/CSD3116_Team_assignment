#ifndef DRAW_H
#define DRAW_H

#include <stdbool.h>

typedef struct
{
    float r, g, b;
} Rgb;

Rgb draw_player_rgb(int idx);

void draw_text(float x, float y, float scale, const char *text);
void draw_text_centered(float cx, float y, float scale, const char *text);
void draw_rect(float x, float y, float w, float h);

void draw_set_player_color(int idx, float brightness);
void draw_background(bool with_center_line);
void draw_dim_overlay(float alpha);
void draw_flash(Rgb color, float alpha);
void draw_button(float x, float y, float w, float h, const char *label, bool selected);

void draw_paddle(float x, float y, int idx, float brightness);
void draw_ball(float cx, float cy, float brightness);

#endif
