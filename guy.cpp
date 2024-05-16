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

void parseRootOffset( nlohmann::json& keyJson, float&offsetX, float& offsetY)
{
    if ( keyJson.contains("RootOffset") && keyJson["RootOffset"].contains("X") && keyJson["RootOffset"].contains("Y") ) {
        offsetX = keyJson["RootOffset"]["X"].get<int>();
        offsetY = keyJson["RootOffset"]["Y"].get<int>();
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

    // commands can have only exc/inc flags?
    if (okKeyFlags == 0 && okCondFlags == 0) {
        return true;
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
    } else if ((okCondFlags & 0x60) == 0x60) {
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

bool getRect(Box &outBox, nlohmann::json rectsJson, int rectsPage, int boxID,  float offsetX, float offsetY, int dir)
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

void Guy::BuildMoveList()
{
    // for UI dropdown selector
    vecMoveList.push_back(strdup("1 no action (obey input)"));
    auto triggerGroupString = to_string_leading_zeroes(0, 3);
    for (auto& [keyID, key] : triggerGroupsJson[triggerGroupString].items())
    {
        std::string actionString = key;
        vecMoveList.push_back(strdup(actionString.c_str()));
    }

    for (auto& [keyID, key] : movesDictJson.items())
    {
        int actionID = key["fab"]["ActionID"];
        int styleID = 0;
        if (key.contains("_PL_StyleID")) {
            styleID = key["_PL_StyleID"];
        }
        mapMoveStyle[actionID] = styleID;
    }
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
    if (input == 0 && inputOverride != 0) {
        input = inputOverride;
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
nlohmann::json Guy::commonAtemiJson = nullptr;

void Guy::UpdateActionData(void)
{
    landingAdjust = 0;

    auto actionIDString = to_string_leading_zeroes(currentAction, 4);
    commonAction = false;
    actionJson = nullptr;

    if (opponentAction) {
        bool validAction = pOpponent->namesJson.contains(actionIDString);
        actionName = validAction ? pOpponent->namesJson[actionIDString] : "invalid";

        actionJson = pOpponent->movesDictJson[actionName];
    } else {
        bool validAction = namesJson.contains(actionIDString);
        actionName = validAction ? namesJson[actionIDString] : "invalid";

        if (commonMovesJson.contains(std::to_string(currentAction))) {
            actionJson = commonMovesJson[std::to_string(currentAction)];
            commonAction = true;
        } else if (movesDictJson.contains(actionName)) {
            actionJson = movesDictJson[actionName];
        }
    }

    if (!actionFrameDataInitialized) {
        //standing has 0 marginframe, pre-jump has -1, crouch -1..
        auto fab = actionJson["fab"];
        mainFrame = fab["ActionFrame"]["MainFrame"];
        followFrame = fab["ActionFrame"]["FollowFrame"];
        marginFrame = fab["ActionFrame"]["MarginFrame"];
        actionFrameDuration = fab["Frame"];
        loopPoint = fab["State"]["EndStateParam"];
        if ( loopPoint == -1 ) {
            loopPoint = 0;
        }
        loopCount = fab["State"]["LoopCount"];
        hasLooped = false;
        actionFrameDataInitialized = true;
    }
}

bool Guy::PreFrame(void)
{
    if (warudo > 0) {
        timeInWarudo++;
        warudo--;
        // increment the frame we skipped at the beginning of warudo
        if (warudo == 0) {
            Frame();
        }
    }
    if (warudo > 0) {
        return false;
    }

    hitThisFrame = false;
    grabbedThisFrame = false;
    blocked = false;
    beenHitThisFrame = false;
    landed = false;
    pushBackThisFrame = 0.0f;

    if (actionJson != nullptr)
    {
        if (isProjectile) {
            auto pdataJson = actionJson["pdata"];
            if (projHitCount == -1) {
                projHitCount = pdataJson["HitCount"];
                // log("initial hitcount " + std::to_string(projHitCount));
            }

            limitShotCategory = pdataJson["Category"];
            noPush = pdataJson["_NoPush"];
        } else {
            noPush = false; // might be overridden below
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
                            posOffsetX = offset.get<float>();
                        } else if (placeKey["Axis"] == 1) {
                            posOffsetY = offset.get<float>();
                        }
                    }
                }
            }
        }

        float prevVelX = velocityX;
        float prevPosY = posY;

        // don't apply steer for looped anims? or just velocity set?
        // this is just a hack to have both looping working jumps but
        // i'm sure this will be replace by something proper
        if (!hasLooped && actionJson.contains("SteerKey"))
        {
            for (auto& [steerKeyID, steerKey] : actionJson["SteerKey"].items())
            {
                if ( !steerKey.contains("_StartFrame") || steerKey["_StartFrame"] > currentFrame || steerKey["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int operationType = steerKey["OperationType"];
                int valueType = steerKey["ValueType"];
                float fixValue = steerKey["FixValue"];
                float targetOffsetX = steerKey["FixTargetOffsetX"];
                float targetOffsetY = steerKey["FixTargetOffsetY"];
                int shotCategory = steerKey["_ShotCategory"];
                int targetType = steerKey["TarType"];

                switch (operationType) {
                    case 1:
                    case 2:
                        switch (valueType) {
                            case 0: doSteerKeyOperation(velocityX, fixValue,operationType); break;
                            case 1: doSteerKeyOperation(velocityY, fixValue,operationType); break;
                            case 3: doSteerKeyOperation(accelX, fixValue,operationType); break;
                            case 4: doSteerKeyOperation(accelY, fixValue,operationType); break;
                        }
                        break;
                    case 6:
                        // uhhhhh
                        if (valueType == 3 && steerKey["_EndFrame"] == currentFrame + 1 && airborne && !landed) {
                            currentFrame--;
                            log(logTransitions, "freezing time until landing!");
                        }
                        break;
                    case 13:
                        if (targetType == 16) {
                            // teleport to projectile, i think
                            bool minionFound = false;
                            for ( auto minion : minions ) {
                                if (shotCategory & (1 << minion->limitShotCategory)) {
                                    minionFound = true;
                                    posX = minion->posX + targetOffsetX;
                                    posY = minion->posY + targetOffsetY;
                                    if (posY > 0.0) {
                                        airborne = true;
                                    }
                                    break;
                                }
                            }
                            if (!minionFound) {
                                log(true, "minion to teleport not found");
                            }
                        } else {
                            log(logUnknowns, "unknown teleport?");
                        }
                        break;
                    default:
                        log(logUnknowns, "unknown steer keyoperation " + std::to_string(operationType));
                        break;
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

        prevVelX = velocityX;
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

        posX += (velocityX * direction);
        posY += velocityY;

        if (hitVelX != 0.0f && !beenHitThisFrame) {
            float prevHitVelX = hitVelX;
            hitVelX += hitAccelX;
            if ((hitVelX * prevHitVelX) < 0.0f || (hitAccelX != 0.0f && hitVelX == 0.0f)) {
                hitAccelX = 0.0f;
                hitVelX = 0.0f;
            }

            posX += hitVelX;
            pushBackThisFrame = hitVelX;
        }

        if (prevPosY == 0.0f && posY > 0.0f) {
            airborne = true; // i think we should go by statusKey instead?
        }
        if (prevPosY > 0.0f && posY == 0.0f) {
            airborne = false;
            landed = true;
        }

        counterState = false;
        punishCounterState = false;

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
                if (flag & 0x800000) {
                    punishCounterState = true;
                }
                if (flag & 0x2) {
                    counterState = true;
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

                int eventType = key["Type"];
                int64_t param1 = key["Param01"];
                int64_t param2 = key["Param02"];
                int64_t param3 = key["Param03"];
                int64_t param4 = key["Param04"];
                int64_t param5 = key["Param05"];

                switch (eventType)
                {
                    case 0:
                        {
                            float steerForward = fixedToFloat(param2);
                            float steerBackward = fixedToFloat(param3);

                            if (currentInput & FORWARD) {
                                posX += steerForward;
                                log(true, "steerForward " + std::to_string(steerForward));
                            } else if (currentInput & BACK) {
                                posX += steerBackward;
                                log(true, "steerBackward " + std::to_string(steerBackward));
                            }
                        }
                        break;
                    case 1:
                        log(logUnknowns, "mystery system event 1, flags " + std::to_string(param1));
                        break;
                    case 2:
                        if (param1 == 0) {
                            styleInstall = param2;
                            // what's param3? it's 1 for solid puncher..
                            styleInstallFrames = param4;
                        } else if (param1 == 1) {
                            if (styleInstall > 0) {
                                styleInstallFrames += param2;
                            } else {
                                log(true, "no style install but point deduction?");
                            }
                        } else {
                            log(logUnknowns, "event type 2 param1 " + std::to_string(param1));
                        }
                        break;
                    case 7:
                        // honda spirit buff, param345 are 1
                        // mini sonic break, 234 are 1, 5 is 3
                        // maaaaaybe
                        if (param3 == 0) { // set
                            uniqueCharge = param4;
                        } else if (param3 == 1) { //add?
                            uniqueCharge += param4;
                            // this feels like a horrible workaround but so far works for
                            // everything - without it honda hands can charge to 2/3/4 and break
                            if (param2 == 0 && uniqueCharge > param5) {
                                uniqueCharge = param5;
                            }
                        }
                        break;
                    default:
                        log(logUnknowns, "unhandled event, type " + std::to_string(eventType));
                    case 11:
                    case 5: //those are kinda everywhere, esp 11
                        break;
                }
            }
        }

        if (countingDownInstall && styleInstallFrames) {
            styleInstallFrames--;

            if ( styleInstallFrames < 0) {
                styleInstallFrames = 0;
                countingDownInstall = false;
            }

            if (styleInstallFrames == 0) {
                styleInstall = 0;
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
                Guy *pNewGuy = new Guy(pParent ? *pParent : *this, posOffsetX, posOffsetY, key["ActionId"].get<int>());
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

        if (actionJson.contains("LockKey"))
        {
            for (auto& [keyID, key] : actionJson["LockKey"].items())
            {
                if (!key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame) {
                    continue;
                }

                int type = key["Type"];
                int param01 = key["Param01"];
                int param02 = key["Param02"];

                if (type == 1) {
                    if (pOpponent) {
                        pOpponent->nextAction = param01;
                        pOpponent->nextActionOpponentAction = true;
                        // test
                        pOpponent->direction = direction;
                        pOpponent->posX = posX;
                        pOpponent->posY = posY;
                    }
                } else if (type == 2) {
                    // apply hit DT param 02
                    std::string hitIDString = to_string_leading_zeroes(param02, 3);
                    auto hitEntry = hitJson[hitIDString]["common"]["0"]; // going by crowd wisdom there
                    if (pOpponent) {
                        pOpponent->ApplyHitEffect(hitEntry, false, true, false, false);
                    }
                }
            }
        }
        
        // steer/etc could have had side effects there
        UpdateBoxes();
    }

    for ( auto minion : minions ) {
        minion->PreFrame();
    }

    return true;
}

void Guy::DoTriggers()
{
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
            // but not 5135
            int condition = key["_Condition"];
            if ( (condition == 5199 || condition == 5131) && canHitID == -1) {
                continue;
            }

            auto triggerGroupString = to_string_leading_zeroes(key["TriggerGroup"], 3);
            std::deque<std::pair<std::string,std::string>> vecReverseTriggers;
            for (auto& [keyID, key] : triggerGroupsJson[triggerGroupString].items())
            {
                vecReverseTriggers.push_front(std::make_pair(keyID, key));
            }
            for (auto& [keyID, key] : vecReverseTriggers)
            {
                int triggerID = atoi(keyID.c_str());
                std::string actionString = key;
                int actionID = atoi(actionString.substr(0, actionString.find(" ")).c_str());

                if (mapMoveStyle[actionID] != 0 && mapMoveStyle[actionID] != styleInstall ) {
                    continue;
                }

                auto triggerIDString = std::to_string(triggerID);
                auto actionIDString = to_string_leading_zeroes(actionID, 4);

                nlohmann::json trigger;

                for (auto& [keyID, key] : triggersJson[actionIDString].items()) {
                    if ( atoi(keyID.c_str()) == triggerID ) {
                        trigger = key;
                        break;
                    }
                }

                if (trigger["_UseUniqueParam"] == true) {
                    int op = trigger["cond_param_ope"];
                    int value = trigger["cond_param_value"];
                    if (op == 0 && value != uniqueCharge ) {
                        continue;
                    }
                    if (op == 5 && value > uniqueCharge ) {
                        continue;
                    }
                }
                int limitShotCount = trigger["cond_limit_shot_num"];
                if (limitShotCount) {
                    int count = 0;
                    int limitShotCategory = trigger["limit_shot_category"];
                    for ( auto minion : minions ) {
                        if (limitShotCategory & (1 << minion->limitShotCategory)) {
                            count++;
                        }
                    }
                    if (count >= limitShotCount) {
                        continue;
                    }
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
                    // guile 1112 has 0s everywhere
                    if ((okKeyFlags || dcExcFlags) && matchInput(inputBuffer[i], okKeyFlags, okCondFlags, dcExcFlags))
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
                        log(logTriggers, "trigger " + actionIDString + " " + triggerIDString + " defer " + std::to_string(defer));
                        break; // we found our trigger walking back, blow up the whole group
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
                                        //log("not quite charged " + std::to_string(chargeID) + " dirCount " + std::to_string(dirCount) + " chargeFrame " + std::to_string(chargeFrames) +
                                        //"keep frame " + std::to_string(keepFrames) + " beginningCharge " + std::to_string(inputBufferCursor)  + " chargeConsumed " + std::to_string(initialI));
                                        break; // cancel trigger
                                    }
                                    //log("allowed charge " + std::to_string(chargeID) + " dirCount " + std::to_string(dirCount) + " began " + std::to_string(inputBufferCursor) + " consumed " + std::to_string(initialI));
                                    inputID--;
                                } else {
                                    log(true, "charge entries mismatch?");
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
                            // try to consume the input
                            inputBuffer[initialI] &= ~(okKeyFlags+dcExcFlags);
                            log(logTriggers, "trigger " + actionIDString + " " + triggerIDString + " defer " + std::to_string(defer) + "initial I " + std::to_string(initialI));
                            break; // we found our trigger walking back, blow up the whole group
                        }
                    }
                }
            }
        }
    }
}

void Guy::UpdateBoxes(void)
{
    pushBoxes.clear();
    hitBoxes.clear();
    hurtBoxes.clear();
    renderBoxes.clear();
    throwBoxes.clear();
    armorBoxes.clear();

    if (actionJson.contains("DamageCollisionKey"))
    {
        for (auto& [hurtBoxID, hurtBox] : actionJson["DamageCollisionKey"].items())
        {
            if ( !hurtBox.contains("_StartFrame") || hurtBox["_StartFrame"] > currentFrame || hurtBox["_EndFrame"] <= currentFrame ) {
                continue;
            }

            bool isArmor = hurtBox["_isArm"];
            int armorID = hurtBox["AtemiDataListIndex"];

            float rootOffsetX = 0;
            float rootOffsetY = 0;
            parseRootOffset( hurtBox, rootOffsetX, rootOffsetY );
            rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
            rootOffsetY += posY + posOffsetY;

            bool drive = isDrive || wasDrive;
            bool parry = currentAction >= 480 && currentAction <= 489;
            // doesn't work for all chars, prolly need to find a system bit like drive
            bool di = currentAction >= 850 && currentAction <= 859;

            Box rect;
            int magicHurtBoxID = 8; // i hate you magic array of boxes

            auto rects = commonAction ? commonRectsJson : rectsJson;

            std::vector<HurtBox> boxes;

            for (auto& [boxNumber, boxID] : hurtBox["HeadList"].items()) {
                if (getRect(rect, rects, magicHurtBoxID, boxID,rootOffsetX, rootOffsetY,direction)) {
                    boxes.push_back({rect, true, false, false});
                }
            }
            for (auto& [boxNumber, boxID] : hurtBox["BodyList"].items()) {
                if (getRect(rect, rects, magicHurtBoxID, boxID,rootOffsetX, rootOffsetY,direction)) {
                    boxes.push_back({rect, false, true, false});
                }
            }
            for (auto& [boxNumber, boxID] : hurtBox["LegList"].items()) {
                if (getRect(rect, rects, magicHurtBoxID, boxID,rootOffsetX, rootOffsetY,direction)) {
                    boxes.push_back({rect, false, false, true});
                }
            }

            for (auto box : boxes) {
                if (isArmor) {
                    armorBoxes.push_back({box, armorID});
                    renderBoxes.push_back({box.box, {0.8,0.5,0.0}, drive,parry,di});
                } else {
                    hurtBoxes.push_back(box);
                    renderBoxes.push_back({box.box, {charColorR,charColorG,charColorB}, drive,parry,di});
                }
            }

            for (auto& [boxNumber, boxID] : hurtBox["ThrowList"].items()) {
                if (getRect(rect, rects, 7, boxID,rootOffsetX, rootOffsetY,direction)) {
                    throwBoxes.push_back(rect);
                    renderBoxes.push_back({rect, {0.15,0.20,0.8}, drive,parry,di});
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
            float rootOffsetX = 0;
            float rootOffsetY = 0;
            parseRootOffset( pushBox, rootOffsetX, rootOffsetY );
            rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
            rootOffsetY += posY + posOffsetY;

            Box rect;
            auto rects = commonAction ? commonRectsJson : rectsJson;

            if (getRect(rect, rects, 5, pushBox["BoxNo"],rootOffsetX, rootOffsetY, direction)) {
                pushBoxes.push_back(rect);
                renderBoxes.push_back({rect, {0.8,0.7,0.0}});            
            } else if (getRect(rect, commonRectsJson, 5, pushBox["BoxNo"],rootOffsetX, rootOffsetY, direction)) {
                pushBoxes.push_back(rect);
                renderBoxes.push_back({rect, {0.8,0.7,0.0}});
            }
        }
    }

    DoHitBoxKey("AttackCollisionKey");
    DoHitBoxKey("OtherCollisionKey");

    for ( auto minion : minions ) {
        minion->UpdateBoxes();
    }
}

void Guy::Render(void) {
    for (auto box : renderBoxes) {
        drawHitBox(box.box,box.col,box.drive,box.parry,box.di);
    }

    float x = posX + (posOffsetX * direction);
    float y = posY + posOffsetY;

    drawQuad(x - 8, y - 8, 16, 16, charColorR,charColorG,charColorB,0.8);
    drawLoop(x - 8, y - 8, 16, 16, 1.0,1.0,1.0,1.0);

    for ( auto minion : minions ) {
        minion->Render();
    }
}

bool Guy::Push(Guy *pOtherGuy)
{
    if ( !pOtherGuy ) return false;

    bool hasPushed = false;
    touchedOpponent = false;
    float pushY = 0;
    float pushX = 0;
    for (auto pushbox : pushBoxes ) {

        if (noPush) break;

        for (auto otherPushBox : *pOtherGuy->getPushBoxes() ) {
            if (doBoxesHit(pushbox, otherPushBox)) {

                float difference = -(pushbox.x + pushbox.w - otherPushBox.x);
                // log("push " + std::to_string(difference));
                pushX = fminf(difference, pushX);
                hasPushed = true;
            }
        }
    }

    for ( auto minion : minions ) {
        minion->Push(pOtherGuy);
    }

    if ( hasPushed ) {
        touchedOpponent = true; // could be touching anyone really but can fix later
        posX += pushX;
        posY += pushY;
        UpdateBoxes();
        return true;
    }

    return false;
}

bool Guy::WorldPhysics(void)
{
    bool hasPushed = false;
    float pushX = 0;
    float pushY = 0;
    bool floorpush = false;
    touchedWall = false;

    const float wallDistance = 765.0;
    const float maxPlayerDistance = 490.0;

    if (!noPush) {
        // Floor

        if (posY - landingAdjust < 0) {
            //log("floorpush pos");
            pushY = -posY;
            floorpush = true;
            hasPushed = true;
        }

        // Walls

        float x = getPosX();
        if (x < -wallDistance ) {
            pushX = -(x - -wallDistance);
            touchedWall = true;
            hasPushed = true;
        }
        if (x > wallDistance ) {
            pushX = -(x - wallDistance);
            touchedWall = true;
            hasPushed = true;
        }

        if (pOpponent && velocityX) {
            float opX = pOpponent->getPosX();
            if (fabsf(opX - x) > maxPlayerDistance) {
                int directionToOpponent = (int)(opX - x) / abs((int)(opX - x));
                // if moving away from opponent, obey virtual wall
                if (directionToOpponent == direction) {
                    touchedWall = true;
                    hasPushed = true;

                    pushX = (fabsf(opX - x) - maxPlayerDistance) * direction;
                }
            }
        }
    }

    // if we're going up (like jsut getting hit), we're not landing,
    // just being helped off the ground - see heavy donkey into lp dp
    bool forceLanding = airborne && prevPoseStatus == 3 && poseStatus > 0 && poseStatus < 3;
    if (forceLanding || (airborne && floorpush && velocityY < 0))
    {
        pushY = 0.0f;
        velocityX = 0.0f;
        velocityY = 0.0f;
        accelX = 0.0f;
        accelY = 0.0f;

        airborne = false;

        // we dont go through the landing transition here
        // we just need to go to the ground and not be airborne
        // so we can move after this script ends, like tatsu
        if (!forceLanding || forceKnockDown) {
            landed = true;
        }
        log (logTransitions, "landed " + std::to_string(hitStun));
    }

    if (!airborne && groundBounce) {
        log (logTransitions, "ground bounce!");
        // dont let stuff below see landed/grounded
        airborne = true;
        landed = false;

        bounced = true;
    }

    for ( auto minion : minions ) {
        minion->WorldPhysics();
    }

    if ( hasPushed ) {
        posX += pushX;
        posY += pushY;

        if (pushBackThisFrame != 0.0f && pushX != 0 && pushX * pushBackThisFrame < 0.0f) {
            // some pushback went into the wall, it needs to go into opponent
            if (pAttacker && !pAttacker->noPush && !noCounterPush) {
                pAttacker->posX += fmaxf(pushX, pushBackThisFrame * -1.0f);
                pAttacker->UpdateBoxes();
            }
        }

        UpdateBoxes();
    }

    if (landed || forceLanding || bounced) {
        // don't update hitboxes before setting posY, the current frame
        // or the box will be too high up as we're still on the falling box
        // see heave donky into lp dp
        posY = 0.0f;
    }

    return hasPushed;
}

bool Guy::CheckHit(Guy *pOtherGuy)
{
    if ( !pOtherGuy ) return false;

    bool retHit = false;
    for (auto hitbox : hitBoxes ) {
        if (hitbox.type != domain && hitbox.hitID < canHitID) {
            continue;
        }
        if (hitbox.type == proximity_guard || hitbox.type == destroy_projectile) {
            // right now we do nothing with those
            continue;
        }
        bool isGrab = hitbox.type == grab;
        bool foundBox = false;
        bool armor = false;
        int armorID = -1;
        HurtBox hurtBox;

        if (isGrab) {
            for (auto throwBox : *pOtherGuy->getThrowBoxes() ) {
                if (hitbox.type == domain || doBoxesHit(hitbox.box, throwBox)) {
                    foundBox = true;
                    break;
                }
            }
        } else {
            for (auto armorBox : *pOtherGuy->getArmorBoxes() ) {
                if (hitbox.type == domain || doBoxesHit(hitbox.box, armorBox.hurtBox.box)) {
                    foundBox = true;
                    hurtBox = armorBox.hurtBox;
                    armor = true;
                    armorID = armorBox.armorID;
                    break;
                }
            }
            if (!foundBox) {
                for (auto hurtbox : *pOtherGuy->getHurtBoxes() ) {
                    if (hitbox.type == domain || doBoxesHit(hitbox.box, hurtbox.box)) {
                        hurtBox = hurtbox;
                        foundBox = true;
                        break;
                    }
                }
            }
        }

        if (foundBox) {
            std::string hitIDString = to_string_leading_zeroes(hitbox.hitEntryID, 3);
            int hitEntryFlag = 0;

            if (pOtherGuy->isDown) {
                // need to check for otg capability there i guess?
                // and cotninue instead of breaking!!!
                break;
            }

            bool otherGuyAirborne = pOtherGuy->airborne || pOtherGuy->poseStatus == 3;

            if (otherGuyAirborne) {
                hitEntryFlag |= air;
            }
            if (pOtherGuy->counterState || (forceCounter && pOtherGuy->comboHits == 0)) {
                hitEntryFlag |= counter;
            }
            // the force from the UI doesn't do the right thing for multi hit moves like hands ATM
            if (pOtherGuy->punishCounterState || (forcePunishCounter && pOtherGuy->comboHits == 0)) {
                hitEntryFlag |= punish_counter;
            }
            bool otherGuyCanBlock = !otherGuyAirborne && pOtherGuy->actionStatus != -1 && pOtherGuy->currentInput & BACK;
            if (isGrab) {
                otherGuyCanBlock = false;
            }
            if (pOtherGuy->blocking || otherGuyCanBlock) {
                hitEntryFlag = block;
                pOtherGuy->blocking = true;
                blocked = true;
                log(logHits, "block!");
            }

            if (isGrab && (pOtherGuy->blocking || pOtherGuy->hitStun)) {
                // a grab would whiff if opponent is in blockstun
                continue;
            }

            std::string hitEntryFlagString = to_string_leading_zeroes(hitEntryFlag, 2);
            auto hitEntry = hitJson[hitIDString]["param"][hitEntryFlagString];
            int destX = hitEntry["MoveDest"]["x"];
            int destY = hitEntry["MoveDest"]["y"];
            int hitHitStun = hitEntry["HitStun"];
            int dmgType = hitEntry["DmgType"];
            int moveType = hitEntry["MoveType"];
            int attr0 = hitEntry["Attr0"];
            // we're hitting for sure after this point (modulo juggle), side effects

            if (armor && armorID) {
                auto atemiIDString = std::to_string(armorID);
                // need to pull from opponents atemi here or put in opponent method
                nlohmann::json atemi = nullptr;
                if (pOtherGuy->atemiJson.contains(atemiIDString)) {
                    atemi = pOtherGuy->atemiJson[atemiIDString];
                } else if (commonAtemiJson.contains(atemiIDString)) {
                    atemi = commonAtemiJson[atemiIDString];
                } else {
                    log(true, "atemi not found!!");
                    break;
                }

                int armorHitStopHitted = atemi["TargetStop"];
                int armorHitStopHitter = atemi["OwnerStop"];
                int armorBreakHitStopHitted = atemi["TargetStopShell"]; // ??
                int armorBreakHitStopHitter = atemi["OwnerStopShell"];

                if (pOtherGuy->currentAtemiID != armorID) {
                    pOtherGuy->atemiHitsLeft = atemi["ResistLimit"].get<int>() + 1;
                    pOtherGuy->currentAtemiID = armorID;
                }
                if ( pOtherGuy->currentAtemiID == armorID ) {
                   pOtherGuy->atemiHitsLeft--;
                    if (pOtherGuy->atemiHitsLeft <= 0) {
                        armor = false;
                        if (pOtherGuy->atemiHitsLeft == 0) {
                            log(logHits, "armor break!");
                            addWarudo(armorBreakHitStopHitter+1);
                            pOtherGuy->addWarudo(armorBreakHitStopHitted+1);
                        }
                    } else {
                        // apply gauge effects here

                        addWarudo(armorHitStopHitter+1);
                        pOtherGuy->addWarudo(armorHitStopHitted+1);
                        log(logHits, "armor hit!");
                    }
                }
            }

            // not hitstun for initial grab hit as we dont want to recover during the lock
            if ( armor || pOtherGuy->ApplyHitEffect(hitEntry, !isGrab, !isGrab, wasDrive, hitbox.type == domain) ) {
                int hitStopSelf = hitEntry["HitStopOwner"];
                if ( !armor && hitStopSelf ) {
                    addWarudo(hitStopSelf+1);
                }

                pOtherGuy->pAttacker = this;

                if (isGrab) {
                    grabbedThisFrame = true;
                }

                canHitID = hitbox.hitID + 1;
                if (!pOtherGuy->blocking) {
                    hitThisFrame = true;
                }
                retHit = true;
                log(logHits, "hit type " + std::to_string(hitbox.type) + " id " + std::to_string(hitbox.hitID) +
                    " dt " + hitIDString + " destX " + std::to_string(destX) + " destY " + std::to_string(destY) +
                    " hitStun " + std::to_string(hitHitStun) + " dmgType " + std::to_string(dmgType) +
                    " moveType " + std::to_string(moveType) );
                log(logHits, "attr0 " + std::to_string(attr0));
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

bool Guy::ApplyHitEffect(nlohmann::json hitEffect, bool applyHit, bool applyHitStun, bool isDrive, bool isDomain)
{
    int juggleFirst = hitEffect["Juggle1st"];
    int juggleAdd = hitEffect["JuggleAdd"];
    int juggleLimit = hitEffect["JuggleLimit"];
    int hitEntryHitStun = hitEffect["HitStun"];
    int destX = hitEffect["MoveDest"]["x"];
    int destY = hitEffect["MoveDest"]["y"];
    int destTime = hitEffect["MoveTime"];
    int dmgValue = hitEffect["DmgValue"];
    int dmgType = hitEffect["DmgType"];
    int moveType = hitEffect["MoveType"];
    int floorTime = hitEffect["FloorTime"];
    int downTime = hitEffect["DownTime"];
    bool noZu = hitEffect["_no_zu"];
    bool jimenBound = hitEffect["_jimen_bound"];
    bool kabeBound = hitEffect["_kabe_bound"];
    int hitStopTarget = hitEffect["HitStopTarget"];
    // int curveTargetID = hitEntry["CurveTgtID"];

    if (isDrive) {
        juggleAdd = 0;
        juggleFirst = 0;
        juggleLimit += 3;
    }

    if (!isDomain && airborne && juggleCounter > juggleLimit) {
        return false;
    }

    // if going airborne, start counting juggle
    if (!airborne && destY != 0) {
        if (juggleCounter == 0) {
            juggleCounter = juggleFirst; // ?
        } else {
            juggleCounter += juggleAdd;
        }
    }

    if (hitStopTarget) {
        addWarudo(hitStopTarget+1);
    }

    if (isDrive) {
        hitEntryHitStun += 4;
    }

    // like guile 4HK has destY but stays grounded if hits grounded
    if (!(dmgType & 8) && !airborne) {
        destY = 0;
    }

    resetHitStunOnLand = false;
    resetHitStunOnTransition = false;
    knockDownFrames = 0;
    if (destY != 0)
    {
        // this is set on honda airborne hands
        // juggle state, just add a bunch of hitstun
        if (dmgValue != 0 && dmgType & 8) {
            hitEntryHitStun += 500000;
            resetHitStunOnLand = true;
        }
        knockDownFrames = downTime;
    }

    if (moveType == 11 || moveType == 10) { //airborne crumples
        hitEntryHitStun += 500000;
        resetHitStunOnTransition = true;
        forceKnockDown = true;
        knockDownFrames = downTime;
    }

    comboDamage += dmgValue;

    if (!blocking && applyHit) {
        beenHitThisFrame = true;
        comboHits++;
    }

    if (applyHitStun) {
        hitStun = hitEntryHitStun + hitStunAdder;
    }

    if (destY > 0 ) {
        airborne = true;
        forceKnockDown = true;
    }

    if (jimenBound) {
        int floorDestX = hitEffect["FloorDest"]["x"];
        int floorDestY = hitEffect["FloorDest"]["y"];

        groundBounce = true;
        groundBounceVelX = -floorDestX / (float)floorTime;
        groundBounceAccelX = floorDestX / 2.0 / (float)floorTime * 2.0 / (float)floorTime;
        groundBounceVelX -= groundBounceAccelX;

        groundBounceVelY = floorDestY * 4.0 / (float)floorTime;
        groundBounceAccelY = floorDestY * -4.0 / (float)floorTime * 2.0 / (float)floorTime;
        groundBounceVelY -= groundBounceAccelY;
    }

    if (kabeBound) {
        int wallDestX = hitEffect["WallDest"]["x"];
        int wallDestY = hitEffect["WallDest"]["y"];
        int wallTime = hitEffect["WallTime"];
        wallStopFrames = hitEffect["WallStop"];

        wallBounce = true;
        wallBounceVelX = -wallDestX / (float)wallTime;
        //wallBounceAccelX = -direction * wallDestX / 2.0 / (float)wallTime * 2.0 / (float)wallTime;
        //wallBounceVelX -= wallBounceAccelX;

        wallBounceVelY = wallDestY * 4.0 / (float)wallTime;
        wallBounceAccelY = wallDestY * -4.0 / (float)wallTime * 2.0 / (float)wallTime;
        wallBounceVelY -= wallBounceAccelY;
    }

    if (destTime != 0) {
        // assume hit direction is opposite as facing for now, not sure if that's true
        // todo pushback in corner - all destX _must_ be traveled by either side
        if ( destX != 0) {
            if (!airborne && !jimenBound) {
                int time = destTime;
                hitVelX = direction * destX * -2.0 / (float)time;
                hitAccelX = direction * destX / (float)time * 2.0 / (float)time;
                hitVelX -= hitAccelX;
            } else {
                // keep itvel pushback from last grounded hit
                velocityX = -destX / (float)destTime;
            }
        }

        if (destY != 0) {
            velocityY = destY * 4 / (float)destTime;
            accelY = destY * -4 / (float)destTime * 2.0 / (float)destTime;
            // i think this vel wants to apply this frame, lame workaround to get same intensity
            velocityY -= accelY; //
        }
    }

    // need to figure out if body or head is getting hit here later

    if (blocking) {
        nextAction = 161;
        if (crouching) {
            nextAction = 175;
        }
    } else {
        //if (dmgType & 3) { // crumple? :/
        if (moveType == 13) { // set on wall bounce
            nextAction = 232;
        } else if (moveType == 11) {
            nextAction = 277; // back crumple?
        } else if (moveType == 10) {
            nextAction = 276;
        } else if (moveType == 3) {
            // not a crumple, just a long stun
            nextAction = 279; // seen on guile 5hp, moveType 3
            //}
            // if (airborne) {
            //     nextAction = 255;
            // }
        } else {
            // HH / 202 if head?
            nextAction = 205; // HIT_MM, not sure how to pick which
            if ( crouching ) {
                nextAction = 213;
            }
            if ((airborne || posY > 0.0) && destY != 0 ) {

                if (destY > destX) {
                    nextAction = 251; // 90
                } else if (destX > destY * 2.5) {
                    nextAction = 253; // 00
                } else {
                    nextAction = 252; // 45
                }
            }
        }
    }

    noCounterPush = noZu;
    return true;
}

void Guy::DoHitBoxKey(const char *name)
{
    if (actionJson.contains(name))
    {
        for (auto& [hitBoxID, hitBox] : actionJson[name].items())
        {
            if ( !hitBox.contains("_StartFrame") || hitBox["_StartFrame"] > currentFrame || hitBox["_EndFrame"] <= currentFrame ) {
                continue;
            }

            bool isOther = strcmp(name, "OtherCollisionKey") == 0;
            //bool isUnique = strcmp(name, "UniqueCollisionKey") == 0;

            float rootOffsetX = 0;
            float rootOffsetY = 0;
            parseRootOffset( hitBox, rootOffsetX, rootOffsetY );
            rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
            rootOffsetY += posY + posOffsetY;

            Box rect={-4096,-4096,8192,8192};
            auto rects = commonAction ? commonRectsJson : rectsJson;

            for (auto& [boxNumber, boxID] : hitBox["BoxList"].items()) {
                int collisionType = hitBox["CollisionType"];
                hitBoxType type = hit;
                color collisionColor = {1.0,0.0,0.0};
                int rectListID = collisionType;
                if (isOther) { 
                    if (collisionType == 7) {
                        type = domain;
                    } else if (collisionType == 10) {
                        collisionColor = {0.0,1.0,0.5};
                        type = destroy_projectile;
                    }
                    rectListID = 9;
                } else {
                    if (collisionType == 3 ) {
                        collisionColor = {0.5,0.5,0.5};
                        type = proximity_guard;
                    } else if (collisionType == 2) {
                        type = grab;
                    } else if (collisionType == 1) {
                        type = projectile;
                    } else if (collisionType == 0) {
                        type = hit;
                    }
                }

                if ((type == domain) || getRect(rect, rects, rectListID, boxID,rootOffsetX, rootOffsetY,direction)) {
                    renderBoxes.push_back({rect, collisionColor, (isDrive || wasDrive) && collisionType != 3 });

                    int hitEntryID = hitBox["AttackDataListIndex"];
                    int hitID = hitBox["HitID"];
                    if (hitEntryID != -1) {
                        hitBoxes.push_back({rect,type,hitEntryID,hitID});
                    }
                }
            }
        }
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
            int64_t branchParam2 = key["Param02"];
            int64_t branchParam3 = key["Param03"];
            int branchAction = key["Action"];
            int branchFrame = key["ActionFrame"];

            switch (branchType) {
                case 0: // always?
                    doBranch = true;
                    break;
                case 1:
                    // else?! jesus christ we're turing complete soon
                    // i dont know what it means though
                    if (deniedLastBranch) {
                        doBranch = true;
                    }
                    break;
                case 2:
                    if (canHitID >= 0) { // has hit ever this move.. not sure if right
                        doBranch = true;
                    }
                    break;
                case 4:
                    if (blocked) { // just this frame.. enough?
                        doBranch = true;
                    }
                    break;
                case 5: // swing.. not hit?
                    if (canHitID == -1) {
                        doBranch = true;
                    }
                    break;
                case 11:
                    if (pParent) {
                        if (pParent->currentAction == branchParam0) {
                           doBranch = true;
                           //log("action branch1");
                        }
                    } else {
                        log(true, "that branch not gonna work");
                    }
                    break;
                case 13:
                    if (!airborne) { // it's technically landed, but like heavy buttslam checks one frame
                        doBranch = true;
                    }
                    break;
                case 14:
                    if (touchedWall) {
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
                        log(logUnknowns, "unknown steer branch");
                    }
                    break;
                case 20:
                    if (branchParam3 == 1 || branchParam3 == 0) { // classic controls? is 0 catchall?
                        if (branchParam0 == 1 && (currentInput & branchParam1)) {
                            doBranch = true;
                        }
                        if (branchParam0 == 0 && !(currentInput & branchParam1)) {
                            doBranch = true;
                        }
                    }
                    break;
                case 29: // unique param
                    if ((branchParam1 == 0 && uniqueCharge == branchParam3) ||
                        (branchParam1 == 1 && uniqueCharge > branchParam3)) {
                        //param 1 can be one.. greater or equals? not sure
                        // honda spirit, guile puncher
                        // probably how jamie drinks work too? not sure yet
                        doBranch = true;
                    }
                    break;
                case 31: // todo loop count
                case 36:
                    if (grabbedThisFrame) {
                        doBranch = true;
                    }
                    break;
                case 37:
                    // Hit catch vs just hit.. is this one "ever hit" and the other 'hit this frame'?
                    // or the opposite...?
                    if (hitThisFrame) {
                        doBranch = true;
                    }
                    break;
                case 40:
                    if (pOpponent) {
                        int distX = branchParam2 & 0xFFFF;
                        int distY = (branchParam2 & 0xFFFF0000) >> 32;

                        // ughh i guess we need an actual distance between pushboxes or something?
                        // jp normal ghost has like 60 distX distance - *2 for now just to unblock
                        if (std::abs(pOpponent->posX - posX) < distX * 2 &&
                            std::abs(pOpponent->posY - posY) < distY) {
                                doBranch = true;
                        }
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
                case 47: // todo incapacitated
                    break;
                case 52: // shot count
                    {
                        int count = 0;
                        for ( auto minion : minions ) {
                            if (branchParam0 == minion->limitShotCategory) {
                                count++;
                            }
                        }
                        if (branchParam1 == 5 && count == branchParam2) {
                            // equals certain count?
                            doBranch = true;
                        }
                        if (branchParam1 == 3 && count == branchParam2) {
                            // same? strict equals? aaa
                            doBranch = true;
                        }
                    }
                    break;
                case 54:
                    if (touchedOpponent) {
                        doBranch = true;
                    }
                    break;
                case 63:
                    // what's the difference between this and 20?
                    // this is used for backthrow, the other thing for held buttons
                    if (branchParam2 == 1 && currentInput & branchParam1) {
                        doBranch = true;
                    }
                    break;
                default:
                    std::string typeName = key["_TypesName"];
                    log(logUnknowns, "unsupported branch id " + std::to_string(branchType) + " type " + typeName);
                    break;
            }

            // do those also override if higher branchID?
            if (doBranch) {
                if (branchAction == currentAction && branchFrame == 0 ) {
                    log(true, "ignoring branch to frame 0? that's how jp stuff owrks dunno");
                } else {
                    if (branchFrame != 0 && branchAction == currentAction) {
                        // unclear where we'll hit it so not sure if we need to offset yet
                        log(true, "if this happens check we're not off by 1");
                        currentFrame = branchFrame;
                    } else {
                        log(logBranches, "branching to action " + std::to_string(branchAction));
                        nextAction = branchAction;
                        nextActionFrame = branchFrame;
                    }
                }

                deniedLastBranch = false;

                keepPlace = key["_KeepPlace"];
                break;
            } else {
                if (branchType != 1) {
                    deniedLastBranch = true;
                }
            }
        }
    }
}

bool Guy::Frame(void)
{
    if (warudo) {
        // if we just entered hitstop, don't go to next frame right now
        // we want to have a chance to get hitstop input before triggers
        // we'll re-run it in PreFrame
        return false;
    }
    DoTriggers();
    DoBranchKey();

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

    if (landed) {
        wallBounce = false; // just in case we didn't reach a wall
        if ( resetHitStunOnLand ) {
            hitStun = 1;
            resetHitStunOnLand = false;
        }
    }

    if (wallBounce && touchedWall) {
        wallStopped = true;
        velocityX = 0.0;
        velocityY = 0.0;
        accelX = 0.0;
        accelY = 0.0f;
        nextAction = 256;
    }

    if (wallStopped) {
        wallStopFrames--;
        if (wallStopFrames <= 0) {
            nextAction = 235; // combo/bounce state

            velocityX = wallBounceVelX;
            accelX = wallBounceAccelX;
            velocityY = wallBounceVelY;
            accelY = wallBounceAccelY;

            wallBounce = false;
            wallStopped = false;
            wallBounceVelX = 0.0f;
            wallBounceAccelX = 0.0f;
            wallBounceVelY = 0.0f;
            wallBounceAccelY = 0.0f;
        }
    }

    if (bounced) {
        nextAction = 350; // combo/bounce state

        velocityX = groundBounceVelX;
        accelX = groundBounceAccelX;
        velocityY = groundBounceVelY;
        accelY = groundBounceAccelY;

        groundBounce = false;
        groundBounceVelX = 0.0f;
        groundBounceAccelX = 0.0f;
        groundBounceVelY = 0.0f;
        groundBounceAccelY = 0.0f;

        bounced = false;
    }

    if (currentAction >= 251 && currentAction <= 253 && nextAction == -1)
    {
        nextAction = currentAction - 21;
    }

    bool recovered = false;

    if (currentFrame >= actionFrameDuration && nextAction == -1)
    {
        if ( currentAction == 33 || currentAction == 34 || currentAction == 35 ) {
            // If done with pre-jump, transition to jump
            nextAction = currentAction + 3;
            airborne = true; // probably should get it thru statuskey?
        } else if (currentAction == 5) {
            nextAction = 4; // finish transition to crouch
        } else if (loopCount == -1 || loopCount > 0) {
            currentFrame = loopPoint;
            hasLooped = true;
            if (loopCount > 0) {
                loopCount--;
            }
        } else {
            if (isProjectile) {
                return false; // die
            }
            nextAction = 1;
        }

        if (resetHitStunOnTransition) {
            hitStun = 1;
            resetHitStunOnTransition = false;
        }
    }

    // first hitstun countdown happens on the same "frame" as hit, before hitstop
    if (hitStun > 0)
    {
        hitStun--;
        if (hitStun == 0)
        {
            nextAction = 1;

            if (forceKnockDown) {
                if (knockDownFrames) {
                    hitStun = knockDownFrames;
                    knockDownFrames = 0;
                    nextAction = 330;
                    isDown = true;
                } else {
                    nextAction = 340;
                    isDown = false;
                    forceKnockDown = false;
                }
            } else {
                blocking = false;
                recovered = true;
            }
        }
    }

    bool canMove = false;
    int actionCheckCanMove = currentAction;
    if (nextAction != -1 ) {
        actionCheckCanMove = nextAction;
    }
    crouching = actionCheckCanMove == 4 || actionCheckCanMove == 5;
    bool movingForward = actionCheckCanMove == 9 || actionCheckCanMove == 10 || actionCheckCanMove == 11;
    bool movingBackward = actionCheckCanMove == 13 || actionCheckCanMove == 14 || actionCheckCanMove == 15;
    if (actionCheckCanMove == 1 || actionCheckCanMove == 2 || actionCheckCanMove == 4 || //stands, crouch
        movingForward || movingBackward || crouching) {
        canMove = true;
    }

    if ((marginFrame != -1 && currentFrame >= marginFrame) && nextAction == -1 ) {
        canMove = true;
    }

    if (airborne || posY > 0.0f) {
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
            if ( !crouching ) {
                crouching = true;
                if (poseStatus == 2) {
                    nextAction = 4; // crouch loop after the first sitting down anim if already crouched
                } else {
                    nextAction = 5; // BAS_STD_CRH
                }
            }
        } else {
            // if ((currentInput & (32+256)) == 32+256) {
            //     nextAction = 480; // DPA_STD_START
            // } else
             if ( currentInput & 4 && !movingBackward ) {
                nextAction = 13; // BAS_BACKWARD_START
            } else if ( currentInput & 8 && !movingForward) {
                nextAction = 9; // BAS_FORWARD_START
            }
        }

        if ( nextAction == -1 && ((currentInput & 0xF) == 0) ) { // only do that if we're not post-margin for correctness
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

    if (canMove && comboHits && nextAction == 1 && neutralMove != 0) {
        std::string moveString = vecMoveList[neutralMove];
        int actionID = atoi(moveString.substr(0, moveString.find(" ")).c_str());
        nextAction = actionID;
    }

    if ( nextAction == -1 && (currentAction == 480 || currentAction == 481) && (currentInput & (32+256)) != 32+256) {
        nextAction = 482; // DPA_STD_END
    }

    if (landed && nextAction == -1) {
        if (currentAction == 36 || currentAction == 37 || currentAction == 38) {
            nextAction = currentAction + 3; // generic landing
        }

        // better assume the script has something in mind for landing :/

        if (hitStun) {
            nextAction = 350; // being down? dunno
        }
    }

    if (recovered || (canMove && comboHits)) {
        int advantage = globalFrameCount - pOpponent->recoveryTiming;
        std::string message = "recovered! adv " + std::to_string(advantage);
        if ( comboHits) {
            message += " combo hits " + std::to_string(comboHits) + " damage " + std::to_string(comboDamage);
        }
        log(true, message );
        comboHits = 0;
        juggleCounter = 0;
        comboDamage = 0;
        pAttacker = nullptr;
    }

    // this can be true even if canMove
    if (deferredAction != 0 && deferredActionFrame == currentFrame) {
        log(logTransitions, "deferred nextAction " + std::to_string(deferredAction));
        nextAction = deferredAction;

        deferredActionFrame = -1;
        deferredAction = 0;

        // don't run triggers again though
        canMove = false;
    }

    // Transition
    if ( nextAction != -1 )
    {
        if (currentAction != 1 && nextAction == 1) {
            recoveryTiming = globalFrameCount;
            //log("recovered!");
        }

        if (currentAction != nextAction) {
            currentAction = nextAction;
            log (logTransitions, "current action " + std::to_string(currentAction) + " keep place " + std::to_string(keepPlace));

            if (styleInstallFrames && !countingDownInstall) {
                // start counting down on wakeup after install super?
                countingDownInstall = true;
            }
        }

        if (nextActionOpponentAction) {
            opponentAction = true;
            nextActionOpponentAction = false;
        } else {
            opponentAction = false;
        }

        if (!keepPlace) {
            currentFrame = nextActionFrame != -1 ? nextActionFrame : 0;

            // commit current place offset
            posX += (posOffsetX * direction);
            posOffsetX = 0.0f;
            posY += posOffsetY;
            posOffsetY = 0.0f;

            canHitID = -1;
            currentAtemiID = -1; // uhhh

            poseStatus = 0; // correct spot?
            actionStatus = 0;
            jumpStatus = 0;
        } else {
            currentFrame--; //rewind
        }
        keepPlace = false;

        nextAction = -1;
        nextActionFrame = -1;

        if (turnaround) {
            direction *= -1;
        }

        // if grounded, reset velocities on transition
        // need to test if still needed?
        // probably just on transitioning to standing? not sure
        // velocities already got transitioned tro 0 on actual landing even if in hitstun
        if ( posY == 0.0 && !isDrive && !hitStun) {
            velocityX = 0;
            velocityY = 0;
            accelX = 0;
            accelY = 0;
        }

        // if not grounded, fall to the ground i guess?
        // i don't think this is supposed to be needed, just a crutch for now
        if ( posY > 0.0 && !hitStun && !isProjectile && accelY == 0.0 ) {
            accelY = -1;
        }

        if (isDrive == true) {
            isDrive = false;
            // at some point make it so we cant drive specials
            wasDrive = true;
        } else {
            wasDrive = false;
        }

        actionFrameDataInitialized = false;

        deferredAction = 0;
        deferredActionFrame = 0;
    }

    if (warudo == 0) {
        timeInWarudo = 0;
    }

    UpdateActionData();

    if (canMove) {
        // if we just went to idle, run triggers again
        DoTriggers();
        // if successful, eat this frame away and go right now
        if (nextAction != -1) {
            currentAction = nextAction;
            actionFrameDataInitialized = false;
            log (logTransitions, "nvm! current action " + std::to_string(currentAction));
            nextAction = -1;
        }
    }

    UpdateActionData();

    // if we need landing adjust/etc during warudo, need this updated now
    prevPoseStatus = poseStatus;
    DoStatusKey();

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

void Guy::DoStatusKey(void)
{
    if (actionJson.contains("StatusKey"))
    {
        for (auto& [keyID, key] : actionJson["StatusKey"].items())
        {
            if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                continue;
            }

            int adjust = key["LandingAdjust"];
            if ( adjust != 0 ) {
                landingAdjust = adjust;
            }
            poseStatus = key["PoseStatus"];
            actionStatus = key["ActionStatus"];
            jumpStatus = key["JumpStatus"];
        }
    }
}