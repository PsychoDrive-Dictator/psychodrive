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
    void renderFrameMeter(int frameIndex);
    void renderFrameMeterCancelWindows(int frameIndex);
    int getSimCharSlot(void) { return rightSide ? 1 : 0; }

    int character;
    int charVersion;
    color charColor;
    bool changed;
    bool rightSide;

    // ui one
    float flStartPosX;
    // real one
    Fixed startPosX;

    // number of frames to show behind present, eg. how far right the current frame arrow is
    static constexpr int kFrameOffset = 8;
    static constexpr float kHorizSpacing = 1.0;
    static constexpr float kFrameButtonWidth = 25.0;

    std::map<int, std::pair<int, int>> timelineTriggers;

    std::vector<std::string> vecTriggerDropDownLabels;
    std::vector<std::pair<int, int>> vecTriggers;
    int pendingTriggerAdd;
};

class SimulationController {
public:
    SimulationController() { Reset(); }
    void Reset(void);
    bool NewSim(void);
    void RenderUI(void);
    void AdvanceUntilComplete(void);
    void doFrameMeterDrag(void);

    std::vector<CharacterUIController> charControllers;

    static constexpr float kFrameMeterDragRatio = 2.5;

    Simulation *pSim = nullptr;
    int scrubberFrame = 0;
    int charCount = 1;

    float pendingFrameMeterMouseDragAmount;
    float frameMeterMouseDragAmount;
    float lastDragDelta;
    bool momentumActive = false;
    float curMomentum;
    ImGuiID activeDragID = 0;
};

extern SimulationController simController;
extern bool simInputsChanged;

void timelineToInputBuffer(std::deque<int> &inputBuffer);
void renderUI(float frameRate, std::deque<std::string> *pLogQueue, int sizeX, int sizeY);
ImGuiIO& initUI(void);
void destroyUI(void);