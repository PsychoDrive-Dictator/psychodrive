
#include "guy.hpp"
#include "main.hpp"
#include <string>

#include <stdio.h>

#include <fstream>
#include <ios>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <bitset>

void parseRootOffset( nlohmann::json& keyJson, int&offsetX, int& offsetY)
{
    if ( keyJson.contains("RootOffset") ) {
        offsetX = keyJson["RootOffset"]["X"];
        offsetY = keyJson["RootOffset"]["Y"];
    }
}

bool matchInput( int input, uint32_t okKeyFlags, uint32_t okCondFlags, uint32_t dcExcFlags = 0 )
{

    if (dcExcFlags != 0 ) {
        if ((dcExcFlags & input) != dcExcFlags) {
            return false;
        }
    }

    if (okCondFlags & 2) {
        if ((input & 0xF) == (okKeyFlags & 0xF)) {
            return true; // match exact direction
        }
        return false;
    }

    if (okKeyFlags & 0x10000) {
        return true; // allow neutral? really not sure
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

Guy::Guy(std::string charName, float startPosX, float startPosY, int startDir, color color)
{
    character = charName;
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
}

void Guy::Input(int input)
{
    currentInput = input;

    inputBuffer.push_front(input);
    // how much is too much?
    if (inputBuffer.size() > 60) {
        inputBuffer.pop_back();
    }
}

void Guy::PreFrame(void)
{
    auto actionIDString = to_string_leading_zeroes(currentAction, 4);
    bool validAction = namesJson.contains(actionIDString);
    actionName = validAction ? namesJson[actionIDString] : "invalid";

    if (movesDictJson.contains(actionName))
    {
        marginFrame = movesDictJson[actionName]["fab"]["ActionFrame"]["MarginFrame"];
        //standing has 0 marginframe, pre-jump has -1, crouch -1..
        actionFrameDuration = movesDictJson[actionName]["fab"]["Frame"];

        if (movesDictJson[actionName].contains("PlaceKey"))
        {
            for (auto& [placeKeyID, placeKey] : movesDictJson[actionName]["PlaceKey"].items())
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

        velocityX += accelX;
        velocityY += accelY;

        // log(std::to_string(currentAction) + " " + std::to_string(prevVelX) + " " + std::to_string(velocityX));

        if ( (velocityX * prevVelX) < 0.0f || (accelX != 0.0f && velocityX == 0.0f) ) {
            // sign change?
            velocityX = 0.0f;
            accelX = 0.0f;
        }

        float oldPosY = posY;

        posX += (velocityX * direction);
        posY += velocityY;

        // landing
        if ( oldPosY > 0.0f && posY <= 0.0f ) // iffy but we prolly move to fixed point anyway at some point
        {
            posY = 0.0f;
            nextAction = 39; // land - need other landing if did air attack?
        }

        prevVelX = velocityX;

        if (movesDictJson[actionName].contains("SteerKey"))
        {
            for (auto& [steerKeyID, steerKey] : movesDictJson[actionName]["SteerKey"].items())
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

        if (wasDrive && movesDictJson[actionName].contains("DriveSteerKey"))
        {
            for (auto& [steerKeyID, steerKey] : movesDictJson[actionName]["DriveSteerKey"].items())
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

        if (movesDictJson[actionName].contains("EventKey"))
        {
            for (auto& [keyID, key] : movesDictJson[actionName]["EventKey"].items())
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
        if (movesDictJson[actionName].contains("PushCollisionKey"))
        {
            for (auto& [pushBoxID, pushBox] : movesDictJson[actionName]["PushCollisionKey"].items())
            {
                if ( !pushBox.contains("_StartFrame") || pushBox["_StartFrame"] > currentFrame || pushBox["_EndFrame"] <= currentFrame ) {
                    continue;
                }
                int rootOffsetX = 0;
                int rootOffsetY = 0;
                parseRootOffset( pushBox, rootOffsetX, rootOffsetY );
                rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
                rootOffsetY += posY + posOffsetY;

                drawRectsBox( rectsJson, 5, pushBox["BoxNo"],rootOffsetX, rootOffsetY, direction, {1.0,1.0,1.0});
            }
        }
        if (movesDictJson[actionName].contains("DamageCollisionKey"))
        {
            for (auto& [hurtBoxID, hurtBox] : movesDictJson[actionName]["DamageCollisionKey"].items())
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

                for (auto& [boxNumber, boxID] : hurtBox["HeadList"].items()) {
                    drawRectsBox( rectsJson, 8, boxID,rootOffsetX, rootOffsetY,direction,{charColorR,charColorG,charColorB},drive,parry,di);
                }
                for (auto& [boxNumber, boxID] : hurtBox["BodyList"].items()) {
                    drawRectsBox( rectsJson, 8, boxID,rootOffsetX, rootOffsetY,direction,{charColorR,charColorG,charColorB},drive,parry,di);
                }
                for (auto& [boxNumber, boxID] : hurtBox["LegList"].items()) {
                    drawRectsBox( rectsJson, 8, boxID,rootOffsetX, rootOffsetY,direction,{charColorR,charColorG,charColorB},drive,parry,di);
                }
                for (auto& [boxNumber, boxID] : hurtBox["ThrowList"].items()) {
                    drawRectsBox( rectsJson, 7, boxID,rootOffsetX, rootOffsetY,direction,{0.75,0.75,0.65} );
                }
            }
        }

        if (movesDictJson[actionName].contains("AttackCollisionKey"))
        {
            for (auto& [hitBoxID, hitBox] : movesDictJson[actionName]["AttackCollisionKey"].items())
            {
                if ( !hitBox.contains("_StartFrame") || hitBox["_StartFrame"] > currentFrame || hitBox["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int rootOffsetX = 0;
                int rootOffsetY = 0;
                parseRootOffset( hitBox, rootOffsetX, rootOffsetY );
                rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
                rootOffsetY += posY + posOffsetY;

                for (auto& [boxNumber, boxID] : hitBox["BoxList"].items()) {
                    if (hitBox["CollisionType"] == 3) {
                        drawRectsBox( rectsJson, 3, boxID,rootOffsetX, rootOffsetY,direction,{0.5,0.5,0.5});
                    } else if (hitBox["CollisionType"] == 0) {
                        drawRectsBox( rectsJson, 0, boxID,rootOffsetX, rootOffsetY,direction,{1.0,0.0,0.0}, isDrive || wasDrive );
                    }
                }                    
            }
        }

        if (movesDictJson[actionName].contains("BranchKey"))
        {
            for (auto& [keyID, key] : movesDictJson[actionName]["BranchKey"].items())
            {
                if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                if ((key["Type"] == 29) && uniqueCharge) {
                    // probably should check the count somewhere?
                    // probably how jamie drinks work too? not sure
                    nextAction = key["Action"];
                }

                if (key["Type"] == 0) //always?
                {
                    nextAction = key["Action"];
                    // do those also override if higher branchID?
                }
            }
        }

        // should this fall through and let triggers also happen? prolly

        if (movesDictJson[actionName].contains("TriggerKey"))
        {
            for (auto& [keyID, key] : movesDictJson[actionName]["TriggerKey"].items())
            {
                if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                bool defer = !key["_NotDefer"];
                int triggerEndFrame = key["_EndFrame"];

                auto triggerGroupString = to_string_leading_zeroes(key["TriggerGroup"], 3);
                for (auto& [keyID, key] : triggerGroupsJson[triggerGroupString].items())
                {
                    int triggerID = atoi(keyID.c_str());
                    std::string actionString = key;
                    int actionID = atoi(actionString.substr(0, actionString.find(" ")).c_str());

                    auto triggerIDString = std::to_string(triggerID);
                    auto actionIDString = to_string_leading_zeroes(actionID, 4);

                    auto trigger = triggersJson[actionIDString][triggerIDString];
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
                    if (okKeyFlags && matchInput(currentInput, okKeyFlags, okCondFlags, dcExcFlags))
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
                        } else {
                            std::string commandNoString = to_string_leading_zeroes(commandNo, 2);
                            auto command = commandsJson[commandNoString]["0"];
                            int chargeBit = command["charge_bit"];
                            int inputID = command["input_num"].get<int>() - 1;
                            auto commandInputs = command["inputs"];

                            // check charge
                            if (chargeBit)
                            {
                                bool chargeMatch = false;
                                nlohmann::json resourceMatch;
                                for (auto& [keyID, key] : chargeJson.items()) {
                                    resourceMatch = key["resource"];
                                    if (resourceMatch["group_bit"] == chargeBit) {
                                        chargeMatch = true;
                                        break;
                                    }
                                }

                                if (chargeMatch) {
                                    uint32_t inputOkKeyFlags = resourceMatch["ok_key_flags"];
                                    uint32_t inputOkCondFlags = resourceMatch["ok_key_cond_check_flags"];
                                    int chargeFrames = resourceMatch["ok_frame"];
                                    // int keepFrames = resourceMatch["keep_frame"];
                                    int dirCount = 0;
                                    int dirNotMatchCount = 0;
                                    // count matching direction in input buffer, super naive but will work for testing
                                    uint32_t inputBufferCursor = 0;
                                    while (inputBufferCursor < inputBuffer.size())
                                    {
                                        if (matchInput(inputBuffer[inputBufferCursor], inputOkKeyFlags, inputOkCondFlags)) {
                                            dirCount++;
                                        } else {
                                            dirNotMatchCount++;
                                        }
                                        inputBufferCursor++;
                                    }

                                    if (dirCount < chargeFrames) {
                                        log ("match charge " + std::to_string(chargeBit) + " dirCount " + std::to_string(dirCount) + " chargeFrame " + std::to_string(chargeFrames));
                                        break; // cancel trigger
                                    }
                                }
                            }

                            uint32_t inputBufferCursor = 0;

                            while (inputID >= 0 )
                            {
                                auto input = commandInputs[to_string_leading_zeroes(inputID, 2)]["normal"];
                                // does 0x40000000 mean neutral?
                                uint32_t inputOkKeyFlags = input["ok_key_flags"];
                                uint32_t inputOkCondFlags = input["ok_key_cond_check_flags"];
                                // condflags..
                                // 10100000000100000: M oicho, but also eg. 22P - any one of three button mask?
                                // 10100000001100000: EX, so any two out of three button mask?
                                // 00100000000100000: heavy punch with one button mask
                                // 00100000001100000: normal throw, two out of two mask t
                                // 00100000010100000: taunt, 6 out of 6 in mask
                                while (inputBufferCursor < inputBuffer.size())
                                {
                                    if (matchInput(inputBuffer[inputBufferCursor++], inputOkKeyFlags, inputOkCondFlags)) {
                                        inputID--;
                                        break;
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
}

void Guy::Frame(void)
{
    currentFrame++;

    // evaluate branches after the frame bump, branch frames are meant to be elided afaict
    if (movesDictJson[actionName].contains("BranchKey"))
    {
        for (auto& [keyID, key] : movesDictJson[actionName]["BranchKey"].items())
        {
            if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                continue;
            }

            if (key["Type"] == 0) //always?
            {
                nextAction = key["Action"];
                // do those also override if higher branchID?
            }
        }
    }

    if (currentFrame >= actionFrameDuration && nextAction == -1)
    {
        if ( currentAction == 33 || currentAction == 34 || currentAction == 35 ) {
            // If done with pre-jump, transition to jump
            nextAction = currentAction + 3;
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
        currentAction = nextAction;
        currentFrame = 0;

        // commit current place offset
        posX += (posOffsetX * direction);
        posOffsetX = 0.0f;
        posY += posOffsetY;
        posOffsetY = 0.0f;

        nextAction = -1;

        if (turnaround) {
            direction *= -1;
        }

        // if grounded, reset velocities on transition
        // need to test if still needed?
        if ( posY == 0.0 && !isDrive) {
            velocityX = 0;
            velocityY = 0;
            accelX = 0;
            accelY = 0;
        }
        if (currentAction == 500 || currentAction == 501 ||
            currentAction == 739 || currentAction == 740) {
            isDrive = true;
        } else if (isDrive == true) {
            isDrive = false;
            wasDrive = true;
        } else {
            wasDrive = false;
        }
    }
}
