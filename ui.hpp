#pragma once

#include <deque>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

void timelineToInputBuffer(std::deque<int> &inputBuffer);
void renderUI(int currentInput, float frameRate, std::deque<std::string> *pLogQueue);
ImGuiIO& initUI(void);
void destroyUI(void);