/**
 * @file    protocol.c
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Communication protocol and message structures.
 *
 * @version 1.0
 */
#include <string.h>
#include <time.h>
#include "protocol.h"

/**
 * @brief Write a 32-bit float to a byte array in little-endian format.
 * @param p The byte array.
 * @param f The float to write.
 */
static void put_f32(uint8_t *p, float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    p[0] = (uint8_t)u;
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
    p[3] = (uint8_t)(u >> 24);
}

/**
 * @brief Read a 32-bit float from a byte array in little-endian format.
 * @param p The byte array.
 * @return The read float.
 */
static float get_f32(const uint8_t *p)
{
    uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

/**
 * @brief Pack a game snapshot into a byte array.
 * @param s Pointer to the GameSnapshot to pack.
 * @param out The byte array to store the packed state.
 */
void pack_state(const GameSnapshot *s, uint8_t out[STATE_PLAIN_LEN])
{
    memset(out, 0, STATE_PLAIN_LEN);
    put_f32(out + 0,  s->ball_x);
    put_f32(out + 4,  s->ball_y);
    put_f32(out + 8,  s->paddle_y[0]);
    put_f32(out + 12, s->paddle_y[1]);
    out[16] = s->score[0];
    out[17] = s->score[1];
    out[18] = s->active_mask;
    out[19] = s->phase;
    out[20] = s->winner;
    out[21] = s->countdown;
    out[22] = s->paused;
    out[23] = s->paused_by;
}

/**
 * @brief Unpack a byte array into a game snapshot.
 * @param in The byte array containing the packed state.
 * @param s Pointer to the GameSnapshot to store the unpacked state.
 */
void unpack_state(const uint8_t in[STATE_PLAIN_LEN], GameSnapshot *s)
{
    s->ball_x      = get_f32(in + 0);
    s->ball_y      = get_f32(in + 4);
    s->paddle_y[0] = get_f32(in + 8);
    s->paddle_y[1] = get_f32(in + 12);
    s->score[0]    = in[16];
    s->score[1]    = in[17];
    s->active_mask = in[18];
    s->phase       = in[19];
    s->winner      = in[20];
    s->countdown   = in[21];
    s->paused      = in[22];
    s->paused_by   = in[23];
}

/**
 * @brief Pack an input payload into a byte array.
 * @param p Pointer to the InputPayload to pack.
 * @param out The byte array to store the packed input.
 */
void pack_input(const InputPayload *p, uint8_t out[INPUT_PLAIN_LEN])
{
    memset(out, 0, INPUT_PLAIN_LEN);
    out[0] = (uint8_t)p->dir;
    out[1] = p->flags;
    out[2] = p->pause_presses;
}

/**
 * @brief Unpack a byte array into an input payload.
 * @param in The byte array containing the packed input.
 * @param p Pointer to the InputPayload to store the unpacked input.
 */
void unpack_input(const uint8_t in[INPUT_PLAIN_LEN], InputPayload *p)
{
    int8_t dir = (int8_t)in[0];
    if (dir < -1) dir = -1;
    if (dir > 1)  dir = 1;
    p->dir = dir;
    p->flags = in[1];
    p->pause_presses = in[2];
}

/**
 * @brief Get the current monotonic time.
 * @return The current monotonic time in DDS time format.
 */
dds_time_t mono_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (dds_time_t)ts.tv_sec * DDS_NSECS_IN_SEC + ts.tv_nsec;
}
