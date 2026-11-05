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

void game_init(Game *g);
void game_reset_paddle(Game *g, int idx);

void game_update(Game *g, const bool active[MAX_PLAYERS], const int8_t dir[MAX_PLAYERS], float dt);
bool game_toggle_pause(Game *g, int player_id, bool both_present);

void game_snapshot(const Game *g, uint8_t active_mask, GameSnapshot *s);

#endif
