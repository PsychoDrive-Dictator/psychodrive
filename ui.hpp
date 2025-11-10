#pragma once

#include <deque>

#include "chara.hpp"
#include "simulation.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

struct inputRegion {
    int frame;
    int duration;
    int input;
};

class CharacterUIController {
public:
    void RenderUI(void);
    void Serialize(std::string &outStr);
    void renderCharSetup(void);
    void renderActionSetup(int frameIndex);
    void renderFrameMeter(int frameIndex);
    void renderFrameMeterCancelWindows(int frameIndex);
    int getSimCharSlot(void) { return rightSide ? 1 : 0; }
    int getInput(int frameIndex);

    int character;
    int charVersion;
    color charColor;
    bool changed;
    bool rightSide;

    // ui one
    float flStartPosX;
    // real one
    Fixed startPosX = Fixed(0);

    // number of frames to show behind present, eg. how far right the current frame arrow is
    static constexpr int kFrameOffset = 8;
    static constexpr float kHorizSpacing = 1.0;
    static constexpr float kFrameButtonWidth = 25.0;

    std::map<int, ActionRef> timelineTriggers;
    std::vector<inputRegion> inputRegions;

    std::vector<std::string> vecTriggerDropDownLabels;
    std::vector<ActionRef> vecTriggers;
    bool triggerAdded = false;
    int pendingTriggerAdd;

    int activeInputDragID = 0;
};

class SimulationController {
public:
    SimulationController() { Reset(); }
    void Reset(void);
    void Restore(std::string strSerialized);
    void Serialize(std::string &outStr);
    bool NewSim(void);
    void RenderUI(void);
    void RenderComboMinerSetup(void);
    void AdvanceUntilComplete(void);
    void doFrameMeterDrag(void);
    void clampFrame(int &frame) {
        if (frame < 0) {
            frame = 0;
        }
        if (frame >= simFrameCount) {
            frame = simFrameCount - 1;
        }
    }

    std::vector<CharacterUIController> charControllers;

    static constexpr float kFrameMeterDragRatio = 2.5;

    Simulation *pSim = nullptr;
    int scrubberFrame = 0;
    int simFrameCount = 0;
    int charCount = 1;

    int viewSelect = 0;

    int maxComboCount = 0;
    int maxComboDamage = 0;

    bool playing = false;
    int playSpeed = 1;
    int playUntilFrame = 0;

    float pendingFrameMeterMouseDragAmount;
    float frameMeterMouseDragAmount;
    float lastDragDelta;
    bool momentumActive = false;
    float curMomentum;
    ImGuiID activeDragID = 0;

    std::vector<Guy*> recordedGuysPool;
    int recordedGuysPoolIndex = 0;
};

extern SimulationController simController;
extern bool simInputsChanged;

void timelineToInputBuffer(std::deque<int> &inputBuffer);
void renderUI(float frameRate, std::deque<std::string> *pLogQueue, int sizeX, int sizeY);
ImGuiIO& initUI(void);
void destroyUI(void);