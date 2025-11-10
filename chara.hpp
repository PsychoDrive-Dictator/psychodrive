#pragma once

#include <map>
#include <vector>

#include "json.hpp"
#include "fixed.hpp"
#include "main.hpp"

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

    int validStyles = 0;

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

struct AtemiData {
    int id;
    int targetStop;
    int ownerStop;
    int targetStopShell;
    int ownerStopShell;
    int resistLimit;
};

struct HitEntry {
    int comboAdd;
    int juggleFirst;
    int juggleAdd;
    int juggleLimit;
    int hitStun;
    int moveDestX;
    int moveDestY;
    int moveTime;
    int dmgValue;
    int dmgType;
    int dmgKind;
    int dmgPower;
    int moveType;
    int floorTime;
    int downTime;
    bool jimenBound;
    bool kabeBound;
    bool kabeTataki;
    bool bombBurst;
    int attr0;
    int attr1;
    int attr2;
    int attr3;
    int ext0;
    int hitStopOwner;
    int hitStopTarget;
    int hitmark;
    int floorDestX;
    int floorDestY;
    int wallTime;
    int wallStop;
    int wallDestX;
    int wallDestY;
};

struct HitData {
    int id;
    HitEntry common[5];
    HitEntry param[20];
};

enum KeyType {
    HurtBoxKeyType = 0,
    PushBoxKeyType = 1,
    HitBoxKeyType = 2
};

struct Key {
    int startFrame;
    int endFrame;
    KeyType keyType;
};

struct BoxKey : Key {
    int condition;
    Fixed offsetX;
    Fixed offsetY;
};

struct HurtBoxKey : BoxKey {
    HurtBoxKey() { keyType = HurtBoxKeyType; }

    bool isArmor = false;
    bool isAtemi = false;
    AtemiData *pAtemiData = nullptr;

    int immunity;
    int flags;

    std::vector<Rect *> headRects;
    std::vector<Rect *> bodyRects;
    std::vector<Rect *> legRects;
    std::vector<Rect *> throwRects;
};

struct PushBoxKey : BoxKey {
    PushBoxKey() { keyType = PushBoxKeyType; }

    Rect *rect;
};

struct HitBoxKey : BoxKey {
    HitBoxKey() { keyType = HitBoxKeyType; }

    hitBoxType type;
    hitBoxFlags flags;
    HitData *pHitData = nullptr;

    bool hasValidStyle = false;
    int validStyle;

    bool hasHitID = false;
    int hitID;

    std::vector<Rect *> rects;
};

struct SteerKey : Key {
    int operationType;
    int valueType;
    Fixed fixValue;
    Fixed targetOffsetX;
    Fixed targetOffsetY;
    int shotCategory;
    int targetType;
    int calcValueFrame;
    int multiValueType;
    int param;
    bool isDrive = false;
};

struct PlaceKeyPos {
    int frame;
    Fixed offset;
};

struct PlaceKey : Key {
    int optionFlag;
    Fixed ratio;
    int axis;
    std::vector<PlaceKeyPos> posList;
};

struct SwitchKey : Key {
    int systemFlag;
    int operationFlag;
    int validStyle = 0;
};

struct EventKey : Key {
    int validStyle = 0;
    int type;
    int id;
    int64_t param01;
    int64_t param02;
    int64_t param03;
    int64_t param04;
    int64_t param05;
};

struct WorldKey : Key {
    int type;
};

struct LockKey : Key {
    int type;
    int param01;
    int param02;
    HitEntry *pHitEntry = nullptr;
};

struct BranchKey : Key {
    int type;
    int64_t param00;
    int64_t param01;
    int64_t param02;
    int64_t param03;
    int64_t param04;
    int branchAction;
    int branchFrame;
    bool keepFrame;
    bool keepPlace;
    std::string typeName;
};

struct ShotKey : Key {
    int validStyle = 0;
    int operation;
    Fixed posOffsetX;
    Fixed posOffsetY;
    int actionId;
    int styleIdx;
};

struct ProjectileData {
    int id;
    int hitCount;
    bool hitFlagToParent;
    bool hitStopToParent;
    int rangeB;
    bool airborne;
    int flags;
    int category;
    bool noPush;
    int lifeTime;
};

struct Action {
    int actionID;
    int styleID;
    std::string name;

    std::vector<HurtBoxKey> hurtBoxKeys;
    std::vector<PushBoxKey> pushBoxKeys;
    std::vector<HitBoxKey> hitBoxKeys;
    std::vector<SteerKey> steerKeys;
    std::vector<PlaceKey> placeKeys;
    std::vector<SwitchKey> switchKeys;
    std::vector<EventKey> eventKeys;
    std::vector<WorldKey> worldKeys;
    std::vector<LockKey> lockKeys;
    std::vector<BranchKey> branchKeys;
    std::vector<ShotKey> shotKeys;

    int activeFrame;
    int recoveryStartFrame;
    int recoveryEndFrame;
    uint64_t actionFlags;
    int actionFrameDuration;
    int loopPoint;
    int loopCount;
    int startScale;
    int comboScale;
    int instantScale;

    ProjectileData *pProjectileData = nullptr;
    int inheritKindFlag = 0;
    bool inheritHitID = false;
    Fixed inheritAccelX = Fixed(1);
    Fixed inheritAccelY = Fixed(1);
    Fixed inheritVelX = Fixed(1);
    Fixed inheritVelY = Fixed(1);
};

struct CharacterData {
    std::string charName;
    int charID;
    int charVersion;

    std::vector<Charge> charges;
    std::vector<Command> commands;
    std::vector<Trigger> triggers;
    std::vector<TriggerGroup> triggerGroups;
    std::vector<Rect> rects;
    std::vector<ProjectileData> projectileDatas;
    std::vector<AtemiData> atemis;
    std::vector<HitData> hits;
    std::vector<Action> actions;

    std::map<int, TriggerGroup*> triggerGroupByID;
    std::map<std::pair<int, int>, Rect*> rectsByIDs;
    std::map<std::pair<int, int>, Action*> actionsByID;
    std::map<int, AtemiData*> atemiByID;
    std::map<int, HitData*> hitByID;

    std::map<std::pair<int, int>, std::pair<std::string, bool>> mapMoveStyle;
    std::map<std::pair<int, int>, nlohmann::json*> mapMoveJson;
    std::vector<const char *> vecMoveList;
};

CharacterData *loadCharacter(std::string charName, int charVersion);
