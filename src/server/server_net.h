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

void  server_net_init(ServerNet *n, PongDds *dds, World *world, Auth *auth, atomic_bool *stop);
void *server_net_thread(void *arg);

#endif
