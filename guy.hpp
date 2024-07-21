#pragma once

#include "json.hpp"
#include <deque>
#include <string>

#include "main.hpp"
#include "render.hpp"
#include "input.hpp"
#include "fixed.hpp"

class Guy {
public:
    void setOpponent(Guy *pGuy) { pOpponent = pGuy; }

    void Input(int input);
    void UpdateActionData(void);
    bool PreFrame(void);
    void Render(void);
    bool Push(Guy *pOtherGuy);
    void UpdateBoxes(void);
    bool WorldPhysics(void);
    bool CheckHit(Guy *pOtherGuy);
    bool ApplyHitEffect(nlohmann::json *pHitEffect, bool applyHit, bool applyHitStun, bool isDrive, bool isDomain);
    void DoBranchKey(bool preHit);
    void DoHitBoxKey(const char *name);
    void DoStatusKey();
    void DoTriggers();
    bool Frame(bool endWarudoFrame = false);
    std::string getActionName(int actionID);

    void addWarudo(int w, bool isFreeze = false) {
        pendingWarudo += w;
        warudoIsFreeze = isFreeze;
    }

    std::string getCharacter() { return character + std::to_string(version); }
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
    void switchDirection() { direction = direction * Fixed(-1); }

    bool needsTurnaround() {
        bool turnaround = false;
        if (pOpponent) {
            if ( direction > 0 && getPosX() > pOpponent->getPosX() ) {
                turnaround = true;
            } else if ( direction < 0 && getPosX() < pOpponent->getPosX() ) {
                turnaround = true;
            }
        }
        return turnaround;
    }

    Fixed getStartPosX() { return startPosX; }
    void setStartPosX( Fixed newPosX ) { startPosX = newPosX; }

    void resetPos() {
        posX = startPosX;
        posY = startPosY;

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

    int getComboHits() { return comboHits; }
    int getJuggleCounter() { return juggleCounter; }
    int getWarudo() { return warudo; }
    int getHitStun() { return hitStun; }
    int getComboDamage() { return comboDamage; }
    int getUniqueParam() { return uniqueCharge; }
    int getDebuffTimer() { return debuffTimer; }
    int getStyle() { return styleInstall; }
    int getInstallFrames() { return styleInstallFrames; }
    std::string getName() { return name; }
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

    bool getProjectile() { return isProjectile; }
    int getProjHitCount() { return projHitCount; }
    int getLimitShotCategory() { return limitShotCategory; }

    std::vector<Box> *getPushBoxes() { return &pushBoxes; }
    std::vector<HitBox> *getHitBoxes() { return &hitBoxes; }
    std::vector<HurtBox> *getHurtBoxes() { return &hurtBoxes; }
    std::vector<Box> *getThrowBoxes() { return &throwBoxes; }

    int getCurrentAction() { return currentAction; }
    int getCurrentFrame() { return currentFrame; }
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

    ~Guy() {
        for (auto minion : minions) {
            delete minion;
        }
        minions.clear();

        if (pParent) {
            // std erase when
            const auto it = std::remove(pParent->minions.begin(), pParent->minions.end(), this);
            pParent->minions.erase(it, pParent->minions.end());
        } else {
            const auto it = std::remove(guys.begin(), guys.end(), this);
            guys.erase(it, guys.end());
        }
    }

    Guy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color)
    {
        const std::string charPath = "data/chars/" + charName + "/";
        const std::string commonPath = "data/chars/common/";
        character = charName;
        version = charVersion;
        name = character;
        posX = startPosX = x;
        posY = startPosY = y;
        direction = startDir;
        charColorR = color.r;
        charColorG = color.g;
        charColorB = color.b;

        movesDictJson = parseCharFile(charPath, character, version, "moves");
        rectsJson = parseCharFile(charPath, character, version, "rects");
        namesJson = parseCharFile(charPath, character, version, "names");
        triggerGroupsJson = parseCharFile(charPath, character, version, "trigger_groups");
        triggersJson = parseCharFile(charPath, character, version, "triggers");
        commandsJson = parseCharFile(charPath, character, version, "commands");
        chargeJson = parseCharFile(charPath, character, version, "charge");
        hitJson = parseCharFile(charPath, character, version, "hit");
        atemiJson = parseCharFile(charPath, character, version, "atemi");
        charInfoJson = parseCharFile(charPath, character, version, "charinfo");

        commonMovesJson = parseCharFile(commonPath, "common", version, "moves");
        commonRectsJson = parseCharFile(commonPath, "common", version, "rects");
        commonAtemiJson = parseCharFile(commonPath, "common", version, "atemi");

        Input(0);

        BuildMoveList();
    
        UpdateActionData();

        health = maxHealth = 10000; // todo pull that, need to make one of the dumps contain vital_max
    }

    Guy(Guy &parent, Fixed posOffsetX, Fixed posOffsetY, int startAction, int styleID, bool isProj)
    {
        character = parent.character;
        version = parent.version;
        name = character + "'s minion";
        direction = parent.direction;
        posX = parent.posX + parent.posOffsetX * direction + posOffsetX;
        posY = parent.posY + parent.posOffsetY + posOffsetY;
        charColorR = parent.charColorR;
        charColorG = parent.charColorG;
        charColorB = parent.charColorB;

        movesDictJson = parent.movesDictJson;
        rectsJson = parent.rectsJson;
        namesJson = parent.namesJson;
        triggerGroupsJson = parent.triggerGroupsJson;
        triggersJson = parent.triggersJson;
        commandsJson = parent.commandsJson;
        chargeJson = parent.chargeJson;
        hitJson = parent.hitJson;
        atemiJson = parent.hitJson;
        charInfoJson = parent.charInfoJson;

        commonMovesJson = parent.commonMovesJson;
        commonRectsJson = parent.commonRectsJson;
        commonAtemiJson = parent.commonAtemiJson;

        pOpponent = parent.pOpponent;

        currentAction = startAction;
        styleInstall = styleID;

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

    std::vector<char *> &getMoveList() { return vecMoveList; }
    int *getNeutralMovePtr() { return &neutralMove; }
    int *getInputOverridePtr() { return &inputOverride; }
    int *getInputIDPtr() { return &inputID; }
    int *getInputListIDPtr() { return &inputListID; }
private:
    std::string name;
    std::string character;
    int version;
    Guy *pOpponent = nullptr;

    void log(bool log, std::string logLine)
    {
        if (!log) return;
        std::string frameDiff = to_string_leading_zeroes(globalFrameCount - lastLogFrame, 3);
        std::string curFrame = to_string_leading_zeroes(currentFrame, 3);
        logQueue.push_back(frameDiff + " " + curFrame + " " + logLine);
        if (logQueue.size() > 15) {
            logQueue.pop_front();
        }
        lastLogFrame = globalFrameCount;
    }
    void log(std::string logLine) { log(true, logLine ); }
    int lastLogFrame = 0;
    std::deque<std::string> logQueue;

    bool GetRect(Box &outBox, int rectsPage, int boxID,  Fixed offsetX, Fixed offsetY, int dir);
    const char* FindMove(int actionID, int styleID, nlohmann::json **ppMoveJson);
    void BuildMoveList();
    std::vector<char *> vecMoveList;
    int neutralMove = 0;

    int inputOverride = 0;
    int inputID = 0;
    int inputListID = 0;

    bool deniedLastBranch = false;

    std::vector<Guy*> minions;
    bool isProjectile = false;
    int projHitCount = 0;
    bool die = false;
    int limitShotCategory = -1;
    bool noPush = false;
    Guy *pParent = nullptr;

    nlohmann::json movesDictJson;
    nlohmann::json rectsJson;
    nlohmann::json namesJson;
    nlohmann::json triggerGroupsJson;
    nlohmann::json triggersJson;
    nlohmann::json commandsJson;
    nlohmann::json chargeJson;
    nlohmann::json hitJson;
    nlohmann::json atemiJson;
    nlohmann::json charInfoJson;

    nlohmann::json commonMovesJson;
    nlohmann::json commonRectsJson;
    nlohmann::json commonAtemiJson;

    std::map<std::pair<int, int>, std::pair<std::string, bool>> mapMoveStyle;

    Fixed posX;
    Fixed posY;
    Fixed direction = Fixed(1);

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
    bool wallbounced = false;
    bool didPush = false;

    int landingAdjust = 0;
    int prevPoseStatus = 0;
    int forcedPoseStatus = 0;
    int actionStatus = 0;
    int jumpStatus = 0;

    bool counterState = false;
    bool punishCounterState = false;
    bool forceKnockDownState = false;

    bool offsetDoesNotPush = false;
    bool noVelNextFrame = false;
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

    Fixed homeTargetX;
    Fixed homeTargetY;

    int maxHealth;
    int health;

    int currentAction = 1;
    int nextAction = -1;
    int nextActionFrame = -1;
    bool keepPlace = false;
    bool keepFrame = false;
    int currentFrame = 0;
    int actionFrameDuration = 0;
    int mainFrame = 0;
    int followFrame = 0;
    int marginFrame = 0;
    int loopCount = 0;
    int loopPoint = 0;
    bool nextActionOpponentAction = false;
    bool opponentAction = false;
    bool locked = false;
    bool hasLooped = false;

    // todo round begin action needs to set that
    int styleInstall = 0;
    int styleInstallFrames = 0;
    bool countingDownInstall = false;

    int airActionCounter = 0;

    int currentInput = 0;
    std::deque<int> inputBuffer;

    // hitting side
    int canHitID = 0;
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

    int warudo = 0;
    int pendingWarudo = 0;
    int timeInWarudo = 0;
    bool warudoIsFreeze = false;

    // getting hit side
    Guy *pAttacker = nullptr;
    int hitStun = 0;
    bool resetHitStunOnLand = false;
    bool resetHitStunOnTransition = false;
    Fixed hitVelX;
    Fixed hitAccelX;
    Fixed pushBackThisFrame;
    bool noCounterPush = false;
    bool beenHitThisFrame = false;
    int comboHits = 0;
    int juggleCounter = 0;
    int comboDamage = 0;
    bool forceKnockDown = false;
    bool isDown = false;
    int knockDownFrames = 0;
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

    int deferredTriggerAction = -1;
    int deferredTriggerGroup = -1;

    bool isDrive = false;
    bool wasDrive = false;

    float charColorR = 1.0;
    float charColorG = 1.0;
    float charColorB = 1.0;

    int uniqueCharge = 0;
    bool uniqueTimer = false;

    int debuffTimer = 0;

    std::string actionName;
    nlohmann::json *pActionJson;
};
