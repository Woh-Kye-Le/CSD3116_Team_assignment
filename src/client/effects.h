#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdbool.h>
#include "protocol.h"
#include "draw.h"

void effects_reset(void);
void effects_observe(const GameSnapshot *s);
void effects_update(float dt, bool paused);
bool effects_active(void);

void  effects_draw_trail(void);
void  effects_draw_particles(void);
void  effects_shake_offset(float *dx, float *dy);
float effects_score_scale(int idx);
float effects_countdown_scale(void);
void  effects_draw_flash(void);

void effects_menu_update(float dt);
void effects_menu_draw(void);
float effects_time(void);

#endif
