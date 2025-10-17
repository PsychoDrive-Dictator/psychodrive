#pragma once

#include <map>
#include <vector>

#include "json.hpp"
#include "fixed.hpp"

enum InputType {
    Normal = 0,
    ChargeRelease = 1,
    Rotation = 2
};

struct Charge {
    int id;

    uint32_t okKeyFlags;
    uint32_t okCondFlags;
    int chargeFrames;
    int keepFrames;
};

struct CommandInput {
    InputType type;
    int numFrames;

    uint32_t okKeyFlags;
    uint32_t okCondFlags;
    uint32_t ngKeyFlags;
    uint32_t ngCondFlags;

    int rotatePointsNeeded;

    Charge *pCharge = nullptr;
};

struct Command {
    int id;

    std::vector<CommandInput> vecInputs;
};

struct Trigger {
    int id;

    Command *pCommandClassic = nullptr;

    int okKeyFlags;
    int okCondFlags;
    int ngKeyFlags;
    int dcExcFlags;
    int dcIncFlags;
    int precedingTime;

    bool useUniqueParam = false;
    int condParamID;
    int condParamOp;
    int condParamValue;

    int limitShotCount;
    int limitShotCategory;

    int airActionCountLimit;

    int vitalOp;
    int vitalRatio;

    int rangeCondition = 0;
    Fixed rangeParam;

    int stateCondition = 0;
    
    bool needsFocus = false;
    int focusCost;

    bool needsGauge = false;
    int gaugeCost;

    int64_t flags = 0;
};

typedef std::vector<std::pair<int,int>> TriggerGroup;

struct CharacterData {
    std::map<int, Charge> mapCharges;
    std::map<int, Command> mapCommands;
    std::map<int,TriggerGroup> mapTriggerGroups;
    std::map<int,Trigger> mapTriggers;
};

CharacterData *loadCharacter(nlohmann::json *pTriggerGroupsJson, nlohmann::json *pTriggersJson, nlohmann::json *pCommandsJson, nlohmann::json *pChargeJson);
