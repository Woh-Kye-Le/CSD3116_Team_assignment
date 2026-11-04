#ifndef PONG_COMMON_H
#define PONG_COMMON_H

#include <stdint.h>
#include <string.h>
#include <time.h>
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

static inline dds_time_t mono_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (dds_time_t)ts.tv_sec * DDS_NSECS_IN_SEC + ts.tv_nsec;
}

static inline void put_f32(uint8_t *p, float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    p[0] = (uint8_t)u; p[1] = (uint8_t)(u >> 8); p[2] = (uint8_t)(u >> 16); p[3] = (uint8_t)(u >> 24);
}

static inline float get_f32(const uint8_t *p)
{
    uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static inline void pack_state(const GameSnapshot *s, uint8_t out[STATE_PLAIN_LEN])
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

static inline void unpack_state(const uint8_t in[STATE_PLAIN_LEN], GameSnapshot *s)
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

typedef struct
{
    dds_entity_t participant;
    dds_entity_t t_join, t_challenge, t_proof, t_response, t_input, t_state;
    dds_qos_t   *qos_handshake, *qos_input, *qos_state;
} PongDds;

static inline int pong_dds_create(PongDds *d)
{
    d->participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    if (d->participant < 0)
    {
        fprintf(stderr, "dds_create_participant: %s\n", dds_strretcode(-d->participant));
        return -1;
    }

    d->qos_handshake = dds_create_qos();
    dds_qset_reliability(d->qos_handshake, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(d->qos_handshake, DDS_HISTORY_KEEP_LAST, 16);

    d->qos_input = dds_create_qos();
    dds_qset_reliability(d->qos_input, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(d->qos_input, DDS_HISTORY_KEEP_LAST, 1);

    d->qos_state = dds_create_qos();
    dds_qset_reliability(d->qos_state, DDS_RELIABILITY_BEST_EFFORT, 0);
    dds_qset_history(d->qos_state, DDS_HISTORY_KEEP_LAST, 1);

    d->t_join      = dds_create_topic(d->participant, &JoinRequest_desc,   "PongJoinRequest",   d->qos_handshake, NULL);
    d->t_challenge = dds_create_topic(d->participant, &JoinChallenge_desc, "PongJoinChallenge", d->qos_handshake, NULL);
    d->t_proof     = dds_create_topic(d->participant, &JoinProof_desc,     "PongJoinProof",     d->qos_handshake, NULL);
    d->t_response  = dds_create_topic(d->participant, &JoinResponse_desc,  "PongJoinResponse",  d->qos_handshake, NULL);
    d->t_input     = dds_create_topic(d->participant, &Input_desc,         "PongInput",         d->qos_input,     NULL);
    d->t_state     = dds_create_topic(d->participant, &GameState_desc,     "PongGameState",     d->qos_state,     NULL);

    if (d->t_join < 0 || d->t_challenge < 0 || d->t_proof < 0 ||
        d->t_response < 0 || d->t_input < 0 || d->t_state < 0)
    {
        fprintf(stderr, "dds_create_topic failed\n");
        return -1;
    }
    return 0;
}

static inline void pong_dds_destroy(PongDds *d)
{
    dds_delete_qos(d->qos_handshake);
    dds_delete_qos(d->qos_input);
    dds_delete_qos(d->qos_state);
    dds_delete(d->participant);
}

static inline dds_entity_t pong_reader(PongDds *d, dds_entity_t topic, const dds_qos_t *qos)
{
    dds_entity_t r = dds_create_reader(d->participant, topic, qos, NULL);
    if (r < 0) DDS_FATAL("dds_create_reader: %s\n", dds_strretcode(-r));
    return r;
}

static inline dds_entity_t pong_writer(PongDds *d, dds_entity_t topic, const dds_qos_t *qos)
{
    dds_entity_t w = dds_create_writer(d->participant, topic, qos, NULL);
    if (w < 0) DDS_FATAL("dds_create_writer: %s\n", dds_strretcode(-w));
    return w;
}

static inline void pong_attach(dds_entity_t waitset, dds_entity_t reader)
{
    dds_entity_t cond = dds_create_readcondition(reader, DDS_ANY_STATE);
    dds_waitset_attach(waitset, cond, reader);
}

#endif
