#include <stdio.h>

#include <fstream>
#include <ios>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <bitset>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

#include "guy.hpp"
#include "main.hpp"
#include "render.hpp"
#include <string>

void parseRootOffset( nlohmann::json& keyJson, Fixed&offsetX, Fixed& offsetY)
{
    if ( keyJson.contains("RootOffset") && keyJson["RootOffset"].contains("X") && keyJson["RootOffset"].contains("Y") ) {
        offsetX = Fixed(keyJson["RootOffset"]["X"].get<int>());
        offsetY = Fixed(keyJson["RootOffset"]["Y"].get<int>());
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

static inline void doSteerKeyOperation(Fixed &value, Fixed keyValue, int operationType)
{
    switch (operationType) {
        case 4: // set sign.. ??
            if (value < Fixed(0)) {
                keyValue = -keyValue;
            }
            [[fallthrough]];
        case 1: // set
            value = keyValue;
            break;
        case 5: // add sign.. ??
            if (value < Fixed(0)) {
                keyValue = -keyValue;
            }
            [[fallthrough]];
        case 2: // add ?
            value = value + keyValue;
            break;
        default:
            log("Uknown steer keyoperation!");
            break;
    }
}

bool Guy::GetRect(Box &outBox, int rectsPage, int boxID, Fixed offsetX, Fixed offsetY, int dir)
{
    std::string pageIDString = to_string_leading_zeroes(rectsPage, 2);
    std::string boxIDString = to_string_leading_zeroes(boxID, 3);
    nlohmann::json *pRects = pRectsJson;
    if (!pRects->contains(pageIDString) || !(*pRects)[pageIDString].contains(boxIDString)) {
        pRects = pCommonRectsJson;
    }
    if (!pRects->contains(pageIDString) || !(*pRects)[pageIDString].contains(boxIDString)) {
        return false;
    }
    nlohmann::json *pRect = &(*pRects)[pageIDString][boxIDString];
    int xOrig = (*pRect)["OffsetX"];
    int yOrig = (*pRect)["OffsetY"];
    int xRadius = (*pRect)["SizeX"];
    int yRadius = (*pRect)["SizeY"];
    xOrig *= dir;

    outBox.x = Fixed(xOrig - xRadius) + offsetX;
    outBox.y = Fixed(yOrig - yRadius) + offsetY;
    outBox.w = Fixed(xRadius * 2);
    outBox.h = Fixed(yRadius * 2);

    return true;
}

const char* Guy::FindMove(int actionID, int styleID, nlohmann::json **ppMoveJson)
{
    auto mapIndex = std::make_pair(actionID, styleID);
    if (mapMoveStyle.find(mapIndex) == mapMoveStyle.end()) {
        int parentStyleID = (*pCharInfoJson)["Styles"][std::to_string(styleID)]["ParentStyleID"];

        if (parentStyleID == -1) {
            return nullptr;
        }

        return FindMove(actionID, parentStyleID, ppMoveJson);
    } else {
        const char *moveName = mapMoveStyle[mapIndex].first.c_str();
        bool commonMove = mapMoveStyle[mapIndex].second;

        *ppMoveJson = commonMove ? &(*pCommonMovesJson)[moveName] : &(*pMovesDictJson)[moveName];
        return moveName;
    }
}

void Guy::BuildMoveList()
{
    // for UI dropdown selector
    vecMoveList.push_back(strdup("1 no action (obey input)"));
    auto triggerGroupString = to_string_leading_zeroes(0, 3);
    for (auto& [keyID, key] : (*pTriggerGroupsJson)[triggerGroupString].items())
    {
        std::string actionString = key;
        vecMoveList.push_back(strdup(actionString.c_str()));
    }

    for (auto& [keyID, key] : pMovesDictJson->items())
    {
        int actionID = key["fab"]["ActionID"];
        int styleID = 0;
        if (key.contains("_PL_StyleID")) {
            styleID = key["_PL_StyleID"];
        }
        auto mapIndex = std::make_pair(actionID, styleID);
        mapMoveStyle[mapIndex] = std::make_pair(keyID, false);
    }

    for (auto& [keyID, key] : pCommonMovesJson->items())
    {
        int actionID = key["fab"]["ActionID"];
        int styleID = 0;
        if (key.contains("_PL_StyleID")) {
            styleID = key["_PL_StyleID"];
        }
        auto mapIndex = std::make_pair(actionID, styleID);
        if (mapMoveStyle.find(mapIndex) == mapMoveStyle.end()) {
            mapMoveStyle[mapIndex] = std::make_pair(keyID, true);
        }
    }
}

void Guy::Input(int input)
{
    if (direction.i() < 0) {
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

std::string Guy::getActionName(int actionID)
{
    auto actionIDString = to_string_leading_zeroes(actionID, 4);
    bool validAction = pNamesJson->contains(actionIDString);
    std::string ret = validAction ? (*pNamesJson)[actionIDString] : "invalid";
    return ret;
}

void Guy::UpdateActionData(void)
{
    auto actionIDString = to_string_leading_zeroes(currentAction, 4);
    pActionJson = nullptr;
    const char *foundAction = nullptr;

    if (opponentAction) {
        foundAction = pOpponent->FindMove(currentAction, 0, &pActionJson);
    } else {
        foundAction = FindMove(currentAction, styleInstall, &pActionJson);
    }

    if (foundAction == nullptr) {
        log(true, "couldn't find next action, reverting to 1 - style lapsed?");
        currentAction = 1;
        foundAction = FindMove(currentAction, styleInstall, &pActionJson);
    }

    actionName = foundAction;

    nlohmann::json *pFab = &(*pActionJson)["fab"];
    mainFrame = (*pFab)["ActionFrame"]["MainFrame"];
    followFrame = (*pFab)["ActionFrame"]["FollowFrame"];
    marginFrame = (*pFab)["ActionFrame"]["MarginFrame"];
    actionFrameDuration = (*pFab)["Frame"];
    loopPoint = (*pFab)["State"]["EndStateParam"];
    if ( loopPoint == -1 ) {
        loopPoint = 0;
    }
    loopCount = (*pFab)["State"]["LoopCount"];
    hasLooped = false;
}

bool Guy::PreFrame(void)
{
    // todo check if it also counts down in screen freeze..
    // if it doesn't we might need to have two kinds of warudos
    if (!warudoIsFreeze || !warudo) {
        if (debuffTimer > 0 ) {
            debuffTimer--;
        }
    }

    if (warudo > 0) {
        timeInWarudo++;
        warudo--;
        // increment the frame we skipped at the beginning of warudo
        if (warudo == 0) {
            if (!Frame(true)) {
                delete this;
                return false;
            }
            warudoIsFreeze = false;
        }
    }
    if (warudo > 0) {
        return false;
    }

    if (uniqueTimer) {
        // might not be in the right spot, adjust if falling out of yoga float too early/late
        uniqueTimerCount++;
    }

    hitThisFrame = false;
    hitArmorThisFrame = false;
    hitAtemiThisFrame = false;
    hasBeenBlockedThisFrame = false;
    punishCounterThisFrame = false;
    grabbedThisFrame = false;
    beenHitThisFrame = false;
    armorThisFrame = false;
    atemiThisFrame = false;
    landed = false;
    pushBackThisFrame = 0.0f;
    offsetDoesNotPush = false;

    if (pActionJson != nullptr)
    {
        if (isProjectile && !projDataInitialized && pActionJson->contains("pdata")) {
            nlohmann::json *pProjData = &(*pActionJson)["pdata"];
                projHitCount = (*pProjData)["HitCount"];
                if (projHitCount == 0) {
                    // stuff that starts at hitcount 0 is probably meant to die some other way
                    // todo implement lifetime, ranges, etc
                    projHitCount = -1;
                }
                // log("initial hitcount " + std::to_string(projHitCount));

                airborne = (*pProjData)["_AirStatus"];

                limitShotCategory = (*pProjData)["Category"];
                noPush = (*pProjData)["_NoPush"];
                projLifeTime = (*pProjData)["LifeTime"];

                if (projLifeTime <= 0) {
                    projLifeTime = 360;
                }

                projLifeTime--;

                projDataInitialized = true;
        } else {
            // todo there's a status bit for this?
            noPush = false; // might be overridden below
        }

        Fixed prevPosY = getPosY();

        if (pActionJson->contains("PlaceKey"))
        {
            for (auto& [placeKeyID, placeKey] : (*pActionJson)["PlaceKey"].items())
            {
                if ( !placeKey.contains("_StartFrame") || placeKey["_StartFrame"] > currentFrame || placeKey["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                for (auto& [frame, offset] : placeKey["PosList"].items()) {
                    int keyStartFrame = placeKey["_StartFrame"];
                    int flag = placeKey["OptionFlag"];
                    // todo there's a bunch of other flags
                    bool curPlaceKeyDoesNotPush = flag & 1;
                    // todo implement ratio here? check on cammy spiralarrow ex as an example
                    if (atoi(frame.c_str()) == currentFrame - keyStartFrame) {
                        if (placeKey["Axis"] == 0) {
                            posOffsetX = Fixed(offset.get<double>());
                        } else if (placeKey["Axis"] == 1) {
                            posOffsetY = Fixed(offset.get<double>());
                        }
                        // do we need to disambiguate which axis doesn't push? that'd be annoying
                        // is vertical pushback even a thing
                        if (curPlaceKeyDoesNotPush) {
                            offsetDoesNotPush = true;
                        }
                    }
                }
            }
        }

        Fixed prevVelX = velocityX;

        if (pActionJson->contains("SteerKey"))
        {
            for (auto& [steerKeyID, steerKey] : (*pActionJson)["SteerKey"].items())
            {
                if ( !steerKey.contains("_StartFrame") || steerKey["_StartFrame"] > currentFrame || steerKey["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int operationType = steerKey["OperationType"];
                int valueType = steerKey["ValueType"];
                Fixed fixValue = Fixed(steerKey["FixValue"].get<double>());
                Fixed targetOffsetX = Fixed(steerKey["FixTargetOffsetX"].get<double>());
                Fixed targetOffsetY = Fixed(steerKey["FixTargetOffsetY"].get<double>());
                int shotCategory = steerKey["_ShotCategory"];
                int targetType = steerKey["TargetType"];
                int calcValueFrame = steerKey["CalcValueFrame"];
                int multiValueType = steerKey["MultiValueType"];

                switch (operationType) {
                    case 1:
                    case 2:
                    case 4:
                    case 5:
                    case 9:
                    case 10:
                        if (operationType == 9 || operationType == 10) {
                            // home to target
                            if (targetType != 0) {
                                log(logUnknowns, "unknown home target type");
                                continue;
                            } else if (!pOpponent) {
                                log(true, "can't home to opponent, no opponent");
                                continue;
                            }
                            if (valueType == 0) {
                                if (operationType == 9 && getPosX() < homeTargetX) {
                                    continue;
                                }
                                if (operationType == 10 && getPosX() > homeTargetX) {
                                    continue;
                                }
                            }
                            if (valueType == 1) {
                                if (operationType == 9 && getPosY() < homeTargetY) {
                                    continue;
                                }
                                if (operationType == 10 && getPosY() > homeTargetY) {
                                    continue;
                                }
                            }
                            // todo CalcValueFrame? is it only recompute every N frames?
                            // fall through to normal set operaiton below after those checks
                            operationType = 1;
                        }
                        switch (valueType) {
                            case 0: doSteerKeyOperation(velocityX, fixValue,operationType); break;
                            case 1: doSteerKeyOperation(velocityY, fixValue,operationType); break;
                            case 3: doSteerKeyOperation(accelX, fixValue,operationType); break;
                            case 4: doSteerKeyOperation(accelY, fixValue,operationType); break;
                        }
                        break;
                    case 12:
                        // set on cancel from current action
                        // guessing there's also one to add on cancel too? not sure
                        operationType = 1;
                        switch (valueType) {
                            case 0: doSteerKeyOperation(cancelVelocityX, fixValue,operationType); break;
                            case 1: doSteerKeyOperation(cancelVelocityY, fixValue,operationType); break;
                            case 3: doSteerKeyOperation(cancelAccelX, fixValue,operationType); break;
                            case 4: doSteerKeyOperation(cancelAccelY, fixValue,operationType); break;
                        }
                        break;
                    case 13:
                        // set teleport/home target
                        {
                            Guy *pGuy = nullptr;
                            if (targetType == 0) {
                                pGuy = this;
                            } else if (targetType == 1) {
                                pGuy = pParent;
                            } else if (targetType == 4) {
                                // todo supposed to be nearest matching projectile?
                                for ( auto minion : minions ) {
                                    if (shotCategory & (1 << minion->limitShotCategory)) {
                                        pGuy = minion;
                                        break;
                                    }
                                }
                            } else if (targetType == 2 || targetType == 5 || targetType == 6 || targetType == 14) {
                                // to opponent (5 is hit target, 6 is grab target)
                                // todo 14 is middle of opponent's collision box?
                                pGuy = pOpponent;
                            }
                            if (pGuy) {
                                homeTargetX = pGuy->getPosX() + targetOffsetX * pGuy->direction * Fixed(-1);
                                homeTargetY = pGuy->getPosY() + targetOffsetY;
                            } else {
                                log(logUnknowns, "unknown/not found set teleport/home target");
                            }
                        }
                        break;
                    case 15:
                        // teleport/lerp position?
                        if (calcValueFrame == 0) {
                            calcValueFrame = 1;
                        }
                        if (multiValueType & 1) {
                            velocityX = -(getPosX() - homeTargetX) / Fixed(calcValueFrame);
                        }
                        if (multiValueType & 2) {
                            velocityY = -(getPosY() - homeTargetY) / Fixed(calcValueFrame);
                        }
                        break;
                    default:
                        log(logUnknowns, "unknown steer keyoperation " + std::to_string(operationType));
                        break;
                }

            }
        }

        if (wasDrive && pActionJson->contains("DriveSteerKey"))
        {
            for (auto& [steerKeyID, steerKey] : (*pActionJson)["DriveSteerKey"].items())
            {
                if ( !steerKey.contains("_StartFrame") || steerKey["_StartFrame"] > currentFrame || steerKey["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int operationType = steerKey["OperationType"];
                int valueType = steerKey["ValueType"];
                Fixed fixValue = Fixed(steerKey["FixValue"].get<double>());

                switch (valueType) {
                    case 0: doSteerKeyOperation(velocityX, fixValue,operationType); break;
                    case 1: doSteerKeyOperation(velocityY, fixValue,operationType); break;
                    case 3: doSteerKeyOperation(accelX, fixValue,operationType); break;
                    case 4: doSteerKeyOperation(accelY, fixValue,operationType); break;
                }
            }
        }

        if ( (accelX != Fixed(0) && prevVelX != Fixed(0) && velocityX == Fixed(0)) ) {
            // if a steerkey just set speed to 0 and there was accel, it seems to want to clear accel
            // to stop movement - see back accel for eg. drive rush normals
            accelX = 0;
        }

        prevVelX = velocityX;
        Fixed prevVelY = velocityY;

        velocityX = velocityX + accelX;
        velocityY = velocityY + accelY;

        if ((velocityY * prevVelY) < Fixed(0) || (accelY != Fixed(0) && velocityY == Fixed(0))) {
            startsFalling = true;
        } else {
            startsFalling = false;
        }

        // log(std::to_string(currentAction) + " " + std::to_string(prevVelX) + " " + std::to_string(velocityX));

        if ( (velocityX * prevVelX) < Fixed(0) || (accelX != Fixed(0) && velocityX == Fixed(0)) ) {
            // sign change?
            velocityX = Fixed(0);
            accelX = Fixed(0);
        }

        if (!noVelNextFrame) {
            posX = posX + (velocityX * direction);
            posY = posY + velocityY;
        } else {
            noVelNextFrame = false;
        }

        if (hitVelX != Fixed(0)) {
            posX = posX + hitVelX;
            pushBackThisFrame = hitVelX;

            Fixed prevHitVelX = hitVelX;
            hitVelX = hitVelX + hitAccelX;
            if ((hitVelX * prevHitVelX) < Fixed(0) || (hitAccelX != Fixed(0)&& hitVelX == Fixed(0))) {
                hitAccelX = Fixed(0);
                hitVelX = Fixed(0);
            }
        }

        if (!isProjectile) {
            // if done on movement, psycho mine becomes airborne when it starts tracking player and stops looping
            if (prevPosY == Fixed(0) && getPosY() > Fixed(0)) {
                airborne = true; // i think we should go by statusKey instead?
            }
        }
        // let projectiles land though - akuma air fb and mai charged fan bounce
        if (prevPosY > Fixed(0) && getPosY() - Fixed(landingAdjust) == Fixed(0)) {
            airborne = false;
            landed = true;
        }

        counterState = false;
        punishCounterState = false;
        forceKnockDownState = false;

        if (pActionJson->contains("SwitchKey"))
        {
            for (auto& [keyID, key] : (*pActionJson)["SwitchKey"].items())
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
                if (flag & 0x400000) {
                    forceKnockDownState = true;
                }
                if (flag & 0x2) {
                    counterState = true;
                }

                int operation = key["OperationFlag"];

                if (operation & 1) {
                    log (logTransitions, "force landing op");
                    forceLanding = true;
                }
            }
        }

        if (pActionJson->contains("ExtSwitchKey"))
        {
            for (auto& [keyID, key] : (*pActionJson)["ExtSwitchKey"].items())
            {
                if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int validStyles = key["_ValidStyle"];
                if ( validStyles != 0 && !(validStyles & (1 << styleInstall)) ) {
                    continue;
                }

                int flag = key["SystemFlag"];

                if (flag & 0x8000000) {
                    isDrive = true;
                }
                if (flag & 0x800000) {
                    punishCounterState = true;
                }
                if (flag & 0x400000) {
                    forceKnockDownState = true;
                }
                if (flag & 0x2) {
                    counterState = true;
                }
            }
        }

        if (pActionJson->contains("EventKey"))
        {
            for (auto& [keyID, key] : (*pActionJson)["EventKey"].items())
            {
                if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int validStyles = key["_ValidStyle"];
                if ( validStyles != 0 && !(validStyles & (1 << styleInstall)) ) {
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
                            Fixed posOffset = Fixed((int)param1);
                            Fixed steerForward;
                            steerForward.data = param2;
                            Fixed steerBackward;
                            steerBackward.data = param3;

                            if (posOffset != Fixed(0)) {
                                bool selfOffset = key["_IsOWNER_SELF_OFFSET"];
                                bool opponentOffset = key["_IsOWNER_RIVAL_OFFSET"];

                                if (selfOffset) {
                                    posX = posX + posOffset * direction;
                                } else if (opponentOffset) {
                                    if (pOpponent) {
                                        posX = pOpponent->getPosX() + posOffset * direction;
                                    }
                                } else {
                                    log(logUnknowns, "unimplemented owner offset");
                                }
                            }

                            if (currentInput & FORWARD) {
                                posX += steerForward * direction;
                                //log(true, "steerForward " + std::to_string(steerForward));
                            } else if (currentInput & BACK) {
                                posX += steerBackward * direction;
                                //log(true, "steerBackward " + std::to_string(steerBackward));
                            }
                        }
                        break;
                    case 1:
                        log(logUnknowns, "mystery system event 1, flags " + std::to_string(param1));
                        break;
                    case 2:
                        if (key["_IsCHARA_STYLE_CHANGE"]) {
                            if (param1 == 0) { // 0 is set, 1 is add? look at jamie
                                // todo wire up Style TerminateState
                                styleInstall = param2;
                            } else if (param1 == 1) {
                                styleInstall += param2;
                            } else {
                                log(logUnknowns, "unknown operator in chara event style change");
                            }
                            if (param3 == 1) {
                                styleInstallFrames = param4;
                            }
                        } else if (key["_IsCHARA_GAUGE_ADD"]) {
                            // todo - see walk forward, etc - param1 is type of bar? 4 for drive
                        } else {
                            if (airborne) {
                                if (param1 == 0) {
                                    airActionCounter = param2;
                                } else if (param1 == 1) {
                                    airActionCounter += param2;
                                } else {
                                    log(logUnknowns, "unknown operator in chara event");
                                }
                                if (airActionCounter < 0) {
                                    airActionCounter = 0;
                                }
                                if (styleInstallFrames > 0) {
                                    // i can't find a flag that differentiates those 2 EventKeys
                                    // so it must be airbrone vs not but i refuse to believe it
                                    // it would mean you couldn't have an air attack that deducts
                                    // install timer like mini-booms.................
                                    log (true, "kinda ambiguous, worth a look");
                                }
                            } else if (styleInstall > 0 && styleInstallFrames > 0) {
                                if (param1 == 0) {
                                    styleInstallFrames = param2;
                                } else if (param1 == 1) {
                                    styleInstallFrames += param2;
                                } else {
                                    log(logUnknowns, "unknown operator in chara event");
                                }
                            } else {
                                log(true, "no style install timer or airborne but point deduction?");
                            }
                        }
                        break;
                    case 7:
                        if (param3 == 0) { // set
                            bool isParam = key["_IsUNIQUE_UNIQUE_PARAM"];
                            if (isParam) {
                                uniqueParam[param2] = param4;
                            } else {
                                // dhalsim unique timer, only one that doesn't have this set?
                                // if it's not a param, clearly it's a timer :harold:
                                uniqueTimer = true;
                                uniqueTimerCount = param4;
                            }
                        } else if (param3 == 1) { //add
                            uniqueParam[param2] += param4;
                            // param5 appears to be the limit
                            if (param4 > 0 && uniqueParam[param2] > param5) {
                                uniqueParam[param2] = param5;
                            }
                            if (param4 < 0 && uniqueParam[param2] < param5) {
                                uniqueParam[param2] = param5;
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
                // todo should go to parent?
                styleInstall = 0;
            }
        }

        if (pActionJson->contains("ShotKey"))
        {
            for (auto& [keyID, key] : (*pActionJson)["ShotKey"].items())
            {
                if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                    continue;
                }

                int validStyles = key["_ValidStyle"];
                int operation = key["Operation"];
                
                if ( validStyles != 0 && !(validStyles & (1 << styleInstall)) ) {
                    continue;
                }

                if (operation == 2) {
                    if (pParent == nullptr) {
                        log(logUnknowns, "shotkey despawn but no parent?");
                    }
                    die = true;
                } else {
                    Fixed posOffsetX = Fixed(key["PosOffset"]["x"].get<double>()) * direction;
                    Fixed posOffsetY = Fixed(key["PosOffset"]["y"].get<double>());

                    // spawn new guy
                    Guy *pNewGuy = new Guy(*this, posOffsetX, posOffsetY, key["ActionId"].get<int>(), key["StyleIdx"].get<int>(), true);
                    pNewGuy->PreFrame();
                    if (pParent) {
                        pParent->minions.push_back(pNewGuy);
                    } else {
                        minions.push_back(pNewGuy);
                    }
                }
            }
        }

        if (pActionJson->contains("WorldKey"))
        {
            bool tokiToTomare = false;
            for (auto& [keyID, key] : (*pActionJson)["WorldKey"].items())
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
                        pOpponent->addWarudo(key["_StartFrame"].get<int>() - currentFrame + 1, true);
                    }
                }
            }
        }

        if (pActionJson->contains("LockKey"))
        {
            for (auto& [keyID, key] : (*pActionJson)["LockKey"].items())
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
                        pOpponent->locked = true;
                        pOpponent->hitStun = 50000;
                        // test
                        pOpponent->direction = direction;
                        pOpponent->posX = posX;
                        pOpponent->posY = posY;
                    }
                } else if (type == 2) {
                    // apply hit DT param 02
                    std::string hitIDString = to_string_leading_zeroes(param02, 3);
                    nlohmann::json *pHitEntry = &(*pHitJson)[hitIDString]["common"]["0"]; // going by crowd wisdom there
                    if (pOpponent) {
                        pOpponent->hitStun = 0;
                        pOpponent->locked = false;
                        pOpponent->ApplyHitEffect(pHitEntry, false, true, false, false);
                    }
                }
            }
        }
        
        // steer/etc could have had side effects there
        UpdateBoxes();
    }

    return true;
}

void Guy::ExecuteTrigger(nlohmann::json *pTrigger)
{
    nextAction = (*pTrigger)["action_id"];
    // apply condition flags BCM.TRIGGER.TAG.NEW_ID.json to make jamie jump cancel divekick work
    uint64_t flags = (*pTrigger)["category_flags"];

    if (flags & (1ULL<<26)) {
        jumped = true;

        if (flags & (1ULL<<46)) {
            jumpDirection = 1;
        } else if (flags & (1ULL<<47)) {
            jumpDirection = -1;
        } else {
            jumpDirection = 0;
        }
    }
}

bool Guy::CheckTriggerGroupConditions(int conditionFlag, int stateFlag)
{
    // condition bits
    // bit 0 = on hit
    // bit 1 = on block
    // bit 2 = on whiff
    // bit 3 = on armor
    // bit 10 is set almost everywhere, but unknown as of yet (PP?)
    // bit 11 = on counter/atemi
    // bit 12 = on parry

    // is 'this move' really broadly correct? or only for defers
    // maybe that's what mystery bit 10 governs
    bool conditionMet = false;
    bool anyHitThisMove = hitThisMove || hitCounterThisMove || hitPunishCounterThisMove;
    if ( conditionFlag & (1<<0) && anyHitThisMove) {
        conditionMet = true;
    }
    if ( conditionFlag & (1<<1) && hasBeenBlockedThisMove) {
        conditionMet = true;
    }
    // todo don't forget to add 'not parried' there
    if ( conditionFlag & (1<<2) &&
        (!anyHitThisMove && !hasBeenBlockedThisMove &&
        !hitAtemiThisMove && !hitArmorThisMove)) {
        conditionMet = true;
    }
    if ( conditionFlag & (1<<3) && hitArmorThisMove) {
        conditionMet = true;
    }
    if ( conditionFlag & (1<<11) && hitAtemiThisMove) {
        conditionMet = true;
    }
    // todo hit parry

    if (!conditionMet) {
        return false;
    }

    bool checkingState = false;
    bool stateMatch = false;
    if (stateFlag & (1<<18)) {
        checkingState = true;
        if (jumped && jumpDirection == 0) {
            stateMatch = true;
        }
    }
    if (stateFlag & (1<<19)) {
        checkingState = true;
        if (jumped && jumpDirection == 1) {
            stateMatch = true;
        }
    }
    if (stateFlag & (1<<20)) {
        checkingState = true;
        if (jumped && jumpDirection == -1) {
            stateMatch = true;
        }
    }
    // if (!!(stateFlag & (1<<4)) == jumped) {
    //     stateMatch = true;
    // }

    if (checkingState && !stateMatch) {
        return false;
    }

    return true;
}

bool Guy::CheckTriggerConditions(nlohmann::json *pTrigger, int triggerID)
{
    int paramID = (*pTrigger)["cond_param_id"];
    if ((*pTrigger)["_UseUniqueParam"] == true && paramID >= 0 && paramID < Guy::uniqueParamCount) {
        int op = (*pTrigger)["cond_param_ope"];
        int value = (*pTrigger)["cond_param_value"];
        bool allowTrigger = false;
        switch (op) {
            case 0:
                if (value == uniqueParam[paramID]) allowTrigger = true;
                break;
            case 1:
                if (value != uniqueParam[paramID]) allowTrigger = true;
                break;
            case 2:
                if (uniqueParam[paramID] < value) allowTrigger = true;
                break;
            case 3:
                if (uniqueParam[paramID] <= value) allowTrigger = true;
                break;
            case 4:
                if (uniqueParam[paramID] > value) allowTrigger = true;
                break;
            case 5:
                if (uniqueParam[paramID] >= value) allowTrigger = true;
                break;
            // supposedly AND - unused as of yet?
            // case 6:
            //     if (uniqueParam[paramID] & value) allowTrigger = true;
            //     break;                }
            default:
                break;
        }
        if (!allowTrigger) {
            return false;
        }
    }
    int limitShotCount = (*pTrigger)["cond_limit_shot_num"];
    if (limitShotCount) {
        int count = 0;
        int limitShotCategory = (*pTrigger)["limit_shot_category"];
        for ( auto minion : minions ) {
            if (limitShotCategory & (1 << minion->limitShotCategory)) {
                count++;
            }
        }
        if (count >= limitShotCount) {
            return false;
        }
    }

    int airActionCountLimit = (*pTrigger)["cond_jump_cmd_count"];
    if (airActionCountLimit) {
        if (airActionCounter >= airActionCountLimit) {
            return false;
        }
    }

    int vitalOp = (*pTrigger)["cond_vital_ope"];
    if (vitalOp != 0) {
        float vitalRatio = (float)health / maxHealth * 100;

        switch (vitalOp) {
            case 2:
                if (vitalRatio > (*pTrigger)["cond_vital_ratio"]) {
                    // todo figure out exact rounding rules here
                    return false;
                }
                break;
            default:
                log(logUnknowns, "unknown vital op on trigger " + std::to_string(triggerID));
                return false;
                break;
        }
    }

    bool checkingRange = false;
    bool rangeCheckMatch = true;
    int rangeCondition = (*pTrigger)["cond_range"];
    Fixed rangeParam = Fixed((*pTrigger)["cond_range_param"].get<double>());

    if (rangeCondition) {
        checkingRange = true;
    }

    switch (rangeCondition) {
        case 3:
        case 4:
        case 5:
            if (getPosY() < rangeParam) {
                rangeCheckMatch = false;
            }
            if (rangeCondition == 4 && velocityY < Fixed(0)) {
                // rnage check on the way up? waive if going down
                rangeCheckMatch = true;
            }
            if (rangeCondition == 5 && velocityY > Fixed(0)) {
                // rnage check on the way down? waive if going up
                rangeCheckMatch = true;
            }
            break;
        default:
            log(logUnknowns, "unimplemented range cond " + std::to_string(rangeCondition));
            break;
        case 0:
            break;
    }

    if (checkingRange && !rangeCheckMatch) {
        return false;
    }

    int stateCondition = (*pTrigger)["cond_owner_state_flags"];
    if (stateCondition && !(stateCondition & (1 << (getPoseStatus() - 1)))) {
        return false;
    }

    return true;
}

bool Guy::CheckTriggerCommand(nlohmann::json *pTrigger)
{
    nlohmann::json *pNorm = &(*pTrigger)["norm"];
    int commandNo = (*pNorm)["command_no"];
    uint32_t okKeyFlags = (*pNorm)["ok_key_flags"];
    uint32_t okCondFlags = (*pNorm)["ok_key_cond_flags"];
    uint32_t dcExcFlags = (*pNorm)["dc_exc_flags"];
    // condflags..
    // 10100000000100000: M oicho, but also eg. 22P - any one of three button mask?
    // 10100000001100000: EX, so any two out of three button mask?
    // 00100000000100000: heavy punch with one button mask
    // 00100000001100000: normal throw, two out of two mask t
    // 00100000010100000: taunt, 6 out of 6 in mask
    uint32_t i = 0, initialI = 0;
    bool initialMatch = false;
    // current frame + buffer - todo globalInputBuffer -> preceding_time?
    uint32_t initialSearch = 1 + globalInputBufferLength + timeInWarudo;
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
            // simple single-input command, initial match is enough
            return true;
        } else {
            std::string commandNoString = to_string_leading_zeroes(commandNo, 2);
            nlohmann::json *pCommand = &(*pCommandsJson)[commandNoString]["0"];
            int inputID = (*pCommand)["input_num"].get<int>() - 1;
            nlohmann::json *pInputs = &(*pCommand)["inputs"];

            uint32_t inputBufferCursor = i;

            while (inputID >= 0 )
            {
                nlohmann::json *pInput = &(*pInputs)[to_string_leading_zeroes(inputID, 2)];
                nlohmann::json *pInputNorm = &(*pInput)["normal"];
                uint32_t inputOkKeyFlags = (*pInputNorm)["ok_key_flags"];
                uint32_t inputOkCondFlags = (*pInputNorm)["ok_key_cond_check_flags"];
                int numFrames = (*pInput)["frame_num"];
                bool match = false;
                int lastMatchInput = i;

                if (inputOkKeyFlags & 0x10000)
                {
                    // charge release
                    bool chargeMatch = false;
                    nlohmann::json *pResourceMatch;
                    int chargeID = inputOkKeyFlags & 0xFF;
                    for (auto& [keyID, key] : pChargeJson->items()) {
                        // support either charge format
                        if (key.contains("resource")) {
                            key = key["resource"];
                        }
                        if (key["charge_id"] == chargeID ) {
                            pResourceMatch = &key;
                            chargeMatch = true;
                            break;
                        }
                    }

                    if (chargeMatch) {
                        uint32_t inputOkKeyFlags = (*pResourceMatch)["ok_key_flags"];
                        uint32_t inputOkCondFlags = (*pResourceMatch)["ok_key_cond_check_flags"];
                        uint32_t chargeFrames = (*pResourceMatch)["ok_frame"];
                        uint32_t keepFrames = (*pResourceMatch)["keep_frame"];
                        uint32_t dirCount = 0;
                        // uint32_t dirNotMatchCount = 0;
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
                            }
                            // else {
                            //     dirNotMatchCount++;
                            // }
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
                    // corner case - if we run out of buffer but had matched the input, finalize
                    // the match now or the trigger won't work - this can happen if you eg. have
                    // a dash input immediately at the beginning of a replay since the last match
                    // is a neutral input, it keeps matching all the way to the beginning of the
                    // buffer
                    if (match == true) {
                        inputID--;
                    }

                    break;
                }
            }

            if (inputID < 0) {
                // ran out of matched inputs, command is OK
                return true;
            }
        }
    }

    return false;
}

struct Trigger {
    std::string triggerName;
    bool deferred;
    int triggerGroup;
};

void Guy::DoTriggers()
{
    bool foundDeferTriggerGroup = false;

    if (pActionJson->contains("TriggerKey"))
    {
        std::map<int,Trigger> mapTriggers; // will sort all the valid triggers by ID

        for (auto& [keyID, key] : (*pActionJson)["TriggerKey"].items())
        {
            if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                continue;
            }

            int validStyles = key["_ValidStyle"];
            if ( validStyles != 0 && !(validStyles & (1 << styleInstall)) ) {
                continue;
            }

            // todo _Input for modern vs. classic

            bool defer = !key["_NotDefer"];
            int triggerGroup = key["TriggerGroup"];
            int condition = key["_Condition"];
            int state = key["_State"];

            if (deferredTriggerGroup != -1 && triggerGroup == deferredTriggerGroup) {
                foundDeferTriggerGroup = true;
            }

            if (!defer && !CheckTriggerGroupConditions(condition, state)) {
                continue;
            }

            if (deferredTriggerGroup != -1 && triggerGroup == deferredTriggerGroup && !defer) {
                // found non-deferred triggergroup and passed condition, trigger action now
                ExecuteTrigger(pDeferredTrigger);

                int triggerActionID = (*pDeferredTrigger)["action_id"];
                log(logTriggers, "did deferred trigger " + std::to_string(triggerActionID));

                pDeferredTrigger = nullptr;
                deferredTriggerGroup = -1;
                // don't need to run the bit at the end there, and nothing else has side-effects here
                return;
            }

            auto triggerGroupString = to_string_leading_zeroes(triggerGroup, 3);
            for (auto& [keyID, key] : (*pTriggerGroupsJson)[triggerGroupString].items())
            {
                int triggerID = atoi(keyID.c_str());
                Trigger newTrig = { key, defer, triggerGroup };
                // interleave all the triggers from groups in that map, sorted by ID
                mapTriggers[triggerID] = newTrig;
            }
        }

        // walk in reverse sorted trigger ID order
        for (auto it = mapTriggers.rbegin(); it != mapTriggers.rend(); it++)
        {
            int triggerID = it->first;
            std::string actionString = it->second.triggerName;
            bool defer = it->second.deferred;
            int triggerGroup = it->second.triggerGroup;
            int actionID = atoi(actionString.substr(0, actionString.find(" ")).c_str());

            nlohmann::json *pMoveJson = nullptr;
            if (FindMove(actionID, styleInstall, &pMoveJson) == nullptr) {
                continue;
            }

            auto triggerIDString = std::to_string(triggerID);
            auto actionIDString = to_string_leading_zeroes(actionID, 4);

            nlohmann::json *pTrigger = nullptr;

            for (auto& [keyID, key] : (*pTriggersJson)[actionIDString].items()) {
                if ( atoi(keyID.c_str()) == triggerID ) {
                    pTrigger = &key;
                    break;
                }
            }

            if (!CheckTriggerConditions(pTrigger, triggerID)) {
                continue;
            }

            if (CheckTriggerCommand(pTrigger)) {
                if (defer) {
                    // queue the deferred trigger
                    pDeferredTrigger = pTrigger;
                    deferredTriggerGroup = triggerGroup;
                    // don't immediately forget it at the end there
                    foundDeferTriggerGroup = true;
                } else {
                    ExecuteTrigger(pTrigger);

                    // assume non-deferred trigger can take precedence for now
                    pDeferredTrigger = nullptr;
                    deferredTriggerGroup = -1;
                }

                // consume the input by removing matching edge bits from matched initial input
                // todo figure out if that's needed
                // we cant always consume or you couldn't defer eg. SA3 + CA for later check
                // you'd think buffer length just takes care of this, check chains after rework rework
                // inputBuffer[initialI] &= ~((okKeyFlags & (LP+MP+HP+LK+MK+HK)) << 6);

                log(logTriggers, "trigger " + actionIDString + " " + triggerIDString + " defer " + std::to_string(defer));
                break; // we found our trigger walking back, skip all other triggers
            }
        }
    }

    if (deferredTriggerGroup != -1 && !foundDeferTriggerGroup) {
        // some deferred trigger groups seem to be dangling and to not have a matching non-defer
        log(logTriggers, "forgetting deferred trigger");
        pDeferredTrigger = nullptr;
        deferredTriggerGroup = -1;
    }
}

void Guy::UpdateBoxes(void)
{
    pushBoxes.clear();
    hitBoxes.clear();
    hurtBoxes.clear();
    renderBoxes.clear();
    throwBoxes.clear();

    if (pActionJson->contains("DamageCollisionKey"))
    {
        bool drive = isDrive || wasDrive;
        bool parry = currentAction >= 480 && currentAction <= 489;
        // doesn't work for all chars, prolly need to find a system bit like drive
        bool di = currentAction >= 850 && currentAction <= 859;

        std::deque<HurtBox> newHurtBoxes;

        for (auto& [hurtBoxID, hurtBox] : (*pActionJson)["DamageCollisionKey"].items())
        {
            if ( !hurtBox.contains("_StartFrame") || hurtBox["_StartFrame"] > currentFrame || hurtBox["_EndFrame"] <= currentFrame ) {
                continue;
            }

            bool isArmor = hurtBox["_isArm"];
            int armorID = hurtBox["AtemiDataListIndex"];
            bool isAtemi = hurtBox["_isAtm"];
            int immune = hurtBox["Immune"];
            int typeFlags = hurtBox["TypeFlag"];

            Fixed rootOffsetX = Fixed(0);
            Fixed rootOffsetY = Fixed(0);
            parseRootOffset( hurtBox, rootOffsetX, rootOffsetY );
            rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
            rootOffsetY = rootOffsetY + posY + posOffsetY;


            Box rect;
            int magicHurtBoxID = 8; // i hate you magic array of boxes

            HurtBox baseBox;
            if (isArmor) {
                baseBox.flags |= armor;
                baseBox.armorID = armorID;
            }
            if (isAtemi) {
                baseBox.flags |= atemi;
            }
            // those are from gelatin's viewer.. why are they not actual flags?
            // every normal hurtbox has typeFlags == 3 so clearly it's not just flags
            if (typeFlags == 1) {
                baseBox.flags |= projectile_invul;
            }
            if (typeFlags == 2) {
                baseBox.flags |= full_strike_invul;
            }
            if (immune == 4) {
                baseBox.flags |= air_strike_invul;
            }
            if (immune == 11) {
                baseBox.flags |= ground_strike_invul;
            }
            for (auto& [boxNumber, boxID] : hurtBox["HeadList"].items()) {
                if (GetRect(rect, magicHurtBoxID, boxID,rootOffsetX, rootOffsetY,direction.i())) {
                    HurtBox newBox = baseBox;
                    newBox.box = rect;
                    newBox.flags |= head;
                    newHurtBoxes.push_front(newBox);
                }
            }
            for (auto& [boxNumber, boxID] : hurtBox["BodyList"].items()) {
                if (GetRect(rect, magicHurtBoxID, boxID,rootOffsetX, rootOffsetY,direction.i())) {
                    HurtBox newBox = baseBox;
                    newBox.box = rect;
                    newBox.flags |= body;
                    newHurtBoxes.push_front(newBox);
                }
            }
            for (auto& [boxNumber, boxID] : hurtBox["LegList"].items()) {
                if (GetRect(rect, magicHurtBoxID, boxID,rootOffsetX, rootOffsetY,direction.i())) {
                    HurtBox newBox = baseBox;
                    newBox.box = rect;
                    newBox.flags |= legs;
                    newHurtBoxes.push_front(newBox);
                }
            }

            for (auto& [boxNumber, boxID] : hurtBox["ThrowList"].items()) {
                if (GetRect(rect, 7, boxID,rootOffsetX, rootOffsetY,direction.i())) {
                    throwBoxes.push_back(rect);
                    renderBoxes.push_back({rect, 35.0, {0.15,0.20,0.8}, drive,parry,di});
                }
            }
        }

        // we queued the hurtboxes in newHurtBoxes in reverse order, and now we add them to the real list
        // hurtboxes at the end of the list are meant to be checked first, and take precedence
        // basing that off of armor moves having the armor box at the end

        for (auto box : newHurtBoxes) {
            hurtBoxes.push_back(box);
            if (box.flags & armor) {
                renderBoxes.push_back({box.box, 30.0, {0.8,0.5,0.0}, drive,parry,di});
            } else {
                renderBoxes.push_back({box.box, (box.flags & head) ? 17.5f : 25.0f, {charColorR,charColorG,charColorB}, drive,parry,di});
            }
        }
    }
    if (pActionJson->contains("PushCollisionKey"))
    {
        for (auto& [pushBoxID, pushBox] : (*pActionJson)["PushCollisionKey"].items())
        {
            if ( !pushBox.contains("_StartFrame") || pushBox["_StartFrame"] > currentFrame || pushBox["_EndFrame"] <= currentFrame ) {
                continue;
            }
            Fixed rootOffsetX = Fixed(0);
            Fixed rootOffsetY = Fixed(0);
            parseRootOffset( pushBox, rootOffsetX, rootOffsetY );
            rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
            rootOffsetY = rootOffsetY + posY + posOffsetY;

            Box rect;

            if (GetRect(rect, 5, pushBox["BoxNo"],rootOffsetX, rootOffsetY, direction.i())) {
                pushBoxes.push_back(rect);
                renderBoxes.push_back({rect, 30.0, {0.4,0.35,0.0}});            
            }
        }
    }

    DoHitBoxKey("AttackCollisionKey");
    DoHitBoxKey("OtherCollisionKey");
}

void Guy::Render(void) {
    Fixed fixedX = posX + (posOffsetX * direction);
    Fixed fixedY = posY + posOffsetY;
    float x = fixedX.f();
    float y = fixedY.f();

    for (auto box : renderBoxes) {
        drawHitBox(box.box,thickboxes?box.thickness:1,box.col,box.drive,box.parry,box.di);
    }

    if (renderPositionAnchors) {
        float radius = 16.5;
        drawBox(x-radius/2,y-radius/2,radius,radius,radius,1.0,1.0,1.0,0.2);
        radius = 15;
        drawBox(x-radius/2,y-radius/2,radius,radius,radius,charColorR,charColorG,charColorB,0.2);
    }
}

bool Guy::Push(Guy *pOtherGuy)
{
    //if (warudo) return false;
    if (didPush) return false;
    if ( !pOtherGuy ) return false;
    // for now, maybe there's other rules
    if (isProjectile) return false;

    bool hasPushed = false;
    touchedOpponent = false;
    Fixed pushXLeft = 0;
    Fixed pushXRight = 0;
    for (auto pushbox : pushBoxes ) {

        if (noPush) break;

        for (auto otherPushBox : *pOtherGuy->getPushBoxes() ) {
            if (doBoxesHit(pushbox, otherPushBox)) {

                pushXLeft = fixMax(pushXLeft, pushbox.x + pushbox.w - otherPushBox.x);
                pushXRight = fixMin(pushXRight, pushbox.x - (otherPushBox.x + otherPushBox.w));
                hasPushed = true;
            }
        }
    }

    if ( hasPushed ) {
        pushXLeft = fixMax(Fixed(0), pushXLeft);
        pushXRight = fixMin(Fixed(0), pushXRight);
        Fixed pushNeeded = -pushXRight;
        if (fixAbs(pushXLeft) < fixAbs(pushXRight)) {
            pushNeeded = -pushXLeft;
        }
        Fixed velDiff = velocityX * direction + pOtherGuy->velocityX * pOtherGuy->direction;
        log(logTransitions, "push needed " + std::to_string(pushNeeded.data) + " vel diff " + std::to_string(velDiff.f()) + " offset no push " + std::to_string(offsetDoesNotPush));
        // if (velDiff * pushNeeded < 0.0) {
        //     // if velDiff different sign, we can deduct it
        //     if (fabsf(velDiff) > fabsf(pushNeeded)) {
        //         float velDiffSign = velDiff / fabsf(velDiff);
        //         velDiff = fabsf(pushNeeded) * velDiffSign;
        //     }
        //     if (velocityX > pOtherGuy->velocityX) {
        //         pOtherGuy->posX += velDiff;
        //     } else {
        //         posX += velDiff;
        //     }
        //     pushNeeded += velDiff;
        // }
        // log(logTransitions, "push still needed " + std::to_string(pushNeeded));


        if (pushNeeded != Fixed(0)) {

            // we only handle this one direction for now - let the other push witht hat logic
            // if both have it, we currently do order-dependent handling, need to check what
            // happens in reality (two simultaneous dashes, with and without gap)
            if (!offsetDoesNotPush && pOpponent->offsetDoesNotPush) {
                return false;
            }

            // does no-push-offset go against push? (different sign)
            if (offsetDoesNotPush && posOffsetX * pushNeeded < Fixed(0)) {
                 Fixed absOffset = fixAbs(posOffsetX);
                Fixed absPushNeeded = fixAbs(pushNeeded);

                if (absPushNeeded > absOffset) {
                    posOffsetX = Fixed(0);
                    absPushNeeded -= absOffset;
                    // restore sign
                    pushNeeded = absPushNeeded * (pushNeeded / fixAbs(pushNeeded));
                } else {
                    pushNeeded = Fixed(0);
                    absOffset -= absPushNeeded;
                    // restore sign
                    posOffsetX = absOffset * (posOffsetX / fixAbs(posOffsetX));
                }
                posX += pushNeeded;
            }

            Fixed halfPushNeeded = pushNeeded / Fixed(2);

            // do regular push with any remaining pushNeeded
            posX = posX + halfPushNeeded;
            pOtherGuy->posX = pOtherGuy->posX - halfPushNeeded;

            int fixedRemainder = pushNeeded.data - halfPushNeeded.data * 2;
            int frameNumber = globalFrameCount;
            if (replayFrameNumber != 0) {
                frameNumber = replayFrameNumber;
            }

            //log(logTransitions, "fixedRemainder " + std::to_string(fixedRemainder) + " frameNum " +  std::to_string(frameNumber) + " " + getCharacter());

            // give remainder to either player depending on frame count
            if (frameNumber & 1) {
                posX.data += fixedRemainder;
            } else {
                pOtherGuy->posX.data += fixedRemainder;
            }

            didPush = true;
            pOtherGuy->didPush = true;
        }

        touchedOpponent = true; // could be touching anyone really but can fix later
        pOtherGuy->touchedOpponent = true;
        UpdateBoxes();
        pOtherGuy->UpdateBoxes();
        return true;
    }

    return false;
}

bool Guy::WorldPhysics(void)
{
    bool hasPushed = false;
    Fixed pushX = 0;
    bool floorpush = false;
    touchedWall = false;
    didPush = false;

    const Fixed wallDistance = Fixed(765.0f);
    const Fixed maxPlayerDistance = Fixed(490.0f);

    if (!noPush) {
        // Floor

        if (getPosY() - Fixed(landingAdjust) < 0) {
            //log("floorpush pos");
            floorpush = true;
            hasPushed = true;
        }

        // Walls

        Fixed x = getPosX();
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

        if (pOpponent && velocityX != Fixed(0)) {
            Fixed opX = pOpponent->getPosX();
            if (fixAbs(opX - x) > maxPlayerDistance) {
                Fixed directionToOpponent = (opX - x) / fixAbs(opX - x);
                // if moving away from opponent, obey virtual wall
                if (directionToOpponent == direction) {
                    touchedWall = true;
                    hasPushed = true;

                    pushX = (fixAbs(opX - x) - maxPlayerDistance) * direction;
                }
            }
        }
    }

    // if we're going up (like jsut getting hit), we're not landing,
    // just being helped off the ground - see heavy donkey into lp dp
    if (forceLanding || (airborne && floorpush && velocityY < 0))
    {
        velocityX = Fixed(0);
        velocityY = Fixed(0);
        accelX = Fixed(0);
        accelY = Fixed(0);

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

    if (landed && forceKnockDown) {
        isDown = true; // can't let stuff hit us on landing frame - see donkey kick into mp dp
    }

    if ( hasPushed ) {
        posX += pushX;

        if (pushBackThisFrame != Fixed(0) && pushX != Fixed(0) && pushX * pushBackThisFrame < Fixed(0)) {
            // some pushback went into the wall, it needs to go into opponent
            if (pAttacker && !pAttacker->noPush && !noCounterPush) {
                pAttacker->posX += fixMax(pushX, pushBackThisFrame * Fixed(-1));
                pAttacker->UpdateBoxes();
            }
        }

        UpdateBoxes();
    }

    if (landed || forceLanding || bounced || floorpush) {
        // don't update hitboxes before setting posY, the current frame
        // or the box will be too high up as we're still on the falling box
        // see heave donky into lp dp
        posY = Fixed(0);
        posOffsetY = Fixed(0);
    }

    forceLanding = false;

    return hasPushed;
}

bool Guy::CheckHit(Guy *pOtherGuy)
{
    if (warudo) return false;
    if ( !pOtherGuy ) return false;

    bool retHit = false;
    for (auto hitbox : hitBoxes ) {
        if (hitbox.type != domain && ((1<<hitbox.hitID) & canHitID)) {
            continue;
        }
        if (hitbox.type == proximity_guard || hitbox.type == destroy_projectile) {
            // todo right now we do nothing with those
            continue;
        }
        bool isGrab = hitbox.type == grab;
        bool foundBox = false;
        HurtBox hurtBox;

        if (isGrab) {
            for (auto throwBox : *pOtherGuy->getThrowBoxes() ) {
                if (hitbox.type == domain || doBoxesHit(hitbox.box, throwBox)) {
                    foundBox = true;
                    hurtBox.box = throwBox;
                    break;
                }
            }
        } else {
            for (auto hurtbox : *pOtherGuy->getHurtBoxes() ) {
                if (hitbox.type == hit && hurtbox.flags & full_strike_invul) {
                    continue;
                }
                if (hitbox.type == projectile && hurtbox.flags & projectile_invul) {
                    continue;
                }
                // todo air/ground strike invul here
                if (hitbox.type == domain || doBoxesHit(hitbox.box, hurtbox.box)) {
                    hurtBox = hurtbox;
                    foundBox = true;
                    break;
                }
            }
        }

        if (foundBox) {
            std::string hitIDString = to_string_leading_zeroes(hitbox.hitEntryID, 3);
            int hitEntryFlag = 0;

            if (pOtherGuy->isDown) {
                // todo need to check for otg capability there i guess?
                // and cotninue instead of breaking!!! move up in the for hurtbox loop
                break;
            }

            bool otherGuyAirborne = pOtherGuy->getAirborne();

            bool hitFlagToParent = false;
            if (isProjectile && pActionJson->contains("pdata") &&
                (*pActionJson)["pdata"]["_HitFlagToPlayer"] == true && pParent) {
                // either this means "to both" - current code or touch branch checks different
                // flags - mai charged fan has a touch branch on the proj but sets this flag
                // also used by double geyser where the player has a trigger condition
                // todo checking touch branch on player would disambiguate
                hitFlagToParent = true;
            }

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
            bool otherGuyHit = pOtherGuy->hitStun && !pOtherGuy->blocking;
            bool otherGuyCanBlock = !otherGuyAirborne && !otherGuyHit && pOtherGuy->actionStatus != -1 && pOtherGuy->currentInput & BACK;
            if (isGrab) {
                otherGuyCanBlock = false;
            }
            if (pOtherGuy->blocking || otherGuyCanBlock) {
                hitEntryFlag = block;
                pOtherGuy->blocking = true;
                hasBeenBlockedThisFrame = true;
                if (hitFlagToParent) pParent->hasBeenBlockedThisFrame = true;
                hasBeenBlockedThisMove = true;
                if (hitFlagToParent) pParent->hasBeenBlockedThisMove = true;
                log(logHits, "block!");
            }

            if (isGrab && (pOtherGuy->blocking || pOtherGuy->hitStun)) {
                // a grab would whiff if opponent is in blockstun
                continue;
            }

            if ((hitbox.type == domain && !pOtherGuy->locked) ||
                (hitbox.type != domain && pOtherGuy->locked)) {
                // only domain when locked and domain only when locked
                // todo check if that's right, eg. see if jp can still combo out of grab
                // proabbly yes because it's actually the unlock juggle he comboes from?
                continue;
            }

            std::string hitEntryFlagString = to_string_leading_zeroes(hitEntryFlag, 2);
            nlohmann::json *pHitEntry = &(*pHitJson)[hitIDString]["param"][hitEntryFlagString];

            bool bombBurst = false;

            // back compat with old versions, this wasn't there before
            if (pHitEntry->contains("_bomb_burst")) {
                bombBurst = (*pHitEntry)["_bomb_burst"];
            }

            // if bomb burst and found a bomb, use the next hit ID instead 
            if (bombBurst && pOpponent->debuffTimer) {
                hitIDString = to_string_leading_zeroes(hitbox.hitEntryID + 1, 3);
                pHitEntry = &(*pHitJson)[hitIDString]["param"][hitEntryFlagString];
            }

            int destX = (*pHitEntry)["MoveDest"]["x"];
            int destY = (*pHitEntry)["MoveDest"]["y"];
            int hitHitStun = (*pHitEntry)["HitStun"];
            int dmgType = (*pHitEntry)["DmgType"];
            int moveType = (*pHitEntry)["MoveType"];
            int attr0 = (*pHitEntry)["Attr0"];
            int hitMark = (*pHitEntry)["Hitmark"];
            // we're hitting for sure after this point (modulo juggle), side effects

            bool hitArmor = false;
            if (hurtBox.flags & armor && hurtBox.armorID) {
                hitArmor = true;
                hitArmorThisFrame = true;
                if (hitFlagToParent) pParent->hitArmorThisFrame = true;
                hitArmorThisMove = true;
                if (hitFlagToParent) pParent->hitArmorThisMove = true;
                auto atemiIDString = std::to_string(hurtBox.armorID);
                // need to pull from opponents atemi here or put in opponent method
                nlohmann::json *pAtemi = nullptr;
                if (pOtherGuy->pAtemiJson->contains(atemiIDString)) {
                    pAtemi = &(*pOtherGuy->pAtemiJson)[atemiIDString];
                } else if (pCommonAtemiJson->contains(atemiIDString)) {
                    pAtemi = &(*pCommonAtemiJson)[atemiIDString];
                } else {
                    log(true, "atemi not found!!");
                    break;
                }

                int armorHitStopHitted = (*pAtemi)["TargetStop"];
                int armorHitStopHitter = (*pAtemi)["OwnerStop"];
                int armorBreakHitStopHitted = (*pAtemi)["TargetStopShell"]; // ??
                int armorBreakHitStopHitter = (*pAtemi)["OwnerStopShell"];

                if (pOtherGuy->currentArmorID != hurtBox.armorID) {
                    pOtherGuy->armorHitsLeft = (*pAtemi)["ResistLimit"].get<int>() + 1;
                    pOtherGuy->currentArmorID = hurtBox.armorID;
                }
                if ( pOtherGuy->currentArmorID == hurtBox.armorID ) {
                    pOtherGuy->armorHitsLeft--;
                    if (pOtherGuy->armorHitsLeft <= 0) {
                        hitArmor = false;
                        if (pOtherGuy->armorHitsLeft == 0) {
                            log(logHits, "armor break!");
                            addWarudo(armorBreakHitStopHitter+1);
                            pOtherGuy->addWarudo(armorBreakHitStopHitted+1);
                        }
                    } else {
                        // apply gauge effects here

                        pOtherGuy->armorThisFrame = true;

                        // todo i think this is wrong, it needs to add to the normal hitstop?
                        // there's TargetStopAdd too, figure it out at some point
                        addWarudo(armorHitStopHitter+1);
                        pOtherGuy->addWarudo(armorHitStopHitted+1);
                        log(logHits, "armor hit!");
                    }
                }
            }

            if (hurtBox.flags & atemi) {
                // like armor except onthing really happens beyond setting the flag
                hitArmor = true;
                hitAtemiThisFrame = true;
                if (hitFlagToParent) pParent->hitAtemiThisFrame = true;
                hitAtemiThisMove = true;
                if (hitFlagToParent) pParent->hitAtemiThisMove = true;
                pOtherGuy->atemiThisFrame = true;
                log(logHits, "atemi hit!");
            }

            // not hitstun for initial grab hit as we dont want to recover during the lock
            if ( hitArmor || pOtherGuy->ApplyHitEffect(pHitEntry, !isGrab, !isGrab, wasDrive, hitbox.type == domain) ) {
                int hitStopSelf = (*pHitEntry)["HitStopOwner"];
                if ( !hitArmor && hitStopSelf ) {
                    addWarudo(hitStopSelf+1);
                }

                Fixed hitMarkerOffsetX = hitbox.box.x - pOtherGuy->getPosX();
                if (direction > Fixed(0)) {
                    hitMarkerOffsetX += hitbox.box.w;
                }
                Fixed hitMarkerOffsetY = hitbox.box.y+hitbox.box.h/Fixed(2) - pOtherGuy->posY;
                int hitMarkerType = 1;
                float hitMarkerRadius = 25.0f;
                if (hasBeenBlockedThisFrame) {
                    hitMarkerType = 2;
                }
                if (hitEntryFlag & punish_counter) {
                    hitMarkerRadius = 35.0f;
                }
                addHitMarker({hitMarkerOffsetX.f(),hitMarkerOffsetY.f(),hitMarkerRadius,pOtherGuy,hitMarkerType, 0, 10});

                pOtherGuy->pAttacker = this;

                if (isGrab) {
                    grabbedThisFrame = true;
                    if (hitFlagToParent) pParent->grabbedThisFrame = true;
                }

                canHitID |= 1 << hitbox.hitID;

                if (!pOtherGuy->blocking && !hitArmor) {
                    if (hitEntryFlag & punish_counter) {
                        punishCounterThisFrame = true;
                        if (hitFlagToParent) pParent->punishCounterThisFrame = true;
                        hitPunishCounterThisMove = true;
                        if (hitFlagToParent) pParent->hitPunishCounterThisMove = true;
                    }
                    if (hitEntryFlag & counter) {
                        hitCounterThisMove = true;
                        if (hitFlagToParent) pParent->hitCounterThisMove = true;
                    }
                    hitThisFrame = true;
                    if (hitFlagToParent) pParent->hitThisFrame = true;
                    hitThisMove = true;
                    if (hitFlagToParent) pParent->hitThisMove = true;

                    int dmgKind = (*pHitEntry)["DmgKind"];

                    if (dmgKind == 11) {
                        // psycho mine spawner guy - style 0 probably OK unconditionally?
                        int actionID = 592; // IMM_VEGA_BOMB - should we find by name instead?
                        Guy *pNewGuy = new Guy(*this, Fixed(0), Fixed(0), actionID, 0, false);
                        pNewGuy->PreFrame();
                        if (pParent) {
                            pParent->minions.push_back(pNewGuy);
                        } else {
                            minions.push_back(pNewGuy);
                        }
                        pNewGuy->Frame(); // he should be dead after that
                    }

                    if (bombBurst) {
                        pOtherGuy->debuffTimer = 0;
                    }
                }
                retHit = true;
                log(logHits, "hit type " + std::to_string(hitbox.type) + " id " + std::to_string(hitbox.hitID) +
                    " dt " + hitIDString + " destX " + std::to_string(destX) + " destY " + std::to_string(destY) +
                    " hitStun " + std::to_string(hitHitStun) + " dmgType " + std::to_string(dmgType) +
                    " moveType " + std::to_string(moveType) );
                log(logHits, "attr0 " + std::to_string(attr0) + "hitmark " + std::to_string(hitMark));
            }
        }
        if (retHit) break;
    }

    return retHit;
}

bool Guy::ApplyHitEffect(nlohmann::json *pHitEffect, bool applyHit, bool applyHitStun, bool isDrive, bool isDomain)
{
    int juggleFirst = (*pHitEffect)["Juggle1st"];
    int juggleAdd = (*pHitEffect)["JuggleAdd"];
    int juggleLimit = (*pHitEffect)["JuggleLimit"];
    int hitEntryHitStun = (*pHitEffect)["HitStun"];
    int destX = (*pHitEffect)["MoveDest"]["x"];
    int destY = (*pHitEffect)["MoveDest"]["y"];
    int destTime = (*pHitEffect)["MoveTime"];
    int dmgValue = (*pHitEffect)["DmgValue"];
    int dmgType = (*pHitEffect)["DmgType"];
    int moveType = (*pHitEffect)["MoveType"];
    int floorTime = (*pHitEffect)["FloorTime"];
    int downTime = (*pHitEffect)["DownTime"];
    bool noZu = (*pHitEffect)["_no_zu"];
    bool jimenBound = (*pHitEffect)["_jimen_bound"];
    bool kabeBound = (*pHitEffect)["_kabe_bound"];
    bool kabeTataki = (*pHitEffect)["_kabe_tataki"];
    int hitStopTarget = (*pHitEffect)["HitStopTarget"];
    int dmgKind = (*pHitEffect)["DmgKind"];
    // int curveTargetID = hitEntry["CurveTgtID"];

    if (isDrive) {
        juggleAdd = 0;
        juggleFirst = 0;
        juggleLimit += 3;
    }

    if (!isDomain && airborne && juggleCounter > juggleLimit) {
        return false;
    }

    // like guile 4HK has destY but stays grounded if hits grounded
    if (!(dmgType & 8) && !airborne) {
        destY = 0;
    }

    if (jimenBound && !airborne) {
        jimenBound = false;
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
#ifdef __EMSCRIPTEN__
        emscripten_vibrate(hitStopTarget*2);
#endif
    }

    if (isDrive) {
        hitEntryHitStun += 4;
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
        if (forceKnockDownState) {
            hitEntryHitStun += 500000;
            forceKnockDown = true;
        }
        knockDownFrames = downTime;
    }

    if (moveType == 11 || moveType == 10) { //airborne crumples
        hitEntryHitStun += 500000;
        resetHitStunOnTransition = true;
        forceKnockDown = true;
        knockDownFrames = downTime;
    }

    health -= dmgValue; // todo scaling here

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

    if (jimenBound && floorTime) {
        int floorDestX = (*pHitEffect)["FloorDest"]["x"];
        int floorDestY = (*pHitEffect)["FloorDest"]["y"];

        groundBounce = true;
        groundBounceVelX = Fixed(-floorDestX) / Fixed(floorTime);
        groundBounceAccelX = Fixed(floorDestX / 4) / Fixed(floorTime * floorTime);
        groundBounceVelX -= groundBounceAccelX;

        groundBounceVelY = Fixed(floorDestY * 4) / Fixed(floorTime);
        groundBounceAccelY = Fixed(floorDestY * -8) / Fixed(floorTime * floorTime);
        groundBounceVelY -= groundBounceAccelY;
    } else {
        groundBounce = false;
    }

    wallSplat = false;
    wallBounce = false;
    int wallTime = (*pHitEffect)["WallTime"];
    if (kabeTataki) {
        // this can happen even if you block! blocked DI
        wallSplat = true;
        wallStopFrames = (*pHitEffect)["WallStop"];
    } else if (kabeBound && wallTime) {
        int wallDestX = (*pHitEffect)["WallDest"]["x"];
        int wallDestY = (*pHitEffect)["WallDest"]["y"];
        wallStopFrames = (*pHitEffect)["WallStop"];

        wallBounce = true;
        wallBounceVelX = Fixed(-wallDestX) / Fixed(wallTime);
        //wallBounceAccelX = -direction * wallDestX / 2.0 / (float)wallTime * 2.0 / (float)wallTime;
        //wallBounceVelX -= wallBounceAccelX;

        wallBounceVelY = Fixed(wallDestY * 4) / Fixed(wallTime);
        wallBounceAccelY = Fixed(wallDestY * -8) / Fixed(wallTime * wallTime);
        wallBounceVelY -= wallBounceAccelY;
    }

    // assume hit direction is opposite as facing for now, not sure if that's true
    if (destTime != 0) {
        if (!airborne && !jimenBound) {
            hitVelX = Fixed(direction.i() * destX * -2) / Fixed(destTime);
            hitAccelX = Fixed(direction.i() * destX * 2) / Fixed(destTime * destTime);
            hitAccelX.data += direction.i(); // there seems to be a bias of 1 raw units
        } else {
            hitVelX = Fixed(0);
            hitAccelX = Fixed(0);
            velocityX = Fixed(-destX) / Fixed(destTime);
        }

        if (destY != 0) {
            if (destY > 0) {
                velocityY = Fixed(destY * 4) / Fixed(destTime);
                accelY = Fixed(destY * -8) / Fixed(destTime * destTime);
                // i think this vel wants to apply this frame, lame workaround to get same intensity
                velocityY -= accelY; //
            } else {
                velocityY = Fixed(destY) / Fixed(destTime);
                accelY = Fixed(0);
            }
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
            if ((airborne || posY > Fixed(0)) && destY != 0 ) {

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

    // fire/elec/psychopower effect
    // the two that seem to matter for gameplay are 9 for poison and 11 for mine
    if (dmgKind == 11) {
        debuffTimer = 300;
    }

    return true;
}

void Guy::DoHitBoxKey(const char *name)
{
    if (pActionJson->contains(name))
    {
        for (auto& [hitBoxID, hitBox] : (*pActionJson)[name].items())
        {
            if ( !hitBox.contains("_StartFrame") || hitBox["_StartFrame"] > currentFrame || hitBox["_EndFrame"] <= currentFrame ) {
                continue;
            }

            int validStyles = hitBox["_ValidStyle"];
            if ( validStyles != 0 && !(validStyles & (1 << styleInstall)) ) {
                continue;
            }

            bool isOther = strcmp(name, "OtherCollisionKey") == 0;
            //bool isUnique = strcmp(name, "UniqueCollisionKey") == 0;

            Fixed rootOffsetX = 0;
            Fixed rootOffsetY = 0;
            parseRootOffset( hitBox, rootOffsetX, rootOffsetY );
            rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
            rootOffsetY += posY + posOffsetY;

            Box rect={-4096,-4096,8192,8192};

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

                float thickness = 50.0;
                if (type == proximity_guard) {
                    thickness = 5.0;
                }

                if ((type == domain) || GetRect(rect, rectListID, boxID,rootOffsetX, rootOffsetY,direction.i())) {
                    renderBoxes.push_back({rect, thickness, collisionColor, (isDrive || wasDrive) && collisionType != 3 });

                    int hitEntryID = hitBox["AttackDataListIndex"];
                    int hitID = hitBox["HitID"];
                    if (hitID < 0) {
                        hitID = 15; // overflow, that's the highest hit bit AFAIK
                    }
                    if (hitEntryID != -1) {
                        hitBoxes.push_back({rect,type,hitEntryID,hitID});
                    }
                }
            }
        }
    }
}

void Guy::DoBranchKey(bool preHit = false)
{
    if (pActionJson != nullptr && pActionJson->contains("BranchKey"))
    {
        for (auto& [keyID, key] : (*pActionJson)["BranchKey"].items())
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
            int64_t branchParam4 = key["Param04"];
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
                    if (hitThisMove) { // has hit ever this move.. not sure if right
                        doBranch = true;
                    }

                    break;
                case 4:
                    if (hasBeenBlockedThisMove) { // just this frame.. enough?
                        doBranch = true;
                    }

                    break;
                case 5: // swing.. not hit?
                    // todo not always right - JP's 4HK has some extra condition to get into (2)
                    if (!preHit && canHitID == 0) {
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
                case 12: // height
                    {
                        // param0 = whose height - 0 owner immediate, 1 target immediate, 2 target offset (?)
                        // would the offset go in param 3 maybe
                        Guy *pGuy = nullptr;
                        if (branchParam0 == 0) {
                            pGuy = this;
                        } else if (branchParam0 == 1) {
                            pGuy = pOpponent;
                        }
                        if (pGuy == nullptr) {
                            log(true, "that height branch not gonna work");
                        } else {
                            if (branchParam1 == 1 && pGuy->getPosY() < Fixed(branchParam2)) {
                                doBranch = true;
                            }
                            if (branchParam1 == 2 && pGuy->getPosY() <= Fixed(branchParam2)) {
                                doBranch = true;
                            }
                            if (branchParam1 == 3 && pGuy->getPosY() > Fixed(branchParam2)) {
                                doBranch = true;
                            }
                            if (branchParam1 == 4 && pGuy->getPosY() >= Fixed(branchParam2)) {
                                doBranch = true;
                            }
                        }
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
                case 16: // side
                    if (branchParam0 == 1 && !needsTurnaround()) {
                        doBranch = true;
                    }
                    if (branchParam0 == 2 && needsTurnaround()) {
                        doBranch = true;
                    }
                    if (branchParam0 == 3 && direction.i() > 0) {
                        doBranch = true;
                    }
                    if (branchParam0 == 4 && direction.i() < 0) {
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
                case 21: // armor
                    if (armorThisFrame) {
                        doBranch = true;
                    }
                    break;
                case 22: // atemi/counter
                    if (atemiThisFrame) {
                        doBranch = true;
                    }
                    break;
                case 29: // unique param
                    if (branchParam1 >= 0 && branchParam1 < Guy::uniqueParamCount) {
                        int paramValue = uniqueParam[branchParam1];
                        switch (branchParam2) {
                            case 0:
                                if (branchParam3 == paramValue) doBranch = true;
                                break;
                            case 1:
                                if (branchParam3 != paramValue) doBranch = true;
                                break;
                            case 2:
                                if (paramValue < branchParam3) doBranch = true;
                                break;
                            case 3:
                                if (paramValue <= branchParam3) doBranch = true;
                                break;
                            case 4:
                                if (paramValue > branchParam3) doBranch = true;
                                break;
                            case 5:
                                if (paramValue >= branchParam3) doBranch = true;
                                break;
                            // supposedly AND - unused as of yet?
                            // case 6:
                            //     if (paramValue & value) allowTrigger = true;
                            //     break;                }
                            default:
                                break;
                        }
                    }
                    break;
                case 30: // unique timer
                    if (uniqueTimer) {
                        if (uniqueTimerCount == branchParam2) {
                            doBranch = true;
                            uniqueTimer = false;
                        }
                    } else {
                        log(logUnknowns, "unique timer branch not in timer?");
                    }
                    if (branchParam1 != 5) {
                        log(logUnknowns, "unique timer branch param unknown");
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
                    // todo what if you hit armor? are there condition bits here?
                    if (hitThisFrame) {
                        doBranch = true;
                    }
                    break;
                case 40: // area target
                    if (pOpponent) {
                        int distX = branchParam2 & 0xFFFF;
                        int distY = (branchParam2 & 0xFFFF0000) >> 16;

                        int offsetX = branchParam1 & 0xFFFF;
                        if (offsetX > 0x8000) offsetX = -(0xFFFF - offsetX);

                        offsetX *= -pOpponent->direction.i();

                        int offsetY = (branchParam1 & 0xFFFF0000) >> 16;
                        if (offsetY > 0x8000) offsetY = -(0xFFFF - offsetY);

                        // and?
                        if (branchParam0 == 0 &&
                            (fixAbs(pOpponent->getPosX() - Fixed(offsetX) - getPosX()) < Fixed(distX) &&
                            fixAbs(pOpponent->getPosY() - Fixed(offsetY) - getPosY()) < Fixed(distY))) {
                            doBranch = true;
                        }

                        // or? no idea
                        // there's also branchParam3 that's 0 or 1 - they're both called AREA_ALL?
                        if (branchParam0 == 1 &&
                            (fixAbs(pOpponent->getPosX() - Fixed(offsetX) - getPosX()) < Fixed(distX) ||
                            fixAbs(pOpponent->getPosY() - Fixed(offsetY) - getPosY()) < Fixed(distY))) {
                            doBranch = true;
                        }
                    }
                    break;
                // case 42: // shot hit - broken, no known working use yet - probably should be on the proj itself like 45 below
                //     for ( auto minion : minions ) {
                //         if (minion->hitThisFrame) { // this frame? this move? no params observed
                //             doBranch = true;
                //         }
                //     }
                //     break;
                case 45:
                    if (isProjectile && projHitCount == 0 ) {
                        // log("hitcount=0 branch");
                        doBranch = true;
                    }
                    break;
                case 46: // counter..
                    if (branchParam0 == 1 && punishCounterThisFrame) {
                        doBranch = true;
                    }
                    break;
                case 47: // todo incapacitated
                    break;
                case 49: // status, used for at least hitstun - see jp sa3
                    {
                        // param0 = who's status - 0, self, 1 opponent, 2 "hit target", 3 owner
                        Guy *pGuy = nullptr;
                        if (branchParam0 == 0) {
                            pGuy = this;
                        } else if (branchParam0 == 1 || branchParam0 == 2) {
                            // what's the delineation between opponent and hit target here?
                            pGuy = pOpponent;
                        } else if (branchParam0 == 3) {
                            pGuy = pParent;
                        }
                        if (pGuy == nullptr) {
                            log(true, "that status branch not gonna work");
                        } else {
                            if (branchParam3 == 1) {
                                // just matching branch in jp SAA_LV3_START(1) for now
                                if (pGuy->hitStun) {
                                    doBranch = true;
                                }
                            } else {
                                log(logUnknowns, "unknown sort of status branch param3");
                            }
                            if (branchParam1) {
                                if (branchParam1 & (1 << (pGuy->getPoseStatus() - 1))) {
                                    doBranch = true;
                                }
                            }
                            if (branchParam4 & 8) {
                                if (pGuy->debuffTimer > 0) {
                                    doBranch = true;
                                }
                            } else if (branchParam4 & 2048) {
                                // bomb is gone/deleted
                                if (pGuy->debuffTimer == 0) {
                                    doBranch = true;
                                }
                            } else if (branchParam4 & 4096) {
                                // debuff runs out on its own (bomb auto-explode)
                                if (pGuy->debuffTimer == 1) {
                                    doBranch = true;
                                }
                            } else if (branchParam4 & 8192) {
                                // debuffed this frame? (bomb is replaced)
                                if (pGuy->debuffTimer == 300) {
                                    doBranch = true;
                                }
                            } else {
                                log(logUnknowns, "unknown sort of status branch param4");
                            }
                        }
                    }
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
                case 54: // touch, but really 'hit' with condition flags..
                    // branchParam1 set on some moves but unclear if used
                    if (branchParam0 == 0) {
                        branchParam0 = 255;
                    }
                    if (branchParam0 & (1<<0) && hitThisMove) {
                        doBranch = true;
                    }
                    if (branchParam0 & (1<<1) && hitCounterThisMove) {
                        doBranch = true;
                    }
                    if (branchParam0 & (1<<2) && hitPunishCounterThisMove) {
                        doBranch = true;
                    }
                    if (branchParam0 & (1<<3) && hasBeenBlockedThisMove) {
                        doBranch = true;
                    }
                    if (branchParam0 & (1<<4) && hitAtemiThisMove) {
                        doBranch = true;
                    }
                    if (branchParam0 & (1<<5) && hitArmorThisMove) {
                        doBranch = true;
                    }
                    // todo 6 and 7 parry and PP

                    // only run that branch after the frame bump, eg. don't immediately branch
                    // on the same-frame hit, see backfist combo touch branch wanting to be
                    // frame 13 after hitstop is done
                    if (!preHit) {
                        doBranch = false;
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

            if (doBranch) {

                bool keepFrame = key["_InheritFrameX"];

                if (keepFrame) {
                    // this will replay the same frame ID, is that correct?
                    // seems needed for guile's branch to 1229, otherwise unique
                    // isn't deducted (eventkey on frame 0)
                    // but maybe governed by flag?
                    branchFrame = currentFrame;
                }

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

                if (isProjectile && projHitCount == 0 ) {
                    // take new hitcount from branch action's pdata
                    // not just for hitcountzero branch, see also mai's charged fan
                    projHitCount = -1;
                }

                // FALL THROUGH - there might be another branch that works later for this frame
                // it should take precedence - see mai's charged fan projectile
            } else {
                if (branchType != 1) {
                    deniedLastBranch = true;
                }
            }
        }
    }
}

bool Guy::Frame(bool endWarudoFrame)
{
    // if this is the frame that was stolen from beginning warudo when it ends, don't
    // add pending warudo yet, so we can play it out fully, in case warudo got added
    // again just now - lots of DR cancels want one frame to play out when they add
    // more screen freeze at the exact end of hitstop
    if (!endWarudoFrame) {
        warudo += pendingWarudo;
        pendingWarudo = 0;
    }

    if (warudo) {
        // if we just entered hitstop, don't go to next frame right now
        // we want to have a chance to get hitstop input before triggers
        // we'll re-run it in PreFrame
        return true;
    }
    int curNextAction = nextAction;
    bool didTrigger = false;
    DoTriggers();
    if (nextAction != curNextAction) {
        didTrigger = true;
    }

    curNextAction = nextAction;
    bool didBranch = false;
    if (!didTrigger) {
        DoBranchKey();
        if (nextAction != curNextAction) {
            didBranch = true;
        }
    }
    currentFrame++;

    if (isProjectile && canHitID != 0) {
        projHitCount--;
        //log("proj hitcount " + std::to_string(projHitCount));
        canHitID = 0; // re-arm, all projectile hitboxes seem to have hitID 0
    }

    // evaluate branches after the frame bump, branch frames are meant to be elided afaict
    if (!didTrigger && !didBranch) {
        DoBranchKey(true);
    }

    if (isProjectile && projHitCount == 0) {
        return false; // die
    }

    if (isProjectile) {
        projLifeTime--;
        if (projLifeTime <= 0) {
            return false;
        }
    }

    if (die) {
        return false;
    }

    if (landed) {
        if ( resetHitStunOnLand ) {
            hitStun = 1;
            resetHitStunOnLand = false;
        }
        if ( airActionCounter ) {
            airActionCounter = 0;
        }
        if (pCharInfoJson->contains("Styles")) {
            nlohmann::json *pStyleJson = &(*pCharInfoJson)["Styles"][std::to_string(styleInstall)];
            if ((*pStyleJson)["StyleData"]["State"]["TerminateState"] == 0x3ff1fffffff) {
                // that apparently means landing... figure out deeper meaning later
                // go to parent or go to 0? :/
                styleInstall = (*pStyleJson)["ParentStyleID"];
            }
        }
    }

    if ((wallBounce || wallSplat) && touchedWall) {
        wallStopped = wallBounce || airborne;
        velocityX = Fixed(0);
        velocityY = Fixed(0);
        accelX = Fixed(0);
        accelY = Fixed(0);
        if (wallSplat) {
            if (airborne) {
                nextAction = 285;
            } else {
                nextAction = 145;
                // crumple
                blocking = false;
                hitStun = 50000;
                resetHitStunOnTransition = true;
                wallSplat = false;
                forceKnockDown = true;
            }
        } else {
            nextAction = 256;
        }
    }

    if (wallStopped) {
        wallStopFrames--;
        if (wallStopFrames <= 0) {
            if (wallSplat) {
                nextAction = 287;
                wallSplat = false;
                // todo ??
                accelY = Fixed(-0.6f, true);
            } else {
                nextAction = 235; // combo/bounce state

                velocityX = wallBounceVelX;
                accelX = wallBounceAccelX;
                velocityY = wallBounceVelY;
                accelY = wallBounceAccelY;

                wallBounce = false;
                wallBounceVelX = 0.0f;
                wallBounceAccelX = 0.0f;
                wallBounceVelY = 0.0f;
                wallBounceAccelY = 0.0f;
            }
            wallStopped = false;
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
            // magic that seems specific to normal jumps
            noVelNextFrame = true;
        } else if (currentAction == 5) {
            nextAction = 4; // finish transition to crouch
        } else if (!getAirborne() && !isProjectile && loopPoint != -1 && (loopCount == -1 || loopCount > 0)) {
            currentFrame = loopPoint;
            hasLooped = true;
            if (loopCount > 0) {
                loopCount--;
            }
        } else if ((isProjectile && loopCount == 0) || (pParent && !isProjectile)) {
            return false; // die if minion at end of script
        } else if (airborne || (isProjectile && loopCount == -1)) {
            // freeze time at the end there, hopefully a branch will get us when we land :/
            // should this apply in general, not just airborne?
            currentFrame--;
        } else {
            currentAction = 1;
        }

        if (resetHitStunOnTransition) {
            hitStun = 1;
            resetHitStunOnTransition = false;
        }
    }

    // first hitstun countdown happens on the same "frame" as hit, before hitstop
    // todo that's no longer true, we play that frame after hitstop?
    if (hitStun > 0)
    {
        hitStun--;
        if (hitStun == 0)
        {
            // todo need to decouple hitstun 0 from progressing to the next script
            // damage and juggle states' scripts' speed need to be scaled to naturally
            // end at the same time at hitstun?
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
    // todo is action 6 ok here? try a dump fo akuma zanku and trying to move immediately on landing
    crouching = actionCheckCanMove == 4 || actionCheckCanMove == 5 || actionCheckCanMove == 6;
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
        // reset status - recovered control to neutral
        jumped = false;

        if ( currentInput & 1 ) {

            jumped = true;

            if ( currentInput & 4 ) {
                jumpDirection = -1;
                nextAction = 35; // BAS_JUMP_B_START
            } else if ( currentInput & 8 ) {
                jumpDirection = 1;
                nextAction = 34; // BAS_JUMP_F_START
            } else {
                jumpDirection = 0;
                nextAction = 33; // BAS_JUMP_N_START
            }
        } else if ( currentInput & 2 ) {
            if ( !crouching ) {
                crouching = true;
                if (forcedPoseStatus == 2) {
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

        // todo only do that if we're not post-margin for correctness
        if ((currentInput & 0xF) == 0) {
            nextAction = 1;
        }

        turnaround = needsTurnaround();
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
        wallBounce = false; // just in case we didn't reach a wall
        wallSplat = false;
    }

    if (didTrigger) {
        // todo scaling stuff here?
    }

    bool didTransition = false;

    // Transition
    if ( nextAction != -1 )
    {
        if (currentAction != 1 && nextAction == 1) {
            recoveryTiming = globalFrameCount;
            //log("recovered!");
        }

        if (currentAction != nextAction) {
            currentAction = nextAction;
            log (logTransitions, "current action " + std::to_string(currentAction) + " keep place " + std::to_string(keepPlace) + " keep frame " + std::to_string(keepFrame));

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
            // commit current place offset
            posX = posX + (posOffsetX * direction);
            posOffsetX = Fixed(0);
            posY = posY + posOffsetY;
            posOffsetY = Fixed(0);
        }

        currentFrame = nextActionFrame != -1 ? nextActionFrame : 0;

        keepPlace = false;

        currentArmorID = -1; // uhhh

        nextAction = -1;
        nextActionFrame = -1;

        if (turnaround) {
            switchDirection();
        }

        UpdateActionData();

        nlohmann::json *pInherit = &(*pActionJson)["fab"]["Inherit"];

        bool inheritHitID = (*pInherit)["_HitID"];
        // see eg. 2MP into light crusher - light crusher has inherit hitID true..
        // assuming it's supposed to be for branches only
        if (didTrigger) {
            inheritHitID = false;
        }

        if (!inheritHitID) {
            canHitID = 0;
        }

        // only reset on doing a trigger - skull diver trigger is on hit, but the hit
        // happens on a prior script, and it doesn't have inherit HitInfo, so it's not that
        // todo SPA_HEADPRESS_P_OD_HIT(2) has both inherit hitinfo and a touch branch, check what that's about
        if (didTrigger) {
            hitThisMove = false;
            hitCounterThisMove = false;
            hitPunishCounterThisMove = false;
            hasBeenBlockedThisMove = false;
            hitArmorThisMove = false;
            hitAtemiThisMove = false;
        }

        if (!hitStun || blocking) {
            // should this use airborne status from previous or new action? currently previous
            if (isDrive || getAirborne()) {
                accelX = accelX * Fixed((*pInherit)["Accelaleration"]["x"].get<double>());
                accelY = accelY * Fixed((*pInherit)["Accelaleration"]["y"].get<double>());
                velocityX = velocityX * Fixed((*pInherit)["Velocity"]["x"].get<double>());
                velocityY = velocityY * Fixed((*pInherit)["Velocity"]["y"].get<double>());
            } else {
                accelX = Fixed(0);
                accelY = Fixed(0);
                velocityX = Fixed(0);
                velocityY = Fixed(0);
            }
        }

        if (didTrigger) {
            if (cancelAccelX != Fixed(0)) {
                accelX = cancelAccelX;
            }
            if (cancelAccelY != Fixed(0)) {
                accelY = cancelAccelY;
            }
            if (cancelVelocityX != Fixed(0)) {
                velocityX = cancelVelocityX;
            }
            if (cancelVelocityY != Fixed(0)) {
                velocityY = cancelVelocityY;
            }
        }
        cancelAccelX = Fixed(0);
        cancelAccelY = Fixed(0);
        cancelVelocityX = Fixed(0);
        cancelVelocityY = Fixed(0);

        if (isDrive == true) {
            isDrive = false;
            // at some point make it so we cant drive specials
            wasDrive = true;
        } else {
            wasDrive = false;
        }

        if (isProjectile) {
            projDataInitialized = false;
        }

        prevPoseStatus = forcedPoseStatus;

        // careful about airborne/etc checks until call do DoStatusKey() below
        forcedPoseStatus = 0;
        actionStatus = 0;
        jumpStatus = 0;
        landingAdjust = 0;

        didTransition = true;
    }

    if (warudo == 0) {
        timeInWarudo = 0;
    }

    if (canMove && didTransition) {
        // if we just went to idle, run triggers again
        DoTriggers();
        // if successful, eat this frame away and go right now
        if (nextAction != -1) {
            currentAction = nextAction;
            UpdateActionData();
            log (logTransitions, "nvm! current action " + std::to_string(currentAction));
            nextAction = -1;
        }
    }

    // if we need landing adjust/etc during warudo, need this updated now
    if (!didTransition) {
        prevPoseStatus = forcedPoseStatus;
    }
    DoStatusKey();

    return true;
}

void Guy::DoStatusKey(void)
{
    if (pActionJson->contains("StatusKey"))
    {
        for (auto& [keyID, key] : (*pActionJson)["StatusKey"].items())
        {
            if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                continue;
            }

            int adjust = key["LandingAdjust"];
            if ( adjust != 0 ) {
                landingAdjust = adjust;
            }
            forcedPoseStatus = key["PoseStatus"];
            actionStatus = key["ActionStatus"];
            jumpStatus = key["JumpStatus"];

            int sideOperation = key["Side"];

            switch (sideOperation) {
                case 0:
                default:
                    break;
                case 1:
                    if (needsTurnaround()) {
                        switchDirection();
                    }
                    break;
                case 3:
                    switchDirection();
                    break;
            }
        }
    }
}