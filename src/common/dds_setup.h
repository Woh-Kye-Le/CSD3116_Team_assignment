/**
 * @file    dds_setup.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   DDS loop optimization and setup.
 *
 * @version 1.0
 */
#ifndef DDS_SETUP_H
#define DDS_SETUP_H

#include "dds/dds.h"

#define MAX_TAKE 8

typedef struct
{
    dds_entity_t participant;
    dds_entity_t t_join, t_challenge, t_proof, t_response, t_input, t_state;
    dds_qos_t   *qos_handshake, *qos_input, *qos_state;
} PongDds;

/**
 * @brief Initializes the DDS participant, QoS policies, and topics.
 *
 * @param d Pointer to the PongDds structure to initialize.
 * @return 0 on success, -1 on failure.
 */
int  pong_dds_create(PongDds *d);

/**
 * @brief Destroys the DDS participant and QoS policies.
 *
 * @param d Pointer to the PongDds structure to destroy.
 */
void pong_dds_destroy(PongDds *d);

/**
 * @brief Creates a DDS reader for a specific topic.
 *
 * @param d Pointer to the PongDds structure containing the participant.
 * @param topic The DDS topic entity to read from.
 * @param qos The QoS policy to apply to the reader.
 * @return The created DDS reader entity, or terminates on failure.
 */
dds_entity_t pong_reader(PongDds *d, dds_entity_t topic, const dds_qos_t *qos);

/**
 * @brief Creates a DDS writer for a specific topic.
 *
 * @param d Pointer to the PongDds structure containing the participant.
 * @param topic The DDS topic entity to write to.
 * @param qos The QoS policy to apply to the writer.
 * @return The created DDS writer entity, or terminates on failure.
 */
dds_entity_t pong_writer(PongDds *d, dds_entity_t topic, const dds_qos_t *qos);

/**
 * @brief Attaches a reader to a waitset with a read condition.
 *
 * @param waitset The DDS waitset entity to attach to.
 * @param reader The DDS reader entity to attach.
 */
void pong_attach(dds_entity_t waitset, dds_entity_t reader);

#endif
