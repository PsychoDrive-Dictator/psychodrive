#pragma once

#include "json.hpp"
#include <cstring>
#include <deque>
#include <string>
#include <set>

#include "main.hpp"
#include "render.hpp"
#include "input.hpp"
#include "fixed.hpp"
#include "simulation.hpp"
#include "chara.hpp"

const Fixed wallDistance = Fixed(765.0f);
const Fixed projWallDistance = Fixed(800.0f);
const Fixed maxPlayerDistance = Fixed(245.0f);
const Fixed maxProjectileDistance = Fixed(280.0f);

const int maxFocus = 60000;

void ResolveHits(std::vector<PendingHit> &pendingHitList);

class Guy;

struct GuyRef {
    int guyID = -1;
    Guy *pGuy = nullptr;
    GuyRef(Guy* pGuy);
    operator Guy*() const { return pGuy; }
    Guy* operator->() const { return pGuy; }
    GuyRef& operator=(Guy* rhs);

    GuyRef() = default;
    GuyRef(const GuyRef&) = default;
    GuyRef& operator=(const GuyRef&) = default;

    bool operator==(Guy* rhs) { return this->pGuy == rhs; }
    void FixRef(std::map<int,Guy*> &guysByID) {
        if (guyID != -1) {
            assert(guysByID.find(guyID) != guysByID.end());
            pGuy = guysByID[guyID];
        } else {
            pGuy = nullptr;
        }
    }
    // GuyRef operator=(std::nullptr_t rhs) {
    //     pGuy = nullptr;
    //     guyID = -1;
    // }
};

static const int uniqueParamCount = 5;

class Guy {
public:
    void setOpponent(Guy *pGuy) { pOpponent = pGuy; }
    void setAttacker(Guy *pGuy) { pAttacker = pGuy; }
    void setSim(Simulation *sim) { pSim = sim; }
    Guy *getAttacker() { return pAttacker; }
    Guy *getOpponent() { return pOpponent; }
    Guy *getParent() { return pParent; }
    void FixRefs(std::map<int,Guy*> &guysByID);

    void Input(int input);
    bool RunFrame(bool advancingTime = true);
    void Render(float z = 0.0);
    bool Push(Guy *pOtherGuy);
    void RunFramePostPush(void);
    bool WorldPhysics(bool onlyFloor = false);
    void CheckHit(Guy *pOtherGuy, std::vector<PendingHit> &pendingHitList);
    bool AdvanceFrame(bool advancingTime = true, bool endHitStopFrame = false, bool endWarudoFrame = false);
    std::string getActionName(int actionID);

    void addHitStop(int w) {
        if (w > pendingHitStop) {
            pendingHitStop = w;
        }
    }

    std::string getName() { return getCharNameFromID(pCharData->charID); }
    std::string getCharacter() { return getCharNameFromID(pCharData->charID); }
    CharacterData *getCharData() { return pCharData; }
    int getVersion() { return pCharData->charVersion; }
    int getUniqueID() { return uniqueID; }
    color getColor() { color ret; ret.r = charColorR; ret.g = charColorG; ret.b = charColorB; return ret; }
    std::deque<std::string> &getLogQueue() { return nc.logQueue; }
    // for opponent direction
    std::vector<GuyRef> &getMinions() { return dc.minions; }
    Fixed getPosX() {
        return posX + (posOffsetX*direction);
    }
    Fixed getPosY() {
        return posY + posOffsetY;
    }
    Fixed getLastPosX() { return lastPosX; }
    Fixed getLastPosY() { return lastPosY; }
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

    void guyLog(bool doLog, std::string logLine);
    void guyLog(std::string logLine) { guyLog(true, logLine ); }

    bool getLogTransitions() { return logTransitions; }
    void setLogTransitions(bool set) { logTransitions = set; }
    bool getLogTriggers() { return logTriggers; }
    void setLogTriggers(bool set) { logTriggers = set; }
    bool getLogUnknowns() { return logUnknowns; }
    void setLogUnknowns(bool set) { logUnknowns = set; }
    bool getLogHits() { return logHits; }
    void setLogHits(bool set) { logHits = set; }
    bool getLogBranches() { return logBranches; }
    void setLogBranches(bool set) { logBranches = set; }
    bool getLogResources() { return logResources; }
    void setLogResources(bool set) { logResources = set; }


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
    bool getIsDown() { return isDown; }
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
    void setHealth(int newHealth) { health = newHealth; }
    int getFocus() { return focus; }
    bool getHasFocusRegenCooldowned() { return hasFocusRegenCooldowned; }
    int getFocusRegenCoolDown() { return focusRegenCooldown; }
    void setFocusRegenCooldown(int newCooldown, bool setFlag = true) {
        focusRegenCooldown = newCooldown;
        if (setFlag) {
            hasFocusRegenCooldowned = true;
        }
    }
    int getGauge() { return gauge; }
    void setFocus(int newFocus) {
        focus = newFocus;
        if (focus < 0) {
            focus = 0;
        }
        if (focus > maxFocus) {
            focus = maxFocus;
        }
    }
    void setGauge(int newGauge) {
        gauge = newGauge;
        if (gauge < 0) {
            gauge = 0;
        }
        if (gauge > pCharData->gauge) {
            gauge = pCharData->gauge;
        }
    }

    bool getProjectile() { return isProjectile; }
    int getProjHitCount() { return projHitCount; }
    int getLimitShotCategory() { return limitShotCategory; }

    void getPushBoxes(std::vector<Box> *pOutPushBoxes, std::vector<RenderBox> *pOutRenderBoxes = nullptr);
    void getHitBoxes(std::vector<HitBox> *pOutHitBoxes, std::vector<RenderBox> *pOutRenderBoxes = nullptr);
    void getHurtBoxes(std::vector<HurtBox> *pOutHurtBoxes, std::vector<Box> *pOutThrowBoxes = nullptr, std::vector<RenderBox> *pOutRenderBoxes = nullptr);

    int getCurrentAction() { return currentAction; }
    Action *getCurrentActionPtr() { return pCurrentAction; }
    int getCurrentFrame() { return currentFrame; }
    void setAction(int actionID, int actionFrame) {
        currentAction = actionID;
        currentFrame = actionFrame;
        currentFrameFrac = Fixed(actionFrame);
        actionSpeed = Fixed(1);
        actionInitialFrame = -1;
        UpdateActionData();
    }
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

    ~Guy() {
        if (!enableCleanup) {
            return;
        }

        for (auto minion : dc.minions) {
            delete minion;
        }
        dc.minions.clear();

        std::vector<Guy*> *pGuyList = &guys;
        if (pSim) {
            pGuyList = &pSim->simGuys;
        }


        if (pParent) {
            // std erase when
            const auto it = std::remove(pParent->dc.minions.begin(), pParent->dc.minions.end(), this);
            pParent->dc.minions.erase(it, pParent->dc.minions.end());
        } else {
            const auto it = std::remove(pGuyList->begin(), pGuyList->end(), this);
            pGuyList->erase(it, pGuyList->end());
        }

        std::vector<Guy *> everyone;
        for (auto guy : *pGuyList) {
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

    Guy& operator=(const Guy& other) {
        if (this != &other) {
            std::memcpy(&uniqueID, &other.uniqueID,
                       reinterpret_cast<char*>(&dc) - reinterpret_cast<char*>(&uniqueID));
            dc = other.dc;
        }
        return *this;
    }

    Guy(Simulation *sim, std::string charName, int version, Fixed x, Fixed y, int startDir, color color)
    {
        Initialize();
        pSim = sim;
        uniqueID = pSim->guyIDCounter++;

        posX = startPosX = lastPosX = x;
        posY = startPosY = lastPosY = y;
        direction = startDir;
        charColorR = color.r;
        charColorG = color.g;
        charColorB = color.b;

        pCharData = loadCharacter(charName, version);

        Input(0);
    
        UpdateActionData();

        DoTriggers();

        health = pCharData->vitality;
        focus = maxFocus;
        gauge = pCharData->gauge;
    }

    Guy(Guy &parent, Fixed posOffsetX, Fixed posOffsetY, int startAction, int styleID, bool isProj)
    {
        Initialize();
        pSim = parent.pSim;
        uniqueID = pSim->guyIDCounter++;

        direction = parent.direction;
        posX = parent.posX + parent.posOffsetX * direction + posOffsetX;
        posY = parent.posY + parent.posOffsetY + posOffsetY;
        lastPosX = posX;
        lastPosY = posY;
        charColorR = parent.charColorR;
        charColorG = parent.charColorG;
        charColorB = parent.charColorB;

        pCharData = parent.pCharData;

        pOpponent = parent.pOpponent;

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

        UpdateActionData();
    }

    std::vector<const char *> &getMoveList() { return pCharData->vecMoveList; }
    std::set<ActionRef> &getFrameTriggers() { return dc.frameTriggers; }
    void setRecordFrameTriggers(bool record, bool lateCancels) { recordFrameTriggers = record; recordLateCancels = lateCancels; }
    int getFrameMeterColorIndex();
    bool canAct() {
        bool a,b,c;
        return canMove(a,b,c);
    }
    bool couldAct() { return couldMove; }
    ActionRef& getForcedTrigger() { return forcedTrigger; }
    int *getNeutralMovePtr() { return &neutralMove; }
    int *getInputOverridePtr() { return &inputOverride; }
    int *getInputIDPtr() { return &inputID; }
    int *getInputListIDPtr() { return &inputListID; }
    Action* FindMove(int actionID, int styleID);
    AtemiData *findAtemi(int atemiID);
private:
    void NextAction(bool didTrigger, bool didBranch, bool bElide = false);
    void UpdateActionData(void);
    Box rectToBox(Rect *pRect, Fixed offsetX, Fixed offsetY, int dir);

    void ApplyHitEffect(HitEntry *pHitEffect, Guy *attacker, bool applyHit, bool applyHitStun, bool isDrive, bool isDomain, bool isTrade = false, HurtBox *pHurtBox = nullptr);

    void ExecuteTrigger(Trigger *pTrigger);
    bool CheckTriggerGroupConditions(int conditionFlag, int stateFlag);
    bool CheckTriggerConditions(Trigger *pTrigger, int fluffFramesBias = 0);
    bool CheckTriggerCommand(Trigger *pTrigger, int &initialI);
    void DoTriggers(int fluffFrameBias = 0);

    void DoBranchKey(bool preHit = false);
    bool CheckHitBoxCondition(int conditionFlag);
    void DoStatusKey();
    void DoSteerKey();
    void DoSteerKeyOperation(Fixed &value, Fixed keyValue, int operationType);
    void DoPlaceKey();
    void DoSwitchKey();
    void DoEventKey(Action *pAction, int frameID);
    void DoWorldKey();
    void DoLockKey();
    void DoShotKey(Action *pAction, int frameID);

    void DoInstantAction(int actionID);

    void ChangeStyle(int newStyleID);
    void ExitStyle();

    bool fluffFrames(int frameBias = 0) {
        if (hitStun) {
            return false;
        }
        if (pCurrentAction && pCurrentAction->recoveryEndFrame != -1 && (currentFrame + frameBias) >= pCurrentAction->recoveryEndFrame && nextAction == -1) {
            return true;
        }
        if (actionInitialFrame != -1 && !hitStun) {
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

        if (pSim->match && !pSim->timerStarted) {
            ret = false;
        }

        if (airborne || posY > 0.0f) {
            ret = false;
        }
        if (hitStun) {
            ret = false;
        }

        if (blocking && !hitStun) {
            // prox guard
            ret = false;
        }

        if (ret == true) {
            outCrouching = isCrouching;
        }

        return ret;
    }

    bool onLeftWall() { return getPosX() == -wallDistance; }
    bool onRightWall() { return getPosX() == wallDistance; }
    bool onWall() { return onLeftWall() || onRightWall(); }

    bool wasOnLeftWall() { return getLastPosX() == -wallDistance; }
    bool wasOnRightWall() { return getLastPosX() == wallDistance; }
    bool wasOnWall() { return wasOnLeftWall() || wasOnRightWall(); }

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
    bool GetRect(Box &outBox, int rectsPage, int boxID,  Fixed offsetX, Fixed offsetY, int dir);

    void Initialize() {
        uniqueID = -1;
        pOpponent = nullptr;
        neutralMove = 0;
        inputOverride = 0;
        inputID = 0;
        inputListID = 0;
        logTransitions = false;
        logTriggers = false;
        logUnknowns = true;
        logHits = false;
        logBranches = false;
        logResources = false;
        deniedLastBranch = false;
        isProjectile = false;
        projHitCount = 0;
        projLifeTime = 0;
        projDataInitialized = false;
        die = false;
        limitShotCategory = -1;
        noPush = false;
        obeyHitID = false;
        pParent = nullptr;
        pCharData = nullptr;
        posX = Fixed(0);
        posY = Fixed(0);
        direction = Fixed(1);
        lastPosX = Fixed(0);
        lastPosY = Fixed(0);
        startPosX = Fixed(0);
        startPosY = Fixed(0);
        airborne = false;
        landed = false;
        forceLanding = false;
        touchedWall = false;
        touchedOpponent = false;
        startsFalling = false;
        crouching = false;
        freeMovement = false;
        blocking = false;
        hurtBoxProxGuarded = false;
        positionProxGuarded = false;
        bounced = false;
        didPush = false;
        jumped = false;
        jumpDirection = 0;
        jumpLandingDisabledFrames = 0;
        couldMove = true;
        didTrigger = false;
        landingAdjust = 0;
        forcedPoseStatus = 0;
        actionStatus = 0;
        jumpStatus = 0;
        counterState = false;
        punishCounterState = false;
        forceKnockDownState = false;
        throwTechable = false;
        canBlock = false;
        offsetDoesNotPush = false;
        ignoreBodyPush = false;
        ignoreScreenPush = false;
        noVelNextFrame = false;
        noAccelNextFrame = false;
        posOffsetX = Fixed(0);
        posOffsetY = Fixed(0);
        setPlaceX = false;
        setPlaceY = false;
        velocityX = Fixed(0);
        velocityY = Fixed(0);
        accelX = Fixed(0);
        accelY = Fixed(0);
        cancelVelocityX = Fixed(0);
        cancelVelocityY = Fixed(0);
        cancelAccelX = Fixed(0);
        cancelAccelY = Fixed(0);
        cancelInheritVelX = Fixed(0);
        cancelInheritVelY = Fixed(0);
        cancelInheritAccelX = Fixed(0);
        cancelInheritAccelY = Fixed(0);
        homeTargetX = Fixed(0);
        homeTargetY = Fixed(0);
        homeTargetType = 0;
        ignoreSteerType = -1;
        health = 0;
        focus = 0;
        focusRegenCooldown = 0;
        deferredFocusCost = 0;
        gauge = 0;
        currentAction = 1;
        prevAction = -1;
        nextAction = -1;
        nextActionFrame = -1;
        actionInitialFrame = -1;
        keepPlace = false;
        noPlaceXNextFrame = false;
        noPlaceYNextFrame = false;
        actionSpeed = Fixed(1);
        currentFrameFrac = Fixed(0);
        currentFrame = 0;
        loopCount = 0;
        instantScale = 0;
        nextActionOpponentAction = false;
        opponentAction = false;
        locked = false;
        hasLooped = false;
        superAction = false;
        superLevel = 0;
        styleInstall = 0;
        styleInstallFrames = 0;
        countingDownInstall = false;
        airActionCounter = 0;
        currentInput = 0;
        framesSinceLastInput = 0;
        canHitID = 0;
        hitThisFrame = false;
        hasBeenBlockedThisFrame = false;
        hitArmorThisFrame = false;
        hitAtemiThisFrame = false;
        punishCounterThisFrame = false;
        grabbedThisFrame = false;
        hitThisMove = false;
        hitCounterThisMove = false;
        hitPunishCounterThisMove = false;
        hasBeenBlockedThisMove = false;
        hitArmorThisMove = false;
        hitAtemiThisMove = false;
        hitStop = 0;
        pendingHitStop = 0;
        timeInHitStop = 0;
        ignoreHitStop = false;
        tokiYoTomare = false;
        warudo = false;
        tokiWaUgokidasu = false;
        scalingTriggerID = 0;
        pAttacker = nullptr;
        hitStun = 0;
        resetHitStunOnLand = false;
        resetHitStunOnTransition = false;
        hitVelX = Fixed(0);
        hitAccelX = Fixed(0);
        pushBackThisFrame = Fixed(0);
        hitReflectVelX = Fixed(0);
        hitReflectAccelX = Fixed(0);
        reflectThisFrame = Fixed(0);
        deferredReflect = false;
        noCounterPush = false;
        beenHitThisFrame = false;
        currentScaling = 0;
        pendingScaling = 0;
        driveScaling = false;
        comboHits = 0;
        juggleCounter = 0;
        comboDamage = 0;
        lastDamageScale = 0;
        lastScalingTriggerID = 0;
        wasHit = false;
        resetComboCount = false;
        pendingUnlockHit = 0;
        knockDown = false;
        isDown = false;
        knockDownFrames = 0;
        nageKnockdown = false;
        recoverForward = false;
        recoverReverse = false;
        groundBounce = false;
        groundBounceVelX = Fixed(0);
        groundBounceVelY = Fixed(0);
        groundBounceAccelX = Fixed(0);
        groundBounceAccelY = Fixed(0);
        wallBounce = false;
        wallSplat = false;
        tumble = false;
        wallBounceVelX = Fixed(0);
        wallBounceVelY = Fixed(0);
        wallBounceAccelX = Fixed(0);
        wallBounceAccelY = Fixed(0);
        wallStopped = false;
        wallStopFrames = 0;
        currentArmor = nullptr;
        armorHitsLeft = 0;
        armorThisFrame = false;
        atemiThisFrame = false;
        recoveryTiming = 0;
        recordFrameTriggers = false;
        recordLateCancels = false;
        forcedTrigger = ActionRef(0, 0);
        isDrive = false;
        wasDrive = false;
        charColorR = 1.0;
        charColorG = 1.0;
        charColorB = 1.0;
        for (int i = 0; i < uniqueParamCount; i++) {
            uniqueParam[i] = 0;
        }
        uniqueTimer = false;
        hasFocusRegenCooldowned = false;
        focusRegenCooldownFrozen = false;
        superFreeze = false;
        uniqueTimerCount = 0;
        debuffTimer = 0;
        pCurrentAction = nullptr;
        pSim = nullptr;
    }

    int uniqueID;
    GuyRef pOpponent;

    int neutralMove;

    int inputOverride;
    int inputID;
    int inputListID;

    bool logTransitions : 1;
    bool logTriggers : 1;
    bool logUnknowns : 1;
    bool logHits : 1;
    bool logBranches : 1;
    bool logResources : 1;
    bool deniedLastBranch : 1;
    bool isProjectile : 1;
    bool projDataInitialized : 1;
    bool die : 1;
    bool noPush : 1;
    bool obeyHitID : 1;
    bool airborne : 1;
    bool landed : 1;
    bool forceLanding : 1;
    bool touchedWall : 1;
    bool touchedOpponent : 1;
    bool startsFalling : 1;
    bool crouching : 1;
    bool freeMovement : 1;
    bool blocking : 1;
    bool hurtBoxProxGuarded : 1;
    bool positionProxGuarded : 1;
    bool bounced : 1;
    bool didPush : 1;
    bool jumped : 1;
    bool couldMove : 1;
    bool didTrigger : 1;
    bool counterState : 1;
    bool punishCounterState : 1;
    bool forceKnockDownState : 1;
    bool throwTechable : 1;
    bool canBlock : 1;
    bool offsetDoesNotPush : 1;
    bool ignoreBodyPush : 1;
    bool ignoreScreenPush : 1;
    bool noVelNextFrame : 1;
    bool noAccelNextFrame : 1;
    bool setPlaceX : 1;
    bool setPlaceY : 1;
    bool keepPlace : 1;
    bool noPlaceXNextFrame : 1;
    bool noPlaceYNextFrame : 1;
    bool nextActionOpponentAction : 1;
    bool opponentAction : 1;
    bool locked : 1;
    bool hasLooped : 1;
    bool superAction : 1;
    bool hitThisFrame : 1;
    bool hasBeenBlockedThisFrame : 1;
    bool hitArmorThisFrame : 1;
    bool hitAtemiThisFrame : 1;
    bool punishCounterThisFrame : 1;
    bool grabbedThisFrame : 1;
    bool hitThisMove : 1;
    bool hitCounterThisMove : 1;
    bool hitPunishCounterThisMove : 1;
    bool hasBeenBlockedThisMove : 1;
    bool hitArmorThisMove : 1;
    bool hitAtemiThisMove : 1;
    bool countingDownInstall : 1;
    bool ignoreHitStop : 1;
    bool tokiYoTomare : 1;
    bool warudo : 1;
    bool tokiWaUgokidasu : 1;
    bool resetHitStunOnLand : 1;
    bool resetHitStunOnTransition : 1;
    bool deferredReflect : 1;
    bool noCounterPush : 1;
    bool beenHitThisFrame : 1;
    bool driveScaling : 1;
    bool wasHit : 1;
    bool resetComboCount : 1;
    bool knockDown : 1;
    bool isDown : 1;
    bool nageKnockdown : 1;
    bool recoverForward : 1;
    bool recoverReverse : 1;
    bool wallBounce : 1;
    bool wallSplat : 1;
    bool tumble : 1;
    bool groundBounce : 1;
    bool wallStopped : 1;
    bool armorThisFrame : 1;
    bool atemiThisFrame : 1;
    bool recordFrameTriggers : 1;
    bool recordLateCancels : 1;
    bool isDrive : 1;
    bool wasDrive : 1;
    bool uniqueTimer : 1;
    bool hasFocusRegenCooldowned : 1;
    bool focusRegenCooldownFrozen : 1;
    bool superFreeze : 1;

    int projHitCount;
    int projLifeTime;

    int limitShotCategory;
    GuyRef pParent;

    CharacterData *pCharData;

    Fixed posX;
    Fixed posY;
    Fixed direction;

    Fixed lastPosX;
    Fixed lastPosY;

    Fixed startPosX;
    Fixed startPosY;

    int jumpDirection;
    int jumpLandingDisabledFrames;

    int landingAdjust;
    int forcedPoseStatus;
    int actionStatus;
    int jumpStatus;

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

    int ignoreSteerType;

    int health;
    int focus;
    int focusRegenCooldown;
    int deferredFocusCost;
    int gauge;

    int currentAction;
    int prevAction;
    int nextAction;
    int nextActionFrame;
    int actionInitialFrame;
    Fixed actionSpeed;
    Fixed currentFrameFrac;
    int currentFrame;
    int loopCount;
    int instantScale;
    int superLevel;

    // todo round begin action needs to set that
    int styleInstall;
    int styleInstallFrames;

    int airActionCounter;

    int currentInput;
    // todo could keep track of frames since any input as a further optim
    int framesSinceLastInput;

    // hitting side
    uint64_t canHitID;

    int hitStop;
    int pendingHitStop;
    int timeInHitStop;

    int scalingTriggerID;

    // getting hit side
    GuyRef pAttacker;
    int hitStun;

    Fixed hitVelX;
    Fixed hitAccelX;
    Fixed pushBackThisFrame;
    Fixed hitReflectVelX;
    Fixed hitReflectAccelX;
    Fixed reflectThisFrame;

    // 2.0 was here
    int currentScaling;
    int pendingScaling;

    int comboHits;
    int juggleCounter;
    int comboDamage;
    int lastDamageScale;
    int lastScalingTriggerID;

    int pendingUnlockHit;

    int knockDownFrames;

    Fixed groundBounceVelX;
    Fixed groundBounceVelY;
    Fixed groundBounceAccelX;
    Fixed groundBounceAccelY;

    Fixed wallBounceVelX;
    Fixed wallBounceVelY;
    Fixed wallBounceAccelX;
    Fixed wallBounceAccelY;

    int wallStopFrames;

    AtemiData *currentArmor;
    int armorHitsLeft;

    int recoveryTiming;

    ActionRef forcedTrigger;

    float charColorR;
    float charColorG;
    float charColorB;

    int uniqueParam[uniqueParamCount];

    int uniqueTimerCount;

    int debuffTimer;

    Action *pCurrentAction;

    Simulation *pSim;

    struct defaultCopy
    {
        std::vector<GuyRef> minions;
        std::set<int> setDeferredTriggerIDs;
        std::set<ActionRef> frameTriggers;
        FixedBuffer<uint32_t, 200> inputBuffer; // todo how much is too much?
        FixedBuffer<int8_t, 200> directionBuffer;
    } dc;

    // stuff that won't get copied around when dumping simulations
    struct noCopy
    {
        noCopy &operator =(__attribute__((unused)) const noCopy &rhs) { return *this; }

        int lastLogFrame = 0;
        std::deque<std::string> logQueue;
    } nc;

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

inline GuyRef& GuyRef::operator=(Guy* rhs) {
    pGuy = rhs;
    if (pGuy) {
        guyID = pGuy->getUniqueID();
    } else {
        guyID = -1;
    }
    return *this;
}

inline GuyRef::GuyRef(Guy *pGuy) {
    this->pGuy = pGuy;
    if (this->pGuy) {
        this->guyID = pGuy->getUniqueID();
    } else {
        this->guyID = -1;
    }
}
