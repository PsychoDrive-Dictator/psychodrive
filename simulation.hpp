#pragma once

#include <string>
#include <vector>

#include "fixed.hpp"
#include "main.hpp"

class Simulation {
public:
    void CreateGuy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color);
    void CreateGuyFromDumpedPlayer(nlohmann::json &playerJson, int version);

    bool SetupFromGameDump(std::string dumpPath, int version);

    enum ErrorType {
        ePos = 0,
        eVel,
        eAccel,
        eHitVel,
        eHitAccel,
        eActionID,
        eActionFrame
    };

    void CompareGameStateFixed( Fixed dumpValue, Fixed realValue, int player, int frame, ErrorType errorType, std::string description );
    void CompareGameStateInt( int64_t dumpValue, int64_t realValue, int player, int frame, ErrorType errorType, std::string description );

    void Log(std::string logLine);

    void AdvanceFrame();

    std::vector<Guy *> simGuys;
    std::vector<Guy *> vecGuysToDelete;

    int frameCounter = 0;

    bool replayingGameStateDump = false;
    nlohmann::json gameStateDump;
    int gameStateFrame = 0;
    int firstGameStateFrame = 0;
    int replayErrors = 0;
};
