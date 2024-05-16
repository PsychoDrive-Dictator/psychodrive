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
    void Hit(int hitStun, int destX, int destY, int destTime, int damage);
    void DoBranchKey();
    void DoHitBoxKey(const char *name, bool domain = false);
    void DoStatusKey();
    void DoTriggers();
    bool Frame(void);

    void addWarudo(int w) { warudo += w; }

    std::string getCharacter() { return character; }
    // for opponent direction
    Guy *getOpponent() { return pOpponent; }
    Guy *getParent() { return pParent; }
    std::vector<Guy*> &getMinions() { return minions; }
    float getPosX() {
        return posX + (posOffsetX*direction);
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

    int getComboHits() { return comboHits; }
    int getJuggleCounter() { return juggleCounter; }
    int getWarudo() { return warudo; }
    int getHitStun() { return hitStun; }
    int getComboDamage() { return comboDamage; }
    std::string getName() { return name; }

    bool getProjectile() { return isProjectile; }
    int getProjHitCount() { return projHitCount; }
    int getLimitShotCategory() { return limitShotCategory; }

    std::vector<Box> *getPushBoxes() { return &pushBoxes; }
    std::vector<HitBox> *getHitBoxes() { return &hitBoxes; }
    std::vector<Box> *getHurtBoxes() { return &hurtBoxes; }

    int getCurrentAction() { return currentAction; }
    int getCurrentFrame() { return currentFrame; }
    std::string getActionName() { return actionName; }
    bool getAirborne() { return airborne; }
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

        static bool commonMovesInitialized = false;
        if (!commonMovesInitialized) {
            commonMovesJson = parse_json_file("common_moves.json");
            commonRectsJson = parse_json_file("common_rects.json");
            commonMovesInitialized = true;
        }
    
        UpdateActionData();
    }

    Guy(Guy &parent, float posOffsetX, float posOffsetY, int startAction)
    {
        character = parent.character;
        name = character + "'s minion";
        posX = parent.posX + parent.posOffsetX * direction + posOffsetX;
        posY = parent.posY + parent.posOffsetY + posOffsetY;
        direction = parent.direction;
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

        pOpponent = parent.pOpponent;

        currentAction = startAction;

        isProjectile = true;
        projHitCount = -1; // unset yet, is in the first action
        pParent = &parent;

        UpdateActionData();
    }

private:
    std::string name;
    std::string character;
    Guy *pOpponent = nullptr;

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

    static nlohmann::json commonMovesJson;
    static nlohmann::json commonRectsJson;

    float posX = 0.0f;
    float posY = 0.0f;
    int direction = 1;

    bool airborne = false;
    bool landed = false;
    bool touchedWall = false;
    bool startsFalling = false;

    int landingAdjust = 0;

    float posOffsetX = 0.0f;
    float posOffsetY = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float accelX = 0.0f;
    float accelY = 0.0f;

    int currentAction = 1;
    int nextAction = -1;
    bool keepPlace = false;
    int currentFrame = 0;
    int actionFrameDuration = 0;
    int marginFrame = 0;
    int loopCount = 0;
    int loopPoint = 0;
    bool commonAction = false;
    bool hasLooped = false;
    bool actionFrameDataInitialized = false;

    int currentInput = 0;
    std::deque<int> inputBuffer;

    // hitting side
    int canHitID = -1;
    bool hitThisFrame = false;

    int warudo = 0;
    int timeInWarudo = 0;

    // getting hit side
    Guy *pAttacker = nullptr;
    int hitStun = 0;
    bool resetHitStunOnLand = false;
    int hitStunOnLand = 0;
    float hitVelX = 0.0f;
    float hitAccelX = 0.0f;
    float pushBackThisFrame = 0.0f;
    bool beenHitThisFrame = false;
    int comboHits = 0;
    int juggleCounter = 0;
    int comboDamage = 0;
    bool knockedDown = false;
    int knockDownFrames = 0;

    int recoveryTiming = 0;

    std::vector<Box> pushBoxes;
    std::vector<HitBox> hitBoxes;
    std::vector<Box> hurtBoxes;

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
