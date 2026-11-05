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

void screens_render(const UiState *ui, const ClientView *view, const char *username);

int  screens_menu_item_at(double mx, double my);
bool screens_back_button_at(double mx, double my);

#endif
