#pragma once

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

void renderUI(int currentInput, float frameRate, std::deque<std::string> *pLogQueue);
