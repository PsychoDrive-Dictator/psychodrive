#pragma once

#include "json.hpp"
#include <deque>
#include <string>
#include "main.hpp"

class Guy {
public:
    Guy(std::string charName, float startPosX, float startPosY, int startDir, color renderColor);
    void setOpponent(Guy *pGuy) { pOpponent = pGuy; }
    void Input(int input);
    void PreFrame(void);
    void Frame(void);

    // for opponent direction
    float getPosX() {
        return posX + (posOffsetX*direction);
    }

    int getCurrentAction() { return currentAction; }
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

    nlohmann::json movesDictJson;
    nlohmann::json rectsJson;
    nlohmann::json namesJson;
    nlohmann::json triggerGroupsJson;
    nlohmann::json triggersJson;
    nlohmann::json commandsJson;
    nlohmann::json chargeJson;

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
    int currentFrame = 0;
    int actionFrameDuration = 0;
    int marginFrame = 0;
    int currentInput = 0;
    std::deque<int> inputBuffer;

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