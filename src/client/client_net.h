/**
 * @file    client_net.h
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Client-side networking and DDS communication.
 *
 * @version 1.0
 */
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

/**
 * @brief Initialize the client network state.
 * @param n Pointer to the ClientNet structure to initialize.
 * @param dds Pointer to the DDS setup structure.
 * @param creds Pointer to the user credentials.
 */
void client_net_init(ClientNet *n, PongDds *dds, const Credentials *creds);

/**
 * @brief Start the client network worker thread.
 * @param n Pointer to the initialized ClientNet structure.
 * @return 0 on success, or a non-zero error code on failure.
 */
int  client_net_start(ClientNet *n);

/**
 * @brief Stop the client network worker thread and clean up.
 * @param n Pointer to the ClientNet structure.
 */
void client_net_stop(ClientNet *n);

#endif
