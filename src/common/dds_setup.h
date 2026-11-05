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

int  pong_dds_create(PongDds *d);
void pong_dds_destroy(PongDds *d);

dds_entity_t pong_reader(PongDds *d, dds_entity_t topic, const dds_qos_t *qos);
dds_entity_t pong_writer(PongDds *d, dds_entity_t topic, const dds_qos_t *qos);

void pong_attach(dds_entity_t waitset, dds_entity_t reader);

#endif
