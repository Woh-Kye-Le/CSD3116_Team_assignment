/**
 * @file    input.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Input handling and processing.
 *
 * @version 1.0
 */
#ifndef INPUT_H
#define INPUT_H

#include <GLFW/glfw3.h>
#include "screens.h"

/**
 * @brief Installs input callbacks for the GLFW window.
 * @param window Pointer to the GLFW window.
 * @param ui Pointer to the UI state.
 */
void input_install(GLFWwindow *window, UiState *ui);

/**
 * @brief Gets the current paddle direction from keyboard input.
 * @param window Pointer to the GLFW window.
 * @param ui Pointer to the UI state.
 * @return The paddle direction (-1 for up, 1 for down, 0 for idle).
 */
int  input_paddle_dir(GLFWwindow *window, const UiState *ui);

#endif
