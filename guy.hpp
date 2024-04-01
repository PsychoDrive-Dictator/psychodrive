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
    void DoBranchKey();
    void DoHitBoxKey(const char *name);
    void DoStatusKey();
    void DoTriggers();
    bool Frame(void);

    void addWarudo(int w) {
        if (pParent) {
            pParent->addWarudo(w);
            return;
        }
        warudo += w;
    }

    std::string getCharacter() { return character; }
    std::deque<std::string> &getLogQueue() { return logQueue; }
    // for opponent direction
    Guy *getOpponent() { return pOpponent; }
    Guy *getParent() { return pParent; }
    std::vector<Guy*> &getMinions() { return minions; }
    float getPosX() {
        return posX + (posOffsetX*direction);
    }
    float getPosY() {
        return posY;
    }

    void resetPosDebug( float x, float y) {
        posX = x;
        posY = y;

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
    int getInstallFrames() { return styleInstallFrames; }
    std::string getName() { return name; }
    int getPoseStatus() { return poseStatus; }
    int getActionStatus() { return actionStatus; }
    int getJumpStatus() { return jumpStatus; }

    bool getProjectile() { return isProjectile; }
    int getProjHitCount() { return projHitCount; }
    int getLimitShotCategory() { return limitShotCategory; }

    std::vector<Box> *getPushBoxes() { return &pushBoxes; }
    std::vector<HitBox> *getHitBoxes() { return &hitBoxes; }
    std::vector<HurtBox> *getHurtBoxes() { return &hurtBoxes; }
    std::vector<Box> *getThrowBoxes() { return &throwBoxes; }
    std::vector<ArmorBox> *getArmorBoxes() { return &armorBoxes; }

    int getCurrentAction() { return currentAction; }
    int getCurrentFrame() { return currentFrame; }
    std::string getActionName() { return actionName; }
    bool getAirborne() { return airborne; }
    float getHitVelX() { return hitVelX; }
    void getPosDebug( float &outPosX, float &outPosY, float &outPosOffsetX, float &outPosOffsetY) {
        outPosX = posX;
        outPosY = posY;
        outPosOffsetX = posOffsetX;
        outPosOffsetY = posOffsetY;
    }
    void getVel( float &outVelX, float &outVelY, float &outAccelX, float &outAccelY) {
        outVelX = velocityX;
        outVelY = velocityY;
        outAccelX = accelX;
        outAccelY = accelY;
    }

    ~Guy() {
        for (auto minion : minions) {
            delete minion;
        }
        minions.clear();
    }

    Guy(std::string charName, float startPosX, float startPosY, int startDir, color color)
    {
        character = charName;
        name = character;
        posX = startPosX;
        posY = startPosY;
        direction = startDir;
        charColorR = color.r;
        charColorG = color.g;
        charColorB = color.b;

        movesDictJson = parse_json_file(character + "_moves.json");
        rectsJson = parse_json_file(character + "_rects.json");
        namesJson = parse_json_file(character + "_names.json");
        triggerGroupsJson = parse_json_file(character + "_trigger_groups.json");
        triggersJson = parse_json_file(character + "_triggers.json");
        commandsJson = parse_json_file(character + "_commands.json");
        chargeJson = parse_json_file(character + "_charge.json");
        hitJson = parse_json_file(character + "_hit.json");
        atemiJson = parse_json_file(character + "_atemi.json");

        static bool commonMovesInitialized = false;
        if (!commonMovesInitialized) {
            commonMovesJson = parse_json_file("common_moves.json");
            commonRectsJson = parse_json_file("common_rects.json");
            commonAtemiJson = parse_json_file("common_atemi.json");
            commonMovesInitialized = true;
        }

        Input(0);

        BuildMoveList();
    
        UpdateActionData();
    }

    Guy(Guy &parent, float posOffsetX, float posOffsetY, int startAction)
    {
        character = parent.character;
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

        pOpponent = parent.pOpponent;

        currentAction = startAction;

        isProjectile = true;
        projHitCount = -1; // unset yet, is in the first action
        pParent = &parent;

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

    static nlohmann::json commonMovesJson;
    static nlohmann::json commonRectsJson;
    static nlohmann::json commonAtemiJson;

    std::map<int, int> mapMoveStyle;

    float posX = 0.0f;
    float posY = 0.0f;
    int direction = 1;

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
    int poseStatus = 0;
    int actionStatus = 0;
    int jumpStatus = 0;

    bool counterState = false;
    bool punishCounterState = false;

    float posOffsetX = 0.0f;
    float posOffsetY = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float accelX = 0.0f;
    float accelY = 0.0f;

    int currentAction = 1;
    int nextAction = -1;
    int nextActionFrame = -1;
    bool keepPlace = false;
    int currentFrame = 0;
    int actionFrameDuration = 0;
    int mainFrame = 0;
    int followFrame = 0;
    int marginFrame = 0;
    int loopCount = 0;
    int loopPoint = 0;
    bool commonAction = false;
    bool nextActionOpponentAction = false;
    bool opponentAction = false;
    bool hasLooped = false;
    bool actionFrameDataInitialized = false;

    int styleInstall = -1;
    int styleInstallFrames = 0;
    bool countingDownInstall = false;

    int currentInput = 0;
    std::deque<int> inputBuffer;

    // hitting side
    int canHitID = -1;
    bool hitThisFrame = false;
    bool punishCounterThisFrame = false;
    bool grabbedThisFrame = false;
    bool blocked = false;

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
    int currentAtemiID = -1;
    int atemiHitsLeft = 0;
    bool atemiThisFrame = false;

    int recoveryTiming = 0;

    std::vector<Box> pushBoxes;
    std::vector<HitBox> hitBoxes;
    std::vector<ArmorBox> armorBoxes;
    std::vector<HurtBox> hurtBoxes;
    std::vector<Box> throwBoxes;

    std::vector<RenderBox> renderBoxes;

    int deferredActionFrame = -1;
    int deferredAction = 0;

    bool isDrive = false;
    bool wasDrive = false;

    float charColorR = 1.0;
    float charColorG = 1.0;
    float charColorB = 1.0;

    int uniqueCharge = 0;

    std::string actionName;
    nlohmann::json actionJson;
};
