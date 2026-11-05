#include <stdio.h>
#include <GLFW/glfw3.h>
#include "screens.h"
#include "draw.h"
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

static float menu_item_y(int i)
{
    return MENU_TOP + (float)i * MENU_GAP;
}

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

bool screens_back_button_at(double mx, double my)
{
    return mx >= BACK_X && mx <= BACK_X + BACK_W && my >= BACK_Y && my <= BACK_Y + BACK_H;
}

static void render_menu(const UiState *ui, const char *username)
{
    draw_background(false);

    glColor3f(0.30f, 0.90f, 0.95f);
    draw_rect(70, 110, PADDLE_W, PADDLE_H);
    glColor3f(0.85f, 0.30f, 0.90f);
    draw_rect(WIDTH - 70 - PADDLE_W, 70, PADDLE_W, PADDLE_H);
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_rect(600, 225, BALL_SIZE, BALL_SIZE);

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

static void render_field(const GameSnapshot *s, int me)
{
    char buf[16];
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        draw_set_player_color(i, 1.0f);
        snprintf(buf, sizeof(buf), "%d", s->score[i]);
        draw_text_centered(WIDTH / 2.0f + (i == 0 ? -70.0f : 70.0f), 25, 6.0f, buf);
    }

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        float x = (i == 0) ? PADDLE_MARGIN : WIDTH - PADDLE_MARGIN - PADDLE_W;
        bool active = (s->active_mask >> i) & 1;
        draw_set_player_color(i, active ? 1.0f : 0.35f);
        draw_rect(x, s->paddle_y[i], PADDLE_W, PADDLE_H);
        if (i == me)
        {
            glColor3f(1.0f, 1.0f, 1.0f);
            draw_text_centered(x + PADDLE_W / 2.0f, s->paddle_y[i] - 18, 2.0f, "YOU");
        }
    }

    if (s->phase == PHASE_PLAYING || s->phase == PHASE_COUNTDOWN)
    {
        glColor3f(1.0f, 1.0f, 1.0f);
        draw_rect(s->ball_x - BALL_SIZE / 2.0f, s->ball_y - BALL_SIZE / 2.0f, BALL_SIZE, BALL_SIZE);
    }
}

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
        snprintf(buf, sizeof(buf), "%d", s->countdown);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 110, 7.0f, buf);
        break;
    case PHASE_GAMEOVER:
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f - 80, 6.0f,
                           s->winner == player_id ? "YOU WIN!" : "YOU LOSE");
        snprintf(buf, sizeof(buf), "Player %d wins %d - %d", s->winner, s->score[0], s->score[1]);
        draw_text_centered(WIDTH / 2.0f, HEIGHT / 2.0f, 2.5f, buf);
        break;
    default:
        break;
    }
}

static void render_footer(const ClientView *v, const char *username)
{
    char buf[128];
    glColor3f(0.55f, 0.55f, 0.6f);
    snprintf(buf, sizeof(buf), "%s - Player %d (%s)    W/S or Up/Down: move    Esc: pause    Q: leave",
             username, v->player_id, v->player_id == 1 ? "left" : "right");
    draw_text_centered(WIDTH / 2.0f, HEIGHT - 22, 1.6f, buf);
}

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

static void render_game(const ClientView *v, const char *username)
{
    draw_background(true);

    if (!v->have_snap)
    {
        render_connecting(v, username);
        return;
    }

    const GameSnapshot *s = &v->snap;
    render_field(s, v->player_id - 1);
    if (!s->paused)
        render_phase_message(s, v->player_id);
    render_footer(v, username);
    if (s->paused)
        render_pause_overlay(s, v->player_id);
}

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
