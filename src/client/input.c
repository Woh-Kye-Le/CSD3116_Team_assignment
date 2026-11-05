#include "input.h"
#include "shared.h"

static UiState *ui_of(GLFWwindow *window)
{
    return (UiState *)glfwGetWindowUserPointer(window);
}

static void open_game(UiState *ui)
{
    ui->screen = SCREEN_GAME;
    shared_request(REQ_PLAY);
}

static void leave_to_menu(UiState *ui)
{
    shared_request(REQ_LEAVE);
    ui->screen = SCREEN_MENU;
}

static void activate_menu_item(GLFWwindow *window, UiState *ui, int item)
{
    switch (item)
    {
    case MENU_PLAY:
        open_game(ui);
        break;
    case MENU_INSTRUCTIONS:
        ui->screen = SCREEN_INSTRUCTIONS;
        ui->back_hover = false;
        break;
    case MENU_QUIT:
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    default:
        break;
    }
}

static bool is_confirm_key(int key)
{
    return key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE;
}

static void on_menu_key(GLFWwindow *window, UiState *ui, int key)
{
    if (key == GLFW_KEY_UP || key == GLFW_KEY_W)
        ui->menu_sel = (ui->menu_sel + MENU_ITEMS - 1) % MENU_ITEMS;
    else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S)
        ui->menu_sel = (ui->menu_sel + 1) % MENU_ITEMS;
    else if (is_confirm_key(key))
        activate_menu_item(window, ui, ui->menu_sel);
}

static void on_instructions_key(UiState *ui, int key)
{
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_BACKSPACE || is_confirm_key(key))
        ui->screen = SCREEN_MENU;
}

static void on_game_key(UiState *ui, int key)
{
    if (key == GLFW_KEY_Q)
        leave_to_menu(ui);
    else if (key == GLFW_KEY_ESCAPE)
    {
        if (shared_in_match())
            shared_press_pause();
        else
            leave_to_menu(ui);
    }
}

static void on_key(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if (action != GLFW_PRESS)
        return;

    UiState *ui = ui_of(window);
    switch (ui->screen)
    {
    case SCREEN_MENU:         on_menu_key(window, ui, key); break;
    case SCREEN_INSTRUCTIONS: on_instructions_key(ui, key); break;
    case SCREEN_GAME:         on_game_key(ui, key); break;
    }
}

static void on_cursor(GLFWwindow *window, double mx, double my)
{
    UiState *ui = ui_of(window);
    if (ui->screen == SCREEN_MENU)
    {
        int item = screens_menu_item_at(mx, my);
        if (item >= 0)
            ui->menu_sel = item;
    }
    else if (ui->screen == SCREEN_INSTRUCTIONS)
        ui->back_hover = screens_back_button_at(mx, my);
}

static void on_mouse_button(GLFWwindow *window, int button, int action, int mods)
{
    (void)mods;
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;

    UiState *ui = ui_of(window);
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    if (ui->screen == SCREEN_MENU)
    {
        int item = screens_menu_item_at(mx, my);
        if (item >= 0)
            activate_menu_item(window, ui, item);
    }
    else if (ui->screen == SCREEN_INSTRUCTIONS && screens_back_button_at(mx, my))
        ui->screen = SCREEN_MENU;
}

static void on_refresh(GLFWwindow *window)
{
    ui_of(window)->need_redraw = true;
}

void input_install(GLFWwindow *window, UiState *ui)
{
    glfwSetWindowUserPointer(window, ui);
    glfwSetKeyCallback(window, on_key);
    glfwSetCursorPosCallback(window, on_cursor);
    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetWindowRefreshCallback(window, on_refresh);
}

int input_paddle_dir(GLFWwindow *window, const UiState *ui)
{
    if (ui->screen != SCREEN_GAME)
        return 0;
    bool up   = glfwGetKey(window, GLFW_KEY_UP)   == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool down = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    return (up && !down) ? -1 : (down && !up) ? 1 : 0;
}
