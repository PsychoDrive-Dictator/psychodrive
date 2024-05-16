#pragma once

#include "json.hpp"
#include <deque>
#include <string>
#include "main.hpp"

class Guy {
public:
    Guy(std::string charName, float startPosX, float startPosY, int startDir, color renderColor);
    Guy(Guy &parent, float posOffsetX, float posOffsetY, int startAction);
    void setOpponent(Guy *pGuy) { pOpponent = pGuy; }

    void Input(int input);
    bool PreFrame(void);
    void Render(void);
    bool Push(Guy *pOtherGuy);
    bool CheckHit(Guy *pOtherGuy);
    void Hit(int hitStun, int destX, int destY, int destTime);
    void DoBranchKey(void);
    bool Frame(void);

    void addWarudo(int w) { warudo += w; }

    // for opponent direction
    float getPosX() {
        return posX + (posOffsetX*direction);
    }

    int getComboHits() { return comboHits; }
    int getJuggleCounter() { return juggleCounter; }
    int getWarudo() { return warudo; }
    int getHitStun() { return hitStun; }

    std::vector<Box> *getPushBoxes() { return &pushBoxes; }
    std::vector<HitBox> *getHitBoxes() { return &hitBoxes; }
    std::vector<Box> *getHurtBoxes() { return &hurtBoxes; }

    int getCurrentAction() { return currentAction; }
    int getCurrentFrame() { return currentFrame; }
    std::string getActionName() { return actionName; }
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

private:
    std::string character;
    Guy *pOpponent = nullptr;

    std::vector<Guy*> minions;
    bool isProjectile = false;
    int projHitCount = 0;
    Guy *pParent = nullptr;

    nlohmann::json movesDictJson;
    nlohmann::json rectsJson;
    nlohmann::json namesJson;
    nlohmann::json triggerGroupsJson;
    nlohmann::json triggersJson;
    nlohmann::json commandsJson;
    nlohmann::json chargeJson;
    nlohmann::json hitJson;

    float posX = 0.0f;
    float posY = 0.0f;
    int direction = 1;

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
    int currentInput = 0;
    std::deque<int> inputBuffer;

    // hitting side
    int canHitID = -1;

    int warudo = 0;
    int timeInWarudo = 0;

    // getting hit side
    int hitStun = 0;
    float hitVelX = 0.0f;
    float hitVelY = 0.0f;
    int hitVelFrames = 0;
    int comboHits = 0;
    int juggleCounter = 0;

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
};