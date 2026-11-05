#include <stdio.h>
#include <math.h>
#include "game.h"

static void center_ball(Game *g)
{
    g->ball_x = WIDTH / 2.0f;
    g->ball_y = HEIGHT / 2.0f;
    g->ball_vx = g->ball_vy = 0.0f;
}

static void to_waiting(Game *g)
{
    g->phase = PHASE_WAITING;
    g->score[0] = g->score[1] = 0;
    g->winner = 0;
    g->paused = false;
    g->paused_by = 0;
    center_ball(g);
}

static void start_match(Game *g)
{
    g->score[0] = g->score[1] = 0;
    g->winner = 0;
    g->phase = PHASE_COUNTDOWN;
    g->timer = COUNTDOWN_START;
    g->serve_dir = randombytes_uniform(2) ? 1 : -1;
    center_ball(g);
    printf("[game] Match starting\n");
}

static void launch_ball(Game *g)
{
    float angle = ((float)randombytes_uniform(1001) / 1000.0f * 2.0f - 1.0f) * 0.5236f;
    g->ball_speed = BALL_SPEED_START;
    g->ball_vx = (float)g->serve_dir * g->ball_speed * cosf(angle);
    g->ball_vy = g->ball_speed * sinf(angle);
}

static bool hit_paddle(Game *g, float old_x, float old_y, int side)
{
    const float half = BALL_SIZE / 2.0f;
    float face, old_edge, new_edge;

    if (side == 0)
    {
        if (g->ball_vx >= 0) return false;
        face = PADDLE_MARGIN + PADDLE_W;
        old_edge = old_x - half;
        new_edge = g->ball_x - half;
        if (!(old_edge >= face && new_edge < face)) return false;
    }
    else
    {
        if (g->ball_vx <= 0) return false;
        face = WIDTH - PADDLE_MARGIN - PADDLE_W;
        old_edge = old_x + half;
        new_edge = g->ball_x + half;
        if (!(old_edge <= face && new_edge > face)) return false;
    }

    float t = (old_edge - face) / (old_edge - new_edge);
    float y = old_y + (g->ball_y - old_y) * t;
    float top = g->paddle_y[side];
    if (y + half < top || y - half > top + PADDLE_H)
        return false;

    float rel = (y - (top + PADDLE_H * 0.5f)) / (PADDLE_H * 0.5f);
    if (rel < -1.0f) rel = -1.0f;
    if (rel >  1.0f) rel =  1.0f;
    float angle = rel * BALL_MAX_ANGLE;

    g->ball_speed = fminf(g->ball_speed * BALL_SPEEDUP, BALL_SPEED_MAX);
    float dir = (side == 0) ? 1.0f : -1.0f;
    g->ball_vx = dir * g->ball_speed * cosf(angle);
    g->ball_vy = g->ball_speed * sinf(angle);
    g->ball_x = (side == 0) ? face + half : face - half;
    g->ball_y = y;
    return true;
}

static void score_point(Game *g, int scorer)
{
    g->score[scorer - 1]++;
    printf("[game] Player %d scores  (%d - %d)\n", scorer, g->score[0], g->score[1]);
    center_ball(g);

    if (g->score[scorer - 1] >= WIN_SCORE)
    {
        g->phase = PHASE_GAMEOVER;
        g->timer = GAMEOVER_TIME;
        g->winner = scorer;
        printf("[game] Player %d wins the match\n", scorer);
    }
    else
    {
        g->phase = PHASE_COUNTDOWN;
        g->timer = COUNTDOWN_POINT;
        g->serve_dir = (scorer == 1) ? 1 : -1;
    }
}

static void step_ball(Game *g, float dt)
{
    const float half = BALL_SIZE / 2.0f;
    float old_x = g->ball_x, old_y = g->ball_y;

    g->ball_x += g->ball_vx * dt;
    g->ball_y += g->ball_vy * dt;

    if (g->ball_y - half < 0.0f)
    {
        g->ball_y = 2.0f * half - g->ball_y;
        g->ball_vy = fabsf(g->ball_vy);
    }
    else if (g->ball_y + half > HEIGHT)
    {
        g->ball_y = 2.0f * (HEIGHT - half) - g->ball_y;
        g->ball_vy = -fabsf(g->ball_vy);
    }

    if (!hit_paddle(g, old_x, old_y, 0))
        hit_paddle(g, old_x, old_y, 1);

    if (g->ball_x + half < 0.0f)
        score_point(g, 2);
    else if (g->ball_x - half > WIDTH)
        score_point(g, 1);
}

static void move_paddles(Game *g, const bool active[MAX_PLAYERS], const int8_t dir[MAX_PLAYERS], float dt)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!active[i])
            continue;
        float y = g->paddle_y[i] + (float)dir[i] * PADDLE_SPEED * dt;
        if (y < 0.0f) y = 0.0f;
        if (y > HEIGHT - PADDLE_H) y = HEIGHT - PADDLE_H;
        g->paddle_y[i] = y;
    }
}

void game_init(Game *g)
{
    to_waiting(g);
    for (int i = 0; i < MAX_PLAYERS; i++)
        game_reset_paddle(g, i);
}

void game_reset_paddle(Game *g, int idx)
{
    g->paddle_y[idx] = (HEIGHT - PADDLE_H) / 2.0f;
}

void game_update(Game *g, const bool active[MAX_PLAYERS], const int8_t dir[MAX_PLAYERS], float dt)
{
    bool both_present = active[0] && active[1];

    if (g->paused)
    {
        if (both_present)
            return;
        g->paused = false;
        g->paused_by = 0;
        printf("[game] Pause cleared (a player left)\n");
    }

    move_paddles(g, active, dir, dt);

    switch (g->phase)
    {
    case PHASE_WAITING:
        if (both_present)
            start_match(g);
        break;

    case PHASE_COUNTDOWN:
        if (!both_present) { to_waiting(g); break; }
        g->timer -= dt;
        if (g->timer <= 0.0f)
        {
            g->phase = PHASE_PLAYING;
            launch_ball(g);
        }
        break;

    case PHASE_PLAYING:
        if (!both_present) { to_waiting(g); break; }
        step_ball(g, dt);
        break;

    case PHASE_GAMEOVER:
        g->timer -= dt;
        if (g->timer <= 0.0f)
        {
            if (both_present) start_match(g);
            else              to_waiting(g);
        }
        break;
    }
}

bool game_toggle_pause(Game *g, int player_id, bool both_present)
{
    if (!both_present || g->phase == PHASE_WAITING)
        return false;
    g->paused = !g->paused;
    g->paused_by = g->paused ? player_id : 0;
    printf("[game] Player %d %s the game\n", player_id, g->paused ? "paused" : "resumed");
    return true;
}

void game_snapshot(const Game *g, uint8_t active_mask, GameSnapshot *s)
{
    s->ball_x = g->ball_x;
    s->ball_y = g->ball_y;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        s->paddle_y[i] = g->paddle_y[i];
        s->score[i] = g->score[i];
    }
    s->active_mask = active_mask;
    s->phase = (uint8_t)g->phase;
    s->winner = (uint8_t)g->winner;
    s->countdown = (g->phase == PHASE_COUNTDOWN) ? (uint8_t)ceilf(g->timer) : 0;
    s->paused = g->paused ? 1 : 0;
    s->paused_by = (uint8_t)g->paused_by;
}
