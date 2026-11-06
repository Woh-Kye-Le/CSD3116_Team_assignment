/**
 * @file    main.c
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Client application entry point.
 *
 * @version 1.0
 */
#include <stdio.h>
#include <string.h>
#include <GLFW/glfw3.h>

#include "protocol.h"
#include "dds_setup.h"
#include "login.h"
#include "shared.h"
#include "client_net.h"
#include "screens.h"
#include "input.h"
#include "textures.h"
#include "effects.h"

typedef struct
{
    int        screen;
    int        menu_sel;
    bool       back_hover;
    ClientView view;
} FrameKey;

/**
 * @brief Initializes and opens a GLFW window for the client.
 *
 * @param username The username of the client to display in the window title.
 * @return A pointer to the created GLFWwindow, or NULL on failure.
 */
static GLFWwindow *open_window(const char *username)
{
    if (!glfwInit())
        return NULL;
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    char title[96];
    snprintf(title, sizeof(title), "Secure DDS Pong - %s", username);
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, title, NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return NULL;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIDTH, HEIGHT, 0, -1, 1);
    return window;
}

/**
 * @brief Captures the current UI state into a FrameKey for rendering comparisons.
 *
 * @param ui Pointer to the current UiState.
 * @param frame Pointer to the FrameKey structure to populate.
 */
static void capture_frame(const UiState *ui, FrameKey *frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->screen = ui->screen;
    frame->menu_sel = ui->menu_sel;
    frame->back_hover = ui->back_hover;
    if (ui->screen == SCREEN_GAME)
        shared_view_get(&frame->view);
}

/**
 * @brief Updates visual effects based on the UI state and game frame changes.
 *
 * @param ui Pointer to the current UiState.
 * @param frame Pointer to the current FrameKey.
 * @param last Pointer to the previous FrameKey.
 * @param dt The time elapsed since the last frame in seconds.
 */
static void update_effects(const UiState *ui, const FrameKey *frame, const FrameKey *last, float dt)
{
    if (ui->screen == SCREEN_MENU)
    {
        effects_menu_update(dt);
        return;
    }
    if (ui->screen != SCREEN_GAME)
        return;

    if (last->screen != SCREEN_GAME || !frame->view.have_snap)
        effects_reset();
    else if (memcmp(&frame->view.snap, &last->view.snap, sizeof(GameSnapshot)) != 0)
        effects_observe(&frame->view.snap);

    effects_update(dt, frame->view.have_snap && frame->view.snap.paused);
}

/**
 * @brief Waits for the required time to maintain the target frame rate.
 *
 * @param next_frame Pointer to the expected time of the next frame.
 * @param frame_time The duration of a single frame.
 */
static void wait_for_next_frame(dds_time_t *next_frame, dds_duration_t frame_time)
{
    *next_frame += frame_time;
    dds_time_t now = mono_now();
    if (now > *next_frame + frame_time)
        *next_frame = now;
    else if (*next_frame > now)
        dds_sleepfor(*next_frame - now);
}

/**
 * @brief Executes the main application frame loop for rendering and input handling.
 *
 * @param window The GLFW window instance.
 * @param ui Pointer to the UI state.
 * @param username The current user's username.
 */
static void run_frame_loop(GLFWwindow *window, UiState *ui, const char *username)
{
    const dds_duration_t frame_time = DDS_NSECS_IN_SEC / TICK_HZ;
    dds_time_t next_frame = mono_now();
    dds_time_t last_time = next_frame;
    FrameKey last_frame;
    memset(&last_frame, 0, sizeof(last_frame));
    last_frame.screen = -1;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        shared_set_input_dir(input_paddle_dir(window, ui));

        dds_time_t now = mono_now();
        float dt = (float)(now - last_time) / DDS_NSECS_IN_SEC;
        last_time = now;

        FrameKey frame;
        capture_frame(ui, &frame);
        update_effects(ui, &frame, &last_frame, dt);

        bool animating = ui->screen == SCREEN_MENU || effects_active();
        bool changed = ui->need_redraw || animating || memcmp(&frame, &last_frame, sizeof(frame)) != 0;
        if (changed)
        {
            screens_render(ui, &frame.view, username);
            glfwSwapBuffers(window);
            last_frame = frame;
            ui->need_redraw = false;
        }

        wait_for_next_frame(&next_frame, frame_time);
    }
}

/**
 * @brief The main entry point for the client application.
 *
 * @param argc The number of command-line arguments.
 * @param argv The command-line arguments.
 * @return 0 on success, non-zero on failure.
 */
int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (sec_init() != 0)
    {
        fprintf(stderr, "libsodium failed to initialise\n");
        return 1;
    }

    Credentials creds;
    memset(&creds, 0, sizeof(creds));
    if (login_read(argc, argv, &creds) != 0)
        return 1;

    PongDds dds;
    if (pong_dds_create(&dds) != 0)
        return 1;

    ClientNet net;
    client_net_init(&net, &dds, &creds);
    if (client_net_start(&net) != 0)
        return 1;

    GLFWwindow *window = open_window(creds.username);
    if (!window)
    {
        client_net_stop(&net);
        return 1;
    }

    textures_init();

    UiState ui = { .screen = SCREEN_MENU, .menu_sel = MENU_PLAY, .back_hover = false, .need_redraw = true };
    input_install(window, &ui);
    run_frame_loop(window, &ui, creds.username);

    client_net_stop(&net);
    textures_destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    pong_dds_destroy(&dds);
    login_wipe(&creds);
    return 0;
}
