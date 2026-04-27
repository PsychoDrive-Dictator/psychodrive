#pragma once

#include <deque>

#include "chara.hpp"
#include "simulation.hpp"
#include "guy.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

struct inputRegion {
    int frame;
    int duration;
    int input;
};

enum CharacterControllerOptionFlags {
    buffLevel1 = 1,
    buffLevel2 = 2,
    buffLevel3 = 4,
    buffLevel4 = 8,
    buffLevel5 = 16
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
    int getOptionFlags();
    void setOptionFlags(int flags);

    int character;
    int charVersion;
    color charColor;
    bool changed;
    bool rightSide;

    // ui one
    float flStartPosX;
    // real one
    Fixed startPosX = Fixed(0);

    int buffLevel = 0;
    int startHealth = 0;
    int maxStartHealth = 0;
    int startFocus = maxFocus;
    int startGauge = 0;
    int maxStartGauge = 0;

    // number of frames to show behind present, eg. how far right the current frame arrow is
    static constexpr int kFrameOffset = 8;
    static constexpr float kHorizSpacing = 1.0;
    static constexpr float kFrameButtonWidth = 25.0;

    std::map<int, ActionRef> timelineTriggers;
    std::vector<inputRegion> inputRegions;
    int forcedInput = 0;

    std::vector<std::string> vecTriggerDropDownLabels;
    std::vector<ActionRef> vecTriggers;
    bool triggerAdded = false;
    int pendingTriggerAdd;

    int activeInputDragID = 0;
};

enum SimulationControllerOptionFlags {
    simCounter = 1,
    simPunishCounter = 2
};

class SimulationController {
public:
    SimulationController() { Reset(); }
    ~SimulationController();
    void Reset(void);
    void Restore(std::string strSerialized);
    void Serialize(std::string &outStr);
    bool NewSim(void);
    void RenderUI(void);
    void RenderComboMinerSetup(void);
    void AdvanceUntilComplete(void);
    void AdvanceFromReplay(ReplayDecoder &decoder);
    void AdvanceFromDump();
    void doFrameMeterDrag(void);
    void RecordFrame(void);
    Guy *getRecordedGuy(int frameIndex, int guyID);
    void renderRecordedHitMarkers(int frameIndex);
    Simulation *getSnapshotAtFrame(int frameIndex);
    void getFinishedSnapshotAtFrame(Simulation *pSimDst, int frameIndex);
    int getOptionFlags();
    void setOptionFlags(int flags);
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
    int prevScrubberFrame = -1;
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

    ObjectPool<Guy> guyPool{600};

    std::deque<RecordedFrame> stateRecording;

    // dump viewer state
    std::string viewerDumpPath;
    int viewerDumpVersion = -1;
    bool viewerDumpIsMatch = false;

    // replay viewer state
    struct ReplayRoundResult {
        std::string summary;
        bool hasErrors = false;
    };
    std::vector<ReplayRoundResult> replayRoundResults;
    nlohmann::json replayInfo;
    std::vector<uint8_t> replayInputData;
    int replayVersion = -1;
    int replayCurrentRound = -1;
    int replayRoundCount = 0;

    void LoadReplayRound(int round);
    void ValidateAllRounds();
    void ReloadViewer();

    bool viewerErrorTypeFilter[14] = {true,false,false,false,false,true,true,true,true,true,true,true,true,true};

    bool viewerLogTransitions = false;
    bool viewerLogTriggers = false;
    bool viewerLogUnknowns = true;
    bool viewerLogHits = false;
    bool viewerLogBranches = false;
    bool viewerLogResources = false;
};

extern SimulationController simController;
extern bool simInputsChanged;

void timelineToInputBuffer(std::deque<int> &inputBuffer);
void renderUI(float frameRate, std::deque<std::string> *pLogQueue, int sizeX, int sizeY);
ImGuiIO& initUI(void);
void destroyUI(void);