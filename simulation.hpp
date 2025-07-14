#pragma once

#include <string>
#include <vector>

#include "fixed.hpp"
#include "main.hpp"

class CharacterUIController;

struct FrameEvent {
    enum Type {
        Hit = 0
    };
    Type type;

    struct HitEventData {
        int targetID;
        float x;
        float y;
        float radius;
        int hitType; // 0 = normal hit, 1 = block
        int seed;
        float dirX;
        float dirY;
    };

    union {
        HitEventData hitEventData;
    };
};

struct RecordedFrame {
    std::map<int,Guy*> guys;
    std::vector<FrameEvent> events;

    Guy* findGuyByID(int uniqueID) {
        auto it = guys.find(uniqueID);
        return (it != guys.end()) ? it->second : nullptr;
    }
};

class Simulation {
public:
    ~Simulation();
    void CreateGuy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color);
    void CreateGuyFromDumpedPlayer(nlohmann::json &playerJson, int version);
    void CreateGuyFromCharController(CharacterUIController &controller);

    bool SetupFromGameDump(std::string dumpPath, int version);

    enum ErrorType {
        ePos = 0,
        eVel,
        eAccel,
        eHitVel,
        eHitAccel,
        eActionID,
        eActionFrame,
        eComboCount,
        eDirection
    };

    void CompareGameStateFixed( Fixed dumpValue, Fixed realValue, int player, int frame, ErrorType errorType, std::string description );
    void CompareGameStateInt( int64_t dumpValue, int64_t realValue, int player, int frame, ErrorType errorType, std::string description );

    void Log(std::string logLine);

    void RunFrame();
    void AdvanceFrame();
    std::vector<FrameEvent>& getCurrentFrameEvents() { return currentFrameEvents; }
    Guy *getRecordedGuy(int frameIndex, int guyID);
    void renderRecordedGuys(int frameIndex);
    void renderRecordedHitMarkers(int frameIndex);

    std::vector<Guy *> everyone;
    int guyIDCounter = 0;
    std::vector<Guy *> simGuys;
    std::vector<Guy *> vecGuysToDelete;

    int frameCounter = 0;

    bool replayingGameStateDump = false;
    nlohmann::json gameStateDump;
    int gameStateFrame = 0;
    int firstGameStateFrame = 0;
    int replayErrors = 0;

    bool recordingState = false;
    std::vector<RecordedFrame> stateRecording;
    std::vector<FrameEvent> currentFrameEvents;
};
