/**
 * @file    dds_setup.c
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   DDS loop optimization and setup.
 *
 * @version 1.0
 */
#include <stdio.h>
#include "dds_setup.h"
#include "messages.h"

/**
 * @brief Initializes the DDS participant, QoS policies, and topics.
 *
 * @param d Pointer to the PongDds structure to initialize.
 * @return 0 on success, -1 on failure.
 */
int pong_dds_create(PongDds *d)
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

/**
 * @brief Destroys the DDS participant and QoS policies.
 *
 * @param d Pointer to the PongDds structure to destroy.
 */
void pong_dds_destroy(PongDds *d)
{
    dds_delete_qos(d->qos_handshake);
    dds_delete_qos(d->qos_input);
    dds_delete_qos(d->qos_state);
    dds_delete(d->participant);
}

/**
 * @brief Creates a DDS reader for a specific topic.
 *
 * @param d Pointer to the PongDds structure containing the participant.
 * @param topic The DDS topic entity to read from.
 * @param qos The QoS policy to apply to the reader.
 * @return The created DDS reader entity, or terminates on failure.
 */
dds_entity_t pong_reader(PongDds *d, dds_entity_t topic, const dds_qos_t *qos)
{
    dds_entity_t r = dds_create_reader(d->participant, topic, qos, NULL);
    if (r < 0)
        DDS_FATAL("dds_create_reader: %s\n", dds_strretcode(-r));
    return r;
}

/**
 * @brief Creates a DDS writer for a specific topic.
 *
 * @param d Pointer to the PongDds structure containing the participant.
 * @param topic The DDS topic entity to write to.
 * @param qos The QoS policy to apply to the writer.
 * @return The created DDS writer entity, or terminates on failure.
 */
dds_entity_t pong_writer(PongDds *d, dds_entity_t topic, const dds_qos_t *qos)
{
    dds_entity_t w = dds_create_writer(d->participant, topic, qos, NULL);
    if (w < 0)
        DDS_FATAL("dds_create_writer: %s\n", dds_strretcode(-w));
    return w;
}

/**
 * @brief Attaches a reader to a waitset with a read condition.
 *
 * @param waitset The DDS waitset entity to attach to.
 * @param reader The DDS reader entity to attach.
 */
void pong_attach(dds_entity_t waitset, dds_entity_t reader)
{
    dds_entity_t cond = dds_create_readcondition(reader, DDS_ANY_STATE);
    dds_waitset_attach(waitset, cond, reader);
}
