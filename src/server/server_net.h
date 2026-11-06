/**
 * @file    server_net.h
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Server-side networking and DDS communication.
 *
 * @version 1.0
 */
#ifndef SERVER_NET_H
#define SERVER_NET_H

#include <stdatomic.h>
#include "dds_setup.h"
#include "auth.h"
#include "world.h"

typedef struct
{
    World        *world;
    Auth         *auth;
    atomic_bool  *stop;
    dds_entity_t  waitset;
    dds_entity_t  join_reader;
    dds_entity_t  proof_reader;
    dds_entity_t  input_reader;
} ServerNet;

/**
 * @brief Initialize the server network state.
 * @param n Pointer to the ServerNet structure to initialize.
 * @param dds Pointer to the DDS setup structure.
 * @param world Pointer to the game world structure.
 * @param auth Pointer to the authentication structure.
 * @param stop Pointer to the atomic stop flag.
 */
void  server_net_init(ServerNet *n, PongDds *dds, World *world, Auth *auth, atomic_bool *stop);

/**
 * @brief The worker thread loop for server networking.
 * @param arg Pointer to the ServerNet structure.
 * @return NULL.
 */
void *server_net_thread(void *arg);

#endif
