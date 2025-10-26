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

    std::vector<std::vector<CommandInput>> variants;
};

struct Trigger {
    int id;

    int actionID;

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

    int comboInst;

    int64_t flags = 0;
};

struct TriggerGroupEntry {
    int actionID;
    int triggerID;
    Trigger *pTrigger;
};

struct TriggerGroup {
    int id;

    std::vector<TriggerGroupEntry> entries;
};

struct Rect {
    int listID;
    int id;

    int xOrig;
    int yOrig;
    int xRadius;
    int yRadius;
};

struct CharacterData {
    std::string charName;
    int charVersion;

    std::vector<Charge> charges;
    std::vector<Command> commands;
    std::vector<Trigger> triggers;
    std::vector<TriggerGroup> triggerGroups;
    std::vector<Rect> rects;

    std::map<int, TriggerGroup*> triggerGroupByID;
    std::map<std::pair<int, int>, Rect*> rectsByIDs;

    std::map<std::pair<int, int>, std::pair<std::string, bool>> mapMoveStyle;
    std::vector<const char *> vecMoveList;
};

CharacterData *loadCharacter(std::string charName, int charVersion);
