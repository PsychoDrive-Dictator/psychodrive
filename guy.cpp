
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
    hitJson = parse_json_file(character + "_hit.json");
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

        if (hitVelFrames > 0) {
            posX += hitVelX;
            posY += hitVelY;
            hitVelFrames--;
        }

        // landing
        if ( oldPosY > 0.0f && posY <= 0.0f ) // iffy but we prolly move to fixed point anyway at some point
        {
            posY = 0.0f;
            velocityY = 0.0f;
            accelY = 0.0f;
            nextAction = 39; // land - need other landing if did air attack?
        }

        // super crude corner snapping
        if ( posX < 20.0 ) {
            posX = 20.0f;
        }
        if ( posX > 800.0f ) {
            posX = 800.0f;
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

        if (movesDictJson[actionName].contains("WorldKey"))
        {
            bool tokiToTomare = false;
            for (auto& [keyID, key] : movesDictJson[actionName]["WorldKey"].items())
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
                    pOpponent->addWarudo(key["_StartFrame"].get<int>() - currentFrame + 1);
                }
            }
        }

        pushBoxes.clear();
        hitBoxes.clear();
        hurtBoxes.clear();
        renderBoxes.clear();

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

                Box rect;

                for (auto& [boxNumber, boxID] : hurtBox["HeadList"].items()) {
                    if (getRect(rect, rectsJson, 8, boxID,rootOffsetX, rootOffsetY,direction)) {
                        hurtBoxes.push_back(rect);
                        renderBoxes.push_back({rect, {charColorR,charColorG,charColorB}, drive,parry,di});
                    }
                }
                for (auto& [boxNumber, boxID] : hurtBox["BodyList"].items()) {
                    if (getRect(rect, rectsJson, 8, boxID,rootOffsetX, rootOffsetY,direction)) {
                        hurtBoxes.push_back(rect);
                        renderBoxes.push_back({rect, {charColorR,charColorG,charColorB}, drive,parry,di});
                    }
                }
                for (auto& [boxNumber, boxID] : hurtBox["LegList"].items()) {
                    if (getRect(rect, rectsJson, 8, boxID,rootOffsetX, rootOffsetY,direction)) {
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

                Box rect;

                for (auto& [boxNumber, boxID] : hitBox["BoxList"].items()) {
                    if (hitBox["CollisionType"] == 3) {
                        if (getRect(rect, rectsJson, 3, boxID,rootOffsetX, rootOffsetY,direction)) {
                            renderBoxes.push_back({rect, {0.5,0.5,0.5}});
                        }
                    } else if (hitBox["CollisionType"] == 0) {
                        if (getRect(rect, rectsJson, 0, boxID,rootOffsetX, rootOffsetY,direction)) {
                            int hitEntryID = hitBox["AttackDataListIndex"];
                            int hitID = hitBox["HitID"];
                            hitBoxes.push_back({rect,hitEntryID,hitID});
                            renderBoxes.push_back({rect, {1.0,0.0,0.0}, isDrive || wasDrive});
                        }
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
                        } else {
                            std::string commandNoString = to_string_leading_zeroes(commandNo, 2);
                            auto command = commandsJson[commandNoString]["0"];
                            int chargeBit = command["charge_bit"];
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
                                while (inputBufferCursor < inputBuffer.size())
                                {
                                    bool thismatch = false;
                                    if (matchInput(inputBuffer[inputBufferCursor], inputOkKeyFlags, inputOkCondFlags)) {
                                        int spaceSinceLastInput = inputBufferCursor - lastMatchInput;
                                        // if ( commandNo == 7 ) {
                                        //     log(std::to_string(inputID) + " " + std::to_string(inputOkKeyFlags) + " spaceSinceLastInput needed " + std::to_string(numFrames) + " found " + std::to_string(spaceSinceLastInput) +
                                        //     " inputbuffercursor " + std::to_string(inputBufferCursor) + " i " + std::to_string(i));
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

                                if (inputBufferCursor == inputBuffer.size()) {
                                    break;
                                }
                            }

                            // check charge
                            if (inputID < 0 && chargeBit)
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
                                        log ("match charge " + std::to_string(chargeBit) + " dirCount " + std::to_string(dirCount) + " chargeFrame " + std::to_string(chargeFrames));
                                        continue; // cancel trigger
                                    }
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

    return true;
}

void Guy::Render(void) {
    for (auto box : renderBoxes) {
        drawHitBox(box.box,box.col,box.drive,box.parry,box.di);
    }
}

bool Guy::Push(Guy *pOtherGuy) 
{
    for (auto pushbox : pushBoxes ) {
        for (auto otherPushBox : *pOtherGuy->getPushBoxes() ) {
            if (doBoxesHit(pushbox, otherPushBox)) {

                int difference = pushbox.x + pushbox.w - otherPushBox.x;
                // log("push " + std::to_string(difference));
                posX -= difference;
                return true;
            }
        }
    }

    return false;
}

bool Guy::CheckHit(Guy *pOtherGuy)
{
    for (auto hitbox : hitBoxes ) {
        if (hitbox.hitID < canHitID) {
            continue;
        }
        for (auto hurtbox : *pOtherGuy->getHurtBoxes() ) {
            if (doBoxesHit(hitbox.box, hurtbox)) {
                std::string hitIDString = to_string_leading_zeroes(hitbox.hitEntryID, 3);
                auto hitEntry = hitJson[hitIDString]["common"]["0"];
                int hitStun = hitEntry["HitStun"];
                int destX = hitEntry["MoveDest"]["x"];
                int destY = hitEntry["MoveDest"]["y"];
                int destTime = hitEntry["MoveTime"];

                // 2X seems to line up with the real game, also +1 since we don't seem to line up
                // test with hands, lots of small hits
                int hitStopSelf = hitEntry["HitStopOwner"];
                int hitStopTarget = hitEntry["HitStopTarget"];
                if ( hitStopSelf ) {
                    addWarudo(hitStopSelf+1);
                }
                if ( hitStopTarget ) {
                    pOpponent->addWarudo(hitStopTarget+1);
                }

                if (wasDrive) {
                    hitStun+=4;
                }
                pOpponent->Hit(hitStun, destX, destY, destTime);
                canHitID = hitbox.hitID + 1;
                return true;
            }
        }
    }
    return false;
}

void Guy::Hit(int stun, int destX, int destY, int destTime)
{
    comboHits++;
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

    nextAction = 205; // HIT_MM, not sure how to pick which
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

            bool branch = false;

            if (key["Type"] == 0) //always?
            {
                branch = true;
            } else if (key["Type"] == 2) // on hit
            {
                if (canHitID > 0) { // has hit ever this move.. not sure if right
                    branch = true;
                }
            }

            // do those also override if higher branchID?
            if (branch) {
                nextAction = key["Action"];
                keepPlace = key["_KeepPlace"];
            }
        }
    }

    if (hitStun > 0)
    {
        hitStun--;
        if (hitStun == 0)
        {
            nextAction = 1;
            comboHits = 0;
        }
    }

    if (warudo > 0)
    {
        warudo--;
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

    if (posY > 0.0f) {
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

    if (warudo == 0) {
        timeInWarudo = 0;
    }
}
