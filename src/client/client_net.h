#ifndef CLIENT_NET_H
#define CLIENT_NET_H

#include <pthread.h>
#include "dds_setup.h"
#include "login.h"

typedef struct
{
    const Credentials *creds;
    dds_entity_t       join_writer, proof_writer, input_writer;
    dds_entity_t       challenge_reader, response_reader, state_reader;
    dds_entity_t       waitset;
    pthread_t          thread;
} ClientNet;

void client_net_init(ClientNet *n, PongDds *dds, const Credentials *creds);
int  client_net_start(ClientNet *n);
void client_net_stop(ClientNet *n);

#endif
