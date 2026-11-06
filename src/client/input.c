/**
 * @file    input.c
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Input handling and processing.
 *
 * @version 1.0
 */
#include "input.h"
#include "shared.h"

/**
 * @brief Retrieves the UI state from the GLFW window's user pointer.
 * @param window Pointer to the GLFW window.
 * @return Pointer to the UiState structure.
 */
static UiState *ui_of(GLFWwindow *window)
{
    return (UiState *)glfwGetWindowUserPointer(window);
}

/**
 * @brief Changes the screen to the game and requests to play.
 * @param ui Pointer to the UI state.
 */
static void open_game(UiState *ui)
{
    ui->screen = SCREEN_GAME;
    shared_request(REQ_PLAY);
}

/**
 * @brief Leaves the game and returns to the menu screen.
 * @param ui Pointer to the UI state.
 */
static void leave_to_menu(UiState *ui)
{
    shared_request(REQ_LEAVE);
    ui->screen = SCREEN_MENU;
}

/**
 * @brief Activates the selected menu item.
 * @param window Pointer to the GLFW window.
 * @param ui Pointer to the UI state.
 * @param item The menu item to activate.
 */
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

/**
 * @brief Checks if a key is a confirmation key (Enter or Space).
 * @param key The GLFW key code.
 * @return true if the key is a confirmation key, false otherwise.
 */
static bool is_confirm_key(int key)
{
    return key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE;
}

/**
 * @brief Handles key presses on the menu screen.
 * @param window Pointer to the GLFW window.
 * @param ui Pointer to the UI state.
 * @param key The GLFW key code.
 */
static void on_menu_key(GLFWwindow *window, UiState *ui, int key)
{
    if (key == GLFW_KEY_UP || key == GLFW_KEY_W)
        ui->menu_sel = (ui->menu_sel + MENU_ITEMS - 1) % MENU_ITEMS;
    else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S)
        ui->menu_sel = (ui->menu_sel + 1) % MENU_ITEMS;
    else if (is_confirm_key(key))
        activate_menu_item(window, ui, ui->menu_sel);
}

/**
 * @brief Handles key presses on the instructions screen.
 * @param ui Pointer to the UI state.
 * @param key The GLFW key code.
 */
static void on_instructions_key(UiState *ui, int key)
{
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_BACKSPACE || is_confirm_key(key))
        ui->screen = SCREEN_MENU;
}

/**
 * @brief Handles key presses on the game screen.
 * @param ui Pointer to the UI state.
 * @param key The GLFW key code.
 */
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

/**
 * @brief GLFW key callback function.
 * @param window Pointer to the GLFW window.
 * @param key The keyboard key that was pressed or released.
 * @param scancode The system-specific scancode of the key.
 * @param action GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
 * @param mods Bit field describing which modifier keys were held down.
 */
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

/**
 * @brief GLFW cursor position callback function.
 * @param window Pointer to the GLFW window.
 * @param mx The new cursor x-coordinate, relative to the left edge of the content area.
 * @param my The new cursor y-coordinate, relative to the top edge of the content area.
 */
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

/**
 * @brief GLFW mouse button callback function.
 * @param window Pointer to the GLFW window.
 * @param button The mouse button that was pressed or released.
 * @param action One of GLFW_PRESS or GLFW_RELEASE.
 * @param mods Bit field describing which modifier keys were held down.
 */
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

/**
 * @brief GLFW window refresh callback function.
 * @param window Pointer to the GLFW window.
 */
static void on_refresh(GLFWwindow *window)
{
    ui_of(window)->need_redraw = true;
}

/**
 * @brief Installs input callbacks for the GLFW window.
 * @param window Pointer to the GLFW window.
 * @param ui Pointer to the UI state.
 */
void input_install(GLFWwindow *window, UiState *ui)
{
    glfwSetWindowUserPointer(window, ui);
    glfwSetKeyCallback(window, on_key);
    glfwSetCursorPosCallback(window, on_cursor);
    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetWindowRefreshCallback(window, on_refresh);
}

/**
 * @brief Gets the current paddle direction from keyboard input.
 * @param window Pointer to the GLFW window.
 * @param ui Pointer to the UI state.
 * @return The paddle direction (-1 for up, 1 for down, 0 for idle).
 */
int input_paddle_dir(GLFWwindow *window, const UiState *ui)
{
    if (ui->screen != SCREEN_GAME)
        return 0;
    bool up   = glfwGetKey(window, GLFW_KEY_UP)   == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool down = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    return (up && !down) ? -1 : (down && !up) ? 1 : 0;
}
