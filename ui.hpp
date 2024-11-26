#pragma once

#include <deque>

#include "simulation.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

struct timelineTrigger {
    int frame;
    int actionID;
    int styleID;
};

class CharacterUIController {
public:
    void RenderUI(void);

    int character;
    int charVersion;
    bool changed;
    bool rightSide;

    std::map<int, std::pair<int, int>> timelineTriggers;

    std::vector<std::string> vecTriggerDropDownLabels;
    std::vector<std::pair<int, int>> vecTriggers;
    int pendingTriggerAdd;
};

class SimulationController {
public:
    void NewSim(void);

    Simulation *pSim = nullptr;
    int scrubberFrame = 0;
};

extern SimulationController simController;

extern CharacterUIController leftCharController;
extern CharacterUIController rightCharController;
extern bool simInputsChanged;

void timelineToInputBuffer(std::deque<int> &inputBuffer);
void renderUI(float frameRate, std::deque<std::string> *pLogQueue, int sizeX, int sizeY);
ImGuiIO& initUI(void);
void destroyUI(void);