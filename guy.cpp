#include <stdio.h>

#include <fstream>
#include <ios>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <bitset>

#include "guy.hpp"
#include "main.hpp"
#include "render.hpp"
#include <string>

void parseRootOffset( nlohmann::json& keyJson, int&offsetX, int& offsetY)
{
    if ( keyJson.contains("RootOffset") && keyJson["RootOffset"].contains("X") && keyJson["RootOffset"].contains("Y") ) {
        offsetX = keyJson["RootOffset"]["X"];
        offsetY = keyJson["RootOffset"]["Y"];
    }
}

bool matchInput( int input, uint32_t okKeyFlags, uint32_t okCondFlags, uint32_t dcExcFlags = 0 )
{
    // do that before stripping held keys since apparently holding parry to drive rush depends on it
    if (dcExcFlags != 0 ) {
        if ((dcExcFlags & input) != dcExcFlags) {
            return false;
        }
    }

    input &= ~(LP+MP+HP+LK+MK+HK);
    input |= (input & (LP_pressed+MP_pressed+HP_pressed+LK_pressed+MK_pressed+HK_pressed)) >> 6;
    input &= ~(LP_pressed+MP_pressed+HP_pressed+LK_pressed+MK_pressed+HK_pressed);

    if (okCondFlags & 2) {
        if ((input & 0xF) == (okKeyFlags & 0xF)) {
            return true; // match exact direction
        }
        return false;
    }

    uint32_t match = okKeyFlags & input;

    if (okCondFlags & 0x80) {
        if (match == okKeyFlags) {
            return true; // match all?
        }
    } else if (okCondFlags & 0x40) {
        if (std::bitset<32>(match).count() >= 2) {
            return true; // match 2?
        }
    } else {
        if (match != 0) {
            return true; // match any? :/
        }
    }


    return false;
}

static inline float fixedToFloat(int fixed)
{
    short integerPart = (fixed & 0xFFFF0000) >> 16;
    float decimalPart = (fixed & 0xFFFF)/(float)0xFFFF;

    return decimalPart + integerPart;
}

static inline void doSteerKeyOperation(float &value, float keyValue, int operationType)
{
    switch (operationType) {
        case 1: // set
        value = keyValue; 
        break;
        case 2: // add ?
        value += keyValue;
        break;
        default:
        log("Uknown steer keyoperation!");
    }
}

bool getRect(Box &outBox, nlohmann::json rectsJson, int rectsPage, int boxID,  int offsetX, int offsetY, int dir)
{
    std::string pageIDString = to_string_leading_zeroes(rectsPage, 2);
    std::string boxIDString = to_string_leading_zeroes(boxID, 3);
    if (!rectsJson.contains(pageIDString) || !rectsJson[pageIDString].contains(boxIDString)) {
        return false;
    }
    auto rectJson = rectsJson[pageIDString][boxIDString];
    int xOrig = rectJson["OffsetX"];
    int yOrig = rectJson["OffsetY"];
    int xRadius = rectJson["SizeX"];
    int yRadius = rectJson["SizeY"];
    xOrig *= dir;

    outBox.x = xOrig - xRadius + offsetX;
    outBox.y = yOrig - yRadius + offsetY;
    outBox.w = xRadius * 2;
    outBox.h = yRadius * 2;

    return true;
}

void Guy::Input(int input)
{
    if (direction < 0) {
        int newMask = 0;
        if (input & BACK) {
            newMask |= FORWARD;
        }
        if (input & FORWARD) {
            newMask |= BACK;
        }
        input &= ~(FORWARD+BACK);
        input |= newMask;
    }
    currentInput = input;

    inputBuffer.push_front(input);
    // how much is too much?
    if (inputBuffer.size() > 200) {
        inputBuffer.pop_back();
    }
}

nlohmann::json Guy::commonMovesJson = nullptr;
nlohmann::json Guy::commonRectsJson = nullptr;

bool Guy::PreFrame(void)
{
    if (warudo > 0) {
        timeInWarudo++;
        warudo--;
    }
    if (warudo > 0) {
        return false;
    }

    auto actionIDString = to_string_leading_zeroes(currentAction, 4);
    bool validAction = namesJson.contains(actionIDString);
    actionName = validAction ? namesJson[actionIDString] : "invalid";

    actionJson = nullptr;
    if (commonMovesJson.contains(std::to_string(currentAction))) {
        actionJson = commonMovesJson[std::to_string(currentAction)];
    } else if (movesDictJson.contains(actionName)) {
        actionJson = movesDictJson[actionName];
    }

    if (actionJson != nullptr)
    {
        marginFrame = actionJson["fab"]["ActionFrame"]["MarginFrame"];
        //standing has 0 marginframe, pre-jump has -1, crouch -1..
        actionFrameDuration = actionJson["fab"]["Frame"];

        if (isProjectile && projHitCount == -1) {
            projHitCount = actionJson["pdata"]["HitCount"];
            // log("initial hitcount " + std::to_string(projHitCount));
        }

        if (actionJson.contains("PlaceKey"))
        {
            for (auto& [placeKeyID, placeKey] : actionJson["PlaceKey"].items())
            {
                if ( !placeKey.contains("_StartFrame") || placeKey["_StartFrame"] > currentFrame || placeKey["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                for (auto& [frame, offset] : placeKey["PosList"].items()) {
                    int keyStartFrame = placeKey["_StartFrame"];
                    if (atoi(frame.c_str()) == currentFrame - keyStartFrame) {
                        if (placeKey["Axis"] == 0) {
                            posOffsetX = offset.get<int>();
                        } else if (placeKey["Axis"] == 1) {
                            posOffsetY = offset.get<int>();    
                        }
                    }
                }
            }
        }

        float prevVelX = velocityX;
        float prevVelY = velocityY;

        velocityX += accelX;
        velocityY += accelY;

        if ((velocityY * prevVelY) < 0.0f || (accelY != 0.0f && velocityY == 0.0f)) {
            startsFalling = true;
        } else {
            startsFalling = false;
        }

        // log(std::to_string(currentAction) + " " + std::to_string(prevVelX) + " " + std::to_string(velocityX));

        if ( (velocityX * prevVelX) < 0.0f || (accelX != 0.0f && velocityX == 0.0f) ) {
            // sign change?
            velocityX = 0.0f;
            accelX = 0.0f;
        }

        float prevPosY = posY;

        posX += (velocityX * direction);
        posY += velocityY;

        if (prevPosY == 0.0f && posY > 0.0f) {
            airborne = true; // i think we should go by statusKey instead
        }

        if (hitVelFrames > 0) {
            posX += hitVelX;
            posY += hitVelY;
            hitVelFrames--;
        }

        prevVelX = velocityX;

        if (actionJson.contains("SteerKey"))
        {
            for (auto& [steerKeyID, steerKey] : actionJson["SteerKey"].items())
            {
                if ( !steerKey.contains("_StartFrame") || steerKey["_StartFrame"] > currentFrame || steerKey["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int operationType = steerKey["OperationType"];
                int valueType = steerKey["ValueType"];
                float fixValue = steerKey["FixValue"];

                switch (valueType) {
                    case 0: doSteerKeyOperation(velocityX, fixValue,operationType); break;
                    case 1: doSteerKeyOperation(velocityY, fixValue,operationType); break;
                    case 3: doSteerKeyOperation(accelX, fixValue,operationType); break;
                    case 4: doSteerKeyOperation(accelY, fixValue,operationType); break;
                }
            }
        }

        if (wasDrive && actionJson.contains("DriveSteerKey"))
        {
            for (auto& [steerKeyID, steerKey] : actionJson["DriveSteerKey"].items())
            {
                if ( !steerKey.contains("_StartFrame") || steerKey["_StartFrame"] > currentFrame || steerKey["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int operationType = steerKey["OperationType"];
                int valueType = steerKey["ValueType"];
                float fixValue = steerKey["FixValue"];

                switch (valueType) {
                    case 0: doSteerKeyOperation(velocityX, fixValue,operationType); break;
                    case 1: doSteerKeyOperation(velocityY, fixValue,operationType); break;
                    case 3: doSteerKeyOperation(accelX, fixValue,operationType); break;
                    case 4: doSteerKeyOperation(accelY, fixValue,operationType); break;
                }
            }
        }

        if ( (velocityX * prevVelX) < 0.0f || (accelX != 0.0f && velocityX == 0.0f) ) {
            // sign change?
            velocityX = 0.0f;
            accelX = 0.0f;
        }

        if (actionJson.contains("SwitchKey"))
        {
            for (auto& [keyID, key] : actionJson["SwitchKey"].items())
            {
                if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int flag = key["SystemFlag"];

                if (flag & 0x8000000) {
                    isDrive = true;
                }
            }
        }

        if (actionJson.contains("EventKey"))
        {
            for (auto& [keyID, key] : actionJson["EventKey"].items())
            {
                if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                if (key["Type"] == 0)
                {
                    float steerForward = fixedToFloat(key["Param02"].get<int>());
                    float steerBackward = fixedToFloat(key["Param03"].get<int>());

                    if (currentInput & FORWARD) {
                        posX += steerForward;
                        log("steerForward " + std::to_string(steerForward));
                    } else if (currentInput & BACK) {
                        posX += steerBackward;
                        log("steerBackward " + std::to_string(steerBackward));
                    }

                } else if (key["_IsUNIQUE_UNIQUE_PARAM_05"] == true) {
                    uniqueCharge = 1;
                }
            }
        }

        if (actionJson.contains("ShotKey"))
        {
            for (auto& [keyID, key] : actionJson["ShotKey"].items())
            {
                if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                float posOffsetX = key["PosOffset"]["x"].get<float>() * direction;
                float posOffsetY = key["PosOffset"]["y"];

                // spawn new guy
                Guy *pNewGuy = new Guy(*this, posOffsetX, posOffsetY, key["ActionId"].get<int>());
                minions.push_back(pNewGuy);
            }
        }

        if (actionJson.contains("WorldKey"))
        {
            bool tokiToTomare = false;
            for (auto& [keyID, key] : actionJson["WorldKey"].items())
            {
                if ( !tokiToTomare && (!key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame)) {
                    continue;
                }

                if (key["Type"] == 0 || key["Type"] == 1) {
                    // time stops, need to find when it resumes
                    // timer field here isn't always set, so we're going to look ahead for type 5
                    //pOpponent->addWarudo(key["Timer"].get<int>() * -1);
                    tokiToTomare = true;
                }

                if (tokiToTomare && key["Type"] == 5) {
                    // if we find a move with two shuffled pairs of those
                    // we might need to introduce more careful matching
                    tokiToTomare = false;
                    if (pOpponent ) {
                        pOpponent->addWarudo(key["_StartFrame"].get<int>() - currentFrame + 1);
                    }
                }
            }
        }

        pushBoxes.clear();
        hitBoxes.clear();
        hurtBoxes.clear();
        renderBoxes.clear();

        if (actionJson.contains("DamageCollisionKey"))
        {
            for (auto& [hurtBoxID, hurtBox] : actionJson["DamageCollisionKey"].items())
            {
                if ( !hurtBox.contains("_StartFrame") || hurtBox["_StartFrame"] > currentFrame || hurtBox["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int rootOffsetX = 0;
                int rootOffsetY = 0;
                parseRootOffset( hurtBox, rootOffsetX, rootOffsetY );
                rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
                rootOffsetY += posY + posOffsetY;

                bool drive = isDrive || wasDrive;
                bool parry = currentAction >= 480 && currentAction <= 489;
                bool di = currentAction >= 850 && currentAction <= 859;

                Box rect;

                int magicHurtBoxID = 8; // i hate you magic array of boxes

                for (auto& [boxNumber, boxID] : hurtBox["HeadList"].items()) {
                    if (getRect(rect, rectsJson, magicHurtBoxID, boxID,rootOffsetX, rootOffsetY,direction)) {
                        hurtBoxes.push_back(rect);
                        renderBoxes.push_back({rect, {charColorR,charColorG,charColorB}, drive,parry,di});
                    }
                }
                for (auto& [boxNumber, boxID] : hurtBox["BodyList"].items()) {
                    if (getRect(rect, rectsJson, magicHurtBoxID, boxID,rootOffsetX, rootOffsetY,direction)) {
                        hurtBoxes.push_back(rect);
                        renderBoxes.push_back({rect, {charColorR,charColorG,charColorB}, drive,parry,di});
                    }
                }
                for (auto& [boxNumber, boxID] : hurtBox["LegList"].items()) {
                    if (getRect(rect, rectsJson, magicHurtBoxID, boxID,rootOffsetX, rootOffsetY,direction)) {
                        hurtBoxes.push_back(rect);
                        renderBoxes.push_back({rect, {charColorR,charColorG,charColorB}, drive,parry,di});
                    }
                }
                for (auto& [boxNumber, boxID] : hurtBox["ThrowList"].items()) {
                    if (getRect(rect, rectsJson, 7, boxID,rootOffsetX, rootOffsetY,direction)) {
                        renderBoxes.push_back({rect, {charColorR,charColorG,charColorB}, drive,parry,di});
                    }
                }
            }
        }
        if (actionJson.contains("PushCollisionKey"))
        {
            for (auto& [pushBoxID, pushBox] : actionJson["PushCollisionKey"].items())
            {
                if ( !pushBox.contains("_StartFrame") || pushBox["_StartFrame"] > currentFrame || pushBox["_EndFrame"] <= currentFrame ) {
                    continue;
                }
                int rootOffsetX = 0;
                int rootOffsetY = 0;
                parseRootOffset( pushBox, rootOffsetX, rootOffsetY );
                rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
                rootOffsetY += posY + posOffsetY;

                Box rect;
                
                if (getRect(rect, rectsJson, 5, pushBox["BoxNo"],rootOffsetX, rootOffsetY, direction)) {
                    pushBoxes.push_back(rect);
                    renderBoxes.push_back({rect, {1.0,1.0,1.0}});
                } else if (getRect(rect, rectsJson, 7, pushBox["BoxNo"],rootOffsetX, rootOffsetY, direction)) {
                    pushBoxes.push_back(rect);
                    renderBoxes.push_back({rect, {1.0,1.0,1.0}});
                }
            }
        }

        if (actionJson.contains("AttackCollisionKey"))
        {
            for (auto& [hitBoxID, hitBox] : actionJson["AttackCollisionKey"].items())
            {
                if ( !hitBox.contains("_StartFrame") || hitBox["_StartFrame"] > currentFrame || hitBox["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int rootOffsetX = 0;
                int rootOffsetY = 0;
                parseRootOffset( hitBox, rootOffsetX, rootOffsetY );
                rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
                rootOffsetY += posY + posOffsetY;

                Box rect;

                for (auto& [boxNumber, boxID] : hitBox["BoxList"].items()) {
                    int collisionType = hitBox["CollisionType"];
                    color collisionColor = {1.0,0.0,0.0};
                    if (collisionType == 3 ) collisionColor = {0.5,0.5,0.5};

                    if (getRect(rect, rectsJson, collisionType, boxID,rootOffsetX, rootOffsetY,direction)) {
                        renderBoxes.push_back({rect, collisionColor, (isDrive || wasDrive) && collisionType != 3 });

                        if (collisionType != 3) {
                            int hitEntryID = hitBox["AttackDataListIndex"];
                            int hitID = hitBox["HitID"];
                            hitBoxes.push_back({rect,hitEntryID,hitID});
                        }
                    }
                }                    
            }
        }

        DoBranchKey();

        // should this fall through and let triggers also happen? prolly

        if (actionJson.contains("TriggerKey"))
        {
            for (auto& [keyID, key] : actionJson["TriggerKey"].items())
            {
                if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                bool defer = !key["_NotDefer"];
                int triggerEndFrame = key["_EndFrame"];
                //int other = key["_Other"];

                // not sure what that is but always some nonsense
                // like 22p into everything, or 3HK special cancellable?
                // if ( other == 64 ) {
                //     continue;
                // }

                // on hit or block? not sure
                // int state = key["_State"];
                // if ( state == 48 && canHitID == -1) {
                //     continue;
                // }

                //int condFlag = key["ConditionFlag"];
                // killbox says "5199 and 5131 are like on hit and on block"
                int condition = key["_Condition"];
                if ( (condition == 5199 || condition == 5131) && canHitID == -1) {
                    continue;
                }

                auto triggerGroupString = to_string_leading_zeroes(key["TriggerGroup"], 3);
                for (auto& [keyID, key] : triggerGroupsJson[triggerGroupString].items())
                {
                    int triggerID = atoi(keyID.c_str());
                    std::string actionString = key;
                    int actionID = atoi(actionString.substr(0, actionString.find(" ")).c_str());

                    auto triggerIDString = std::to_string(triggerID);
                    auto actionIDString = to_string_leading_zeroes(actionID, 4);

                    nlohmann::json trigger;

                    for (auto& [keyID, key] : triggersJson[actionIDString].items()) {
                        if ( atoi(keyID.c_str()) == triggerID ) {
                            trigger = key;
                            break;
                        }
                    }
                    bool usinguniquecharge = false;

                    if (trigger["_UseUniqueParam"] == true) {
                        if (!uniqueCharge) {
                            continue;
                        }
                        usinguniquecharge = true;
                    }

                    auto norm = trigger["norm"];
                    int commandNo = norm["command_no"];
                    uint32_t okKeyFlags = norm["ok_key_flags"];
                    uint32_t okCondFlags = norm["ok_key_cond_flags"];
                    uint32_t dcExcFlags = norm["dc_exc_flags"];
                    // condflags..
                    // 10100000000100000: M oicho, but also eg. 22P - any one of three button mask?
                    // 10100000001100000: EX, so any two out of three button mask?
                    // 00100000000100000: heavy punch with one button mask
                    // 00100000001100000: normal throw, two out of two mask t
                    // 00100000010100000: taunt, 6 out of 6 in mask
                    uint32_t i = 0, initialI = 0;
                    bool initialMatch = false;
                    uint32_t initialSearch = globalInputBufferLength + timeInWarudo;
                    if (inputBuffer.size() < initialSearch) {
                        initialSearch = inputBuffer.size();
                    }
                    while (i < initialSearch)
                    {
                        if (okKeyFlags && matchInput(inputBuffer[i], okKeyFlags, okCondFlags, dcExcFlags))
                        {
                            initialMatch = true;
                        } else if (initialMatch == true) {
                            i--;
                            initialI = i;
                            break; // break once initialMatch no longer true, set i on last true
                        }
                        i++;
                    }
                    if (initialMatch)
                    {
                        //  check deferral like heavy donkey into lvl3 doesnt shot hitbox
                        if ( commandNo == -1 ) {
                            if (defer) {
                                deferredAction = actionID;
                                deferredActionFrame = triggerEndFrame;
                            } else {
                                nextAction = actionID;
                            }
                            if ( usinguniquecharge ) {
                                uniqueCharge = 0;
                            }
                            //log("trigger " + actionIDString + " " + triggerIDString + " defer " + std::to_string(defer));
                        } else {
                            std::string commandNoString = to_string_leading_zeroes(commandNo, 2);
                            auto command = commandsJson[commandNoString]["0"];
                            int inputID = command["input_num"].get<int>() - 1;
                            auto commandInputs = command["inputs"];

                            uint32_t inputBufferCursor = i;

                            while (inputID >= 0 )
                            {
                                auto input = commandInputs[to_string_leading_zeroes(inputID, 2)];
                                auto inputNorm = input["normal"];
                                uint32_t inputOkKeyFlags = inputNorm["ok_key_flags"];
                                uint32_t inputOkCondFlags = inputNorm["ok_key_cond_check_flags"];
                                int numFrames = input["frame_num"];
                                bool match = false;
                                int lastMatchInput = i;

                                if (inputOkKeyFlags & 0x10000)
                                {
                                    // charge release
                                    bool chargeMatch = false;
                                    nlohmann::json resourceMatch;
                                    int chargeID = inputOkKeyFlags & 0xFF;
                                    for (auto& [keyID, key] : chargeJson.items()) {
                                        resourceMatch = key["resource"];
                                        if (resourceMatch["charge_id"] == chargeID ) {
                                            chargeMatch = true;
                                            break;
                                        }
                                    }

                                    if (chargeMatch) {
                                        uint32_t inputOkKeyFlags = resourceMatch["ok_key_flags"];
                                        uint32_t inputOkCondFlags = resourceMatch["ok_key_cond_check_flags"];
                                        uint32_t chargeFrames = resourceMatch["ok_frame"];
                                        uint32_t keepFrames = resourceMatch["keep_frame"];
                                        uint32_t dirCount = 0;
                                        uint32_t dirNotMatchCount = 0;
                                        // count matching direction in input buffer, super naive but will work for testing
                                        inputBufferCursor = i;
                                        uint32_t searchArea = inputBufferCursor + chargeFrames + keepFrames;
                                        while (inputBufferCursor < inputBuffer.size() && inputBufferCursor < searchArea)
                                        {
                                            if (matchInput(inputBuffer[inputBufferCursor], inputOkKeyFlags, inputOkCondFlags)) {
                                                dirCount++;
                                                if (dirCount >= chargeFrames) {
                                                    break;
                                                }
                                            } else {
                                                dirNotMatchCount++;
                                            }
                                            inputBufferCursor++;
                                        }

                                        if (dirCount < chargeFrames || (inputBufferCursor - initialI) > (chargeFrames + keepFrames)) {
                                            log("not quite charged " + std::to_string(chargeID) + " dirCount " + std::to_string(dirCount) + " chargeFrame " + std::to_string(chargeFrames) +
                                            "keep frame " + std::to_string(keepFrames) + " beginningCharge " + std::to_string(inputBufferCursor)  + " chargeConsumed " + std::to_string(initialI));
                                            break; // cancel trigger
                                        }
                                        //log("allowed charge " + std::to_string(chargeID) + " dirCount " + std::to_string(dirCount) + " began " + std::to_string(inputBufferCursor) + " consumed " + std::to_string(initialI));
                                        inputID--;
                                    } else {
                                        log("charge entries mismatch?");
                                        break; // cancel trigger
                                    }
                                } else {
                                    while (inputBufferCursor < inputBuffer.size())
                                    {
                                        bool thismatch = false;
                                        if (matchInput(inputBuffer[inputBufferCursor], inputOkKeyFlags, inputOkCondFlags)) {
                                            int spaceSinceLastInput = inputBufferCursor - lastMatchInput;
                                            // if ( commandNo == 32 ) {
                                            //     log(std::to_string(inputID) + " " + std::to_string(inputOkKeyFlags) + " spaceSinceLastInput needed " + std::to_string(numFrames) + " found " + std::to_string(spaceSinceLastInput) +
                                            //     " inputbuffercursor " + std::to_string(inputBufferCursor) + " i " + std::to_string(i) + " initial " + std::to_string(initialSearch));
                                            // }
                                            if (numFrames <= 0 || (spaceSinceLastInput < numFrames)) {
                                                match = true;
                                                thismatch = true;
                                                i = inputBufferCursor;
                                            }
                                        }
                                        if (match == true && (thismatch == false || numFrames <= 0)) {
                                            //inputBufferCursor++;
                                            inputID--;
                                            break;
                                        }
                                        inputBufferCursor++;
                                    }
                                }

                                if (inputBufferCursor == inputBuffer.size()) {
                                    break;
                                }
                            }                           

                            if (inputID < 0) {
                                if (defer) {
                                    deferredAction = actionID;
                                    deferredActionFrame = triggerEndFrame;
                                } else {
                                    nextAction = actionID;
                                }
                                if ( usinguniquecharge ) {
                                    uniqueCharge = 0;
                                }
                                //log("trigger " + actionIDString + " " + triggerIDString + " defer " + std::to_string(defer));
                            }
                        }
                        // specifically don't break here, i think another trigger can have higher priority
                        // walking in reverse and breaking would be smarter :-)
                    }
                }
            }
        }

        if ( deferredActionFrame == currentFrame ) {
            nextAction = deferredAction;

            deferredActionFrame = -1;
            deferredAction = 0;
        }
    }

    for ( auto minion : minions ) {
        minion->PreFrame();
    }

    return true;
}

void Guy::Render(void) {
    for (auto box : renderBoxes) {
        drawHitBox(box.box,box.col,box.drive,box.parry,box.di);
    }

    for ( auto minion : minions ) {
        minion->Render();
    }
}

bool Guy::Push(Guy *pOtherGuy) 
{
    if ( !pOtherGuy ) return false;

    bool hasPushed = false;
    float pushY = 0;
    float pushX = 0;
    for (auto pushbox : pushBoxes ) {
        for (auto otherPushBox : *pOtherGuy->getPushBoxes() ) {
            if (doBoxesHit(pushbox, otherPushBox)) {

                float difference = -(pushbox.x + pushbox.w - otherPushBox.x);
                // log("push " + std::to_string(difference));
                pushX = std::min(difference, pushX);
                hasPushed = true;
            }
        }
    }

    for ( auto minion : minions ) {
        minion->Push(pOtherGuy);
    }

    if ( hasPushed ) {
        posX += pushX;
        posY += pushY;
        return true;
    }

    return false;
}

bool Guy::WorldPhysics(void)
{
    // needs to run after pushboxes computed, after player push?
    bool hasPushed = false;
    float pushX = 0;
    float pushY = 0;
    bool floorpush = false;
    touchedWall = false;
    for (auto pushbox : pushBoxes ) {
        if (pushbox.y < 0) {
            pushY = std::max(-pushbox.y, pushY);
            floorpush = true;
            hasPushed = true;
        }

        if (pushbox.x < 0.0 ) {
            pushX = std::max(-pushbox.x, pushX);
            touchedWall = true;
            hasPushed = true;
        }
        if (pushbox.x + pushbox.w > 500.0f ) {
            float diff = -(pushbox.x + pushbox.w - 500.0f);
            pushX = std::min(diff, pushX);
            touchedWall = true;
            hasPushed = true;
        }
    }

    landed = false;

    // landing - not sure about velocity check there, but otherwise we land during buttslam ascent
    if (floorpush && airborne && velocityY < 0) 
    {
        posY = 0.0f;
        velocityY = 0.0f;
        accelY = 0.0f;
        nextAction = 39; // land - need other landing if did air attack?

        if ( resetHitStunOnLand ) {
            hitStun = 1;
            resetHitStunOnLand = false;
        }

        if ( hitStunOnLand > 0 ) {
            // there's some additional knockdown delay there
            hitStun += hitStunOnLand + 1 + 30; 
            hitStunOnLand = 0;
        }

        airborne = false;
        landed = true;
        //log ("landed " + std::to_string(hitStun));
    }

    for ( auto minion : minions ) {
        minion->WorldPhysics();
    }

    if ( hasPushed ) {
        posX += pushX;
        posY += pushY;
        return true;
    }

    return false;
}

bool Guy::CheckHit(Guy *pOtherGuy)
{
    if ( !pOtherGuy ) return false;

    bool retHit = false;
    for (auto hitbox : hitBoxes ) {
        if (hitbox.hitID < canHitID) {
            continue;
        }
        for (auto hurtbox : *pOtherGuy->getHurtBoxes() ) {
            if (doBoxesHit(hitbox.box, hurtbox)) {
                std::string hitIDString = to_string_leading_zeroes(hitbox.hitEntryID, 3);

                int hitEntryFlag = 0;

                bool otherGuyAirborne = pOtherGuy->posY > 0;

                if (otherGuyAirborne > 0) {
                    hitEntryFlag |= air;
                }
                if (forceCounter && pOtherGuy->comboHits == 0) {
                    hitEntryFlag |= counter;
                }
                if (forcePunishCounter && pOtherGuy->comboHits == 0) {
                    hitEntryFlag |= punish_counter;
                }

                std::string hitEntryFlagString = to_string_leading_zeroes(hitEntryFlag, 2);
                auto hitEntry = hitJson[hitIDString]["param"][hitEntryFlagString];
                int juggleFirst = hitEntry["Juggle1st"];
                int juggleAdd = hitEntry["JuggleAdd"];
                int juggleLimit = hitEntry["JuggleLimit"];
                int targetHitStun = hitEntry["HitStun"];
                int destX = hitEntry["MoveDest"]["x"];
                int destY = hitEntry["MoveDest"]["y"];
                int destTime = hitEntry["MoveTime"];
                int dmgValue = hitEntry["DmgValue"];
                int dmgType = hitEntry["DmgType"];
                int floorTime = hitEntry["FloorTime"];

                if (wasDrive) {
                    juggleAdd = 0;
                    juggleFirst = 0;
                    juggleLimit += 3;
                    // trying like that
                    // supposedly drive attacks dont add to the limit but obey some limit?
                }

                if (otherGuyAirborne && pOtherGuy->juggleCounter > juggleLimit) {
                    break;
                }

                // we're hitting for sure after this point, side effects
                //log("hit! frame " + std::to_string(currentFrame) + " id " + hitIDString + " entry " + hitEntryFlagString);

                // other guy is going airborne, apply juggle
                if (!otherGuyAirborne && destY != 0) {
                    if (pOtherGuy->juggleCounter == 0) {
                        pOtherGuy->juggleCounter = juggleFirst; // ?
                    } else {
                        pOtherGuy->juggleCounter += juggleAdd;
                    }
                }

                // +1 since we don't seem to line up, test with hands, lots of small hits
                // todo need to move this where +1 isn't needed
                int hitStopSelf = hitEntry["HitStopOwner"];
                int hitStopTarget = hitEntry["HitStopTarget"];
                if ( hitStopSelf ) {
                    if (pParent) {
                        pParent->addWarudo(hitStopSelf+1);
                    } else {
                        addWarudo(hitStopSelf+1);
                    }
                }
                if ( hitStopTarget && pOpponent) {
                    pOpponent->addWarudo(hitStopTarget+1);
                }

                if (wasDrive) {
                    targetHitStun+=4;
                }

                // this is set on honda airborne hands
                // free hit until falls to ground - implement properly at some point
                if (dmgType & 8) {
                    targetHitStun += 500000;
                    pOtherGuy->resetHitStunOnLand = true;
                } else {
                    pOtherGuy->resetHitStunOnLand = false;
                }

                pOtherGuy->hitStunOnLand = floorTime;

                if (pOpponent) {
                    int moveType = hitEntry["MoveType"];
                    int curveTargetID = hitEntry["CurveTgtID"];
                    log("hit id " + hitIDString + " destX " + std::to_string(destX) + " destY " + std::to_string(destY) + " moveType " + std::to_string(moveType) + " curveTargetID " + std::to_string(curveTargetID));
                    pOpponent->Hit(targetHitStun, destX, destY, destTime, dmgValue);
                }
                canHitID = hitbox.hitID + 1;
                retHit = true;
                break;
            }
        }
        if (retHit) break;
    }

    for ( auto minion : minions ) {
        if ( minion->CheckHit(pOtherGuy) ) {
            retHit = true;
        }
    }

    return retHit;
}

void Guy::Hit(int stun, int destX, int destY, int destTime, int damage)
{
    comboHits++;
    comboDamage += damage;
    // +1 because i think we're off by one frame where we run this
    hitStun = stun + 1 + hitStunAdder;
    hitVelFrames = destTime;

    // assume hit direction is opposite as facing for now, not sure if that's true
    hitVelX = (direction * destX * -1) / (float)destTime;
    velocityY = destY * 4 / (float)destTime;
    //hitVelY = destY * 2 / (float)destTime;
    //velocityX = (direction * destX * -1) / (float)destTime;
    accelY = destY * -4 / (float)destTime * 2.0 / (float)destTime;

    // i think this vel wants to apply this frame, lame workaround to get same intensity
    velocityY -= accelY; //

    if (destY > 0 ) {
        airborne = true;
    }

    nextAction = 205; // HIT_MM, not sure how to pick which
    if (  destY != 0 ) {

        //nextAction = 253; // 246 if head - we should do a pre-histop trsansition there
        if (destY > destX) {
            nextAction = 230; // 90
        } else if (destX > destY * 2.5) {
            nextAction = 232; // 00
        } else {
            nextAction = 231;
        }
        // if ((curveTargetID & 0x9) == 0x9) {
        //     nextAction = 230; // 90
        // } else if (curveTargetID & 0x1) {
        //     nextAction = 231; // 45
        // } else {
        //     nextAction = 232; // 00
        // }
    }
}

void Guy::DoBranchKey(void)
{
    if (actionJson != nullptr && actionJson.contains("BranchKey"))
    {
        for (auto& [keyID, key] : actionJson["BranchKey"].items())
        {
            if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                continue;
            }

            bool doBranch = false;
            int branchType = key["Type"];
            int64_t branchParam0 = key["Param00"];
            int64_t branchParam1 = key["Param01"];
            int branchAction = key["Action"];

            switch (branchType) {
                case 0: // always?
                    doBranch = true; 
                    break;
                case 2:
                    if (canHitID >= 0) { // has hit ever this move.. not sure if right
                        doBranch = true;
                    }
                    break;
                case 13:
                    if (landed) {
                        doBranch = true;
                    }
                    break;
                case 18:
                    if ((branchParam0 == -2147483647) && (branchParam1 == 2)) {
                        // sign change on vel? used for transitioning between rise and fall
                        if (startsFalling) {
                            doBranch = true;
                        }
                    } else {
                        log("unknown steer branch");
                    }
                    break;
                case 29:
                    if (uniqueCharge) {
                        // probably should check the count somewhere?
                        // probably how jamie drinks work too? not sure
                        doBranch = true;
                    }
                    break;
                case 45:
                    if (isProjectile && projHitCount == 0 ) {
                        // log("hitcount=0 branch");
                        // branch action will reset hitcount?
                        projHitCount = -1;
                        doBranch = true;
                    }
                    break;
                case 31: // todo loop count
                case 47: // todo incapacitated
                    break;
                default:
                    std::string typeName = key["_TypesName"];
                    log("unsupported branch id " + std::to_string(branchType) + " type " + typeName);
                    break;
            }

            // do those also override if higher branchID?
            if (doBranch) {
                nextAction = branchAction;
                keepPlace = key["_KeepPlace"];
            }
        }
    }
}

bool Guy::Frame(void)
{
    currentFrame++;

    if (isProjectile && canHitID >= 0) {
        projHitCount--;
        //log("proj hitcount " + std::to_string(projHitCount));
        canHitID = -1; // re-arm, all projectile hitboxes seem to have hitID 0
    }

    // evaluate branches after the frame bump, branch frames are meant to be elided afaict
    DoBranchKey();

    if (isProjectile && projHitCount == 0) {
        return false; // die
    }

    // do we want to count down while in hitstop? that may be why we need +1
    if (hitStun > 0)
    {
        hitStun--;
        if (hitStun == 0)
        {
            int advantage = globalFrameCount - pOpponent->recoveryTiming;
            log("recovered! adv " + std::to_string(advantage - 1) + " combo hits " + std::to_string(comboHits) + " damage " + std::to_string(comboDamage));
            nextAction = 1;
            comboHits = 0;
            juggleCounter = 0;
            comboDamage = 0;
        }

    }

    if (!hitStun && currentFrame >= (actionFrameDuration - 2) && nextAction == -1)
    {
        if ( isProjectile ) {
            //currentFrame = 0; // just loop? :/
            return false; // die
        } else if ( currentAction == 33 || currentAction == 34 || currentAction == 35 ) {
            // If done with pre-jump, transition to jump
            nextAction = currentAction + 3;
            airborne = true; // probably should get it thru statuskey?
        }
        else {
            nextAction = 1;
        }
    }

    bool canMove = false;
    int actionCheckCanMove = currentAction;
    if (nextAction != -1 ) {
        actionCheckCanMove = nextAction;
    }
    bool movingForward = actionCheckCanMove == 9 || actionCheckCanMove == 10 || actionCheckCanMove == 11;
    bool movingBackward = actionCheckCanMove == 13 || actionCheckCanMove == 14 || actionCheckCanMove == 15;
    if (actionCheckCanMove == 1 || actionCheckCanMove == 2 || actionCheckCanMove == 4 || //stands, crouch
        movingForward || movingBackward) {
        canMove = true;
    }

    if ( marginFrame != -1 && currentFrame >= marginFrame ) {
        canMove = true;
    }

    if (posY > 0.0f) {
        canMove = false;
    }
    if (hitStun) {
        canMove = false;
    }

    bool turnaround = false;

    // Process movement if any
    if ( canMove )
    {
        if ( currentInput & 1 ) {
            if ( currentInput & 4 ) {
                nextAction = 35; // BAS_JUMP_B_START
            } else if ( currentInput & 8 ) {
                nextAction = 34; // BAS_JUMP_F_START
            } else {
                nextAction = 33; // BAS_JUMP_N_START
            }
        } else if ( currentInput & 2 ) {
            nextAction = 4; // BAS_CRH_Loop
        } else {
            if ((currentInput & (32+256)) == 32+256) {
                nextAction = 480; // DPA_STD_START
            } else if ( currentInput & 4 && !movingBackward ) {
                nextAction = 13; // BAS_BACKWARD_START
            } else if ( currentInput & 8 && !movingForward) {
                nextAction = 9; // BAS_FORWARD_START
            }
        }

        if ( currentInput == 0 ) { // only do that if we're not post-margin for correctness
            nextAction = 1;
        }

        if (pOpponent) {
            if ( direction > 0 && getPosX() > pOpponent->getPosX() ) {
                turnaround = true;
            } else if ( direction < 0 && getPosX() < pOpponent->getPosX() ) {
                turnaround = true;
            }
        }
    }

    if ( nextAction == -1 && (currentAction == 480 || currentAction == 481) && (currentInput & (32+256)) != 32+256) {
        nextAction = 482; // DPA_STD_END
    }

    // Transition
    if ( nextAction != -1 )
    {
        if (currentAction != 1 && nextAction == 1) {
            recoveryTiming = globalFrameCount;
            //log("recovered!");
        }
        currentAction = nextAction;
        if (!keepPlace) {
            currentFrame = 0;

            // commit current place offset
            posX += (posOffsetX * direction);
            posOffsetX = 0.0f;
            posY += posOffsetY;
            posOffsetY = 0.0f;
            
            canHitID = -1;
        } else {
            currentFrame--; //rewind
        }
        keepPlace = false;

        nextAction = -1;

        if (turnaround) {
            direction *= -1;
        }

        // if grounded, reset velocities on transition
        // need to test if still needed?
        // probably just on transitioning to standing? not sure
        if ( posY == 0.0 && !isDrive && !hitStun) {
            velocityX = 0;
            velocityY = 0;
            accelX = 0;
            accelY = 0;
        }

        if ( posY > 0.0 && !hitStun ) { // if not grounded, fall to the ground i guess?
            accelY = -1;        
        }

        if (isDrive == true) {
            isDrive = false;
            // at some point make it so we cant drive specials
            wasDrive = true;
        } else {
            wasDrive = false;
        }
    }

    if (warudo == 0) {
        timeInWarudo = 0;
    }

    std::vector<Guy*> minionsNotFinished;
    for ( auto minion : minions ) {
        if (minion->Frame()) {
            minionsNotFinished.push_back(minion);
        } else {
            delete minion;
        }
    }
    minions = minionsNotFinished;

    return true;
}
