#ifndef TEXTURES_H
#define TEXTURES_H

void textures_init(void);
void textures_destroy(void);

void tex_draw_background(void);
void tex_draw_glow(float cx, float cy, float w, float h, float r, float g, float b, float a);

#endif
