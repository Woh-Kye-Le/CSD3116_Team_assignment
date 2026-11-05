#ifndef INPUT_H
#define INPUT_H

#include <GLFW/glfw3.h>
#include "screens.h"

void input_install(GLFWwindow *window, UiState *ui);
int  input_paddle_dir(GLFWwindow *window, const UiState *ui);

#endif
