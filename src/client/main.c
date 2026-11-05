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

typedef struct
{
    int        screen;
    int        menu_sel;
    bool       back_hover;
    ClientView view;
} FrameKey;

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

static void capture_frame(const UiState *ui, FrameKey *frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->screen = ui->screen;
    frame->menu_sel = ui->menu_sel;
    frame->back_hover = ui->back_hover;
    if (ui->screen == SCREEN_GAME)
        shared_view_get(&frame->view);
}

static void wait_for_next_frame(dds_time_t *next_frame, dds_duration_t frame_time)
{
    *next_frame += frame_time;
    dds_time_t now = mono_now();
    if (now > *next_frame + frame_time)
        *next_frame = now;
    else if (*next_frame > now)
        dds_sleepfor(*next_frame - now);
}

static void run_frame_loop(GLFWwindow *window, UiState *ui, const char *username)
{
    const dds_duration_t frame_time = DDS_NSECS_IN_SEC / TICK_HZ;
    dds_time_t next_frame = mono_now();
    FrameKey last_frame;
    memset(&last_frame, 0, sizeof(last_frame));
    last_frame.screen = -1;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        shared_set_input_dir(input_paddle_dir(window, ui));

        FrameKey frame;
        capture_frame(ui, &frame);
        bool changed = ui->need_redraw || memcmp(&frame, &last_frame, sizeof(frame)) != 0;
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

    UiState ui = { .screen = SCREEN_MENU, .menu_sel = MENU_PLAY, .back_hover = false, .need_redraw = true };
    input_install(window, &ui);
    run_frame_loop(window, &ui, creds.username);

    client_net_stop(&net);
    glfwDestroyWindow(window);
    glfwTerminate();
    pong_dds_destroy(&dds);
    login_wipe(&creds);
    return 0;
}
