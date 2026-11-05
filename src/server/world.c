#include <stdio.h>
#include <string.h>
#include "world.h"

static bool both_present(const World *w)
{
    return w->players[0].active && w->players[1].active;
}

static void remove_player(World *w, int idx, const char *reason)
{
    Player *p = &w->players[idx];
    printf("[game] Player %d (%s) %s\n", idx + 1, p->username, reason);
    sec_wipe(p->session_key, sizeof(p->session_key));
    memset(p, 0, sizeof(*p));
}

static void apply_pause_presses(World *w, Player *p, int player_id, uint8_t presses)
{
    if (!p->pause_seen)
    {
        p->pause_seen = true;
        p->pause_presses = presses;
    }
    uint8_t new_presses = (uint8_t)(presses - p->pause_presses);
    p->pause_presses = presses;
    if (new_presses & 1)
        game_toggle_pause(&w->game, player_id, both_present(w));
}

void world_init(World *w)
{
    memset(w, 0, sizeof(*w));
    pthread_mutex_init(&w->mutex, NULL);
    game_init(&w->game);
}

void world_destroy(World *w)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
        sec_wipe(w->players[i].session_key, SEC_KEY_LEN);
    pthread_mutex_destroy(&w->mutex);
}

void world_lock(World *w)
{
    pthread_mutex_lock(&w->mutex);
}

void world_unlock(World *w)
{
    pthread_mutex_unlock(&w->mutex);
}

int world_admit_locked(World *w, const char *username, dds_time_t now)
{
    int free_idx = -1;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (w->players[i].active && strcmp(w->players[i].username, username) == 0)
            return JOIN_ALREADY;
        if (!w->players[i].active && free_idx < 0)
            free_idx = i;
    }
    if (free_idx < 0)
        return JOIN_FULL;

    Player *p = &w->players[free_idx];
    memset(p, 0, sizeof(*p));
    p->active = true;
    snprintf(p->username, sizeof(p->username), "%s", username);
    p->session_gen = ++w->session_counter;
    p->last_input_time = now;
    game_reset_paddle(&w->game, free_idx);
    return free_idx + 1;
}

uint8_t *world_session_key_locked(World *w, int player_id)
{
    return w->players[player_id - 1].session_key;
}

bool world_session_locked(World *w, int player_id, SessionView *out)
{
    const Player *p = &w->players[player_id - 1];
    out->active = p->active;
    out->session_gen = p->session_gen;
    out->last_rx_seq = p->last_rx_seq;
    memcpy(out->session_key, p->session_key, SEC_KEY_LEN);
    return p->active;
}

void world_apply_input_locked(World *w, int player_id, uint32_t session_gen, uint64_t seq,
                              const InputPayload *in, dds_time_t now)
{
    int idx = player_id - 1;
    Player *p = &w->players[idx];
    if (!p->active || p->session_gen != session_gen || seq <= p->last_rx_seq)
        return;

    p->last_rx_seq = seq;
    p->last_input_time = now;
    p->dir = in->dir;
    apply_pause_presses(w, p, player_id, in->pause_presses);

    if (in->flags & INPUT_FLAG_LEAVE)
        remove_player(w, idx, "left the game");
}

void world_tick_locked(World *w, float dt, dds_time_t now, GameSnapshot *snap, Outbox *out)
{
    bool    active[MAX_PLAYERS];
    int8_t  dir[MAX_PLAYERS];
    uint8_t active_mask = 0;

    for (int i = 0; i < MAX_PLAYERS; i++)
        if (w->players[i].active && now - w->players[i].last_input_time > PLAYER_TIMEOUT)
            remove_player(w, i, "timed out (no messages for 3 s)");

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        active[i] = w->players[i].active;
        dir[i] = w->players[i].dir;
        if (active[i])
            active_mask |= (uint8_t)(1u << i);
    }

    game_update(&w->game, active, dir, dt);
    game_snapshot(&w->game, active_mask, snap);

    out->count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!w->players[i].active)
            continue;
        out->player_id[out->count] = i + 1;
        out->seq[out->count] = ++w->players[i].tx_seq;
        memcpy(out->session_key[out->count], w->players[i].session_key, SEC_KEY_LEN);
        out->count++;
    }
}
