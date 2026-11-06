/**
 * @file    protocol.h
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Communication protocol and message structures.
 *
 * @version 1.0
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "dds/dds.h"
#include "messages.h"
#include "secure.h"

#define WIDTH  800
#define HEIGHT 600

#define MAX_PLAYERS    2
#define PADDLE_W       12
#define PADDLE_H       90
#define PADDLE_MARGIN  30
#define PADDLE_SPEED   420.0f

#define BALL_SIZE        12
#define BALL_SPEED_START 330.0f
#define BALL_SPEED_MAX   780.0f
#define BALL_SPEEDUP     1.06f
#define BALL_MAX_ANGLE   1.0472f

#define WIN_SCORE        5
#define TICK_HZ          60
#define COUNTDOWN_START  3.0f
#define COUNTDOWN_POINT  1.0f
#define GAMEOVER_TIME    4.0f

#define PLAYER_TIMEOUT   DDS_SECS(3)
#define SERVER_TIMEOUT   DDS_SECS(3)
#define HEARTBEAT        DDS_MSECS(100)

enum { PHASE_WAITING = 0, PHASE_COUNTDOWN = 1, PHASE_PLAYING = 2, PHASE_GAMEOVER = 3 };

#define JOIN_BAD_LOGIN  0
#define JOIN_FULL      -1
#define JOIN_ALREADY   -2

#define INPUT_FLAG_LEAVE 0x01
#define INPUT_PLAIN_LEN  4
#define STATE_PLAIN_LEN  24

_Static_assert(sizeof(((Input *)0)->data) == INPUT_PLAIN_LEN + SEC_BOX_OVERHEAD, "Input.data size mismatch with IDL");
_Static_assert(sizeof(((GameState *)0)->data) == STATE_PLAIN_LEN + SEC_BOX_OVERHEAD, "GameState.data size mismatch with IDL");
_Static_assert(MAX_PLAYERS == 2, "state packing assumes 2 players");

typedef struct
{
    float   ball_x, ball_y;
    float   paddle_y[MAX_PLAYERS];
    uint8_t score[MAX_PLAYERS];
    uint8_t active_mask;
    uint8_t phase;
    uint8_t winner;
    uint8_t countdown;
    uint8_t paused;
    uint8_t paused_by;
} GameSnapshot;

typedef struct
{
    int8_t  dir;
    uint8_t flags;
    uint8_t pause_presses;
} InputPayload;

/**
 * @brief Pack a game snapshot into a byte array.
 * @param s Pointer to the GameSnapshot to pack.
 * @param out The byte array to store the packed state.
 */
void pack_state(const GameSnapshot *s, uint8_t out[STATE_PLAIN_LEN]);

/**
 * @brief Unpack a byte array into a game snapshot.
 * @param in The byte array containing the packed state.
 * @param s Pointer to the GameSnapshot to store the unpacked state.
 */
void unpack_state(const uint8_t in[STATE_PLAIN_LEN], GameSnapshot *s);

/**
 * @brief Pack an input payload into a byte array.
 * @param p Pointer to the InputPayload to pack.
 * @param out The byte array to store the packed input.
 */
void pack_input(const InputPayload *p, uint8_t out[INPUT_PLAIN_LEN]);

/**
 * @brief Unpack a byte array into an input payload.
 * @param in The byte array containing the packed input.
 * @param p Pointer to the InputPayload to store the unpacked input.
 */
void unpack_input(const uint8_t in[INPUT_PLAIN_LEN], InputPayload *p);

/**
 * @brief Get the current monotonic time.
 * @return The current monotonic time in DDS time format.
 */
dds_time_t mono_now(void);

#endif
