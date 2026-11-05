#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

enum { CONN_IDLE, CONN_CONNECTING, CONN_AUTHENTICATING, CONN_JOINED, CONN_REJECTED };
enum { REQ_NONE, REQ_PLAY, REQ_LEAVE };

#define STATUS_LEN 96

typedef struct
{
    int          conn;
    int          player_id;
    bool         have_snap;
    GameSnapshot snap;
    char         status[STATUS_LEN];
} ClientView;

void shared_view_get(ClientView *out);
void shared_view_set_conn(int conn, int player_id, const char *status);
void shared_view_set_snapshot(const GameSnapshot *snap);
bool shared_in_match(void);

void   shared_set_input_dir(int dir);
int8_t shared_input_dir(void);

void shared_request(int req);
int  shared_take_request(void);

void    shared_press_pause(void);
uint8_t shared_pause_presses(void);

void shared_request_quit(void);
bool shared_quit_requested(void);

#endif
