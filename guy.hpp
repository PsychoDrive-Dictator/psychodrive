#pragma once

#include "json.hpp"
#include <deque>
#include <string>

#include "main.hpp"
#include "render.hpp"
#include "input.hpp"

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
    bool ApplyHitEffect(nlohmann::json hitEffect, bool applyHit, bool applyHitStun, bool isDrive, bool isDomain);
    void DoBranchKey(bool preHit);
    void DoHitBoxKey(const char *name);
    void DoStatusKey();
    void DoTriggers();
    bool Frame(void);
    std::string getActionName(int actionID);

    void addWarudo(int w) {
        warudo += w;
    }

    std::string getCharacter() { return character + std::to_string(version); }
    color getColor() { color ret; ret.r = charColorR; ret.g = charColorG; ret.b = charColorB; return ret; }
    std::deque<std::string> &getLogQueue() { return logQueue; }
    // for opponent direction
    Guy *getOpponent() { return pOpponent; }
    Guy *getParent() { return pParent; }
    std::vector<Guy*> &getMinions() { return minions; }
    float getPosX() {
        return posX + (posOffsetX*direction);
    }
    float getPosY() {
        return posY + posOffsetY;
    }
    int getDirection() { return direction; }
    void switchDirection() { direction *= -1; }

    float getStartPosX() { return startPosX; }
    void setStartPosX( float newPosX ) { startPosX = newPosX; }

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
    void setPos( float x, float y) {
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
    bool getCrouchingDebug() { return crouching; }
    bool getAirborneDebug() { return airborne; }
    float getHitVelX() { return hitVelX; }
    void getPosDebug( float &outPosX, float &outPosY, float &outPosOffsetX, float &outPosOffsetY) {
        outPosX = posX;
        outPosY = posY;
        outPosOffsetX = posOffsetX;
        outPosOffsetY = posOffsetY;
    }
    void getVel( float &outVelX, float &outVelY, float &outAccelX, float &outAccelY) {
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

    Guy(std::string charName, int charVersion, float x, float y, int startDir, color color)
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

    Guy(Guy &parent, float posOffsetX, float posOffsetY, int startAction, int styleID)
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

        isProjectile = true;
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

    bool GetRect(Box &outBox, int rectsPage, int boxID,  float offsetX, float offsetY, int dir);
    const char* FindMove(int actionID, int styleID, nlohmann::json &moveJson);
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

    float posX = 0.0f;
    float posY = 0.0f;
    int direction = 1;

    float startPosX = 0.0f;
    float startPosY = 0.0f;

    bool airborne = false;
    bool landed = false;
    bool touchedWall = false;
    bool touchedOpponent = false;
    bool startsFalling = false;
    bool crouching = false;
    bool blocking = false;
    bool bounced = false;
    bool wallbounced = false;

    int landingAdjust = 0;
    int prevPoseStatus = 0;
    int forcedPoseStatus = 0;
    int actionStatus = 0;
    int jumpStatus = 0;

    bool counterState = false;
    bool punishCounterState = false;
    bool forceKnockDownState = false;

    float posOffsetX = 0.0f;
    float posOffsetY = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float accelX = 0.0f;
    float accelY = 0.0f;

    float cancelVelocityX = 0.0f;
    float cancelVelocityY = 0.0f;
    float cancelAccelX = 0.0f;
    float cancelAccelY = 0.0f;

    float homeTargetX = 0.0;
    float homeTargetY = 0.0;

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
    bool hasLooped = false;

    // todo round begin action needs to set that
    int styleInstall = 0;
    int styleInstallFrames = 0;
    bool countingDownInstall = false;

    int airActionCounter = 0;

    int currentInput = 0;
    std::deque<int> inputBuffer;

    // hitting side
    int canHitID = -1;
    bool hitThisFrame = false;
    bool hasBeenBlockedThisFrame = false;
    bool hitArmorThisFrame = false;
    bool hitAtemiThisFrame = false;
    bool punishCounterThisFrame = false;
    bool grabbedThisFrame = false;
    bool hitThisMove = false;
    bool hasBeenBlockedThisMove = false;
    bool hitArmorThisMove = false;
    bool hitAtemiThisMove = false;

    int warudo = 0;
    int timeInWarudo = 0;

    // getting hit side
    Guy *pAttacker = nullptr;
    int hitStun = 0;
    bool resetHitStunOnLand = false;
    bool resetHitStunOnTransition = false;
    float hitVelX = 0.0f;
    float hitAccelX = 0.0f;
    float pushBackThisFrame = 0.0f;
    bool noCounterPush = false;
    bool beenHitThisFrame = false;
    int comboHits = 0;
    int juggleCounter = 0;
    int comboDamage = 0;
    bool forceKnockDown = false;
    bool isDown = false;
    int knockDownFrames = 0;
    bool groundBounce = false;
    float groundBounceVelX = 0.0f;
    float groundBounceVelY = 0.0f;
    float groundBounceAccelX = 0.0f;
    float groundBounceAccelY = 0.0f;
    bool wallBounce = false;
    bool wallSplat = false;
    float wallBounceVelX = 0.0f;
    float wallBounceVelY = 0.0f;
    float wallBounceAccelX = 0.0f;
    float wallBounceAccelY = 0.0f;
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

    std::string actionName;
    nlohmann::json actionJson;
};
