#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "effects.h"
#include "textures.h"

#define MAX_PARTICLES   320
#define TRAIL_LEN       14
#define JUMP_DISTANCE   80.0f
#define PADDLE_ZONE     120.0f
#define MENU_PADDLE_X   70.0f

typedef struct
{
    float x, y, vx, vy;
    float life, max_life;
    float size;
    Rgb   color;
} Particle;

typedef struct
{
    float ball_x, ball_y, ball_vx, ball_vy;
    float paddle_y[MAX_PLAYERS];
} MenuDemo;

static Particle     g_particles[MAX_PARTICLES];
static int          g_particle_count = 0;
static float        g_trail_x[TRAIL_LEN];
static float        g_trail_y[TRAIL_LEN];
static int          g_trail_len = 0;
static GameSnapshot g_prev;
static bool         g_have_prev = false;
static float        g_prev_dx = 0.0f;
static float        g_prev_dy = 0.0f;
static float        g_flash = 0.0f;
static Rgb          g_flash_color = { 1.0f, 1.0f, 1.0f };
static float        g_shake = 0.0f;
static float        g_score_pulse[MAX_PLAYERS];
static float        g_countdown_pulse = 0.0f;
static float        g_time = 0.0f;
static MenuDemo     g_menu = { WIDTH / 2.0f, 380.0f, 260.0f, 150.0f, { 330.0f, 330.0f } };

static const Rgb k_white = { 1.0f, 1.0f, 1.0f };

static float frand(float lo, float hi)
{
    return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}

static float signf(float v)
{
    return (v > 0.0f) - (v < 0.0f);
}

static void spawn_burst(float x, float y, int count, Rgb color, float dir_x,
                        float speed_min, float speed_max, float life, float size)
{
    for (int i = 0; i < count && g_particle_count < MAX_PARTICLES; i++)
    {
        float angle = frand(0.0f, 6.2832f);
        float speed = frand(speed_min, speed_max);
        Particle *p = &g_particles[g_particle_count++];
        p->x = x;
        p->y = y;
        p->vx = cosf(angle) * speed + dir_x * speed * 0.8f;
        p->vy = sinf(angle) * speed;
        p->max_life = p->life = frand(life * 0.6f, life);
        p->size = frand(size * 0.6f, size);
        p->color = color;
    }
}

static void trail_reset(void)
{
    g_trail_len = 0;
}

static void trail_push(float x, float y)
{
    if (g_trail_len < TRAIL_LEN)
        g_trail_len++;
    for (int i = g_trail_len - 1; i > 0; i--)
    {
        g_trail_x[i] = g_trail_x[i - 1];
        g_trail_y[i] = g_trail_y[i - 1];
    }
    g_trail_x[0] = x;
    g_trail_y[0] = y;
}

static void detect_bounces(const GameSnapshot *s, float dx, float dy)
{
    bool near_paddle = s->ball_x < PADDLE_ZONE || s->ball_x > WIDTH - PADDLE_ZONE;
    if (g_prev_dx != 0.0f && dx != 0.0f && signf(dx) != signf(g_prev_dx) && near_paddle)
    {
        int side = s->ball_x < WIDTH / 2.0f ? 0 : 1;
        spawn_burst(s->ball_x, s->ball_y, 22, draw_player_rgb(side), signf(dx), 60.0f, 260.0f, 0.55f, 18.0f);
        if (g_shake < 3.0f)
            g_shake = 3.0f;
    }
    if (g_prev_dy != 0.0f && dy != 0.0f && signf(dy) != signf(g_prev_dy))
        spawn_burst(s->ball_x, s->ball_y, 8, k_white, 0.0f, 40.0f, 140.0f, 0.35f, 12.0f);
}

static void detect_scores(const GameSnapshot *s)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (s->score[i] <= g_prev.score[i])
            continue;
        Rgb c = draw_player_rgb(i);
        float goal_x = (i == 0) ? WIDTH - 20.0f : 20.0f;
        spawn_burst(goal_x, g_prev.ball_y, 45, c, i == 0 ? -1.0f : 1.0f, 80.0f, 340.0f, 0.9f, 22.0f);
        g_score_pulse[i] = 1.0f;
        g_flash = 1.0f;
        g_flash_color = c;
        g_shake = 9.0f;
    }
}

static void detect_phase_changes(const GameSnapshot *s)
{
    if (s->phase == PHASE_GAMEOVER && g_prev.phase != PHASE_GAMEOVER && s->winner >= 1)
    {
        Rgb c = draw_player_rgb(s->winner - 1);
        for (int k = 0; k < 6; k++)
            spawn_burst(frand(120.0f, WIDTH - 120.0f), frand(80.0f, 260.0f), 25, k % 2 ? c : k_white,
                        0.0f, 60.0f, 300.0f, 1.6f, 20.0f);
        g_flash = 1.0f;
        g_flash_color = c;
    }
    if (s->phase == PHASE_COUNTDOWN && s->countdown != g_prev.countdown)
        g_countdown_pulse = 1.0f;
}

void effects_reset(void)
{
    g_particle_count = 0;
    g_have_prev = false;
    g_prev_dx = g_prev_dy = 0.0f;
    g_flash = g_shake = g_countdown_pulse = 0.0f;
    for (int i = 0; i < MAX_PLAYERS; i++)
        g_score_pulse[i] = 0.0f;
    trail_reset();
}

void effects_observe(const GameSnapshot *s)
{
    if (!g_have_prev)
    {
        g_prev = *s;
        g_have_prev = true;
        trail_reset();
        return;
    }

    float dx = s->ball_x - g_prev.ball_x;
    float dy = s->ball_y - g_prev.ball_y;
    bool jumped = fabsf(dx) > JUMP_DISTANCE || fabsf(dy) > JUMP_DISTANCE;
    bool playing = s->phase == PHASE_PLAYING && g_prev.phase == PHASE_PLAYING;

    if (playing && !jumped && !s->paused)
    {
        trail_push(s->ball_x, s->ball_y);
        detect_bounces(s, dx, dy);
    }
    else if (!s->paused)
        trail_reset();

    detect_scores(s);
    detect_phase_changes(s);

    if (jumped || !playing)
        g_prev_dx = g_prev_dy = 0.0f;
    else
    {
        if (dx != 0.0f) g_prev_dx = dx;
        if (dy != 0.0f) g_prev_dy = dy;
    }
    g_prev = *s;
}

static void decay(float *v, float rate, float dt)
{
    *v -= rate * dt;
    if (*v < 0.0f)
        *v = 0.0f;
}

void effects_update(float dt, bool paused)
{
    g_time += dt;
    if (paused)
        return;

    decay(&g_flash, 2.5f, dt);
    decay(&g_shake, 28.0f, dt);
    decay(&g_countdown_pulse, 3.0f, dt);
    for (int i = 0; i < MAX_PLAYERS; i++)
        decay(&g_score_pulse[i], 2.5f, dt);

    float drag = expf(-3.0f * dt);
    for (int i = 0; i < g_particle_count;)
    {
        Particle *p = &g_particles[i];
        p->life -= dt;
        if (p->life <= 0.0f)
        {
            *p = g_particles[--g_particle_count];
            continue;
        }
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vx *= drag;
        p->vy *= drag;
        i++;
    }
}

bool effects_active(void)
{
    return g_particle_count > 0 || g_flash > 0.0f || g_shake > 0.0f ||
           g_countdown_pulse > 0.0f || g_score_pulse[0] > 0.0f || g_score_pulse[1] > 0.0f;
}

void effects_draw_trail(void)
{
    for (int i = g_trail_len - 1; i >= 1; i--)
    {
        float t = 1.0f - (float)i / TRAIL_LEN;
        float size = BALL_SIZE * (1.0f + 1.6f * t);
        tex_draw_glow(g_trail_x[i], g_trail_y[i], size, size, 0.75f, 0.90f, 1.0f, 0.35f * t);
    }
}

void effects_draw_particles(void)
{
    for (int i = 0; i < g_particle_count; i++)
    {
        const Particle *p = &g_particles[i];
        float t = p->life / p->max_life;
        float size = p->size * (0.4f + 0.6f * t);
        tex_draw_glow(p->x, p->y, size, size, p->color.r, p->color.g, p->color.b, t);
    }
}

void effects_shake_offset(float *dx, float *dy)
{
    *dx = g_shake > 0.0f ? frand(-g_shake, g_shake) : 0.0f;
    *dy = g_shake > 0.0f ? frand(-g_shake, g_shake) : 0.0f;
}

float effects_score_scale(int idx)
{
    return 1.0f + 0.6f * g_score_pulse[idx];
}

float effects_countdown_scale(void)
{
    return 1.0f + 0.5f * g_countdown_pulse;
}

void effects_draw_flash(void)
{
    draw_flash(g_flash_color, 0.22f * g_flash);
}

float effects_time(void)
{
    return g_time;
}

void effects_menu_update(float dt)
{
    MenuDemo *m = &g_menu;
    const float half = BALL_SIZE / 2.0f;
    const float left_face = MENU_PADDLE_X + PADDLE_W;
    const float right_face = WIDTH - MENU_PADDLE_X - PADDLE_W;

    m->ball_x += m->ball_vx * dt;
    m->ball_y += m->ball_vy * dt;

    if (m->ball_y - half < 0.0f || m->ball_y + half > HEIGHT)
    {
        m->ball_vy = -m->ball_vy;
        m->ball_y = m->ball_y - half < 0.0f ? half : HEIGHT - half;
    }
    if (m->ball_x - half < left_face && m->ball_vx < 0.0f)
    {
        m->ball_vx = -m->ball_vx;
        m->ball_vy = frand(-220.0f, 220.0f);
        spawn_burst(left_face, m->ball_y, 12, draw_player_rgb(0), 1.0f, 40.0f, 180.0f, 0.5f, 14.0f);
    }
    if (m->ball_x + half > right_face && m->ball_vx > 0.0f)
    {
        m->ball_vx = -m->ball_vx;
        m->ball_vy = frand(-220.0f, 220.0f);
        spawn_burst(right_face, m->ball_y, 12, draw_player_rgb(1), -1.0f, 40.0f, 180.0f, 0.5f, 14.0f);
    }

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        float target = m->ball_y - PADDLE_H / 2.0f;
        float step = 260.0f * dt;
        float diff = target - m->paddle_y[i];
        m->paddle_y[i] += diff > step ? step : (diff < -step ? -step : diff);
        if (m->paddle_y[i] < 0.0f) m->paddle_y[i] = 0.0f;
        if (m->paddle_y[i] > HEIGHT - PADDLE_H) m->paddle_y[i] = HEIGHT - PADDLE_H;
    }

    trail_push(m->ball_x, m->ball_y);
    effects_update(dt, false);
}

void effects_menu_draw(void)
{
    const MenuDemo *m = &g_menu;
    draw_paddle(MENU_PADDLE_X, m->paddle_y[0], 0, 0.45f);
    draw_paddle(WIDTH - MENU_PADDLE_X - PADDLE_W, m->paddle_y[1], 1, 0.45f);
    effects_draw_trail();
    effects_draw_particles();
    draw_ball(m->ball_x, m->ball_y, 0.6f);
}
