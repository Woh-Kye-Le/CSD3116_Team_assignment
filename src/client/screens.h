/**
 * @file    screens.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Screen and UI state management.
 *
 * @version 1.0
 */
#ifndef SCREENS_H
#define SCREENS_H

#include <stdbool.h>
#include "shared.h"

enum { SCREEN_MENU, SCREEN_INSTRUCTIONS, SCREEN_GAME };
enum { MENU_PLAY, MENU_INSTRUCTIONS, MENU_QUIT, MENU_ITEMS };

typedef struct
{
    int  screen;
    int  menu_sel;
    bool back_hover;
    bool need_redraw;
} UiState;

/**
 * @brief Renders the current screen based on the UI state.
 *
 * @param ui Pointer to the UI state.
 * @param view Pointer to the client view state.
 * @param username The username of the player.
 */
void screens_render(const UiState *ui, const ClientView *view, const char *username);

/**
 * @brief Determines which menu item is located at the given coordinates.
 *
 * @param mx The x-coordinate of the mouse.
 * @param my The y-coordinate of the mouse.
 * @return The index of the menu item, or -1 if no item is at the coordinates.
 */
int  screens_menu_item_at(double mx, double my);

/**
 * @brief Checks if the back button is located at the given coordinates.
 *
 * @param mx The x-coordinate of the mouse.
 * @param my The y-coordinate of the mouse.
 * @return True if the back button is at the coordinates, false otherwise.
 */
bool screens_back_button_at(double mx, double my);

#endif
