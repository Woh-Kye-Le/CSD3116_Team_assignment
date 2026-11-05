#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <signal.h>
#include <pthread.h>

#include "protocol.h"
#include "dds_setup.h"
#include "users.h"
#include "world.h"
#include "auth.h"
#include "server_net.h"
#include "perf.h"

static atomic_bool g_stop = false;

static void on_sigint(int sig)
{
    (void)sig;
    atomic_store(&g_stop, true);
}

static int print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s                         run the server\n"
                    "       %s adduser <name> <pass>   add a login to %s\n",
            prog, prog, users_path());
    return 1;
}

static void broadcast_state(dds_entity_t state_writer, const GameSnapshot *snap, Outbox *out)
{
    uint8_t plain[STATE_PLAIN_LEN];
    pack_state(snap, plain);

    for (int k = 0; k < out->count; k++)
    {
        GameState gs;
        gs.int_player_id = out->player_id[k];
        gs.seq = out->seq[k];
        sec_seal(gs.data, plain, STATE_PLAIN_LEN, SEC_DIR_S2C, out->player_id[k], out->seq[k],
                 out->session_key[k]);
        dds_return_t rc = dds_write(state_writer, &gs);
        if (rc != DDS_RETCODE_OK)
            fprintf(stderr, "dds_write(state): %s\n", dds_strretcode(-rc));
    }
    sec_wipe(out->session_key, sizeof(out->session_key));
}

static void sleep_until(dds_time_t *next_tick, dds_duration_t tick, dds_time_t now)
{
    *next_tick += tick;
    if (now > *next_tick + 5 * tick)
        *next_tick = now;
    dds_duration_t wait = *next_tick - mono_now();
    if (wait > 0)
        dds_sleepfor(wait);
}

static void run_simulation(World *world, dds_entity_t state_writer)
{
    const dds_duration_t tick = DDS_NSECS_IN_SEC / TICK_HZ;
    const float dt = 1.0f / TICK_HZ;
    dds_time_t next_tick = mono_now();

    PerfStats perf;
    perf_init(&perf, next_tick);

    while (!atomic_load(&g_stop))
    {
        dds_time_t t0 = mono_now();
        GameSnapshot snap;
        Outbox out;

        world_lock(world);
        world_tick_locked(world, dt, t0, &snap, &out);
        world_unlock(world);

        int players = out.count;
        broadcast_state(state_writer, &snap, &out);

        dds_time_t t1 = mono_now();
        perf_record(&perf, t0, t1, players);
        sleep_until(&next_tick, tick, t1);
    }
}

static int run_server(void)
{
    if (users_load() <= 0)
    {
        fprintf(stderr, "No users found in %s. Add one with: ./bin/server adduser <name> <password>\n",
                users_path());
        return 1;
    }
    printf("Loaded %d user(s) from %s\n", users_count(), users_path());

    signal(SIGINT, on_sigint);

    PongDds dds;
    if (pong_dds_create(&dds) != 0)
        return 1;

    World world;
    world_init(&world);

    Auth auth;
    auth_init(&auth, &world,
              pong_writer(&dds, dds.t_challenge, dds.qos_handshake),
              pong_writer(&dds, dds.t_response,  dds.qos_handshake));

    ServerNet net;
    server_net_init(&net, &dds, &world, &auth, &g_stop);

    dds_entity_t state_writer = pong_writer(&dds, dds.t_state, dds.qos_state);

    pthread_t thread;
    if (pthread_create(&thread, NULL, server_net_thread, &net) != 0)
    {
        perror("pthread_create");
        return 1;
    }

    printf("Secure Pong server running. Waiting for players... (Ctrl+C to stop)\n");
    run_simulation(&world, state_writer);

    printf("\nShutting down...\n");
    pthread_join(thread, NULL);
    pong_dds_destroy(&dds);
    auth_destroy(&auth);
    world_destroy(&world);
    users_wipe();
    return 0;
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (sec_init() != 0)
    {
        fprintf(stderr, "libsodium failed to initialise\n");
        return 1;
    }
    users_resolve_path();

    if (argc == 4 && strcmp(argv[1], "adduser") == 0)
        return users_add(argv[2], argv[3]);
    if (argc != 1)
        return print_usage(argv[0]);
    return run_server();
}
