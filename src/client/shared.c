#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include "shared.h"

static ClientView      g_view = { .conn = CONN_IDLE, .player_id = -1, .status = "" };
static pthread_mutex_t g_view_mutex = PTHREAD_MUTEX_INITIALIZER;

static atomic_int  g_input_dir = 0;
static atomic_int  g_request = REQ_NONE;
static atomic_uint g_pause_presses = 0;
static atomic_bool g_quit = false;

void shared_view_get(ClientView *out)
{
    pthread_mutex_lock(&g_view_mutex);
    *out = g_view;
    pthread_mutex_unlock(&g_view_mutex);
}

void shared_view_set_conn(int conn, int player_id, const char *status)
{
    pthread_mutex_lock(&g_view_mutex);
    g_view.conn = conn;
    g_view.player_id = player_id;
    if (player_id < 0)
        g_view.have_snap = false;
    snprintf(g_view.status, sizeof(g_view.status), "%s", status);
    pthread_mutex_unlock(&g_view_mutex);
}

void shared_view_set_snapshot(const GameSnapshot *snap)
{
    pthread_mutex_lock(&g_view_mutex);
    g_view.snap = *snap;
    g_view.have_snap = true;
    pthread_mutex_unlock(&g_view_mutex);
}

bool shared_in_match(void)
{
    pthread_mutex_lock(&g_view_mutex);
    bool in_match = g_view.conn == CONN_JOINED && g_view.have_snap;
    pthread_mutex_unlock(&g_view_mutex);
    return in_match;
}

void shared_set_input_dir(int dir)
{
    atomic_store(&g_input_dir, dir);
}

int8_t shared_input_dir(void)
{
    return (int8_t)atomic_load(&g_input_dir);
}

void shared_request(int req)
{
    atomic_store(&g_request, req);
}

int shared_take_request(void)
{
    return atomic_exchange(&g_request, REQ_NONE);
}

void shared_press_pause(void)
{
    atomic_fetch_add(&g_pause_presses, 1);
}

uint8_t shared_pause_presses(void)
{
    return (uint8_t)atomic_load(&g_pause_presses);
}

void shared_request_quit(void)
{
    atomic_store(&g_quit, true);
}

bool shared_quit_requested(void)
{
    return atomic_load(&g_quit);
}
