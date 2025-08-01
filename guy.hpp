#pragma once

#include "json.hpp"
#include <deque>
#include <string>
#include <set>

#include "main.hpp"
#include "render.hpp"
#include "input.hpp"
#include "fixed.hpp"
#include "simulation.hpp"

const Fixed wallDistance = Fixed(765.0f);
const Fixed maxPlayerDistance = Fixed(245.0f);

extern nlohmann::json staticPlayer;
extern bool staticPlayerLoaded;

void ResolveHits(std::vector<PendingHit> &pendingHitList);

class Guy {
public:
    void setOpponent(Guy *pGuy) { pOpponent = pGuy; }
    void setSim(Simulation *sim) { pSim = sim; uniqueID = pSim->guyIDCounter++; }

    void Input(int input);
    bool RunFrame(void);
    void Render(void);
    bool Push(Guy *pOtherGuy);
    void RunFramePostPush(void);
    bool WorldPhysics(void);
    void CheckHit(Guy *pOtherGuy, std::vector<PendingHit> &pendingHitList);
    bool AdvanceFrame(bool endHitStopFrame = false);
    std::string getActionName(int actionID);

    void addHitStop(int w) {
        pendingHitStop += w;
    }

    std::string *getName() { return &name; }
    std::string getCharacter() { return character + std::to_string(version); }
    int getVersion() { return version; }
    int getUniqueID() { return uniqueID; }
    color getColor() { color ret; ret.r = charColorR; ret.g = charColorG; ret.b = charColorB; return ret; }
    std::deque<std::string> &getLogQueue() { return logQueue; }
    // for opponent direction
    Guy *getOpponent() { return pOpponent; }
    Guy *getParent() { return pParent; }
    std::vector<Guy*> &getMinions() { return minions; }
    Fixed getPosX() {
        return posX + (posOffsetX*direction);
    }
    Fixed getPosY() {
        return posY + posOffsetY;
    }
    int getDirection() { return direction.i(); }
    int getCurrentInput() { return currentInput; }
    void switchDirection() {
        direction = direction * Fixed(-1);

        // swap l/r in current input
        currentInput = invertDirection(currentInput);

        posOffsetX = posOffsetX * Fixed(-1);
    }

    Fixed getStartPosX() { return startPosX; }
    void setStartPosX( Fixed newPosX ) { startPosX = newPosX; }

    void resetPos() {
        posX = startPosX;
        posY = startPosY;
        lastPosX = posX;
        lastPosY = posY;

        airborne = false;
        landed = false;
        touchedWall = false;
        startsFalling = false;

        posOffsetX = 0.0f;
        posOffsetY = 0.0f;
        velocityX = 0.0f;
        velocityY = 0.0f;
        accelX = 0.0f;
        accelY = 0.0f;
    }
    void setPos( Fixed x, Fixed y) {
        posX = x;
        posY = y;
    }

    bool logTransitions = false;
    bool logTriggers = false;
    bool logUnknowns = true;
    bool logHits = false;
    bool logBranches = false;

    static const int uniqueParamCount = 5;

    int getComboHits() { return comboHits; }
    int getJuggleCounter() { return juggleCounter; }
    int getHitStopForDump() { if (pendingHitStop) return pendingHitStop - 1; return std::max(0, hitStop - 1); }
    int getHitStop() { 
        if (ignoreHitStop) {
            return 0;
        }
        return hitStop;
    }
    bool getWarudo() { return warudo; }
    int getHitStun() { return hitStun; }
    int getComboDamage() { return comboDamage; }
    int getLastDamageScale() { return lastDamageScale; }
    std::string getUniqueParam() {
        std::string ret;
        for ( int i = 0; i < uniqueParamCount; i++) {
            ret += std::to_string(uniqueParam[i]);
            if (i < uniqueParamCount - 1) {
                ret += ",";
            }
        }
        return ret;
    }
    int getDebuffTimer() { return debuffTimer; }
    int getStyle() { return styleInstall; }
    int getInstallFrames() { return styleInstallFrames; }
    int getUniqueTimer() { return uniqueTimerCount; }
    int getforcedPoseStatus() { return forcedPoseStatus; }
    int getPoseStatus() {
        if (forcedPoseStatus) {
            return forcedPoseStatus;
        }
        if (airborne) {
            return 3;
        }
        if (crouching) {
            return 2;
        }
        return 1;
    }
    bool getAirborne() { return getPoseStatus() == 3; }
    bool getCrouching() { return getPoseStatus() == 2; }
    int getActionStatus() { return actionStatus; }
    int getJumpStatus() { return jumpStatus; }
    int getLandingAdjust() { return landingAdjust; }

    int getHealth() { return health; }
    int getMaxHealth() { return maxHealth; }
    void setHealth(int newHealth) { health = newHealth; }

    bool getProjectile() { return isProjectile; }
    int getProjHitCount() { return projHitCount; }
    int getLimitShotCategory() { return limitShotCategory; }

    std::vector<Box> *getPushBoxes() { return &pushBoxes; }
    std::vector<HitBox> *getHitBoxes() { return &hitBoxes; }
    std::vector<HurtBox> *getHurtBoxes() { return &hurtBoxes; }
    std::vector<Box> *getThrowBoxes() { return &throwBoxes; }

    int getCurrentAction() { return currentAction; }
    int getCurrentFrame() { return currentFrame; }
    void setAction(int actionID, int actionFrame) {
        currentAction = actionID;
        currentFrame = actionFrame;
        UpdateActionData();
        UpdateBoxes();
    }
    std::string getActionName() { return actionName; }
    bool getIsDrive() { return isDrive; }
    bool getCrouchingDebug() { return crouching; }
    bool getAirborneDebug() { return airborne; }
    Fixed getHitVelX() { return hitVelX; }
    Fixed getHitAccelX() { return hitAccelX; }
    void getPosDebug( Fixed &outPosX, Fixed &outPosY, Fixed &outPosOffsetX, Fixed &outPosOffsetY) {
        outPosX = posX;
        outPosY = posY;
        outPosOffsetX = posOffsetX;
        outPosOffsetY = posOffsetY;
    }
    void getVel( Fixed &outVelX, Fixed &outVelY, Fixed &outAccelX, Fixed &outAccelY) {
        outVelX = velocityX * direction;
        outVelY = velocityY;
        outAccelX = accelX * direction;
        outAccelY = accelY;
    }

    bool enableCleanup = true;
    bool facSimile = false;

    ~Guy() {
        if (facSimile) {
            // movelist is just pointers to the original in this case, we don't have
            // any manually allocated memory, just stl container members
            return;
        }
        for (auto moveString : vecMoveList) {
            free((void*)moveString);
        }
        vecMoveList.clear();

        if (!enableCleanup) {
            return;
        }

        for (auto minion : minions) {
            delete minion;
        }
        minions.clear();

        std::vector<Guy*> &guyList = guys;
        if (pSim) {
            guyList = pSim->simGuys;
        }


        if (pParent) {
            // std erase when
            const auto it = std::remove(pParent->minions.begin(), pParent->minions.end(), this);
            pParent->minions.erase(it, pParent->minions.end());
        } else {
            const auto it = std::remove(guyList.begin(), guyList.end(), this);
            guyList.erase(it, guyList.end());
        }

        std::vector<Guy *> everyone;
        for (auto guy : guyList) {
            everyone.push_back(guy);
            for ( auto minion : guy->getMinions() ) {
                everyone.push_back(minion);
            }
        }
        for (auto guy : everyone) {
            if (guy->pOpponent == this) {
                guy->pOpponent = nullptr;
            }
            if (guy->pAttacker == this) {
                guy->pAttacker = nullptr;
            }
        }
    }

    Guy(void) {}

    Guy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color)
    {
        const std::string charPath = "data/chars/" + charName + "/";
        const std::string commonPath = "data/chars/common/";
        character = charName;
        version = charVersion;
        name = character;
        posX = startPosX = lastPosX = x;
        posY = startPosY = lastPosY = y;
        direction = startDir;
        charColorR = color.r;
        charColorG = color.g;
        charColorB = color.b;

        if (!staticPlayerLoaded) {
            staticPlayer = parse_json_file("data/chars/static_player.json");
            staticPlayerLoaded = true;
        }

        pMovesDictJson = loadCharFile(charPath, character, version, "moves");
        pRectsJson = loadCharFile(charPath, character, version, "rects");
        pNamesJson = loadCharFile(charPath, character, version, "names");
        pTriggerGroupsJson = loadCharFile(charPath, character, version, "trigger_groups");
        pTriggersJson = loadCharFile(charPath, character, version, "triggers");
        pCommandsJson = loadCharFile(charPath, character, version, "commands");
        pChargeJson = loadCharFile(charPath, character, version, "charge");
        pHitJson = loadCharFile(charPath, character, version, "hit");
        pAtemiJson = loadCharFile(charPath, character, version, "atemi");
        pCharInfoJson = loadCharFile(charPath, character, version, "charinfo");

        pCommonMovesJson = loadCharFile(commonPath, "common", version, "moves");
        pCommonRectsJson = loadCharFile(commonPath, "common", version, "rects");
        pCommonAtemiJson = loadCharFile(commonPath, "common", version, "atemi");

        Input(0);

        BuildMoveList();
    
        UpdateActionData();

        DoTriggers();

        health = maxHealth = (*pCharInfoJson)["PlData"]["Vitality"];
    }

    Guy(Guy &parent, Fixed posOffsetX, Fixed posOffsetY, int startAction, int styleID, bool isProj)
    {
        character = parent.character;
        version = parent.version;
        name = character + "'s minion";
        direction = parent.direction;
        posX = parent.posX + parent.posOffsetX * direction + posOffsetX;
        posY = parent.posY + parent.posOffsetY + posOffsetY;
        lastPosX = posX;
        lastPosY = posY;
        charColorR = parent.charColorR;
        charColorG = parent.charColorG;
        charColorB = parent.charColorB;

        pMovesDictJson = parent.pMovesDictJson;
        pRectsJson = parent.pRectsJson;
        pNamesJson = parent.pNamesJson;
        pTriggerGroupsJson = parent.pTriggerGroupsJson;
        pTriggersJson = parent.pTriggersJson;
        pCommandsJson = parent.pCommandsJson;
        pChargeJson = parent.pChargeJson;
        pHitJson = parent.pHitJson;
        pAtemiJson = parent.pAtemiJson;
        pCharInfoJson = parent.pCharInfoJson;

        pCommonMovesJson = parent.pCommonMovesJson;
        pCommonRectsJson = parent.pCommonRectsJson;
        pCommonAtemiJson = parent.pCommonAtemiJson;

        pOpponent = parent.pOpponent;
        if (parent.pSim) {
            setSim(parent.pSim);
        }

        currentAction = startAction;
        styleInstall = styleID;

        scalingTriggerID = parent.scalingTriggerID;

        isProjectile = isProj;
        projHitCount = -1; // unset yet, is in the first action
        if (parent.pParent) {
            pParent = parent.pParent;
        } else {
            pParent = &parent;
        }

        BuildMoveList();

        UpdateActionData();
    }

    std::vector<const char *> &getMoveList() { return vecMoveList; }
    std::set<std::pair<int,int>> &getFrameTriggers() { return frameTriggers; }
    int getFrameMeterColorIndex();
    bool canAct() {
        bool a,b,c;
        return canMove(a,b,c);
    }
    std::map<std::pair<int, int>, std::pair<std::string, bool>> &getMapMoveStyle() { return mapMoveStyle; }
    std::pair<int, int> & getForcedTrigger() { return forcedTrigger; }
    int *getNeutralMovePtr() { return &neutralMove; }
    int *getInputOverridePtr() { return &inputOverride; }
    int *getInputIDPtr() { return &inputID; }
    int *getInputListIDPtr() { return &inputListID; }
    const char* FindMove(int actionID, int styleID, nlohmann::json **ppMoveJson = nullptr);
    nlohmann::json *findAtemi(int atemiID);
private:
    void NextAction(bool didTrigger, bool didBranch, bool bElide = false);
    void UpdateActionData(void);
    void UpdateBoxes(void);

    void ApplyHitEffect(nlohmann::json *pHitEffect, Guy *attacker, bool applyHit, bool applyHitStun, bool isDrive, bool isDomain, HurtBox *pHurtBox = nullptr);

    void ExecuteTrigger(nlohmann::json *pTrigger);
    bool CheckTriggerGroupConditions(int conditionFlag, int stateFlag);
    bool CheckTriggerConditions(nlohmann::json *pTrigger, int triggerID);
    bool CheckTriggerCommand(nlohmann::json *pTrigger, int &initialI);
    void DoTriggers(int fluffFrameBias = 0);

    void DoBranchKey(bool preHit = false);
    bool CheckHitBoxCondition(int conditionFlag);
    void DoHitBoxKey(const char *name);
    void DoStatusKey();

    void DoSwitchKey(const char *name);
    void DoEventKey(nlohmann::json *pAction, int frameID);
    void DoShotKey(nlohmann::json *pAction, int frameID);

    void DoInstantAction(int actionID);

    void ChangeStyle(int newStyleID);
    void ExitStyle();

    bool fluffFrames(int frameBias = 0) {
        if ((marginFrame != -1 && (currentFrame + frameBias) >= marginFrame) && nextAction == -1 ) {
            return true;
        }
        return false;
    }

    bool canMove(bool &outCrouching, bool &forward, bool &backward, int frameBias = 0) {
        bool ret = false;
        int actionCheckCanMove = currentAction;
        if (nextAction != -1 ) {
            actionCheckCanMove = nextAction;
        }
        // todo is action 6 ok here? try a dump fo akuma zanku and trying to move immediately on landing
        bool isStanding = actionCheckCanMove == 1 || actionCheckCanMove == 2;
        bool isTurningAround = actionCheckCanMove == 7 || actionCheckCanMove == 8;
        bool isCrouching = actionCheckCanMove == 4 || actionCheckCanMove == 5;
        forward = actionCheckCanMove == 9 || actionCheckCanMove == 10;
        backward = actionCheckCanMove == 13 || actionCheckCanMove == 14;
        if (isStanding || isCrouching || isTurningAround || actionCheckCanMove == 6 ||
            forward || actionCheckCanMove == 11 ||
            backward || actionCheckCanMove == 15) {
            ret = true;
        }

        if (fluffFrames(frameBias)) {
            ret = true;
        }

        if (airborne || posY > 0.0f) {
            ret = false;
        }
        if (hitStun) {
            ret = false;
        }

        if (ret == true) {
            outCrouching = isCrouching;
        }

        return ret;
    }

    bool onLeftWall() { return getPosX() == -wallDistance; }
    bool onRightWall() { return getPosX() == wallDistance; }

    bool conditionOperator(int op, int operand, int threshold, std::string desc);

    bool needsTurnaround(Fixed threshold = Fixed(0), Fixed directionOverride = Fixed(0)) {
        bool turnaround = false;
        Fixed checkDirection = direction;
        if (directionOverride != Fixed(0)) {
            checkDirection = directionOverride;
        }
        if (pOpponent) {
            if ( checkDirection > 0 && getPosX() > pOpponent->getPosX() ) {
                turnaround = true;
            } else if ( checkDirection < 0 && getPosX() < pOpponent->getPosX() ) {
                turnaround = true;
            }
            if (threshold != Fixed(0) && fixAbs(getPosX() - pOpponent->getPosX()) <= threshold) {
                turnaround = false;
            }
        }
        return turnaround;
    }

    std::string name;
    std::string character;
    int version;
    int uniqueID = -1;
    Guy *pOpponent = nullptr;

    void log(bool log, std::string logLine)
    {
        if (!log) return;
        std::string frameDiff = to_string_leading_zeroes(globalFrameCount - lastLogFrame, 3);
        std::string curFrame = to_string_leading_zeroes(currentFrame, 3);
        logQueue.push_back(std::to_string(currentAction) + ":" + curFrame + "(+" + frameDiff + ") " + logLine);
        if (logQueue.size() > 15) {
            logQueue.pop_front();
        }
        lastLogFrame = globalFrameCount;
    }
    void log(std::string logLine) { log(true, logLine ); }
    int lastLogFrame = 0;
    std::deque<std::string> logQueue;

    bool GetRect(Box &outBox, int rectsPage, int boxID,  Fixed offsetX, Fixed offsetY, int dir);
    void BuildMoveList();
    std::vector<const char *> vecMoveList;
    int neutralMove = 0;

    int inputOverride = 0;
    int inputID = 0;
    int inputListID = 0;

    bool deniedLastBranch = false;

    std::vector<Guy*> minions;
    bool isProjectile = false;
    int projHitCount = 0;
    int projLifeTime = 0;
    bool projDataInitialized = false;
    bool die = false;
    int limitShotCategory = -1;
    bool noPush = false;
    bool obeyHitID = false;
    Guy *pParent = nullptr;

    nlohmann::json *pMovesDictJson;
    nlohmann::json *pRectsJson;
    nlohmann::json *pNamesJson;
    nlohmann::json *pTriggerGroupsJson;
    nlohmann::json *pTriggersJson;
    nlohmann::json *pCommandsJson;
    nlohmann::json *pChargeJson;
    nlohmann::json *pHitJson;
    nlohmann::json *pAtemiJson;
    nlohmann::json *pCharInfoJson;

    nlohmann::json *pCommonMovesJson;
    nlohmann::json *pCommonRectsJson;
    nlohmann::json *pCommonAtemiJson;

    std::map<std::pair<int, int>, std::pair<std::string, bool>> mapMoveStyle;

    Fixed posX;
    Fixed posY;
    Fixed direction = Fixed(1);

    Fixed lastPosX;
    Fixed lastPosY;

    Fixed startPosX;
    Fixed startPosY;

    bool airborne = false;
    bool landed = false;
    bool forceLanding = false;
    bool touchedWall = false;
    bool touchedOpponent = false;
    bool startsFalling = false;
    bool crouching = false;
    bool blocking = false;
    bool bounced = false;
    bool didPush = false;
    bool jumped = false;
    int jumpDirection = 0;
    int jumpLandingDisabledFrames = 0;
    bool couldMove = false;

    int landingAdjust = 0;
    int prevPoseStatus = 0;
    int forcedPoseStatus = 0;
    int actionStatus = 0;
    int jumpStatus = 0;

    bool counterState = false;
    bool punishCounterState = false;
    bool forceKnockDownState = false;

    bool throwTechable = false;

    bool offsetDoesNotPush = false;
    bool ignoreBodyPush = false;
    bool noVelNextFrame = false;
    bool noAccelNextFrame = false;
    Fixed posOffsetX;
    Fixed posOffsetY;
    Fixed velocityX;
    Fixed velocityY;
    Fixed accelX;
    Fixed accelY;

    Fixed cancelVelocityX;
    Fixed cancelVelocityY;
    Fixed cancelAccelX;
    Fixed cancelAccelY;

    Fixed cancelInheritVelX;
    Fixed cancelInheritVelY;
    Fixed cancelInheritAccelX;
    Fixed cancelInheritAccelY;

    Fixed homeTargetX;
    Fixed homeTargetY;
    int homeTargetType;

    int ignoreSteerType = -1;

    int maxHealth;
    int health;

    int currentAction = 1;
    int nextAction = -1;
    int nextActionFrame = -1;
    bool keepPlace = false;
    bool noPlaceXNextFrame = false;
    bool noPlaceYNextFrame = false;
    int currentFrame = 0;
    int actionFrameDuration = 0;
    int mainFrame = 0;
    int followFrame = 0;
    int marginFrame = 0;
    int loopCount = 0;
    int loopPoint = 0;
    int startScale = 0;
    int comboScale = 0;
    int instantScale = 0;
    bool nextActionOpponentAction = false;
    bool opponentAction = false;
    bool locked = false;
    bool hasLooped = false;

    bool superAction = false;

    // todo round begin action needs to set that
    int styleInstall = 0;
    int styleInstallFrames = 0;
    bool countingDownInstall = false;

    int airActionCounter = 0;

    int currentInput = 0;
    std::deque<int> inputBuffer;
    std::deque<int> directionBuffer;

    // hitting side
    uint64_t canHitID = 0;
    bool hitThisFrame = false;
    bool hasBeenBlockedThisFrame = false;
    bool hitArmorThisFrame = false;
    bool hitAtemiThisFrame = false;
    bool punishCounterThisFrame = false;
    bool grabbedThisFrame = false;
    bool hitThisMove = false;
    bool hitCounterThisMove = false;
    bool hitPunishCounterThisMove = false;
    bool hasBeenBlockedThisMove = false;
    bool hitArmorThisMove = false;
    bool hitAtemiThisMove = false;

    int hitStop = 0;
    int pendingHitStop = 0;
    int timeInHitStop = 0;
    bool ignoreHitStop = false;
    bool tokiYoTomare = false;
    bool warudo = false;
    bool tokiWaUgokidasu = false;

    int scalingTriggerID = 0;

    // getting hit side
    Guy *pAttacker = nullptr;
    int hitStun = 0;
    bool resetHitStunOnLand = false;
    bool resetHitStunOnTransition = false;
    Fixed hitVelX;
    Fixed hitAccelX;
    Fixed pushBackThisFrame;
    Fixed hitReflectVelX;
    Fixed hitReflectAccelX;
    Fixed reflectThisFrame;
    bool noCounterPush = false;
    bool beenHitThisFrame = false;
    // 2.0 was here
    int currentScaling = 0;
    int pendingScaling = 0;
    bool driveScaling = false;
    int comboHits = 0;
    int juggleCounter = 0;
    int comboDamage = 0;
    int lastDamageScale = 0;
    int lastScalingTriggerID = 0;
    bool wasHit = false;
    bool resetComboCount = false;

    int pendingLockHit = -1;

    bool forceKnockDown = false;
    bool isDown = false;
    int knockDownFrames = 0;
    bool nageKnockdown = false;

    bool recoverForward = false;
    bool recoverReverse = false;

    bool groundBounce = false;
    Fixed groundBounceVelX;
    Fixed groundBounceVelY;
    Fixed groundBounceAccelX;
    Fixed groundBounceAccelY;

    bool wallBounce = false;
    bool wallSplat = false;
    Fixed wallBounceVelX;
    Fixed wallBounceVelY;
    Fixed wallBounceAccelX;
    Fixed wallBounceAccelY;
    bool wallStopped = false;
    int wallStopFrames = 0;

    int currentArmorID = -1;
    int armorHitsLeft = 0;
    bool armorThisFrame = false;
    bool atemiThisFrame = false;

    int recoveryTiming = 0;

    std::vector<Box> pushBoxes;
    std::vector<HitBox> hitBoxes;
    std::vector<HurtBox> hurtBoxes;
    std::vector<Box> throwBoxes;

    std::vector<RenderBox> renderBoxes;

    std::set<int> setDeferredTriggerIDs;

    std::set<std::pair<int,int>> frameTriggers;
    std::pair<int, int> forcedTrigger;

    bool isDrive = false;
    bool wasDrive = false;

    float charColorR = 1.0;
    float charColorG = 1.0;
    float charColorB = 1.0;

    int uniqueParam[uniqueParamCount] = { 0 };
    bool uniqueTimer = false;
    int uniqueTimerCount = 0;

    int debuffTimer = 0;

    std::string actionName;
    nlohmann::json *pActionJson;

    Simulation *pSim = nullptr;
    friend void ResolveHits(std::vector<PendingHit> &pendingHitList);
};

static inline void gatherEveryone(std::vector<Guy*> &vecGuys, std::vector<Guy*> &vecOutEveryone)
{
    vecOutEveryone.clear();
    for (auto guy : vecGuys) {
        vecOutEveryone.push_back(guy);
        for ( auto minion : guy->getMinions() ) {
            vecOutEveryone.push_back(minion);
        }
    }
}

