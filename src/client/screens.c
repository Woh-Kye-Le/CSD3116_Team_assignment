/**
 * @file    screens.c
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Screen and UI state management.
 *
 * @version 1.0
 */
#include <stdio.h>
#include <GLFW/glfw3.h>
#include "screens.h"
#include "draw.h"
#include "effects.h"
#include "textures.h"
#include <math.h>
#include "protocol.h"

#define MENU_ITEM_W  320.0f
#define MENU_ITEM_H  54.0f
#define MENU_TOP     270.0f
#define MENU_GAP     72.0f
#define BACK_X       (WIDTH / 2.0f - 110.0f)
#define BACK_Y       538.0f
#define BACK_W       220.0f
#define BACK_H       44.0f

static const char *k_menu_labels[MENU_ITEMS] = { "PLAY", "INSTRUCTIONS", "QUIT" };

enum { LINE_HEADER, LINE_TEXT, LINE_KEY };

typedef struct
{
    int         kind;
    const char *a;
    const char *b;
} InstructionLine;

static const InstructionLine k_instructions[] =
{
    { LINE_HEADER, "CONNECTING", NULL },
    { LINE_TEXT,   "1. Start the server first:   ./bin/server", NULL },
    { LINE_TEXT,   "2. Each player runs:   ./bin/client <username>", NULL },
    { LINE_TEXT,   "3. Type your password in the terminal.", NULL },
    { LINE_TEXT,   "4. Press PLAY. The match starts when 2 players join.", NULL },
    { LINE_TEXT,   "Two laptops: same Wi-Fi, then in every terminal run", NULL },
    { LINE_TEXT,   "   source scripts/lan.sh <other laptop IP>   (see README)", NULL },
    { LINE_HEADER, "CONTROLS", NULL },
    { LINE_KEY,    "W / S  or  Up / Down", "move your paddle" },
    { LINE_KEY,    "Esc", "pause / resume for BOTH players" },
    { LINE_KEY,    "Q", "leave the match, back to this menu" },
    { LINE_HEADER, "RULES", NULL },
    { LINE_TEXT,   "Get the ball past your opponent's paddle to score.", NULL },
    { LINE_TEXT,   "Paddle edges give steep angles; the ball speeds up.", NULL },
    { LINE_TEXT,   "First to 5 points wins, then a new match starts.", NULL },
};

/**
 * @brief Calculates the Y coordinate for a menu item.
 *
 * @param i The index of the menu item.
 * @return The Y coordinate of the menu item.
 */
static float menu_item_y(int i)
{
    return MENU_TOP + (float)i * MENU_GAP;
}

/**
 * @brief Determines which menu item is located at the given coordinates.
 *
 * @param mx The x-coordinate of the mouse.
 * @param my The y-coordinate of the mouse.
 * @return The index of the menu item, or -1 if no item is at the coordinates.
 */
int screens_menu_item_at(double mx, double my)
{
    float x = WIDTH / 2.0f - MENU_ITEM_W / 2.0f;
    for (int i = 0; i < MENU_ITEMS; i++)
    {
        float y = menu_item_y(i);
        if (mx >= x && mx <= x + MENU_ITEM_W && my >= y && my <= y + MENU_ITEM_H)
            return i;
    }
    return -1;
}

/**
 * @brief Checks if the back button is located at the given coordinates.
 *
 * @param mx The x-coordinate of the mouse.
 * @param my The y-coordinate of the mouse.
 * @return True if the back button is at the coordinates, false otherwise.
 */
bool screens_back_button_at(double mx, double my)
{
    return mx >= BACK_X && mx <= BACK_X + BACK_W && my >= BACK_Y && my <= BACK_Y + BACK_H;
}

/**
 * @brief Renders the main menu screen.
 *
 * @param ui Pointer to the UI state.
 * @param username The username of the player.
 */
static void render_menu(const UiState *ui, const char *username)
{
    draw_background(false);
    effects_menu_draw();

    float pulse = 0.5f + 0.5f * sinf(effects_time() * 2.0f);
    tex_draw_glow(WIDTH / 2.0f, 108, 560, 220, 0.30f, 0.90f, 0.95f, 0.10f + 0.10f * pulse);
    tex_draw_glow(WIDTH / 2.0f, 108, 360, 140, 0.85f, 0.30f, 0.90f, 0.08f + 0.08f * (1.0f - pulse));

    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text_centered(WIDTH / 2.0f, 70, 10.0f, "PONG");
    glColor3f(0.55f, 0.55f, 0.60f);
    draw_text_centered(WIDTH / 2.0f, 190, 2.5f, "SECURE DDS EDITION");

    for (int i = 0; i < MENU_ITEMS; i++)
        draw_button(WIDTH / 2.0f - MENU_ITEM_W / 2.0f, menu_item_y(i), MENU_ITEM_W, MENU_ITEM_H,
                    k_menu_labels[i], i == ui->menu_sel);

    char buf[96];
    glColor3f(0.55f, 0.55f, 0.60f);
    draw_text_centered(WIDTH / 2.0f, 500, 1.8f, "Up/Down + Enter, or click with the mouse");
    snprintf(buf, sizeof(buf), "User: %s", username);
    draw_text_centered(WIDTH / 2.0f, 560, 2.0f, buf);
}

/**
 * @brief Renders the instructions screen.
 *
 * @param ui Pointer to the UI state.
 */
static void render_instructions(const UiState *ui)
{
    draw_background(false);

    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text_centered(WIDTH / 2.0f, 22, 4.0f, "INSTRUCTIONS");

    float y = 82.0f;
    for (size_t i = 0; i < sizeof(k_instructions) / sizeof(k_instructions[0]); i++)
    {
        const InstructionLine *l = &k_instructions[i];
        switch (l->kind)
        {
        case LINE_HEADER:
            y += 8.0f;
            glColor3f(0.30f, 0.90f, 0.95f);
            draw_text(60, y, 2.4f, l->a);
            y += 28.0f;
            break;
        case LINE_TEXT:
            glColor3f(0.88f, 0.88f, 0.90f);
            draw_text(80, y, 1.9f, l->a);
            y += 22.0f;
            break;
        case LINE_KEY:
            glColor3f(0.85f, 0.30f, 0.90f);
            draw_text(80, y, 1.9f, l->a);
            glColor3f(0.88f, 0.88f, 0.90f);
            draw_text(330, y, 1.9f, l->b);
            y += 22.0f;
            break;
        }
    }

    draw_button(BACK_X, BACK_Y, BACK_W, BACK_H, "BACK", ui->back_hover);
}

/**
 * @brief Renders the connecting screen while waiting for the server.
 *
 * @param v Pointer to the client view state.
 * @param username The username of the player.
 */
static void render_connecting(const ClientView *v, const char *username)
{
    char buf[64];
    glColor3f(0.9f, 0.9f, 0.9f);
    draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 60, 3.0f, v->status);
    glColor3f(0.55f, 0.55f, 0.6f);
    draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f + 10, 2.0f, "Press Q or Esc to go back to the menu");
    snprintf(buf, sizeof(buf), "User: %s", username);
    draw_text_centered(WIDTH / 2.0f, HEIGHT - 40, 2.0f, buf);
}

/**
 * @brief Renders the player scores.
 *
 * @param s Pointer to the game snapshot containing the scores.
 */
static void render_scores(const GameSnapshot *s)
{
    char buf[16];
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        float scale = 6.0f * effects_score_scale(i);
        float cx = WIDTH / 2.0f + (i == 0 ? -70.0f : 70.0f);
        float y = 25.0f - (scale - 6.0f) * 3.5f;
        Rgb c = draw_player_rgb(i);
        tex_draw_glow(cx, 46, 120 * effects_score_scale(i), 90, c.r, c.g, c.b, 0.18f);
        draw_set_player_color(i, 1.0f);
        snprintf(buf, sizeof(buf), "%d", s->score[i]);
        draw_text_centered(cx, y, scale, buf);
    }
}

/**
 * @brief Renders the game field, including paddles and the ball.
 *
 * @param s Pointer to the game snapshot.
 * @param me The index of the current player.
 */
static void render_field(const GameSnapshot *s, int me)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        float x = (i == 0) ? PADDLE_MARGIN : WIDTH - PADDLE_MARGIN - PADDLE_W;
        bool active = (s->active_mask >> i) & 1;
        draw_paddle(x, s->paddle_y[i], i, active ? 1.0f : 0.3f);
        if (i == me)
        {
            glColor3f(1.0f, 1.0f, 1.0f);
            draw_text_centered(x + PADDLE_W / 2.0f, s->paddle_y[i] - 18, 2.0f, "YOU");
        }
    }

    if (s->phase == PHASE_PLAYING || s->phase == PHASE_COUNTDOWN)
    {
        effects_draw_trail();
        draw_ball(s->ball_x, s->ball_y, 1.0f);
    }
    effects_draw_particles();
}

/**
 * @brief Renders phase-specific messages (e.g., waiting, countdown, game over).
 *
 * @param s Pointer to the game snapshot.
 * @param player_id The ID of the current player.
 */
static void render_phase_message(const GameSnapshot *s, int player_id)
{
    char buf[64];
    glColor3f(1.0f, 1.0f, 1.0f);
    switch (s->phase)
    {
    case PHASE_WAITING:
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 70, 3.0f, "Waiting for an opponent...");
        break;
    case PHASE_COUNTDOWN:
    {
        float scale = 7.0f * effects_countdown_scale();
        snprintf(buf, sizeof(buf), "%d", s->countdown);
        tex_draw_glow(WIDTH / 2.0f, HEIGHT / 2.0f - 85, 180 * effects_countdown_scale(), 180, 1.0f, 1.0f, 1.0f, 0.18f);
        glColor3f(1.0f, 1.0f, 1.0f);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 110 - (scale - 7.0f) * 3.5f, scale, buf);
        break;
    }
    case PHASE_GAMEOVER:
        tex_draw_glow(WIDTH / 2.0f, HEIGHT / 2.0f - 50, 520, 200, 1.0f, 1.0f, 1.0f, 0.15f);
        glColor3f(1.0f, 1.0f, 1.0f);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 80, 6.0f,
                           s->winner == player_id ? "YOU WIN!" : "YOU LOSE");
        snprintf(buf, sizeof(buf), "Player %d wins %d - %d", s->winner, s->score[0], s->score[1]);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f, 2.5f, buf);
        break;
    default:
        break;
    }
}

/**
 * @brief Renders the footer containing user and control information.
 *
 * @param v Pointer to the client view state.
 * @param username The username of the player.
 */
static void render_footer(const ClientView *v, const char *username)
{
    char buf[128];
    glColor3f(0.55f, 0.55f, 0.6f);
    snprintf(buf, sizeof(buf), "%s - Player %d (%s)    W/S or Up/Down: move    Esc: pause    Q: leave",
             username, v->player_id, v->player_id == 1 ? "left" : "right");
    draw_text_centered(WIDTH / 2.0f, HEIGHT - 22, 1.6f, buf);
}

/**
 * @brief Renders the pause overlay when the game is paused.
 *
 * @param s Pointer to the game snapshot.
 * @param player_id The ID of the current player.
 */
static void render_pause_overlay(const GameSnapshot *s, int player_id)
{
    char buf[64];
    draw_dim_overlay(0.6f);
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 70, 8.0f, "PAUSED");
    if (s->paused_by == player_id)
        snprintf(buf, sizeof(buf), "Paused by you");
    else
        snprintf(buf, sizeof(buf), "Paused by Player %d", s->paused_by);
    glColor3f(0.75f, 0.75f, 0.80f);
    draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f + 20, 2.5f, buf);
    draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f + 60, 2.0f, "Esc: resume      Q: leave to menu");
}

/**
 * @brief Renders the game screen.
 *
 * @param v Pointer to the client view state.
 * @param username The username of the player.
 */
static void render_game(const ClientView *v, const char *username)
{
    draw_background(true);

    if (!v->have_snap)
    {
        render_connecting(v, username);
        return;
    }

    const GameSnapshot *s = &v->snap;
    float sx, sy;
    effects_shake_offset(&sx, &sy);
    glPushMatrix();
    glTranslatef(sx, sy, 0.0f);
    render_field(s, v->player_id - 1);
    glPopMatrix();

    render_scores(s);
    if (!s->paused)
        render_phase_message(s, v->player_id);
    render_footer(v, username);
    effects_draw_flash();
    if (s->paused)
        render_pause_overlay(s, v->player_id);
}

/**
 * @brief Renders the current screen based on the UI state.
 *
 * @param ui Pointer to the UI state.
 * @param view Pointer to the client view state.
 * @param username The username of the player.
 */
void screens_render(const UiState *ui, const ClientView *view, const char *username)
{
    switch (ui->screen)
    {
    case SCREEN_MENU:
        render_menu(ui, username);
        break;
    case SCREEN_INSTRUCTIONS:
        render_instructions(ui);
        break;
    case SCREEN_GAME:
        render_game(view, username);
        break;
    }
}
