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

#include <cmath>
#include <numbers>
#include <unordered_set>

#define log(channel, string) if (channel) { guyLog(channel, string); }
#define otherGuyLog(otherGuy, channel, string) if (channel) { otherGuy->guyLog(channel, string); }

bool doBoxesHitXAxis(Box box1, Box box2)
{
    if (box1.x + box1.w < box2.x) {
        return false;
    }
    if (box2.x + box2.w < box1.x) {
        return false;
    }
    return true;
}

bool doBoxesHitYAxis(Box box1, Box box2)
{
    if (box1.y + box1.h < box2.y) {
        return false;
    }
    if (box2.y + box2.h < box1.y) {
        return false;
    }
    return true;
}

bool doBoxesHit(Box box1, Box box2)
{
    if (!doBoxesHitXAxis(box1, box2)) {
        return false;
    }
    if (!doBoxesHitYAxis(box1, box2)) {
        return false;
    }
    //log ("boxes hit! " + std::to_string(box1.x.f() + box1.w.f() - box2.x.f()));
    return true;
}

bool matchFrameButton( int input, uint32_t okKeyFlags, uint32_t okCondFlags, uint32_t dcExcFlags = 0, uint32_t dcIncFlags = 0, uint32_t ngKeyFlags = 0, uint32_t ngCondFlags = 0 )
{
    // do that before stripping held keys since apparently holding parry to drive rush depends on it
    if (dcExcFlags != 0 ) {
        if ((dcExcFlags & input) != dcExcFlags) {
            return false;
        }
    }
    if (dcIncFlags != 0 ) {
        if (!(dcIncFlags & input)) {
            return false;
        }
    }

    if (ngCondFlags & 2) {
        if ((input & 0xF) == (ngKeyFlags & 0xF)) {
            return false;
        }
    } else if (ngKeyFlags & input) {
        return false;
    }

    // commands can have only exc/inc flags?
    if (okKeyFlags == 0) {
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

    if (okCondFlags & 1) {
        if (((input & 0xF) & (okKeyFlags & 0xF)) == (okKeyFlags & 0xF)) {
            return true; // match exact direction with slop
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

void Guy::DoSteerKeyOperation(Fixed &value, Fixed keyValue, int operationType)
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
        case 3: // ratio
            value = value * keyValue;
            break;
        default:
            log(logUnknowns, "Uknown steer keyoperation!");
            break;
    }
}

void Guy::guyLog(bool doLog, std::string logLine)
{
    if (!doLog) return;
    std::string frameDiff = to_string_leading_zeroes(pSim->frameCounter - nc.lastLogFrame, 3);
    std::string curFrame = to_string_leading_zeroes(currentFrame, 3);
    nc.logQueue.push_back(std::to_string(currentAction) + ":" + curFrame + "(+" + frameDiff + ") " + logLine);
    if (nc.logQueue.size() > 15) {
        nc.logQueue.pop_front();
    }
    nc.lastLogFrame = pSim->frameCounter;
}

bool Guy::conditionOperator(int op, int operand, int threshold, std::string desc)
{
    switch (op) {
        case 0:
            if (threshold == operand) return true;
            break;
        case 1:
            if (threshold != operand) return true;
            break;
        case 2:
            if (operand < threshold) return true;
            break;
        case 3:
            if (operand <= threshold) return true;
            break;
        case 4:
            if (operand > threshold) return true;
            break;
        case 5:
            if (operand >= threshold) return true;
            break;
        // supposedly AND - unused as of yet?
        // case 6:
        //     if  (operand & threshold) return true;
        //     break;
        default:
            log(logUnknowns, "unhandled " + desc + " operator");
            break;
    }
    return false;
}

Action* Guy::FindMove(int actionID, int styleID)
{
    ActionRef mapIndex(actionID, styleID);
    auto it = pCharData->actionsByID.find(mapIndex);

    if (it != pCharData->actionsByID.end()) {
        return it->second;
    }

    int parentStyleID = -1;
    if (styleID >= 0 && (size_t)styleID < pCharData->styles.size()) {
        parentStyleID = pCharData->styles[styleID].parentStyleID;
    }

    if (parentStyleID == -1) {
        return nullptr;
    }

    return FindMove(actionID, parentStyleID);
}

void Guy::Input(int input)
{
    int directionToUse = direction.i();
    if (needsTurnaround()) {
        directionToUse *= -1;
    }
    if (directionToUse < 0) {
        input = invertDirection(input);
    }
    if (input == 0 && inputOverride != 0) {
        input = inputOverride;
    }
    framesSinceLastInput++;
    if (input != 0) {
        framesSinceLastInput = 0;
    }
    currentInput = input;

    if (warudo) {
        input |= FROZEN;
    }

    dc.inputBuffer.push_front(input);
}

std::string Guy::getActionName(int actionID)
{
    Action *pAction = FindMove(actionID, styleInstall);
    if (pAction) {
        return pAction->name;
    }
    return "invalid";
}

void Guy::UpdateActionData(void)
{
    if (opponentAction) {
        pCurrentAction = pOpponent->FindMove(currentAction, 0);
    } else {
        pCurrentAction = FindMove(currentAction, styleInstall);
    }

    if (pCurrentAction == nullptr) {
        log(true, "couldn't find next action, reverting to 1 - style lapsed?");
        currentAction = 1;
        pCurrentAction = FindMove(currentAction, styleInstall);
    }

    if (pCurrentAction) {
        hasLooped = false;
        loopCount = pCurrentAction->loopCount;

        actionInstantScale = pCurrentAction->instantScale;
        if (pOpponent && !pOpponent->comboHits) {
            actionInstantScale = 0;
        }
    }
}

bool Guy::RunFrame(bool advancingTime)
{
    dc.frameTriggers.clear();

    if (parryFreeze || (pOpponent && pOpponent->wallStopped && pOpponent->stunned)) {
        return false;
    }

    if (advancingTime && !getWarudo()) {
        if (debuffTimer > 0 ) {
            debuffTimer--;
        }
        if (debuffTimer == 1 && getHitStop() == 1) {
            debuffTimer = 5;
        } else if (debuffTimer == 1 && getHitStop() > 0) {
            debuffTimer++;
        }
    }

    if (!warudo && tokiWaUgokidasu) {
        tokiWaUgokidasu = false;
        if (!AdvanceFrame(advancingTime, false, true)) {
            delete this;
            return false;
        }
    }

    if (getWarudo()) {
        return false;
    }

    if (advancingTime && getHitStop()) {
        timeInHitStop++;
        hitStop--;
        if (getHitStop() == 0) {
            // increment the frame we skipped at the beginning of hitstop
            if (!AdvanceFrame(true, true, false)) {
                delete this;
                return false;
            }
        }
    }

    if (getHitStop()) {
        return false;
    }

    if (hitSpanFrames) {
        hitSpanFrames--;
    }

    // training mode refill rule, 'action' starts and both start regen (soon)
    // not in AdvanceFrame because that's where cooldown decreases and there's a sequencing issue if we do both there
    if (!pSim->match && advancingTime && didTrigger && currentAction != 17 && currentAction != 18 && focusRegenCooldown == -1 && !deferredFocusCost) {
        if (pOpponent && !pOpponent->comboHits) {
            focusRegenCooldown = 2;
            log(logResources, "regen cooldown " + std::to_string(focusRegenCooldown) + " (refill action start)");
            pOpponent->focusRegenCooldown = 2;
            otherGuyLog(pOpponent, logResources, "regen cooldown " + std::to_string(pOpponent->focusRegenCooldown) + " (refill action start)");
        }
    }

    if (advancingTime && uniqueTimer) {
        // might not be in the right spot, adjust if falling out of yoga float too early/late
        uniqueTimerCount++;
    }

    if (advancingTime && jumpLandingDisabledFrames) {
        jumpLandingDisabledFrames--;
    }

    hitThisFrame = false;
    hitArmorThisFrame = false;
    hitAtemiThisFrame = false;
    hasBeenBlockedThisFrame = false;
    hasBeenParriedThisFrame = false;
    hasBeenPerfectParriedThisFrame = false;
    punishCounterThisFrame = false;
    grabbedThisFrame = false;
    beenHitThisFrame = false;
    grabAdjust = false;
    armorThisFrame = false;
    atemiThisFrame = false;
    landed = false;
    pushBackThisFrame = Fixed(0);
    reflectThisFrame = Fixed(0);
    offsetDoesNotPush = false;
    freeMovement = false;
    hurtBoxProxGuarded = false;
    positionProxGuarded = false;

    if (pCurrentAction != nullptr)
    {
        if (isProjectile && !projDataInitialized && pCurrentAction && pCurrentAction->pProjectileData) {
            ProjectileData *pProjData = pCurrentAction->pProjectileData;
                projHitCount = pProjData->hitCount;
                if (projHitCount == 0) {
                    // stuff that starts at hitcount 0 is probably meant to die some other way
                    // todo implement lifetime, ranges, etc
                    projHitCount = -1;
                }
                // log("initial hitcount " + std::to_string(projHitCount));

                hitSpanFrames = 0;
                airborne = pProjData->airborne;
                int flagsX = pProjData->flags;
                obeyHitID = flagsX & (1<<0);
                limitShotCategory = pProjData->category;
                noPush = pProjData->noPush;
                projLifeTime = pProjData->lifeTime;

                if (projLifeTime <= 0) {
                    projLifeTime = 360;
                }

                projLifeTime--;

                projDataInitialized = true;
        } else {
            // todo there's a status bit for this?
            noPush = false; // might be overridden below
        }

        DoWorldKey();

        DoLockKey();

        Fixed prevPosY = getPosY();

        if (!noPlaceXNextFrame && !setPlaceX) {
            posOffsetX = Fixed(0);
        }

        if (!noPlaceYNextFrame && !setPlaceY) {
            posOffsetY = Fixed(0);
        }

        // assuming it doesn't stick for now
        ignoreSteerType  = -1;

        // if we're already unlocked from opponent action, don't advance place
        if (!pOpponent || !pOpponent->pendingUnlockHitDelayed) {
            DoPlaceKey();
        }

        noPlaceXNextFrame = false;
        noPlaceYNextFrame = false;

        Fixed prevVelX = velocityX;

        cancelInheritVelX = Fixed(0);
        cancelInheritVelY = Fixed(0);
        cancelInheritAccelX = Fixed(0);
        cancelInheritAccelY = Fixed(0);

        DoSteerKey();

        if ( (accelX != Fixed(0) && prevVelX != Fixed(0) && velocityX == Fixed(0)) ) {
            // if a steerkey just set speed to 0 and there was accel, it seems to want to clear accel
            // to stop movement - see back accel for eg. drive rush normals
            accelX = 0;
        }
        // todo same with Y?

        prevVelX = velocityX;
        Fixed prevVelY = velocityY;

        if (!noAccelNextFrame) {
            if (ignoreSteerType != 3) {
                velocityX = velocityX + accelX;
            }
            if (ignoreSteerType != 4) {
                velocityY = velocityY + accelY;
            }
        } else {
            noAccelNextFrame = false;
        }

        if ((velocityY * prevVelY) < Fixed(0) || (accelY != Fixed(0) && velocityY == Fixed(0))) {
            startsFalling = true;
        } else {
            startsFalling = false;
        }

        // log(std::to_string(currentAction) + " " + std::to_string(prevVelX) + " " + std::to_string(velocityX));

        if ((velocityX * prevVelX) < Fixed(0) || (accelX < Fixed(0) && (prevVelX*velocityX == Fixed(0)))) {
            // sign change?
            velocityX = Fixed(0);
            accelX = Fixed(0);
        }

        if (!noVelNextFrame) {
            if (ignoreSteerType != 0) {
                posX = posX + (velocityX * direction);
            }
            if (ignoreSteerType != 1) {
                posY = posY + velocityY;
            }
        } else {
            noVelNextFrame = false;
        }

        if (pOpponent && !pOpponent->warudo) {
            if (hitVelX != Fixed(0)) {
                if (!locked && !nageKnockdown) {
                    posX = posX + hitVelX;
                    pushBackThisFrame = hitVelX;
                }

                Fixed prevHitVelX = hitVelX;
                hitVelX = hitVelX + hitAccelX;
                if ((hitVelX * prevHitVelX) < Fixed(0) || (hitAccelX != Fixed(0)&& hitVelX == Fixed(0))) {
                    hitAccelX = Fixed(0);
                    hitVelX = Fixed(0);
                }
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
        throwTechable = false;
        canBlock = false;
        ignoreBodyPush = false;
        ignoreHitStop = false;
        ignoreScreenPush = false;
        ignoreCornerPushback = false;
        teleported = false;

        DoSwitchKey();

        DoEventKey(pCurrentAction, currentFrame);

        if (advancingTime && countingDownInstall && styleInstallFrames) {
            styleInstallFrames--;

            if ( styleInstallFrames < 0) {
                styleInstallFrames = 0;
                countingDownInstall = false;
            }

            if (styleInstallFrames == 0) {
                ExitStyle();
            }
        }
    }

    return true;
}

void Guy::RunFramePostPush(void)
{
    if (parryFreeze || warudo || (pOpponent && pOpponent->wallStopped && pOpponent->stunned)) {
        return;
    }

    if (!wallStopped || !stunned) {
        DoFocusRegen();
    }

    // throw tech - do it before AdvanceFrame to prevent sequencing issue
    // todo modern
    if (locked && pOpponent && pOpponent->throwTechable && (currentInput & (LP+LK)) == LP+LK && nextAction == -1) {
        nextAction = 320;
        hitStop = 0;
        hitStun = 0;
        pOpponent->nextAction = 321;
        pOpponent->hitStop = 0;
        if (needsTurnaround()) {
            switchDirection();
        }
    }

    if (recoverableHealthCooldown) {
        recoverableHealthCooldown--;
    }

    if (recoverableHealth && !recoverableHealthCooldown && (!pOpponent || !pOpponent->warudo)) {
        recoverableHealth -= 2;
        health += 2;
        if (recoverableHealth < 0) {
            health += recoverableHealth;
        }
        if (health > pCharData->vitality) {
            health = pCharData->vitality;
        }
    }

    if (deferredFocusCost < 0 && (!pOpponent || !pOpponent->warudo)) {
        setFocus(focus + deferredFocusCost);
        log(logResources, "focus " + std::to_string(deferredFocusCost) + ", total " + std::to_string(focus));
        deferredFocusCost = 0;
        if (!parrying || !successfulParry) {
            if (!setFocusRegenCooldown(parrying?240:120)) {
                focusRegenCooldown--;
            }
            focusRegenCooldownFrozen = true;
        }
        log(logResources, "regen cooldown " + std::to_string(focusRegenCooldown) + " (deferred spend, frozen)");
    }

    if (getHitStop()) {
        return;
    }



    DoShotKey(pCurrentAction, currentFrame);

    if (touchedWall && pushBackThisFrame != Fixed(0) && pOpponent && pOpponent->reflectThisFrame == Fixed(0)) {
        pOpponent->deferredReflect = true;
        log (logTransitions, "deferred reflect!");
    }
}

void Guy::ExecuteTrigger(Trigger *pTrigger)
{
    nextAction = pTrigger->actionID;
    uint64_t flags = pTrigger->flags;

    blocking = false;
    moveStartPos = getPosX();

    bool normalAttack = flags & (1ULL<<11);
    bool uniqueAttack = flags & (1ULL<<12);
    bool specialAttack = flags & (1ULL<<13);
    // one-way, reset on combo end
    driveRushCancel = driveRushCancel || flags & (1ULL<<20);
    parryDriveRush =  parryDriveRush || flags & (1ULL<<21);
    bool dash = flags & (1ULL<<22) || flags & (1ULL<<23);
    bool jump = flags & (1ULL<<26);
    parrying = flags & (1ULL<<40);

    // if (flags & (1ULL<<20)) {
    //     if (setFocusRegenCooldown(120)) {
    //         focusRegenCooldown--;
    //     }
    // }
    if (flags & (1ULL<<21)) {
        if (setFocusRegenCooldown(240)) {
            focusRegenCooldown--;
        }
    }
    if (specialAttack) {
        isDrive = false;
        wasDrive = false;
    }
    if (isDrive && (normalAttack || uniqueAttack)) {
        wasDrive = true;
    }
    if (!isDrive) {
        wasDrive = false;
    }

    if (parrying && getCrouching()) {
        nextAction = 489;
    }

    if (parrying) {
        successfulParry = false;
        subjectToParryRecovery = true;
    }

    if (jump) {
        jumped = true;

        if (flags & (1ULL<<46)) {
            jumpDirection = 1;
        } else if (flags & (1ULL<<47)) {
            jumpDirection = -1;
        } else {
            jumpDirection = 0;
        }

        log(logTransitions, "forced jump status from trigger, direction " + std::to_string(jumpDirection));
    }

    uniqueOpsAppliedMask = 0;

    triggerInstantScale = pTrigger->comboInst;
    if (pOpponent && !pOpponent->comboHits) {
        // todo is this right if projectiles hit out of order?
        triggerInstantScale = 0;
    }
    triggerSuperGainScaling = pTrigger->comboSuperScaling;

    superAction = flags & (1ULL<<15);
    superLevel = 0;
    if (flags & (1ULL<<7)) superLevel = 1;
    if (flags & (1ULL<<8)) superLevel = 2;
    if (flags & (1ULL<<9)) superLevel = 3;
    if (flags & (1ULL<<10)) superLevel = 3; // ca?

    // meters
    if (pTrigger->needsFocus) {
        deferredFocusCost = pTrigger->focusCost;
        log(logResources, "queuing deferred cost");
    }
    if (focusRegenCooldownFrozen) {
        // if we cancel out of the od move
        focusRegenCooldownFrozen = false;
        // for some reason first-frame cancel of dash doesn't do that...
        if ((!jump && !dash) && focusRegenCooldown) {
            focusRegenCooldown--;
        }
        log(logResources, "regen cooldown unfrozen (cancelled out of freeze move)");
    }

    if (pTrigger->needsGauge) {
        gauge -= pTrigger->gaugeCost;
        if (gauge < 0) {
            log(true, "not eonugh gauge to execute?! not supposed to happen");
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

bool Guy::CheckTriggerConditions(Trigger *pTrigger, int fluffFramesBias)
{
    if (pSim->match && !pSim->timerStarted) {
        return false;
    }

    if ( pTrigger->validStyles != 0 && !(pTrigger->validStyles & (1 << styleInstall)) ) {
        return false;
    }

    if (!pTrigger->okKeyFlags && !pTrigger->dcExcFlags && !pTrigger->dcIncFlags) {
        // some triggers don't seem hooked to anything?
        return false;
    }

    if (pTrigger->useUniqueParam && pTrigger->condParamID >= 0 && pTrigger->condParamID < uniqueParamCount) {
        if (!conditionOperator(pTrigger->condParamOp, uniqueParam[pTrigger->condParamID], pTrigger->condParamValue, "trigger unique param")) {
            return false;
        }
    }

    if (pTrigger->limitShotCount > 0) {
        int count = 0;
        for ( auto minion : dc.minions ) {
            if (pTrigger->limitShotCategory & (1 << minion->limitShotCategory)) {
                count++;
            }
        }
        if (count >= pTrigger->limitShotCount) {
            return false;
        }
    }

    if (pTrigger->airActionCountLimit > 0) {
        if (airActionCounter >= pTrigger->airActionCountLimit) {
            return false;
        }
    }

    if (pTrigger->vitalOp != 0) {
        float vitalRatio = (float)health / pCharData->vitality * 100;

        switch (pTrigger->vitalOp) {
            case 2:
                if (vitalRatio > pTrigger->vitalRatio) {
                    // todo figure out exact rounding rules here
                    return false;
                }
                break;
            default:
                log(logUnknowns, "unknown vital op on trigger " + std::to_string(pTrigger->id));
                return false;
                break;
        }
    }

    bool checkingRange = false;
    bool rangeCheckMatch = true;

    if (pTrigger->rangeCondition) {
        checkingRange = true;
    }

    switch (pTrigger->rangeCondition) {
        case 3:
        case 4:
        case 5:
            if (getPosY() < pTrigger->rangeParam) {
                rangeCheckMatch = false;
            }
            if (pTrigger->rangeCondition == 4 && !(velocityY > Fixed(0))) {
                // rnage check on the way up? waive if not going up
                rangeCheckMatch = true;
            }
            if (pTrigger->rangeCondition == 5 && !(velocityY < Fixed(0))) {
                // rnage check on the way down? waive if not going down
                rangeCheckMatch = true;
            }
            break;
        default:
            log(logUnknowns, "unimplemented range cond " + std::to_string(pTrigger->rangeCondition));
            break;
        case 0:
            break;
    }

    if (checkingRange && !rangeCheckMatch) {
        return false;
    }

    if (pTrigger->stateCondition && !(pTrigger->stateCondition & (1 << (getPoseStatus() - 1)))) {
        return false;
    }

    // meters
    if (pTrigger->needsFocus) {
        if (focus == 0) {
            // no need to check cost here as we can bottom out this bar
            return false;
        }
    }

    if (pTrigger->needsGauge) {
        if (gauge < pTrigger->gaugeCost) {
            return false;
        }
    }

    // prevent non-jump triggers if about to jump
    bool a,b,c;
    // only before frame advance
    if (fluffFramesBias == 1 && canMove(a,b,c, fluffFramesBias) && (currentInput & 1)) {
        if (!(pTrigger->flags & (1ULL<<26))) {
            return false;
        }
    }

    return true;
}

bool Guy::MatchNormalCommandInput(CommandInput *pInput, uint32_t &inputBufferCursor, uint32_t maxSearch, bool needPositiveEdge) {
    bool pass = false;
    bool fail = false;
    bool match = false;
    uint32_t inputOkKeyFlags = pInput->okKeyFlags;
    uint32_t inputOkCondFlags = pInput->okCondFlags;
    int lastMatchInput = -1;

    bool matchThisFrame = false;
    while (inputBufferCursor < maxSearch)
    {
        int bufferInput = dc.inputBuffer[inputBufferCursor];

        bool inputNg = false;
        bool inputFail = false;

        // we venture past the allowed buffer here technically
        bool onlyNegativeEdge = needPositiveEdge && inputBufferCursor == maxSearch - 1;

        if (pInput->ngCondFlags & 2) {
            if ((bufferInput & 0xF) == (pInput->ngKeyFlags & 0xF)) {
                inputNg = true;
            }
        } else if (pInput->ngKeyFlags & bufferInput) {
            inputNg = true;
        }
        if (pInput->failCondFlags & 2) {
            if ((bufferInput & 0xF) == (pInput->failKeyFlags & 0xF)) {
                inputFail = true;
            }
        } else if (pInput->failKeyFlags & bufferInput) {
            inputFail = true;
        }
        if (inputFail && !match) {
            fail = true;
            break;
        }
        if (!inputNg && matchFrameButton(bufferInput, inputOkKeyFlags, inputOkCondFlags)) {
            if (onlyNegativeEdge) {
                break;
            }
            match = true;
            matchThisFrame = true;
            lastMatchInput = inputBufferCursor;
            if (inputOkCondFlags & 1) {
                // inclusive lever shift initial match
                // turn the match into exclusive of current lever position to look for the edge of it settling here
                inputOkCondFlags &= ~1;
                inputOkCondFlags |= 2;
                inputOkKeyFlags = bufferInput & 0xF;
            }
        } else if (match) {
            matchThisFrame = false;
            break;
        }
        if (onlyNegativeEdge) {
            break;
        }
        // if (pCommand->id == 36) {
        //     guyLog(true, std::to_string(inputID) + " " + std::to_string(inputOkKeyFlags) +
        //     " inputbuffercursor " + std::to_string(inputBufferCursor) + " matchThisInput " + std::to_string(matchThisFrame) + " buffer " + std::to_string(bufferInput));
        // }
        inputBufferCursor++;
    }

    // if (fail) {
    //     if (pCommand->id == 36) {
    //         guyLog(true, "fail " + std::to_string(dc.inputBuffer[inputBufferCursor]));
    //     }
    //     break;
    // }

    pass = false;
    if (!fail && match) {
        pass = true;
        if (matchThisFrame && needPositiveEdge) {
            // need positive edge but never not-matched until end of window
            pass = false;
        }
    }

    // in strings of inclusive lever matches the last pass might pass the next one too, so don't
    if (pass && lastMatchInput != -1) {
        inputBufferCursor = lastMatchInput + 1;
    }

    return pass;
}

bool Guy::MatchChargeCommandInput(Charge *pCharge, uint32_t &cursorPos, uint32_t maxSearch, uint32_t initialI) {
    bool pass = false;
    uint32_t inputOkKeyFlags = pCharge->okKeyFlags;
    uint32_t inputOkCondFlags = pCharge->okCondFlags;
    uint32_t chargeFrames = pCharge->chargeFrames;
    uint32_t keepFrames = pCharge->keepFrames;
    uint32_t dirCount = 0;
    maxSearch = initialI + chargeFrames + keepFrames;
    while (cursorPos < dc.inputBuffer.size() && cursorPos < maxSearch)
    {
        if (matchFrameButton(dc.inputBuffer[cursorPos], inputOkKeyFlags, inputOkCondFlags)) {
            dirCount++;
            if (dirCount >= chargeFrames) {
                break;
            }
        }
        else {
            dirCount = 0;
        }
        cursorPos++;
    }

    if (dirCount >= chargeFrames) {
        pass = true;
    }

    return pass;
}

bool Guy::MatchRotateCommandInput(CommandInput *pCommandInput, uint32_t &cursorPos, uint32_t searchArea) {
    // rotate
    int pointsNeeded = pCommandInput->rotatePointsNeeded;
    int curAngle = 0;
    int pointsForward = 0;
    int pointsBackwards = 0;
    bool pass = false;
    while (cursorPos < dc.inputBuffer.size() && cursorPos < searchArea)
    {
        int bufferInput = dc.inputBuffer[cursorPos];
        int targetAngle = inputAngle(bufferInput);
        if (targetAngle) {
            if (curAngle == 0) {
                curAngle = targetAngle;
            }
            int diff = angleDiff(curAngle, targetAngle);
            //log(std::to_string(diff) + " " + std::to_string(pointsForward) + " " + std::to_string(pointsBackwards));
            if (diff >= 90 && diff < 180) {
                pointsForward++;
                curAngle = targetAngle;
            }
            if (diff <= -90 && diff > -180) {
                pointsBackwards++;
                curAngle = targetAngle;
            }
        }
        cursorPos++;
    }
    if (pointsNeeded <= pointsForward || pointsNeeded <= pointsBackwards) {
        pass = true;
    }
    return pass;
}

bool Guy::MatchCommandInput(CommandInput *pCommandInput, uint32_t &cursorPos, uint32_t startSearch, uint32_t maxSearch, uint32_t initialI, bool needPositiveEdge) {
    bool pass = false;
    uint32_t inputSearchArea = 0;
    if (pCommandInput->type == InputType::Rotation) {
        inputSearchArea = startSearch + pCommandInput->numFrames * 2; // todo, i think that's best case
        // if (inputSearchArea > maxSearch) {
        //     inputSearchArea = maxSearch;
        // }
        pass = MatchRotateCommandInput(pCommandInput, cursorPos, inputSearchArea);
    } else if (pCommandInput->type == InputType::ChargeRelease) {
        if (pCommandInput->pCharge) {
            pass = MatchChargeCommandInput(pCommandInput->pCharge, cursorPos, maxSearch, initialI);
        }
    } else {
        uint32_t inputSearchArea = startSearch + pCommandInput->numFrames;
        if (needPositiveEdge) {
            inputSearchArea += 1; // one grace frame to look for the negative edge
        }
        // todo or is this fine?
        if (pCommandInput->numFrames == -1) {
            inputSearchArea = maxSearch;
        }
        if (inputSearchArea > dc.inputBuffer.size()) {
            inputSearchArea = dc.inputBuffer.size();
        }
        if (inputSearchArea > maxSearch) {
            inputSearchArea = maxSearch;
        }
        pass = MatchNormalCommandInput(pCommandInput, cursorPos, inputSearchArea, needPositiveEdge);
    }
    return pass;
}

bool Guy::MatchCommand(CommandVariant *pVariant, int startInput, uint32_t startCursorPos, uint32_t initialI)
{
    int maxSearch = startCursorPos + pVariant->totalMaxFrames + 1;
    uint32_t inputBufferCursor = startCursorPos;

    if (startInput == -1) {
        // all done!
        return true;
    }

    bool needPositiveEdge = startInput != 0;
    bool pass = false;
    do {
        // recursive depth first search to try all combinations
        // todo make simple walk forward an option for less lenient matching
        pass = MatchCommandInput(&pVariant->inputs[startInput], inputBufferCursor, startCursorPos, maxSearch, initialI, needPositiveEdge);

        if (!pass) {
            return false;
        }

        if (MatchCommand(pVariant, startInput - 1, inputBufferCursor, initialI)) {
            return true;
        }
    } while (pass);

    return false;
}

bool Guy::MatchInitialInput(Trigger *pTrigger, uint32_t &cursorPos)
{
    uint32_t okKeyFlags = pTrigger->okKeyFlags;
    uint32_t okCondFlags = pTrigger->okCondFlags;
    uint32_t ngKeyFlags = pTrigger->ngKeyFlags;
    uint32_t dcExcFlags = pTrigger->dcExcFlags;
    uint32_t dcIncFlags = pTrigger->dcIncFlags;
    int precedingTime = pTrigger->precedingTime;
    bool match = false;
    uint32_t initialCursorPos = cursorPos;
    int initialI = -1;
    bool initialMatch = false;
    // current frame + buffer
    int hitStopTime = timeInHitStop;
    uint32_t initialSearch = 1 + precedingTime + hitStopTime;
    if (dc.inputBuffer.size() < (size_t)initialSearch) {
        initialSearch = dc.inputBuffer.size();
    }
    // early out if no input
    if (framesSinceLastInput > initialSearch) {
        return false;
    }

    if ((okCondFlags & 0x60) == 0x60) {
        int parallelMatchesFound = 0;
        int button = LP;
        bool bothButtonsPressed = false;
        bool atLeastOneNotConsumed = true; // todo disable this for now - is it a flag?
        while (button <= HK) {
            if (button & okKeyFlags) {
                cursorPos = initialCursorPos;
                initialMatch = false;
                while (cursorPos < initialSearch) {
                    if (matchFrameButton(dc.inputBuffer[cursorPos], button, 0, dcExcFlags, dcIncFlags, ngKeyFlags))
                    {
                        if (!(dc.inputBuffer[cursorPos] & CONSUMED)) {
                            atLeastOneNotConsumed = true;
                        }
                        if (std::bitset<32>(dc.inputBuffer[cursorPos] & okKeyFlags).count() >= 2) {
                            bothButtonsPressed = true;
                        }
                        initialMatch = true;
                    } else if (initialMatch == true) {
                        cursorPos--;
                        // set most recent initialI to get marked consumed?
                        if (initialI == -1 || (int)cursorPos < initialI) {
                            initialI = cursorPos;
                        }
                        break; // break once initialMatch no longer true, set i on last true
                    }
                    cursorPos++;
                }
                if (initialMatch) {
                    parallelMatchesFound++;
                }
            }
            button = button << 1;
        }
        initialMatch = atLeastOneNotConsumed && bothButtonsPressed && (parallelMatchesFound >= 2);
    } else {
        bool atLeastOneNotConsumed = false;
        while (cursorPos < initialSearch)
        {
            // possible to be all zeroes?
            if ((okKeyFlags || dcExcFlags || dcIncFlags) && matchFrameButton(dc.inputBuffer[cursorPos], okKeyFlags, okCondFlags, dcExcFlags, dcIncFlags, ngKeyFlags))
            {
                if (!(dc.inputBuffer[cursorPos] & CONSUMED)) {
                    atLeastOneNotConsumed = true;
                }
                initialMatch = true;
                initialI = cursorPos;
                match = true;
                if (okCondFlags & 1 && okCondFlags & 1024) {
                    // inclusive lever shift initial match
                    // turn the match into exclusive of current lever position to look for the edge of it settling here
                    okCondFlags &= ~1;
                    okCondFlags |= 2;
                    okKeyFlags = dc.inputBuffer[cursorPos] & 0xF;
                }
            } else if (initialMatch == true) {
                cursorPos--;
                initialI = cursorPos;
                match = false;
                break; // break once initialMatch no longer true, set i on last true
            }
            if (dc.inputBuffer[cursorPos] & FROZEN) {
                initialSearch++;
                //log(true, "extending backroll window frozen " + std::to_string(searchWindow));
            }
            cursorPos++;
        }
        if (atLeastOneNotConsumed == false) {
            initialMatch = false;
        }
        if (okCondFlags & 1024 && match) {
            // if we never got to not-matching, we never saw the initial edge
            initialMatch = false;
        }
    }
    if (initialI == -1) {
        initialI = cursorPos;
    }
    cursorPos = initialI;
    // there's some weirdness here - parry can be buffered sometimes?
    if (match || !(pTrigger->flags & (1ULL<<40))) {
        if (okKeyFlags == 0) {
            if (dcExcFlags != 0 ) {
                if ((dcExcFlags & currentInput) != dcExcFlags) {
                    return false;
                }
            }
            if (dcIncFlags != 0 ) {
                if (!(dcIncFlags & currentInput)) {
                    return false;
                }
            }
        }
    }
        return initialMatch;
}

bool Guy::CheckTriggerCommand(Trigger *pTrigger, uint32_t &initialI)
{
    initialI = 0;
    while (true) {
        bool initialMatch = MatchInitialInput(pTrigger, initialI);
        if (!initialMatch) {
            return false;
        }

        if ( pTrigger->pCommandClassic == nullptr ) {
            // simple single-input command, initial match is enough
            return true;
        } else {
            Command *pCommand = pTrigger->pCommandClassic;

            for (auto& variant : pCommand->variants) {
                if (MatchCommand(&variant, variant.inputs.size() - 1, initialI, initialI)) {
                    return true;
                }
            }
        }

        // command did not pass with initial match, continue walking for another potential initial match in the window
        initialI++;
    }

    return false;
}

const int triggerCheckArraySize = 1024;

struct TriggerCheckState {
    int triggerID;
    int actionID;
    Trigger *pTrigger;
    bool hasNormal;
    bool hasDeferred;
    bool hasAntiNormal;
    bool late;
};

void Guy::DoTriggers(int fluffFrameBias)
{
    std::set<int> keptDeferredTriggerIDs;
    bool hasTriggerKey = pCurrentAction && !pCurrentAction->triggerKeys.empty();
    if (hasTriggerKey || fluffFrames(fluffFrameBias))
    {
        TriggerCheckState triggers[triggerCheckArraySize];
        int triggerCount = 0;

        if (hasTriggerKey) {
            for (auto& triggerKey : pCurrentAction->triggerKeys)
            {
                if (triggerKey.startFrame > currentFrame || triggerKey.endFrame <= currentFrame) {
                    continue;
                }

                if (triggerKey.validStyle != 0 && !(triggerKey.validStyle & (1 << styleInstall))) {
                    continue;
                }

                // todo _Input for modern vs. classic

                bool defer = triggerKey.other & 1<<6;
                int condition = triggerKey.condition;
                int state = triggerKey.state;

                bool antiNormal = triggerKey.other & 1<<17;

                if (!defer && !CheckTriggerGroupConditions(condition, state)) {
                    continue;
                }

                if (!triggerKey.pTriggerGroup) {
                    continue;
                }

                TriggerGroup *pTriggerGroup = triggerKey.pTriggerGroup;
                for (auto& entry : pTriggerGroup->entries)
                {
                    int actionID = entry.actionID;
                    int triggerID = entry.triggerID;
                    int entryIndex = -1;
                    for (int i = 0; i < triggerCount; i++) {
                        if (triggers[i].triggerID == triggerID) {
                            entryIndex = i;
                            break;
                        }
                    }

                    if (entryIndex == -1) {
                        if (triggerCount >= triggerCheckArraySize) {
                            log(true, "trigger check array overflow!!");
                            continue;
                        }
                        entryIndex = triggerCount++;
                        triggers[entryIndex].triggerID = triggerID;
                        triggers[entryIndex].actionID = actionID;
                        triggers[entryIndex].pTrigger = entry.pTrigger;
                        triggers[entryIndex].hasNormal = false;
                        triggers[entryIndex].hasDeferred = false;
                        triggers[entryIndex].hasAntiNormal = false;
                        triggers[entryIndex].late = false;
                    }

                    if (!defer && !antiNormal) {
                        triggers[entryIndex].hasNormal = true;
                        triggers[entryIndex].late = currentFrame != triggerKey.startFrame && !canAct();

                        if (pTriggerGroup->id == 0) {
                            freeMovement = true;
                        }
                    }
                    if (defer && !antiNormal) {
                        triggers[entryIndex].hasDeferred = true;
                    }
                    if (antiNormal) {
                        triggers[entryIndex].hasAntiNormal = true;
                    }
                }
            }
        }

        // add trigger group 0 in fluff frames and prox guard
        bool addTriggerGroupZero = fluffFrames(fluffFrameBias);
        if (proxGuard()) {
            addTriggerGroupZero = true;
        }

        if (addTriggerGroupZero) {
            auto it = pCharData->triggerGroupByID.find(0);
            if (it != pCharData->triggerGroupByID.end()) {
                TriggerGroup *pTriggerGroup = it->second;
                for (auto& entry : pTriggerGroup->entries) {
                    int actionID = entry.actionID;
                    int triggerID = entry.triggerID;
                    int entryIndex = -1;
                    for (int i = 0; i < triggerCount; i++) {
                        if (triggers[i].triggerID == triggerID) {
                            entryIndex = i;
                            break;
                        }
                    }

                    if (entryIndex == -1) {
                        if (triggerCount >= triggerCheckArraySize) {
                            log(true, "trigger check array overflow!!");
                            continue;
                        }
                        entryIndex = triggerCount++;
                        triggers[entryIndex].triggerID = triggerID;
                        triggers[entryIndex].actionID = actionID;
                        triggers[entryIndex].pTrigger = entry.pTrigger;
                        triggers[entryIndex].hasNormal = true;
                        triggers[entryIndex].hasDeferred = false;
                        triggers[entryIndex].hasAntiNormal = false;
                        triggers[entryIndex].late = false;
                    }
                }
            }
        }

        // possibly need a fixed array here too if it shows up in mining profile
        std::set<uint32_t> initialIsToConsume;

        std::sort(triggers, triggers + triggerCount, [](const TriggerCheckState& a, const TriggerCheckState& b) {
            return a.triggerID < b.triggerID;
        });

        // walk in reverse sorted trigger ID order
        for (int idx = triggerCount - 1; idx >= 0; idx--)
        {
            TriggerCheckState& trigState = triggers[idx];
            int triggerID = trigState.triggerID;
            int actionID = trigState.actionID;
            Trigger *pTrigger = trigState.pTrigger;

            if (!pTrigger) {
                continue;
            }

            if (FindMove(actionID, styleInstall) == nullptr) {
                continue;
            }

            bool forceTrigger = false;

            if (forcedTrigger == ActionRef(actionID, styleInstall)) {
                forceTrigger = true;
            }

            bool recordThisTrigger = true;

            if (!recordFrameTriggers) {
                recordThisTrigger = false;
            } else if (!recordLateCancels && trigState.late) {
                recordThisTrigger = false;
            }

            if (recordThisTrigger && trigState.hasNormal && CheckTriggerConditions(pTrigger, fluffFrameBias)) {
                dc.frameTriggers.insert(ActionRef(actionID, styleInstall));
            }

            uint32_t initialI = 0;

            if (dc.setDeferredTriggerIDs.find(triggerID) != dc.setDeferredTriggerIDs.end()) {
                // check deferred trigger activation
                if (trigState.hasNormal && CheckTriggerConditions(pTrigger, fluffFrameBias)) {
                    log(logTriggers, "did deferred trigger " + std::to_string(actionID));

                    ExecuteTrigger(pTrigger);

                    // skip further triggers and cancel any delayed triggers
                    dc.setDeferredTriggerIDs.clear();
                    keptDeferredTriggerIDs.clear();
                    break;
                }

                // carry forward
                if (trigState.hasDeferred) {
                    keptDeferredTriggerIDs.insert(triggerID);
                }

                // skip further triggers
                if (trigState.hasAntiNormal) {
                    break;
                }
            } else if (forceTrigger || CheckTriggerCommand(pTrigger, initialI)) {
                log(logTriggers, "trigger " + std::to_string(actionID) + " " + std::to_string(triggerID) + " defer " +
                    std::to_string(trigState.hasDeferred) + " normal " + std::to_string(trigState.hasNormal) +
                    + " antinormal " + std::to_string(trigState.hasAntiNormal));
                if (trigState.hasDeferred || trigState.hasAntiNormal) {
                    // queue the deferred trigger
                    dc.setDeferredTriggerIDs.insert(triggerID);
                    keptDeferredTriggerIDs.insert(triggerID);

                    if (!trigState.hasAntiNormal) {
                        initialIsToConsume.insert(initialI);
                    }

                    if (trigState.hasAntiNormal) {
                        break;
                    }
                }

                if (trigState.hasNormal && !trigState.hasAntiNormal && CheckTriggerConditions(pTrigger, fluffFrameBias)) {
                    ExecuteTrigger(pTrigger);

                    // consume the input by removing matching edge bits from matched initial input
                    // otherwise chains trigger off of one input since the cancel window starts
                    // before input buffer ends
                    //int okKeyFlags = (*pTrigger)["norm"]["ok_key_flags"];
                    //dc.inputBuffer[initialI] &= ~((okKeyFlags & (LP+MP+HP+LK+MK+HK)) << 6);

                    lastTriggerFrame = pSim->frameCounter - initialI;
                    dc.inputBuffer[initialI] |= CONSUMED;
                    lastTriggerInput = dc.inputBuffer[initialI];

                    // skip further triggers and cancel any delayed triggers
                    dc.setDeferredTriggerIDs.clear();
                    keptDeferredTriggerIDs.clear();
                    break;
                }
            }
        }

        for (auto &initialI : initialIsToConsume) {
            lastTriggerFrame = pSim->frameCounter - initialI;
            dc.inputBuffer[initialI] |= CONSUMED;
            // if someone uses both trigger input branch and multiple ambiguous deferred triggers,
            // we have to start remembering trigger input per kept deferred trigger
            lastTriggerInput = dc.inputBuffer[initialI];
        }
    }

    for (int id : dc.setDeferredTriggerIDs) {
        if (keptDeferredTriggerIDs.find(id) == keptDeferredTriggerIDs.end()) {
            log(logTriggers, "forgetting deferred trigger " + std::to_string(id));
        }
    }
    dc.setDeferredTriggerIDs = keptDeferredTriggerIDs;
}

Box Guy::rectToBox(Rect *pRect, Fixed offsetX, Fixed offsetY, int dir)
{
    Box outBox;
    outBox.x = Fixed(pRect->xOrig * dir - pRect->xRadius) + offsetX;
    outBox.y = Fixed(pRect->yOrig - pRect->yRadius) + offsetY;
    outBox.w = Fixed(pRect->xRadius * 2);
    outBox.h = Fixed(pRect->yRadius * 2);
    return outBox;
}

void Guy::getPushBoxes(std::vector<Box> *pOutPushBoxes, std::vector<RenderBox> *pOutRenderBoxes)
{
    if (!pCurrentAction) {
        return;
    }

    for (auto& pushBoxKey : pCurrentAction->pushBoxKeys)
    {
        if (pushBoxKey.startFrame > currentFrame || pushBoxKey.endFrame <= currentFrame) {
            continue;
        }

        if (!CheckHitBoxCondition(pushBoxKey.condition)) {
            continue;
        }

        Fixed rootOffsetX = pushBoxKey.offsetX;
        Fixed rootOffsetY = pushBoxKey.offsetY;
        rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
        rootOffsetY = rootOffsetY + posY + posOffsetY;

        Box rect = rectToBox(pushBoxKey.rect, rootOffsetX, rootOffsetY, direction.i());
        if (pOutPushBoxes) {
            pOutPushBoxes->push_back(rect);
        }
        if (pOutRenderBoxes) {
            pOutRenderBoxes->push_back({rect, 30.0, {0.4,0.35,0.0}});
        }
    }
}

void Guy::getHurtBoxes(std::vector<HurtBox> *pOutHurtBoxes, std::vector<HurtBox> *pOutThrowBoxes, std::vector<RenderBox> *pOutRenderBoxes)
{
    if (!pCurrentAction) {
        return;
    }

    bool drive = isDrive || wasDrive;
    bool parry = currentAction >= 480 && currentAction <= 489;
    // doesn't work for all chars, prolly need to find a system bit like drive
    bool di = currentAction >= 850 && currentAction <= 859;

    for (auto& hurtBoxKey : pCurrentAction->hurtBoxKeys)
    {
        if (hurtBoxKey.startFrame > currentFrame || hurtBoxKey.endFrame <= currentFrame) {
            continue;
        }

        if (!CheckHitBoxCondition(hurtBoxKey.condition)) {
            continue;
        }

        bool isArmor = hurtBoxKey.isArmor;
        bool isAtemi = hurtBoxKey.isAtemi;
        int immune = hurtBoxKey.immunity;
        int typeFlags = hurtBoxKey.flags;

        Fixed rootOffsetX = hurtBoxKey.offsetX;
        Fixed rootOffsetY = hurtBoxKey.offsetY;
        rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
        rootOffsetY = rootOffsetY + posY + posOffsetY;

        HurtBox baseBox;
        if (isArmor) {
            baseBox.flags |= armor;
            baseBox.pAtemiData = hurtBoxKey.pAtemiData;
        }
        if (isAtemi) {
            baseBox.flags |= atemi;
        }
        // reverse hitbox type, which ones they interact with
        if (!(typeFlags & 1)) {
            baseBox.flags |= invul_hit;
        }
        if (!(typeFlags & 2)) {
            baseBox.flags |= invul_projectile;
        }
        if (immune & (1<<0)) {
            baseBox.flags |= invul_standing_opponent;
        }
        if (immune & (1<<1)) {
            baseBox.flags |= invul_crouching_opponent;
        }
        if (immune & (1<<2)) {
            baseBox.flags |= invul_airborne_opponent;
        }

        for (auto pRect : hurtBoxKey.legRects) {
            HurtBox newBox = baseBox;
            newBox.box = rectToBox(pRect, rootOffsetX, rootOffsetY, direction.i());
            newBox.flags |= legs;
            if (pOutHurtBoxes) {
                pOutHurtBoxes->insert(pOutHurtBoxes->begin(), newBox);
            }
            if (pOutRenderBoxes) {
                if (newBox.flags & armor) {
                    pOutRenderBoxes->insert(pOutRenderBoxes->begin(), {newBox.box, 30.0, {0.8,0.5,0.0}, drive,parry,di});
                } else {
                    pOutRenderBoxes->insert(pOutRenderBoxes->begin(), {newBox.box, 25.0f, {charColorR,charColorG,charColorB}, drive,parry,di});
                }
            }
        }
        for (auto pRect : hurtBoxKey.bodyRects) {
            HurtBox newBox = baseBox;
            newBox.box = rectToBox(pRect, rootOffsetX, rootOffsetY, direction.i());
            newBox.flags |= body;
            if (pOutHurtBoxes) {
                pOutHurtBoxes->insert(pOutHurtBoxes->begin(), newBox);
            }
            if (pOutRenderBoxes) {
                if (newBox.flags & armor) {
                    pOutRenderBoxes->insert(pOutRenderBoxes->begin(), {newBox.box, 30.0, {0.8,0.5,0.0}, drive,parry,di});
                } else {
                    pOutRenderBoxes->insert(pOutRenderBoxes->begin(), {newBox.box, 25.0f, {charColorR,charColorG,charColorB}, drive,parry,di});
                }
            }
        }
        for (auto pRect : hurtBoxKey.headRects) {
            HurtBox newBox = baseBox;
            newBox.box = rectToBox(pRect, rootOffsetX, rootOffsetY, direction.i());
            newBox.flags |= head;
            if (pOutHurtBoxes) {
                pOutHurtBoxes->insert(pOutHurtBoxes->begin(), newBox);
            }
            if (pOutRenderBoxes) {
                if (newBox.flags & armor) {
                    pOutRenderBoxes->insert(pOutRenderBoxes->begin(), {newBox.box, 30.0, {0.8,0.5,0.0}, drive,parry,di});
                } else {
                    pOutRenderBoxes->insert(pOutRenderBoxes->begin(), {newBox.box, 17.5f, {charColorR,charColorG,charColorB}, drive,parry,di});
                }
            }
        }

        for (auto pRect : hurtBoxKey.throwRects) {
            HurtBox newBox = baseBox;
            newBox.box = rectToBox(pRect, rootOffsetX, rootOffsetY, direction.i());
            if (pOutThrowBoxes) {
                pOutThrowBoxes->push_back(newBox);
            }
            if (pOutRenderBoxes) {
                pOutRenderBoxes->push_back({newBox.box, 35.0, {0.15,0.20,0.8}, drive,parry,di});
            }
        }
    }
}

void Guy::getHitBoxes(std::vector<HitBox> *pOutHitBoxes, std::vector<RenderBox> *pOutRenderBoxes, hitBoxType typeFilter)
{
    if (!pCurrentAction) {
        return;
    }

    bool drive = isDrive || wasDrive;

    for (auto& hitBoxKey : pCurrentAction->hitBoxKeys)
    {
        if (typeFilter != none && hitBoxKey.type != typeFilter) {
            continue;
        }
        if (hitBoxKey.startFrame > currentFrame || hitBoxKey.endFrame <= currentFrame) {
            continue;
        }

        if (!CheckHitBoxCondition(hitBoxKey.condition)) {
            continue;
        }

        if (hitBoxKey.hasValidStyle && !(hitBoxKey.validStyle & (1 << styleInstall))) {
            continue;
        }

        Fixed rootOffsetX = hitBoxKey.offsetX;
        Fixed rootOffsetY = hitBoxKey.offsetY;
        rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
        rootOffsetY = rootOffsetY + posY + posOffsetY;

        hitBoxType type = hitBoxKey.type;
        color collisionColor = {1.0,0.0,0.0};
        if (type == domain) {
            collisionColor = {1.0,0.0,0.0};
        } else if (type == destroy_projectile) {
            collisionColor = {0.0,1.0,0.5};
        } else if (type == proximity_guard) {
            collisionColor = {0.5,0.5,0.5};
        } else if (type == screen_freeze) {
            collisionColor = {0.0,0.4,0.9};
        } else if (type == clash) {
            collisionColor = {0.0,0.7,0.2};
        }

        float thickness = 50.0;
        if (type == proximity_guard) {
            thickness = 5.0;
        }

        if (type == domain) {
            Box rect = {-4096,-4096,8192,8192};
            if (pOutRenderBoxes) {
                pOutRenderBoxes->push_back({rect, thickness, collisionColor, drive});
            }
            if (pOutHitBoxes) {
                pOutHitBoxes->push_back({rect, type, hitBoxKey.hitID, hitBoxKey.flags, hitBoxKey.pHitData});
            }
        } else {
            for (auto pRect : hitBoxKey.rects) {
                Fixed rootOffsetToUse = rootOffsetX;
                if (hitBoxKey.flags & fixed_position) {
                    rootOffsetToUse = moveStartPos + hitBoxKey.offsetX * direction;
                }
                Box rect = rectToBox(pRect, rootOffsetToUse, rootOffsetY, direction.i());
                if (pOutRenderBoxes) {
                    pOutRenderBoxes->push_back({rect, thickness, collisionColor, drive && type != proximity_guard});
                }

                if (pOutHitBoxes) {
                    pOutHitBoxes->push_back({rect, type, hitBoxKey.hitID, hitBoxKey.flags, hitBoxKey.pHitData});
                }
            }
        }
    }
}

void Guy::getUniqueBoxes(std::vector<UniqueBox> *pOutHitBoxes, std::vector<RenderBox> *pOutRenderBoxes)
{
    if (!pCurrentAction) {
        return;
    }

    for (auto& hitBoxKey : pCurrentAction->uniqueBoxKeys)
    {
        if (hitBoxKey.startFrame > currentFrame || hitBoxKey.endFrame <= currentFrame) {
            continue;
        }

        if (!CheckHitBoxCondition(hitBoxKey.condition)) {
            continue;
        }

        Fixed rootOffsetX = hitBoxKey.offsetX;
        Fixed rootOffsetY = hitBoxKey.offsetY;
        rootOffsetX = posX + ((rootOffsetX + posOffsetX) * direction);
        rootOffsetY = rootOffsetY + posY + posOffsetY;

        color boxColor = hitBoxKey.uniquePitcher ? color(0.3,0.496,0.5) : color(0.422,0.265,0.429);

        for (auto pRect : hitBoxKey.rects) {
            Box rect = rectToBox(pRect, rootOffsetX, rootOffsetY, direction.i());
            if (pOutRenderBoxes) {
                pOutRenderBoxes->push_back({rect, 50.0, boxColor, false});
            }
            if (pOutHitBoxes) {
                pOutHitBoxes->push_back({rect, hitBoxKey.checkMask, hitBoxKey.uniquePitcher, hitBoxKey.applyOpToTarget, &hitBoxKey.ops});
            }
        }
    }
}


void Guy::Render(float z /* = 0.0f */) {
    Fixed fixedX = posX + (posOffsetX * direction);
    Fixed fixedY = posY + posOffsetY;
    float x = fixedX.f();
    float y = fixedY.f();

    std::vector<RenderBox> renderBoxes;
    getHurtBoxes(nullptr, nullptr, &renderBoxes);
    getPushBoxes(nullptr, &renderBoxes);
    getHitBoxes(nullptr, &renderBoxes);
    getUniqueBoxes(nullptr, &renderBoxes);

    for (auto box : renderBoxes) {
        drawHitBox(box.box,thickboxes?box.thickness:1,z,box.col,box.drive,box.parry,box.di);
    }

    if (renderPositionAnchors) {
        float radius = 6.0;
        float thickness = thickboxes?radius:1;
        drawBox(x-radius/2,y-radius/2,radius,radius,thickness,z,charColorR,charColorG,charColorB,0.2);
        // radius = 5.0;
        // drawBox(x-radius/2,y-radius/2,radius,radius,thickness,1.0,1.0,1.0,0.2);

        // meters
        drawBox(x - 35.0, y - 5.0, focus / float(maxFocus) * 30.0, 3.0, 1.0, z, 0.20, 0.80, 0.0, 0.2);
        drawBox(x + 5.0, y - 5.0, gauge / float(pCharData->gauge) * 30.0, 3.0, 1.0, z, 0.0, 0.80, 0.80, 0.2);
    }
}

int Guy::getFrameMeterColorIndex() {
    int ret = 0;
    bool a,b,c;
    if (canMove(a,b,c)) {
        ret = 1;
    }
    if (punishCounterState) {
        ret = 2;
    }
    if (counterState) {
        ret = 3;
    }
    std::vector<HitBox> hitBoxes;
    getHitBoxes(&hitBoxes);
    for (auto &hitBox : hitBoxes) {
        if (hitBox.type != proximity_guard) {
            ret = 4;
        }
    }
    if (hitStun) {
        ret = 5;
    }
    if (getHitStop() || warudo) {
        ret = 6;
    }
    return ret;
}

bool Guy::Push(Guy *pOtherGuy)
{
    // might need to put this at the end if we find ordering difference, but convenient for now
    if (pOtherGuy) {
        for (GuyRef minion : pOtherGuy->getMinions()) {
            Push(minion);
        }
    }
    // seems like ignore hitstop bit lingers for one frame but just for push?
    // probably symptom of an ordering difference
    if (warudo || (getHitStop() && !wasIgnoreHitStop)) return false;

    // do reflect before push, since vel could be winning to push us flush against someone
    // in theory should do before wall touch too but not sure if both can be touching the
    // wall at the same time while stll being affected by reflect
    if (pOpponent && !pOpponent->warudo && (reflectThisFrame == Fixed(0) || deferredReflect)) {
        if (hitReflectVelX != Fixed(0)) {
            if (!locked && !nageKnockdown) {
                posX = posX + hitReflectVelX;
            }

            log(logTransitions, "reflect " + std::to_string(hitReflectVelX.f()));
            reflectThisFrame = hitReflectVelX;
            Fixed prevHitVelX = hitReflectVelX;
            hitReflectVelX = hitReflectVelX + hitReflectAccelX;
            if ((hitReflectVelX * prevHitVelX) < Fixed(0) || (hitReflectAccelX != Fixed(0) && hitReflectVelX == Fixed(0))) {
                hitReflectAccelX = Fixed(0);
                hitReflectVelX = Fixed(0);
            }
        }
    }
    deferredReflect = false;

    if (didPush) return false;
    if ( !pOtherGuy ) return false;
    // for now, maybe there's other rules
    if (isProjectile && !pOtherGuy->isProjectile) return false;
    if (!isProjectile && pOtherGuy->isProjectile) return false;


    if (isProjectile) {
        if (projHitCount < 0 || pOtherGuy->projHitCount < 0) {
            return false;
        }
        if (getHitStop() || pOtherGuy->getHitStop()) {
            return false;
        }
    }

    std::vector<Box> pushBoxes;
    std::vector<Box> otherPushBoxes;
    getPushBoxes(&pushBoxes);
    pOtherGuy->getPushBoxes(&otherPushBoxes);

    bool hasPushed = false;
    touchedOpponent = false;
    Fixed pushXLeft = 0;
    Fixed pushXRight = 0;
    for (auto pushbox : pushBoxes ) {
        for (auto otherPushBox : otherPushBoxes ) {
            if (doBoxesHit(pushbox, otherPushBox)) {

                // check hitcount in case we already clashed but still in hitstop
                if (isProjectile && projHitCount > 0 && pOtherGuy->isProjectile && pOtherGuy->projHitCount > 0) {
                    // projectile clash
                    int ownHitStop = 10;
                    int otherHitStop = 10;
                    if (pCurrentAction->pProjectileData->clashPriority < pOtherGuy->pCurrentAction->pProjectileData->clashPriority) {
                        projHitCount--;
                        // if (projHitCount != 0) {
                        //     otherHitStop += pOtherGuy->pCurrentAction->pProjectileData->extraHitStop;
                        // }
                    } else if (pCurrentAction->pProjectileData->clashPriority > pOtherGuy->pCurrentAction->pProjectileData->clashPriority) {
                        pOtherGuy->projHitCount--;
                        // if (pOtherGuy->projHitCount != 0) {
                        //     ownHitStop += pCurrentAction->pProjectileData->extraHitStop;
                        // }
                    } else {
                        projHitCount--;
                        pOtherGuy->projHitCount--;
                    }
                    addHitStop(ownHitStop);
                    pOtherGuy->addHitStop(otherHitStop);
                }
                pushXLeft = fixMax(pushXLeft, pushbox.x + pushbox.w - otherPushBox.x);
                pushXRight = fixMin(pushXRight, pushbox.x - (otherPushBox.x + otherPushBox.w));
                //log(logTransitions, "push left/right " + std::to_string(pushXLeft.f()) + " " + std::to_string(pushXRight.f()));
                hasPushed = true;
                if (noPush) {
                    hasPushed = false;
                }
            }
        }
    }

    if ( hasPushed ) {
        //log(logTransitions, "pushXLeft " + std::to_string(pushXLeft.f()) + "pushXRight " + std::to_string(pushXRight.f()));
        // pushXLeft = fixMax(Fixed(0), pushXLeft);
        // pushXRight = fixMin(Fixed(0), pushXRight);

        // todo if you move fast enough to completely move through someone without ever touching them
        // it still needs to push apparently and there's no side-switch in that situation

        Fixed pushNeeded = Fixed(0);
        if (getAirborne() || pOpponent->getAirborne()) {
            if (teleported) {
                if (getPosX() < pOpponent->getPosX()) {
                    pushNeeded = -pushXLeft;
                }
                if (getPosX() > pOpponent->getPosX()) {
                    pushNeeded = -pushXRight;
                }
            } else {
                // if your pushbox descends on someone else's the same frame you land behind,
                // it seems to stay in front, hence using the last frame's position
                if (getLastPosX() < pOpponent->getLastPosX()) {
                    pushNeeded = -pushXLeft;
                }
                if (getLastPosX() > pOpponent->getLastPosX()) {
                    pushNeeded = -pushXRight;
                }
            }

            if (getPosX() == pOpponent->getPosX() && direction > Fixed(0)) {
                pushNeeded = -pushXLeft;
            }
            if (getPosX() == pOpponent->getPosX() && direction < Fixed(0)) {
                pushNeeded = -pushXRight;
            }
        } else {
            if (fixAbs(pushXLeft) < fixAbs(pushXRight)) {
                pushNeeded = -pushXLeft;
            } else {
                pushNeeded = -pushXRight;
            }
        }

        // if (fixAbs(pushXLeft) < fixAbs(pushXRight)) {
        //     pushNeeded = -pushXLeft;
        // }
        // if (onLeftWall() && !pOpponent->onLeftWall()) {
        //     pushNeeded = -pushXLeft;
        // }
        // if (!onLeftWall() && pOpponent->onLeftWall()) {
        //     pushNeeded = -pushXRight;
        // }
        // if (onRightWall() && !pOpponent->onRightWall()) {
        //     pushNeeded = -pushXRight;
        // }
        // if (!onRightWall() && pOpponent->onRightWall()) {
        //     pushNeeded = -pushXLeft;
        // }
        Fixed velDiff = velocityX * direction + pOtherGuy->velocityX * pOtherGuy->direction;
        log(logTransitions, "push needed " + std::to_string(pushNeeded.f()) +
" vel diff " + std::to_string(velDiff.f()) + " offset no push " + std::to_string
(offsetDoesNotPush));
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
            // todo figure out if this ever existed, or is this was just cosmetic offset
            // if (!offsetDoesNotPush && pOpponent->offsetDoesNotPush) {
            //     return false;
            // }

            // // does no-push-offset go against push? (different sign)
            // if (offsetDoesNotPush && posOffsetX * pushNeeded < Fixed(0)) {
            //      Fixed absOffset = fixAbs(posOffsetX);
            //     Fixed absPushNeeded = fixAbs(pushNeeded);

            //     if (absPushNeeded > absOffset) {
            //         posOffsetX = Fixed(0);
            //         absPushNeeded -= absOffset;
            //         // restore sign
            //         pushNeeded = absPushNeeded * (pushNeeded / fixAbs(pushNeeded));
            //     } else {
            //         pushNeeded = Fixed(0);
            //         absOffset -= absPushNeeded;
            //         // restore sign
            //         posOffsetX = absOffset * (posOffsetX / fixAbs(posOffsetX));
            //     }
            //     posX += pushNeeded;
            // }

            Fixed halfPushNeeded = pushNeeded / Fixed(2);

            // do regular push with any remaining pushNeeded
            if ((ignoreBodyPush && !pOtherGuy->ignoreBodyPush) || (warudo)) {
                pOtherGuy->posX -= pushNeeded;
            } else if ((!ignoreBodyPush && pOtherGuy->ignoreBodyPush) || (pOtherGuy->warudo)) {
                posX += pushNeeded;
            } else {
                posX = posX + halfPushNeeded;
                pOtherGuy->posX = pOtherGuy->posX - halfPushNeeded;

                int fixedRemainder = pushNeeded.data - halfPushNeeded.data * 2;
                int frameNumber = pSim->frameCounter;

                //log(logTransitions, "fixedRemainder " + std::to_string(fixedRemainder) + " frameNum " +  std::to_string(frameNumber) + " " + getCharacter());

                // give remainder to either player depending on frame count
                if (frameNumber & 1) {
                    posX.data += fixedRemainder;
                } else {
                    pOtherGuy->posX.data += fixedRemainder;
                }
            }

            // both are in contact now, slide back/forth appropriately if anyone got pushed into a wall
            Fixed wallDiff = Fixed(0);
            if (getPosX() < -wallDistance) {
                wallDiff = -wallDistance;
                wallDiff -= getPosX();
                touchedWall = true;
            }
            if (getPosX() > wallDistance) {
                wallDiff = wallDistance;
                wallDiff -= getPosX();
                touchedWall = true;
            }
            if (pOtherGuy->getPosX() < -wallDistance) {
                wallDiff = -wallDistance;
                wallDiff -= pOtherGuy->getPosX();
                pOtherGuy->touchedWall = true;
            }
            if (pOtherGuy->getPosX() > wallDistance) {
                wallDiff = wallDistance;
                wallDiff -= pOtherGuy->getPosX();
                pOtherGuy->touchedWall = true;
            }
            if (wallDiff != Fixed(0)) {
                posX += wallDiff;
                pOtherGuy->posX += wallDiff;
            }

            didPush = true;
            pOtherGuy->didPush = true;
        }

        touchedOpponent = true; // could be touching anyone really but can fix later
        pOtherGuy->touchedOpponent = true;
        return true;
    }

    return false;
}

bool Guy::WorldPhysics(bool onlyFloor, bool projBoundaries)
{
    bool hasPushed = false;
    Fixed pushX = Fixed(0);
    bool floorpush = false;
    touchedWall = false;
    didPush = false;

    if (!noPush) {
        // Floor

        Fixed landThreshold = Fixed(1);
        if (landingAdjust) {
            landThreshold = Fixed(0);
        }

        if (getPosY() - Fixed(landingAdjust) <= landThreshold) {
            //log("floorpush pos");
            floorpush = true;
        }

        // walls and screen
        if (!onlyFloor) {
            Fixed screenCenterX = Fixed(0);
            if (pOpponent) {
                Guy *pScreenGuyOne = pOpponent;
                Guy *pScreenGuyTwo = isProjectile ? pParent.pGuy : this;
                Fixed bothPlayerPos = pScreenGuyOne->getLastPosX(true) + pScreenGuyTwo->getLastPosX(true);
                screenCenterX = bothPlayerPos / Fixed(2);
                int fixedRemainder = bothPlayerPos.data - screenCenterX.data * 2;
                screenCenterX.data += fixedRemainder;
                //log(true, "screencenter " + std::to_string(screenCenterX.f()));
                screenCenterX = fixMax(-maxScreenCenterDisplacement, screenCenterX);
                screenCenterX = fixMin(maxScreenCenterDisplacement, screenCenterX);
                if (pScreenGuyOne->onLeftWall() || pScreenGuyTwo->onLeftWall()) {
                    screenCenterX = -maxScreenCenterDisplacement;
                }
                if (pScreenGuyOne->onRightWall() || pScreenGuyTwo->onRightWall()) {
                    screenCenterX = maxScreenCenterDisplacement;
                }
            }

            Fixed x = getPosX();
            if (!isProjectile) {
                // screen
                if (pOpponent && !ignoreScreenPush) {
                    if (x < screenCenterX - maxPlayerDistance) {
                        pushX = -(x - (screenCenterX - maxPlayerDistance));
                    }
                    if (x > screenCenterX + maxPlayerDistance) {
                        pushX = -(x - (screenCenterX + maxPlayerDistance));
                    }
                    if (pushX != Fixed(0)) {
                        log (logTransitions, "screen push " + std::to_string(pushX.f()));
                    }
                }

                if (x < -wallDistance ) {
                    pushX = -(x - -wallDistance);
                }
                if (x > wallDistance ) {
                    pushX = -(x - wallDistance);
                }
            } else if (pCurrentAction->pProjectileData && pOpponent) {
                Fixed &forward = pCurrentAction->pProjectileData->wallBoxForward;
                Fixed &back = pCurrentAction->pProjectileData->wallBoxBack;
                if (direction > Fixed(0)) {
                    if (forward != Fixed(0)) {
                        Fixed nose = x + forward;
                        if (nose > projWallDistance) {
                            pushX = -(nose - projWallDistance);
                        }
                        if (projBoundaries && nose > screenCenterX + maxProjectileDistance) {
                            pushX = -(nose - (screenCenterX + maxProjectileDistance));
                        }
                    }
                    if (back != Fixed(0)) {
                        Fixed tail = x - back;
                        if (tail < -projWallDistance) {
                            pushX = -(tail - -projWallDistance);
                        }
                        if (projBoundaries && tail < screenCenterX - maxProjectileDistance) {
                            pushX = -(tail - (screenCenterX - maxProjectileDistance));
                        }
                    }
                } else {
                    if (forward != Fixed(0)) {
                        Fixed nose = x - forward;
                        if (nose < -projWallDistance) {
                            pushX = -(nose - -projWallDistance);
                        }
                        if (projBoundaries && nose < screenCenterX - maxProjectileDistance) {
                            pushX = -(nose - (screenCenterX - maxProjectileDistance));
                        }
                    }
                    if (back != Fixed(0)) {
                        Fixed tail = x + back;
                        if (tail > projWallDistance) {
                            pushX = -(tail - projWallDistance);
                        }
                        if (projBoundaries && tail > screenCenterX + maxProjectileDistance) {
                            pushX = -(tail - (screenCenterX + maxProjectileDistance));
                        }
                    }
                }
            }
        }
    }

    if (pushX != Fixed(0)) {
        touchedWall = true;
        hasPushed = true;
    }

    // you can still be in floor contact on your way up
    // especially with the 1 threshold
    bool landedByFloorPush = airborne && floorpush && velocityY < 0;

    if (forceLanding || landedByFloorPush)
    {
        velocityY = Fixed(0);
        accelY = Fixed(0);

        airborne = false;

        // we dont go through the landing transition here
        // we just need to go to the ground and not be airborne
        // so we can move after this script ends, like tatsu
        if (!forceLanding || knockDown) {
            landed = true;
        }
        if (!forceLanding) {
            bool doEmptyLanding = currentAction == 36 || currentAction == 37 || currentAction == 38;
            if (hitStun && !locked && !knockDown) {
                // air recovery
                doEmptyLanding = true;
            }
            if (doEmptyLanding) {
                // empty jump landing, immediate transition
                // this is why empty jumps have a frame shaved off
                if (hitStun) {
                    nextAction = 39;
                    // air recovery landing
                    hitStun = 1;
                    strikeProtectionFrames = 1;
                    resetComboCount = true;
                } else {
                    nextAction = currentAction + 3;
                }
                AdvanceFrame(false);
            }
        }
        log (logTransitions, "landed " + std::to_string(hitStun));
    }

    if (!airborne && (groundBounce || tumble || slide)) {
        log (logTransitions, "ground bounce/tumble/slide!");
        // dont let stuff below see landed/grounded
        landed = false;

        // tumble is airborne while tumbling so works for both
        airborne = true;

        // use this for tumble too, 'special landing needing transition'
        bounced = true;
    }

    if (landed && knockDown) {
        isDown = true; // can't let stuff hit us on landing frame - see donkey kick into mp dp
    }

    if ( hasPushed ) {
        posX += pushX;
        log (logTransitions, "push " + std::to_string(pushX.f()));
        // 1:1 pushback for opponent during lock, and vice versa
        if (locked && pAttacker && !pAttacker->pendingUnlockHit && !pAttacker->ignoreCornerPushback) {
            pAttacker->posX += pushX;
            //log (logTransitions, "lock reflect " + std::to_string(pushX.f()));
        }
        if (pOpponent && pOpponent->locked && !pendingUnlockHit && !pOpponent->ignoreCornerPushback) {
            pOpponent->posX += pushX;
        }

        if (pushBackThisFrame != Fixed(0) && pushX != Fixed(0) && pushX * pushBackThisFrame < Fixed(0)) {
            // touched the wall during pushback
            if (pAttacker && !pAttacker->noPush && !noCounterPush) {
                pAttacker->reflectThisFrame = fixMin(fixAbs(pushX), fixAbs(pushBackThisFrame));
                if (pushX < Fixed(0)) {
                    pAttacker->reflectThisFrame *= Fixed(-1);
                }
                pAttacker->posX += pAttacker->reflectThisFrame;
                pAttacker->hitReflectVelX = hitVelX * Fixed(-1);
                pAttacker->hitReflectAccelX = hitAccelX * Fixed(-1);
                log(logTransitions, "start reflect! initial " + std::to_string(pAttacker->reflectThisFrame.f()));
            }
            hitVelX = Fixed(0);
            hitAccelX = Fixed(0);
        }
    }

    if (getPosY() - Fixed(landingAdjust) < Fixed(0) || landedByFloorPush || forceLanding)
    {
        // don't update hitboxes before setting posY, the current frame
        // or the box will be too high up as we're still on the falling box
        // see heave donky into lp dp
        posY = Fixed(0);
        posOffsetY = Fixed(0);
    }

    if (landed || (bounced && (tumble || slide))) {
        // the frame you land is supposed to instantly turn into 330
        if (resetHitStunOnLand && knockDown) {
            log(logTransitions, "hack extra landing frame");
            AdvanceFrame(false); // only transition
        }
    }

    forceLanding = false;

    return hasPushed;
}

void Guy::CheckHit(Guy *pOtherGuy, std::vector<PendingHit> &pendingHitList)
{
    if (parryFreeze || warudo || getHitStop() || (pOpponent && pOpponent->wallStopped && pOpponent->stunned)) return;
    if ( !pOtherGuy ) return;

    std::vector<HitBox> hitBoxes;
    std::vector<HurtBox> otherThrowBoxes;
    std::vector<HurtBox> otherHurtBoxes;
    bool hasEvaluatedThrowBoxes = false;
    bool hasEvaluatedHurtBoxes = false;

    getHitBoxes(&hitBoxes);

    for (auto const &hitbox : hitBoxes) {
        if ((hitbox.type == hit || hitbox.type == projectile || hitbox.type == grab) && isProjectile && hitSpanFrames) {
            continue;
        }
        if (hitbox.hitID != -1 && ((1ULL<<hitbox.hitID) & canHitID)) {
            continue;
        }
        if (hitbox.type == destroy_projectile) {
            // todo right now we do nothing with those
            continue;
        }
        bool isGrab = hitbox.type == grab;
        bool foundBox = false;
        HurtBox hurtBox = {};

        bool applyFlags = true;

        if (hitbox.type == domain) {
            applyFlags = false;
        }

        // flags didn't seem to apply to projectile prox guard boxes, but now they do?
        if (pCharData->charVersion >= 39 && isProjectile && hitbox.type == proximity_guard) {
            applyFlags = false;
        }

        if (applyFlags) {
            if (pOtherGuy->airborne) {
                if (hitbox.flags & avoids_airborne) {
                    continue;
                }
            } else {
                if (hitbox.flags & avoids_standing && !pOtherGuy->getCrouching()) {
                    continue;
                }
                if (hitbox.flags & avoids_crouching && pOtherGuy->getCrouching()) {
                    continue;
                }
            }
            if (hitbox.flags & only_hits_in_combo && (pOtherGuy->hitStun == 0 || pOtherGuy->blocking)) {
                continue;
            }
            Fixed posDiff = pOtherGuy->getPosX() - getPosX();
            posDiff *= getDirection();
            if (posDiff > Fixed(0)) {
                if (hitbox.flags & only_hits_behind) {
                    continue;
                }
            } else if (posDiff < Fixed(0)) {
                if (hitbox.flags & only_hits_front) {
                    continue;
                }
            }
        }

        if (isGrab) {
            if (!pOtherGuy->throwProtectionFrames) {
                if (!hasEvaluatedThrowBoxes) {
                    pOtherGuy->getHurtBoxes(nullptr, &otherThrowBoxes, nullptr);
                    hasEvaluatedThrowBoxes = true;
                }
                for (auto throwBox : otherThrowBoxes ) {
                    if (getAirborne()) {
                        if (throwBox.flags & invul_airborne_opponent) {
                            continue;
                        }
                    } else if (getCrouching()) {
                        if (throwBox.flags & invul_crouching_opponent) {
                            continue;
                        }
                    } else {
                        if (throwBox.flags & invul_standing_opponent) {
                            continue;
                        }
                    }
                    if (doBoxesHit(hitbox.box, throwBox.box)) {
                        foundBox = true;
                        hurtBox.box = throwBox.box;
                        break;
                    }
                }
            }
        } else {
            // everything except grab
            if (hitbox.type == hit || hitbox.type == projectile) {
                if (pOtherGuy->isDown) {
                    // todo is real otg a thing?
                    continue;
                }
                if (pOtherGuy->airborne && pOtherGuy->hitStun && !pOtherGuy->knockDown && !pOtherGuy->locked) {
                    // air recovery
                    continue;
                }
                if (pOtherGuy->strikeProtectionFrames) {
                    continue;
                }
            }
            if (hitbox.type == proximity_guard) {
                if (pOtherGuy->hurtBoxProxGuarded && pOtherGuy->positionProxGuarded) {
                    // already did the worst we were going to do with those
                    continue;
                }
                Fixed otherGuyPosX = pOtherGuy->getPosX();
                Fixed boxX1, boxX2;
                boxX1 = hitbox.box.x;
                boxX2 = hitbox.box.x + hitbox.box.w;
                if (otherGuyPosX >= boxX1 && otherGuyPosX < boxX2) {
                    pOtherGuy->positionProxGuarded = true;
                }
            }
            if (hitbox.type == domain) {
                // todo maybe filter by grab target?
                foundBox = true;
            } else if (hitbox.type == hit || hitbox.type == projectile || hitbox.type == proximity_guard || hitbox.type == direct_damage) {
                // those want hurtboxes
                if (!hasEvaluatedHurtBoxes) {
                    pOtherGuy->getHurtBoxes(&otherHurtBoxes, nullptr);
                    hasEvaluatedHurtBoxes = true;
                }
                for (auto const&hurtbox : otherHurtBoxes ) {
                    if (hitbox.type == hit && hurtbox.flags & invul_hit) {
                        continue;
                    }
                    if (hitbox.type == projectile && hurtbox.flags & invul_projectile) {
                        continue;
                    }
                    if (getAirborne()) {
                        if (hurtbox.flags & invul_airborne_opponent) {
                            continue;
                        }
                    } else if (getCrouching()) {
                        if (hurtBox.flags & invul_crouching_opponent) {
                            continue;
                        }
                    } else {
                        if (hurtBox.flags & invul_standing_opponent) {
                            continue;
                        }
                    }

                    if (hitbox.type == proximity_guard) {
                        if (!pOtherGuy->hurtBoxProxGuarded) {
                            // prox guard boxes only consider x extents
                            if (doBoxesHitXAxis(hitbox.box, hurtbox.box)) {
                                pOtherGuy->hurtBoxProxGuarded = true;
                                // nothing else to do but mark
                            }
                        }
                    } else {
                        if (doBoxesHit(hitbox.box, hurtbox.box)) {
                            hurtBox = hurtbox;
                            foundBox = true;
                            //log(logHits, "foundbox");
                            break; // don't do further hitboxes from us
                        }
                    }
                }
            } else if (hitbox.type == screen_freeze) {
                // those want other screen_freeze boxes
                // those tend to come in 1s so probably not worth trying to recycle the vec?
                std::vector<HitBox> otherFreezeBoxes;
                pOtherGuy->getHitBoxes(&otherFreezeBoxes, nullptr, hitBoxType::screen_freeze);
                for (auto const & freezeBox : otherFreezeBoxes ) {
                    if (doBoxesHit(hitbox.box, freezeBox.box)) {
                        parryFreeze += 61;
                        pOtherGuy->parryFreeze += 61;

                        if (hitbox.hitID != -1) {
                            canHitID |= 1ULL << hitbox.hitID;
                        }
                        if (freezeBox.hitID != -1) {
                            pOtherGuy->canHitID |= 1ULL << freezeBox.hitID;
                        }
                    }
                }
            } else if (hitbox.type == clash) {
                // those want other clash boxes
                std::vector<HitBox> otherClashBoxes;
                pOtherGuy->getHitBoxes(&otherClashBoxes, nullptr, hitBoxType::clash);
                for (auto const & clashBox : otherClashBoxes ) {
                    if (doBoxesHit(hitbox.box, clashBox.box)) {
                        // in reality there is one more frame of hitstop but movement mysteriously starts 1f before hitstop ends
                        // we do one frame less of hitstop and we hold here for 1f, which equates to the same thing (right???)
                        ApplyHitEffect(&clashBox.pHitData->param[0], pOtherGuy, false, false, false, false, false, true);
                        addHitStop(clashBox.pHitData->param[0].hitStopTarget+1);
                        currentFrameFrac -= Fixed(1);

                        pOtherGuy->ApplyHitEffect(&hitbox.pHitData->param[0], this, false, false, false, false, false, true);
                        pOtherGuy->addHitStop(hitbox.pHitData->param[0].hitStopTarget+1);
                        pOtherGuy->currentFrameFrac -= Fixed(1);

                        if (hitbox.hitID != -1) {
                            canHitID |= 1ULL << hitbox.hitID;
                        }

                        if (clashBox.hitID != -1) {
                            pOtherGuy->canHitID |= 1ULL << clashBox.hitID;
                        }
                    }
                }
            }
        }

        if (foundBox) {
            if (hitbox.type != hit && hitbox.type != projectile && hitbox.type != grab && hitbox.type != domain && hitbox.type != direct_damage) {
                // not supposed to get through!
                exit(0);
                log(true, "wtf! " + std::to_string(hitbox.type));
                continue;
            }

            int hitEntryFlag = 0;

            bool otherGuyAirborne = pOtherGuy->getAirborne();

            if (pOtherGuy->counterState || (forceCounter && pOtherGuy->comboHits == 0)) {
                hitEntryFlag |= counter;
            }
            // the force from the UI doesn't do the right thing for multi hit moves like hands ATM
            if (pOtherGuy->punishCounterState || (forcePunishCounter && pOtherGuy->comboHits == 0)) {
                hitEntryFlag |= punish_counter;
            }

            if (hitbox.flags & multi_counter) {
                if (hitCounterThisMove) {
                    hitEntryFlag |= counter;
                }
                if (hitPunishCounterThisMove) {
                    hitEntryFlag |= punish_counter;
                }
            }

            bool otherGuyHit = pOtherGuy->hitStun && !pOtherGuy->blocking;
            bool otherGuyCanBlock = pOtherGuy->canAct() || pOtherGuy->blocking;
            // can block special status (some stances?)
            if (pOtherGuy->canBlock) {
                otherGuyCanBlock = true;
            }
            // but parry recovery is magic?!
            if (pOtherGuy->currentAction == 482) {
                otherGuyCanBlock = true;
            }
            // empty jump landing also magic
            if (pOtherGuy->currentAction == 39 || pOtherGuy->currentAction == 40 || pOtherGuy->currentAction == 41) {
                otherGuyCanBlock = true;
            }
            if (!(pOtherGuy->currentInput & BACK)) {
                otherGuyCanBlock = false;
            }
            if (otherGuyAirborne || otherGuyHit || isGrab) {
                otherGuyCanBlock = false;
            }
            bool blocked = false;
            bool parried = false;
            if (pOtherGuy->parrying && !isGrab) {
                parried = true;
            } else if (pOtherGuy->blocking || otherGuyCanBlock) {
                blocked = true;
            }

            if (hitbox.flags & overhead && (pOtherGuy->currentInput & (DOWN+BACK)) != BACK) {
                blocked = false;
            }
            if (hitbox.flags & low && (pOtherGuy->currentInput & (DOWN+BACK)) != DOWN+BACK) {
                blocked = false;
            }

            if (blocked) {
                hitEntryFlag = pOtherGuy->burnout ? special : block;
            }

            // apply those modifiers on top of the block base if needed
            // todo find test for block+crouch
            if (pOtherGuy->getCrouching()) {
                hitEntryFlag |= crouch;
            }
            if (otherGuyAirborne) {
                hitEntryFlag |= air;
            }

            if (isGrab && (pOtherGuy->blocking && pOtherGuy->hitStun)) {
                // a grab would whiff if opponent is already in blockstun
                continue;
            }

            if (hitbox.type == domain && !pOtherGuy->locked) {
                // domain only when locked, other things can hit during lock
                continue;
            }

            HitEntry *pHitEntry = nullptr;
            bool bombBurst = false;

            if (!hitbox.pHitData) {
                continue;
            }
            HitData *pHitData = hitbox.pHitData;

            if (isGrab) {
                pHitEntry = &pHitData->common[0];
            } else if (parried) {
                pHitEntry = &pHitData->common[4];
            } else {
                pHitEntry = &pHitData->param[hitEntryFlag];
            }

            // if bomb burst and found a bomb, use the next hit ID instead
            if (pHitEntry->bombBurst && pOpponent->debuffTimer) {
                bombBurst = true;
                int nextHitID = pHitData->id + 1;
                auto bombHitIt = pCharData->hitByID.find(nextHitID);
                if (bombHitIt != pCharData->hitByID.end()) {
                    pHitData = bombHitIt->second;
                    pHitEntry = &pHitData->param[hitEntryFlag];
                }
            }

            int juggleLimit = pHitEntry->juggleLimit;
            if (wasDrive) {
                juggleLimit += 3;
            }
            if (hitbox.type != domain && pOtherGuy->getAirborne() && pOtherGuy->juggleCounter > juggleLimit) {
                continue;
            }

            // juggle was the last thing to check, hit is valid
            pendingHitList.push_back({
                this, pOtherGuy, hitbox, hurtBox, pHitEntry,
                hitEntryFlag, hitbox.pHitData ? hitbox.pHitData->id : 0, blocked, parried, bombBurst
            });
        }
    }

    if (pendingUnlockHit) {
        // really we should save the lock target, etc.
        if (pOpponent) {
            HitEntry *pEntry = &pCharData->hitByID[pendingUnlockHit]->common[0];
            if (pendingUnlockHitDelayed || pEntry->floorTime == 0) {
                pOpponent->ApplyHitEffect(pEntry, this, true, false, false, false);
                // clamp
                pOpponent->setFocus(pOpponent->focus);
                pOpponent->setGauge(pOpponent->gauge);
                setFocus(focus);
                setGauge(gauge);
                otherGuyLog(pOpponent, pOpponent->logHits, "lock hit dt " + std::to_string(pendingUnlockHit) + " dmgType " + std::to_string(pEntry->dmgType) + " moveType " + std::to_string(pEntry->moveType));
                pOpponent->locked = false;
                pendingUnlockHit = 0;
                pendingUnlockHitDelayed = false;
            } else if (!pendingUnlockHitDelayed && pEntry->floorTime != 0) {
                // ?????
                // todo why does floortime matter on unlock hit? is it really what it is???
                pendingUnlockHitDelayed = true;
            }
        } else {
            pendingUnlockHit = 0;
        }
    }
}

AtemiData *Guy::findAtemi(int atemiID)
{
    auto it = pCharData->atemiByID.find(atemiID);
    if (it != pCharData->atemiByID.end()) {
        return it->second;
    }
    return nullptr;
}

void ResolveHits(Simulation *pSim, std::vector<PendingHit> &pendingHitList)
{
    std::unordered_set<Guy *> hitGuys;
    std::unordered_set<Guy *> clampGuys;

    for (auto &pendingHit : pendingHitList) {
        HitBox &hitBox = pendingHit.hitBox;
        HurtBox &hurtBox = pendingHit.hurtBox;
        HitEntry *pHitEntry = pendingHit.pHitEntry;
        Guy *pOtherGuy = pendingHit.pGuyGettingHit;
        Guy *pGuy = pendingHit.pGuyHitting;
        int hitEntryFlag = pendingHit.hitEntryFlag;

        bool trade = false;
        PendingHit tradeHit = {};
        for (auto &otherPendingHit : pendingHitList) {
            if (otherPendingHit.pGuyGettingHit == pGuy) {
                otherGuyLog(pOtherGuy, pOtherGuy->logHits, "trade!");
                trade = true;
                // todo see simultaneous hit question thing below
                tradeHit = otherPendingHit;
                break;
            }
        }

        // for now don't hit the same guy twice
        // todo figure out what happens when two simultaneous hits happen
        if (hitBox.type != direct_damage && hitGuys.find(pOtherGuy) != hitGuys.end()) {
            continue;
        } else {
            hitGuys.insert(pOtherGuy);
        }

        bool isGrab = hitBox.type == grab;

        // todo make this configurable pluggable rule
        if (isGrab && trade && tradeHit.hitBox.type == hit) {
            // strike loses vs. throw
            continue;
        }

        // todo sf5 style normal priority system could also go here

        bool hitFlagToParent = false;
        bool hitStopToParent = false;
        if (pGuy->isProjectile && pGuy->pCurrentAction && pGuy->pCurrentAction->pProjectileData && pGuy->pParent) {
            // either this means "to both" - current code or touch branch checks different
            // flags - mai charged fan has a touch branch on the proj but sets this flag
            // also used by double geyser where the player has a trigger condition
            // todo checking touch branch on player would disambiguate
            hitFlagToParent = pGuy->pCurrentAction->pProjectileData->hitFlagToParent;
            hitStopToParent = pGuy->pCurrentAction->pProjectileData->hitStopToParent;
        }

        if (pendingHit.blocked || pendingHit.parried) {
            pOtherGuy->blocking = true;
        }

        if (pendingHit.blocked) {
            pGuy->hasBeenBlockedThisFrame = true;
            if (hitFlagToParent) pGuy->pParent->hasBeenBlockedThisFrame = true;
            pGuy->hasBeenBlockedThisMove = true;
            if (hitFlagToParent) pGuy->pParent->hasBeenBlockedThisMove = true;
            otherGuyLog(pOtherGuy, pOtherGuy->logHits, "block!");
        }

        if (pendingHit.parried) {
            pGuy->hasBeenParriedThisFrame = true;
            if (hitFlagToParent) pGuy->pParent->hasBeenParriedThisFrame = true;
            pGuy->hasBeenParriedThisMove = true;
            if (hitFlagToParent) pGuy->pParent->hasBeenParriedThisMove = true;
            pGuy->hasBeenPerfectParriedThisFrame = false;
            if (hitFlagToParent) pGuy->pParent->hasBeenPerfectParriedThisFrame = false;
            pGuy->hasBeenPerfectParriedThisMove = false;
            if (hitFlagToParent) pGuy->pParent->hasBeenPerfectParriedThisMove = false;
            otherGuyLog(pOtherGuy, pOtherGuy->logHits, "parry!");

            pOtherGuy->successfulParry = true;
            pOtherGuy->subjectToParryRecovery = false;
        }

        bool hitArmor = false;
        bool hitAtemi = false;
        if (hurtBox.flags & armor && hurtBox.pAtemiData) {
            hitArmor = true;
            pGuy->hitArmorThisFrame = true;
            if (hitFlagToParent) pGuy->pParent->hitArmorThisFrame = true;
            pGuy->hitArmorThisMove = true;
            if (hitFlagToParent) pGuy->pParent->hitArmorThisMove = true;
            AtemiData *pAtemi = hurtBox.pAtemiData;
            auto atemiIDString = std::to_string(pAtemi->id);

            if (pOtherGuy->currentArmor != pAtemi) {
                pOtherGuy->armorHitsLeft = pAtemi->resistLimit + 1;
                pOtherGuy->currentArmor = pAtemi;
            }
            if (pOtherGuy->currentArmor == pAtemi) {
                pOtherGuy->armorHitsLeft--;
                if (pOtherGuy->armorHitsLeft <= 0) {
                    hitArmor = false;
                    if (pOtherGuy->armorHitsLeft == 0) {
                        otherGuyLog(pOtherGuy, pOtherGuy->logHits, "armor break!");
                    }
                } else {
                    pOtherGuy->armorThisFrame = true;
                    // hit stop will be replaced/added down
                    otherGuyLog(pOtherGuy, pOtherGuy->logHits, "armor hit! atemi id " + atemiIDString);
                }
            }
        }

        if (hurtBox.flags & atemi) {
            // like armor except onthing really happens beyond setting the flag
            hitArmor = true;
            hitAtemi = true;
            pGuy->hitAtemiThisFrame = true;
            if (hitFlagToParent) pGuy->pParent->hitAtemiThisFrame = true;
            pGuy->hitAtemiThisMove = true;
            if (hitFlagToParent) pGuy->pParent->hitAtemiThisMove = true;
            pOtherGuy->atemiThisFrame = true;

            // // is that hardcoded on atemi? not sure if the number is right
            // pGuy->addHitStop(13+1);
            // pOtherGuy->addHitStop(13+1);

            otherGuyLog(pOtherGuy, pOtherGuy->logHits, "atemi hit!");
        }

        if (hitArmor || hitAtemi) {
            // undo conter/PC if hit armor
            hitEntryFlag = hitEntryFlag & ~(punish_counter|counter);
            pHitEntry = &hitBox.pHitData->param[hitEntryFlag];
        }

        if (hitArmor && !hitAtemi) {
            //otherGuyLog(pOtherGuy, true, std::to_string(pHitEntry->dmgValue) + " " + std::to_string(hurtBox.pAtemiData->damageRatio));
            pOtherGuy->health -= pHitEntry->dmgValue * hurtBox.pAtemiData->damageRatio / 100;
            pOtherGuy->recoverableHealth += pHitEntry->dmgValue * hurtBox.pAtemiData->recoverRatio / 100;
            pOtherGuy->recoverableHealthCooldown = 120;
            // we don't call ApplyHitEffect so do this by hand?
            pOtherGuy->comboHits += pHitEntry->comboAdd;

            Guy *pResourceGuy = pGuy;
            if (pGuy->isProjectile) {
                pResourceGuy = pGuy->pParent;
            }
            // set resources directly, will be clamped later for trades

            // focus hitter
            if (!pResourceGuy->driveRushCancel && !pOtherGuy->driveScaling) {
                int scaledFocusGain = pHitEntry->focusGainOwn;
                if (pOtherGuy->perfectScaling) {
                    scaledFocusGain = scaledFocusGain * 50 / 100;
                }
                pResourceGuy->focus += scaledFocusGain;
            }

            // super target
            int scaledSuperGain = pHitEntry->superGainTarget;
            if (pOtherGuy->perfectScaling) {
                scaledSuperGain = scaledSuperGain * 80 / 100;
            }
            scaledSuperGain = scaledSuperGain * pOtherGuy->pCharData->styles[pOtherGuy->styleInstall].gaugeGainRatio / 100;
            scaledSuperGain = scaledSuperGain * hurtBox.pAtemiData->superRatio / 100;
            pOtherGuy->gauge += scaledSuperGain;

            // super hitter
            StyleData &style = pResourceGuy->pCharData->styles[pResourceGuy->styleInstall];
            scaledSuperGain = pHitEntry->superGainOwn * style.gaugeGainRatio / 100;
            if (pOtherGuy->perfectScaling) {
                scaledSuperGain = scaledSuperGain * 80 / 100;
            }
            scaledSuperGain = scaledSuperGain * pResourceGuy->superGainScaling / 100;
            pResourceGuy->gauge += scaledSuperGain;

            clampGuys.insert(pOtherGuy);
            clampGuys.insert(pResourceGuy);
        }

        if (hitArmor || hitAtemi) {
            // hitstop comes from block entry??
            pHitEntry = &hitBox.pHitData->param[block];
        }

        int destX = pHitEntry->moveDestX;
        int destY = pHitEntry->moveDestY;
        int hitHitStun = pHitEntry->hitStun;
        int dmgType = pHitEntry->dmgType;
        int moveType = pHitEntry->moveType;
        int attr0 = pHitEntry->attr0;
        int hitMark = pHitEntry->hitmark;

        // not hitstun for initial grab hit as we dont want to recover during the lock
        bool applyHit = !isGrab;
        if (hitBox.type == direct_damage) {
            // don't count in combos/etc, just apply DT
            applyHit = false;
        }

        // grab or hitgrab
        bool hitGrab = (pHitEntry->attr2 & (1<<1));
        if (trade) {
            // just apply the hit on trade
            hitGrab = false;
        }
        if (pendingHit.blocked || pendingHit.parried) {
            // and on block
            hitGrab = false;
        }
        if ((isGrab || hitGrab)) {
            pGuy->grabbedThisFrame = true;
            if (hitFlagToParent) pGuy->pParent->grabbedThisFrame = true;
        }

        int hitStopSelf = pHitEntry->hitStopOwner;
        int hitStopTarget = pHitEntry->hitStopTarget;
        // or it could be that normal throws take their value from somewhere else
        if (hitStopTarget == -1) {
            hitStopTarget = hitStopSelf;
        }

        if (pOtherGuy->parrying && pGuy->pSim->frameCounter - pOtherGuy->lastTriggerFrame < 2) {
            // perfect
            bool parryAsStrike = hitBox.type == hit;
            if (pGuy->isProjectile && pGuy->pCurrentAction->pProjectileData->flags & (1<<30)) {
                parryAsStrike = true;
            }
            bool didPerfect = true;
            if (parryAsStrike) {
                bool semiPerfect = false;
                if (hitBox.flags & overhead && (pOtherGuy->currentInput & (DOWN+BACK)) != BACK) {
                    semiPerfect = true;
                }
                if (hitBox.flags & low && (pOtherGuy->currentInput & (DOWN+BACK)) != DOWN+BACK) {
                    semiPerfect = true;
                }
                if (semiPerfect) { // demoted to regular parry
                    didPerfect = false;
                    // todo some gauge shit here?
                } else {
                    hitStopSelf = 0;
                    hitStopTarget = 1;
                    for (auto guy : pGuy->pSim->everyone) {
                        guy->parryFreeze = 61;
                    }
                    pOtherGuy->pOpponent->perfectScaling = true;
                }
            } else { // projectile pp
                hitStopSelf = 9 + 1;
                hitStopTarget = 9 + 1;
                pOtherGuy->parryHoldFreebieFrames = 13; // ?
            }
            if (didPerfect) {
                pGuy->hasBeenPerfectParriedThisFrame = true;
                if (hitFlagToParent) pGuy->pParent->hasBeenPerfectParriedThisFrame = true;
                pGuy->hasBeenPerfectParriedThisMove = true;
                if (hitFlagToParent) pGuy->pParent->hasBeenPerfectParriedThisMove = true;
            }
        }
        if (pOtherGuy->forceKnockDownState && !pOtherGuy->comboHits) {
            if (hitStopSelf) {
                hitStopSelf += 5;
            }
            if (hitStopTarget) {
                hitStopTarget += 5;
            }
        }
        if (!hitArmor) {
            if (applyHit && (!pendingHit.blocked && !pendingHit.parried)) {
                pOtherGuy->blocking = false;
            }
            if (applyHit && !pendingHit.parried) {
                pOtherGuy->parrying = false;
            }
            HitEntry clonedEntry;
            if (hitEntryFlag & special && !(hitEntryFlag & counter)) {
                // stupid, but apparently burnout block takes pushback value from normal block???
                int newHitFlag = hitEntryFlag & ~special;
                newHitFlag |= block;
                clonedEntry = *pHitEntry;
                clonedEntry.moveTime = hitBox.pHitData->param[newHitFlag].moveTime;
                pHitEntry = &clonedEntry;
            }
            pOtherGuy->ApplyHitEffect(pHitEntry, pGuy, applyHit, pGuy->grabbedThisFrame, pGuy->wasDrive, hitBox.type == domain, trade, false, &hurtBox);

            if (pendingHit.parried) {
                Fixed hitVel = pOtherGuy->hitVelX;
                Fixed halfHitVel = hitVel / Fixed(2);
                pGuy->hitVelX = -halfHitVel;
                pOtherGuy->hitVelX = halfHitVel; // todo no push? if proj
                if (halfHitVel < 0 && halfHitVel.data & 63) {
                    // this bias not like the others?
                    halfHitVel.data -= 1;
                }
                // if (hitVel.data & 1) {
                //     if (pGuy->pSim->frameCounter & 1) {
                //         pGuy->hitVelX.data += 1;
                //     } else {
                //         pOtherGuy->hitVelX.data += 1;
                //     }
                // }
                Fixed hitAccel = pOtherGuy->hitAccelX;
                Fixed halfHitAccel = hitAccel / Fixed(2);
                if (halfHitAccel < 0 && halfHitAccel.data & 63) {
                    // this bias not like the others?
                    halfHitAccel.data -= 1;
                }
                pGuy->hitAccelX = -halfHitAccel;
                pOtherGuy->hitAccelX = halfHitAccel;

                // find the parry gains in common[0], weird
                pOtherGuy->focus += hitBox.pHitData->common[0].parryGain;

                pOtherGuy->focusRegenCooldown = 20;
                pOtherGuy->focusRegenCooldownFrozen = true;
            }

            if (pGuy->isProjectile) {
                clampGuys.insert(pGuy->pParent);
            } else {
                clampGuys.insert(pGuy);
            }
            clampGuys.insert(pOtherGuy);
        }

        bool tradeHitStop = trade;
        if (hitBox.type != hit || tradeHit.hitBox.type != hit) {
            tradeHitStop = false;
        }
        if (hurtBox.flags & armor || tradeHit.hurtBox.flags & armor) {
            // todo that won't work for armor break, we need to compute that stuff ahead of trade maybe
            tradeHitStop = false;
        }
        if (tradeHitStop) {
            // todo not for projectile trades? weird
            if (hitStopSelf<15) {
                hitStopSelf = 15;
            }
            if (hitStopTarget<15) {
                hitStopTarget = 15;
            }
        }
        if (hitArmor && !hitAtemi) {
            if (hurtBox.pAtemiData->ownerStop != -1) {
                hitStopTarget = hurtBox.pAtemiData->ownerStop;
            }
            if (hurtBox.pAtemiData->targetStop != -1) {
                hitStopSelf = hurtBox.pAtemiData->targetStop;
            }
            hitStopTarget += hurtBox.pAtemiData->ownerStopAdd;
            hitStopSelf += hurtBox.pAtemiData->targetStopAdd;
        }
        Box hitIntersection;
        hitIntersection.x = fixMax(hitBox.box.x, hurtBox.box.x);
        hitIntersection.y = fixMax(hitBox.box.y, hurtBox.box.y);
        hitIntersection.w = fixMin(hitBox.box.x + hitBox.box.w, hurtBox.box.x + hurtBox.box.w) - hitIntersection.x;
        hitIntersection.h = fixMin(hitBox.box.y + hitBox.box.h, hurtBox.box.y + hurtBox.box.h) - hitIntersection.y;

        float hitMarkerOffsetX = hitIntersection.x.f() + hitIntersection.w.f() - pOtherGuy->getPosX().f();
        if (pGuy->direction < Fixed(0)) {
            hitMarkerOffsetX = hitIntersection.x.f() - pOtherGuy->getPosX().f();
        }
        float hitMarkerOffsetY = (hitIntersection.y.f() + (hitIntersection.h.f() / 2.0f)) - pOtherGuy->getPosY().f();
        int hitMarkerType = 1;
        float hitMarkerRadius = 35.0f;
        if (pGuy->hasBeenBlockedThisFrame) {
            hitMarkerType = 2;
            hitMarkerRadius = 30.0f;
        } else if (pGuy->hasBeenParriedThisFrame) {
            hitMarkerType = 2;
            hitMarkerRadius = 60.0f;
        } else if ((hitEntryFlag & punish_counter) == punish_counter) {
            hitMarkerRadius = 45.0f;
        }
        if (pGuy->pSim != &defaultSim) {
            FrameEvent event;
            event.type = FrameEvent::Hit;
            event.hitEventData.targetID = pOtherGuy->getUniqueID();
            event.hitEventData.x = hitMarkerOffsetX;
            event.hitEventData.y = hitMarkerOffsetY;
            event.hitEventData.radius = hitMarkerRadius;
            event.hitEventData.hitType = pGuy->hasBeenBlockedThisFrame ? 2 : 1;
            event.hitEventData.seed = pGuy->pSim->frameCounter + int(hitMarkerOffsetX + hitMarkerOffsetY);
            event.hitEventData.dirX = pGuy->direction.f();
            event.hitEventData.dirY = 0.0f;
            pGuy->pSim->getCurrentFrameEvents().push_back(event);
        } else {
            int hitSeed = pGuy->pSim->frameCounter + int(hitMarkerOffsetX + hitMarkerOffsetY);
            addHitMarker({hitMarkerOffsetX,hitMarkerOffsetY,hitMarkerRadius,pOtherGuy,hitMarkerType, 0, 10, hitSeed, pGuy->direction.f(), 0.0f});
        }

        int hitID = hitBox.hitID;

        if (pGuy->isProjectile) {
            pGuy->projHitCount--;
            // mmmm
            //hitStopSelf += pGuy->pCurrentAction->pProjectileData->extraHitStop;
            if (hitBox.type == projectile && !pGuy->obeyHitID) {
                hitID = -1;
            }
            pGuy->hitSpanFrames = pGuy->pCurrentAction->pProjectileData->hitSpan;
        }

        if (hitID != -1) {
            pGuy->canHitID |= 1ULL << hitID;
        }

        if (!pOtherGuy->blocking && !hitArmor) {
            if ((hitEntryFlag & punish_counter) == punish_counter) {
                pGuy->punishCounterThisFrame = true;
                if (hitFlagToParent) pGuy->pParent->punishCounterThisFrame = true;
                pGuy->hitPunishCounterThisMove = true;
                if (hitFlagToParent) pGuy->pParent->hitPunishCounterThisMove = true;
            }
            if (hitEntryFlag & counter) {
                pGuy->hitCounterThisMove = true;
                if (hitFlagToParent) pGuy->pParent->hitCounterThisMove = true;
            }
            pGuy->hitThisFrame = true;
            if (hitFlagToParent) pGuy->pParent->hitThisFrame = true;
            pGuy->hitThisMove = true;
            if (hitFlagToParent) pGuy->pParent->hitThisMove = true;

            int dmgKind = pHitEntry->dmgKind;

            if (dmgKind == 11) {
                pGuy->DoInstantAction(592); // IMM_VEGA_BOMB
            }

            if (pendingHit.bombBurst) {
                pOtherGuy->debuffTimer = 0;
            }
        }
        otherGuyLog(pOtherGuy, pOtherGuy->logHits, "hit type " + std::to_string(hitBox.type) + " hitID " + std::to_string(hitBox.hitID) +
            " dt " + std::to_string(pendingHit.hitDataID) + "/" + std::to_string(hitEntryFlag) + " destX " + std::to_string(destX) + " destY " + std::to_string(destY) +
            " hitStun " + std::to_string(hitHitStun) + " dmgType " + std::to_string(dmgType) +
            " moveType " + std::to_string(moveType) );
        otherGuyLog(pOtherGuy, pOtherGuy->logHits, "attr0 " + std::to_string(attr0) + "hitmark " + std::to_string(hitMark));

        // let's try to be doing the grab before time stops
        if (pGuy->grabbedThisFrame && pGuy->nextAction == -1) {
            pGuy->DoBranchKey();
            if (pGuy->nextAction != -1) {
                // only transition
                pGuy->AdvanceFrame(false);
                // only run keys, force position reset as this is a new hit
                pOtherGuy->locked = false;
                pGuy->RunFrame(false);
            } else {
                otherGuyLog(pGuy, true, "instagrab branch not found!");
            }
        }

        // add warudo after potential instagrab branch (technical term)
        if (hitStopSelf > 0) {
            if (hitStopToParent) {
                pGuy->pParent->addHitStop(hitStopSelf+1);
                // todo is it instead of to self? :/
            }
            pGuy->addHitStop(hitStopSelf+1);
        }
        if (hitStopTarget > 0) {
            pOtherGuy->addHitStop(hitStopTarget+1);
#ifdef __EMSCRIPTEN__
            // only vibrate here in realtime mode
            if (pGuy->pSim == &defaultSim) {
                emscripten_vibrate(hitStopTarget*2);            
            }
#endif
        }
    }

    for (auto &guy : clampGuys) {
        if (guy->deferredFocusCost) {
            if (guy->deferredFocusCost < 0) {
                guy->deferredFocusCost *= -1;
            }
            guy->setFocus(guy->focus - guy->deferredFocusCost);
            //log(logResources, "focus " + std::to_string(deferredFocusCost) + ", total " + std::to_string(focus));
            guy->deferredFocusCost = 0;
            if (!guy->parrying || !guy->successfulParry) {
                if (!guy->setFocusRegenCooldown(guy->parrying?240:120)) {
                    guy->focusRegenCooldown--;
                }
                guy->focusRegenCooldownFrozen = true;
            }
            //log(logResources, "regen cooldown " + std::to_string(focusRegenCooldown) + " (deferred spend, frozen)");
        }
        // clamp once everything is done, in case of trade
        guy->setFocus(guy->focus);
        guy->setGauge(guy->gauge);
    }

    // do unique interactions now
    std::unordered_set<Guy*> pitchers;
    std::unordered_set<Guy*> catchers;

    for (Guy *guy : pSim->everyone) {
        std::vector<UniqueBox> uniqueBoxes;
        guy->getUniqueBoxes(&uniqueBoxes);
        for (UniqueBox &box : uniqueBoxes) {
            if (box.uniquePitcher) {
                pitchers.insert(guy);
            } else {
                catchers.insert(guy);
            }
        }
    }

    for (Guy *pitcher : pitchers) {
        for (Guy *catcher : catchers) {
            std::vector<UniqueBox> uniqueBoxesPitcher;
            pitcher->getUniqueBoxes(&uniqueBoxesPitcher);
            std::vector<UniqueBox> uniqueBoxesCatcher;
            catcher->getUniqueBoxes(&uniqueBoxesCatcher);

            for (UniqueBox &pitcherBox : uniqueBoxesPitcher) {
                if (!pitcherBox.uniquePitcher) {
                    continue;
                }
                for (UniqueBox &catcherBox : uniqueBoxesCatcher) {
                    if (catcherBox.uniquePitcher) {
                        continue;
                    }
                    if (!(pitcherBox.checkMask & catcherBox.checkMask)) {
                        continue;
                    }
                    if (doBoxesHit(pitcherBox.box, catcherBox.box)) {
                        if (((catcher->uniqueOpsAppliedMask & pitcherBox.checkMask) != pitcherBox.checkMask) ||
                            ((pitcher->uniqueOpsAppliedMask & catcherBox.checkMask) != catcherBox.checkMask)) {
                            catcher->ApplyUniqueBoxOps(pitcherBox, pitcher);
                            pitcher->ApplyUniqueBoxOps(catcherBox, catcher);
                        }

                        pitcher->uniqueOpsAppliedMask |= catcherBox.checkMask;
                        catcher->uniqueOpsAppliedMask |= pitcherBox.checkMask;
                    }
                }
            }
        }
    }
}

void Guy::ApplyUniqueBoxOps(UniqueBox &box, Guy *src)
{
    Guy *pGuyUniqueParamOp = box.holderIsTarget ? this : src;
    for (UniqueBoxOp &op : *box.pOps) {
        if (op.op == 1) {
            if (op.opParam0) {
                if (op.opParam1 == 0) {
                    pGuyUniqueParamOp->uniqueParam[op.opParam0-1] = op.opParam2;
                } else if (op.opParam1 == 1) { 
                    pGuyUniqueParamOp->uniqueParam[op.opParam0-1] += op.opParam2;
                }
            }
        } else if (op.op == 2) {
            pGuyUniqueParamOp->velocityX.data += op.opParam0;
            pGuyUniqueParamOp->velocityY.data += op.opParam1;
            pGuyUniqueParamOp->accelX.data += op.opParam2;
            pGuyUniqueParamOp->accelY.data += op.opParam3;
        } else if (op.op == 3) {
            // inertia?
            //pGuyUniqueParamOp->cancelInheritVelX.data = op.opParam0;
        } else if (op.op == 8) {
            pGuyUniqueParamOp->DoInstantAction(op.opParam4);
        } else if (op.op != 0 && op.op != 9) { // nop and.. hitmarker?
            log(logUnknowns, "unknown unique op " + std::to_string(op.op));
        }
    }
}

void Guy::ApplyHitEffect(HitEntry *pHitEffect, Guy* attacker, bool applyHit, bool isGrab, bool isDrive, bool isDomain, bool isTrade, bool isClash, HurtBox *pHurtBox)
{
    int comboAdd = pHitEffect->comboAdd;
    int juggleFirst = pHitEffect->juggleFirst;
    int juggleAdd = pHitEffect->juggleAdd;
    int hitEntryHitStun = pHitEffect->hitStun;
    int destX = pHitEffect->moveDestX;
    int destY = pHitEffect->moveDestY;
    int destTime = pHitEffect->moveTime;
    int dmgValue = pHitEffect->dmgValue;
    int dmgType = pHitEffect->dmgType;
    int moveType = pHitEffect->moveType;
    int floorTime = pHitEffect->floorTime;
    int downTime = pHitEffect->downTime;
    bool jimenBound = pHitEffect->jimenBound;
    bool kabeBound = pHitEffect->kabeBound;
    bool kabeTataki = pHitEffect->kabeTataki;
    int attackStrength = pHitEffect->dmgPower;
    int attr0 = pHitEffect->attr0;
    int attr1 = pHitEffect->attr1;
    int attr2 = pHitEffect->attr2;
    int attr3 = pHitEffect->attr3;
    int ext0 = pHitEffect->ext0;

    int dmgKind = pHitEffect->dmgKind;
    // int dmgPart = (*pHitEffect)["DmgPart"];
    // int dmgVari = (*pHitEffect)["DmgVari"];
    // int curveTargetID = hitEntry["CurveTgtID"];

    if (isDrive) {
        juggleAdd = 0;
        juggleFirst = 0;
    }

    if (!hitStun) {
        // in case the previous 'combo start' was an armor hit or something
        comboHits = 0;
    }

    noCounterPush = attr0 & (1<<0);
    bool piyoBound = attr1 & (1<<2);
    noBackRecovery = attr1 & (1<<6);
    bool useParentDirection = attr1 & (1<<10);
    bool usePositionAsDirection = attr1 & (1<<11);
    recoverForward = attr3 & (1<<0);
    recoverReverse = attr3 & (1<<1);
    frontDamage = attr3 & (1<<2);
    backDamage = attr3 & (1<<3);

    pAttacker = attacker;

    if (useParentDirection && attacker->pParent) {
        // for the purpose of checking direction below
        attacker = attacker->pParent;
    }

    bool doSwitchDirection = applyHit && !isGrab;
    if (dmgType == 21 || dmgType == 22) {
        doSwitchDirection = false; // grab type hits that happen even as unlock
        usePositionAsDirection = true;
    }

    Fixed attackerDirection = attacker->direction;
    if (usePositionAsDirection) {
        attackerDirection = attacker->direction;
        if (pAttacker->needsTurnaround(Fixed(0))) {
            attackerDirection *= Fixed(-1);
        }
    }
    Fixed hitVelDirection = attackerDirection * Fixed(-1);
    // in a real crossup, hitvel will go opposite the direction of the hit player
    if (!attacker->isProjectile && pAttacker->needsTurnaround(Fixed(10))) {
        attackerDirection *= Fixed(-1);
    }

    if (doSwitchDirection && !recoverReverse && !isDomain && direction == attackerDirection) {
        // like in a sideswitch combo
        switchDirection();
    }

    // like guile 4HK has destY but stays grounded if hits grounded
    if (!(dmgType & 8) && !(dmgType == 21) && !(dmgType == 32) && !airborne) {
        destY = 0;
    }

    if (blocking || isClash) {
        // todo should we remove other special status here?
        destY = 0;
    }

    // if going airborne, start counting juggle
    bool startingJuggle = (!airborne && destY != 0) || (airborne && comboHits == 0);
    if (!isDomain && applyHit && startingJuggle) {
        juggleCounter = juggleFirst;
    }
    if (!isDomain && applyHit && airborne) {
        juggleCounter += juggleAdd;
    }

    if (isDrive) {
        hitEntryHitStun += 4;
    }

    if (applyHit) {
        resetHitStunOnLand = false;
        resetHitStunOnTransition = false;
        knockDown = false;
        knockDownFrames = 3;
    }
    if (downTime <= 1) {
        downTime = 3;
    }
    if (applyHit && !isGrab) {
        if (destY != 0 && !isDomain)
        {
            // this is set on honda airborne hands
            // juggle state, just add a bunch of hitstun
            hitEntryHitStun += 500000;
            resetHitStunOnLand = true;

            knockDownFrames = downTime;

            if (forceKnockDownState || dmgType == 11 || dmgType == 15 || isDrive) {
                knockDown = true;
                // slide velocity while on the ground?
                groundBounceVelX = Fixed(-pHitEffect->boundDest) / Fixed(downTime);
            } else {
                knockDown = false;
                groundBounceVelX = Fixed(0);
            }
        }

        if (moveType == 11 || moveType == 10 || moveType == 18) { // crumples, hkds
            hitEntryHitStun += 500000;
            resetHitStunOnTransition = true;
            knockDown = true;
            knockDownFrames = downTime;
        }
    }

    bool appliedAction = false;

    if (applyHit && !isGrab) {
        if (dmgType == 13) {
            knockDown = true;
            if (!isDomain && applyHit && piyoBound) { // ??
                knockDownFrames = 0;
                hitEntryHitStun = downTime;
                resetHitStunOnLand = false;
                resetHitStunOnTransition = false;
                nextAction = 330;
                knockDownFrameCounter = pSim->frameCounter;
                appliedAction = true;
                isDown = true;
            } else {
                knockDownFrames = downTime;
            }

            //resetHitStunOnTransition = true;
        }

        if (jimenBound && !pHitEffect->floorDestY) {
            jimenBound = false;
            // it stays on the ground one more frame in lieu of a ground bounce?
            knockDownFrames += 1;
        }

        if (dmgType == 30 && blocking) {
            kabeTataki = false;
        }

        if (dmgType == 31) {
            kabeTataki = true;
        }

        if (dmgType == 27) {
            kabeTataki = true;
            stunSplat = true;
        }

        if (attr0 & (1<<9) && comboHits) {
            kabeBound = false;
            kabeTataki = false;
        }
    }

    int attackerInstantScale = pAttacker->triggerInstantScale + pAttacker->actionInstantScale;

    if (applyHit && dmgValue) {
        if (comboHits == 0 && !stunned) {
            currentScaling = 100;
            pendingScaling = 0;
        }

        bool applyScaling = !pAttacker->appliedScaling;

        if (pendingScaling && applyScaling) {
            currentScaling -= pendingScaling;
            log(logHits, "applied " + std::to_string(pendingScaling) + " pending scaling current" + std::to_string(currentScaling));
            pendingScaling = 0;
        }

        if (applyScaling) {
            if (comboHits == 0 && !stunned) {
                pendingScaling = pAttacker->pCurrentAction ? pAttacker->pCurrentAction->startScale : 0;
                pendingScaling += attackerInstantScale;
            } else {
                pendingScaling = pAttacker->pCurrentAction ? pAttacker->pCurrentAction->comboScale : 10;
                pendingScaling += attackerInstantScale;
                if (currentScaling == 100) {
                    pendingScaling += 10;
                }
            }
            log(logHits, "queued " + std::to_string(pendingScaling) + " pending scaling")
        }

        Guy *pResourceGuy = pAttacker->pParent ? pAttacker->pParent : pAttacker;
        if (pResourceGuy->triggerSuperGainScaling < superGainScaling) {
            superGainScaling = pResourceGuy->triggerSuperGainScaling;
        }
        for (Guy *pResourceMinion : pResourceGuy->getMinions()) {
            if (pResourceMinion->triggerSuperGainScaling < superGainScaling) {
                superGainScaling = pResourceMinion->triggerSuperGainScaling;
            }
        }

        if (pAttacker->scalingTriggerID != lastScalingTriggerID && !applyScaling) {
            // the combo has moved past that attack, so the instant scale is baked in
            attackerInstantScale = 0;
        }
        pAttacker->appliedScaling = true;
        if (applyScaling) {
            lastScalingTriggerID = pAttacker->scalingTriggerID;
        }
        if (pAttacker->isProjectile && pAttacker->pParent) {
            if (pAttacker->pParent->scalingTriggerID == pAttacker->scalingTriggerID) {
                pAttacker->pParent->appliedScaling = true;
            }
            for (Guy *pAttackerSibling : pAttacker->pParent->getMinions()) {
                if (pAttackerSibling->scalingTriggerID == pAttacker->scalingTriggerID) {
                    pAttackerSibling->appliedScaling = true;
                }
            }
        }
        for (Guy *pAttackerMinion : pAttacker->getMinions()) {
            if (pAttackerMinion->scalingTriggerID == pAttacker->scalingTriggerID) {
                pAttackerMinion->appliedScaling = true;
            }
        }
    }

    int effectiveScaling = currentScaling - attackerInstantScale;

    if (effectiveScaling < 10) {
        effectiveScaling = 10;
    }

    StyleData &attackerStyleData = pAttacker->pCharData->styles[pAttacker->styleInstall];
    effectiveScaling = effectiveScaling * attackerStyleData.attackScale / 100;

    if (driveScaling) {
        effectiveScaling = effectiveScaling * 85 / 100;
    }
    if (perfectScaling) {
        effectiveScaling = effectiveScaling * 50 / 100;
    }

    if (pAttacker->superAction) {
        if (pAttacker->superLevel == 1 && effectiveScaling < 30) {
            effectiveScaling = 30;
        }
        if (pAttacker->superLevel == 2 && effectiveScaling < 40) {
            effectiveScaling = 40;
        }
        if (pAttacker->superLevel == 3 && effectiveScaling < 50) {
            effectiveScaling = 50;
        }
    }

    if (!isClash) {
        int moveDamage = dmgValue * effectiveScaling / 100;
        int prevHealth = health;
        health -= moveDamage;
        if (alreadyDeadButDoesNotKnow && !(attr1 & (1<<4))) {
            health = 0;
        }
        recoverableHealth = 0;
        log(logHits, "effective scaling " + std::to_string(effectiveScaling) + " " + std::to_string(moveDamage) + " attacker scalingTriggerID " + std::to_string(pAttacker->scalingTriggerID));

        if (health < 0) {
            if (prevHealth && attr1 & (1<<4)) {
                health = 1;
                alreadyDeadButDoesNotKnow = true;
            } else {
                health = 0;
            }
        }

        // todo if health 0 mark finish here?

        if (moveDamage > 0) {
            // look for minions to delete on damage
            for (auto &minion: getMinions()) {
                if (minion->isProjectile && minion->pCurrentAction->pProjectileData->flags & (1<<6)) {
                    minion->die = true;
                }
            }

            comboDamage += moveDamage;
            lastDamageScale = effectiveScaling;

            DoInstantAction(582); // IMM_DAMAGE_INIT (_init? is there another?)
        }

        int scaledFocusGain = pHitEffect->focusGainTarget;
        if (!parrying && (!burnout || scaledFocusGain > 0)) {
            if (perfectScaling && scaledFocusGain < 0) {
                scaledFocusGain = scaledFocusGain * 50 / 100;
            }
            focus += scaledFocusGain;
            log(logResources, "focus " + std::to_string(scaledFocusGain) + " (hit), total " + std::to_string(focus));
            if (scaledFocusGain < 0 && !superFreeze) {
                // todo apparently start of hitstun except if super where it's after??
                if (!setFocusRegenCooldown(91) && !focusRegenCooldownFrozen) {
                    focusRegenCooldown++; // it freezes for one frame but doesn't apply
                }
                log(logResources, "regen cooldown " + std::to_string(focusRegenCooldown) + " (hit)");
            }
        }
        int scaledSuperGain = pHitEffect->superGainTarget;
        if (perfectScaling) {
            scaledSuperGain = scaledSuperGain * 80 / 100;
        }
        scaledSuperGain = scaledSuperGain * pCharData->styles[styleInstall].gaugeGainRatio / 100;
        gauge += scaledSuperGain;

        Guy *pResourceGuy = pAttacker;
        if (pAttacker->isProjectile) {
            pResourceGuy = pAttacker->pParent;
        }
        // set resources directly, will be clamped by caller, for trades
        if (!pResourceGuy->driveRushCancel && !driveScaling) {
            scaledFocusGain = pHitEffect->focusGainOwn;
            if (perfectScaling) {
                scaledFocusGain = scaledFocusGain * 50 / 100;
            }
            pResourceGuy->focus += scaledFocusGain;
        }
        StyleData &style = pResourceGuy->pCharData->styles[pResourceGuy->styleInstall];
        scaledSuperGain = pHitEffect->superGainOwn * style.gaugeGainRatio / 100;
        if (perfectScaling) {
            scaledSuperGain = scaledSuperGain * 80 / 100;
        }
        scaledSuperGain = scaledSuperGain * superGainScaling / 100;
        pResourceGuy->gauge += scaledSuperGain;
    }

    if (applyHit) {
        if (!blocking) {
            beenHitThisFrame = true;
            comboHits += comboAdd;
        }
        wasHit = true;
    }

    bool appliedHitStun = false;

    if (!isDomain && (applyHit || isClash)) {
        // not sure if the right check to reset hitstun to 0 only in some cases
        // maybe time to start adding lots of hitstun as a juggle state
        if (hitEntryHitStun > 0 || dmgType == 21 || dmgType == 22) {
            hitStun = hitEntryHitStun;
            appliedHitStun = true;
        }
    }

    //int origPoseStatus = getPoseStatus() - 1;

    if (isGrab) {
        grabAdjust = !(attr2 & (1<<5));
        if (moveType == 0) {
            destX = 0;
            destY = 0;
        }
    }

    if (applyHit || isClash) {
        if (destY > 0 && !isDomain) {
            airborne = true;
            // todo this might be too late if they had knockdown state on.. we can delay extra landing transition if so
            landed = false;
        }

        // special ground status, ground bounce OR tumble OR slide
        groundBounce = false;
        tumble = false;
        slide = false;

        // special wall status, splat OR bounce (NOT exclusive from special gronud satatus)
        wallSplat = false;
        wallBounce = false;

        wallStopped = false;
        wallStopFrames = false;

        if (!isDomain && !isGrab) {
            if (jimenBound && floorTime) {
                int floorDestX = pHitEffect->floorDestX;
                int floorDestY = pHitEffect->floorDestY;

                if (backDamage) {
                    floorDestX *= -1;
                }

                groundBounce = true;
                groundBounceVelX = Fixed(-floorDestX) / Fixed(floorTime);
                groundBounceAccelX = Fixed(floorDestX) / Fixed(floorTime * 32);
                log(true, "floorDestX" + std::to_string(floorDestX) + " floorTime" + std::to_string(floorTime));
                if (direction == Fixed(-1) && groundBounceAccelX.data & 63) {
                    // ??
                    groundBounceAccelX.data += 1;
                }
                groundBounceVelX -= groundBounceAccelX;

                groundBounceVelY = Fixed(floorDestY * 4) / Fixed(floorTime);
                groundBounceAccelY = fixDivWithBias(Fixed(floorDestY * -8) , Fixed(floorTime * floorTime));
                groundBounceVelY -= groundBounceAccelY;
            } else {
                if (moveType == 69) {
                    knockDown = true;
                    tumble = true;
                    // todo move this into some kind of 'next vel' and debloat the guy struct
                    groundBounceVelX = Fixed(-pHitEffect->boundDest) / Fixed(downTime - 27); // TWENTY-SEVEN
                    knockDownFrames = downTime - 27;
                }
                if (moveType == 9 || moveType == 19) {

                    groundBounceVelX = Fixed(-pHitEffect->boundDest * 2) / Fixed(28);
                    groundBounceAccelX = fixDivWithBias(Fixed(pHitEffect->boundDest * 2), Fixed(28 * 28));

                    if (moveType == 9) {
                        // grounded transition
                        nextAction = 332;
                        appliedAction = true;
                        knockDownFrames = downTime + destTime;
                    } else {
                        knockDown = true;
                        slide = true;
                        knockDownFrames = 31;
                    }
                }
            }

            int wallTime = pHitEffect->wallTime;
            if (kabeTataki) {
                // this can happen even if you block! blocked DI
                wallSplat = true;
                wallStopFrames = pHitEffect->wallStop + 2;
                if (stunSplat) {
                    wallStopFrames = 30;
                }
            } else if (kabeBound && wallTime) {
                int wallDestX = pHitEffect->wallDestX;
                int wallDestY = pHitEffect->wallDestY;
                wallStopFrames = pHitEffect->wallStop + 2;

                wallBounce = true;
                wallBounceVelX = fixDivWithBias(Fixed(-wallDestX) , Fixed(wallTime));
                wallBounceVelY = Fixed(wallDestY * 4) / Fixed(wallTime);
                wallBounceAccelY = fixDivWithBias(Fixed(wallDestY * -8) , Fixed(wallTime * wallTime));
                wallBounceVelY -= wallBounceAccelY;
            }
        }

        //if (destTime != 0)
        {
            if ((dmgType == 21 || dmgType == 22) && pAttacker->pendingUnlockHit) {
                // thrown? constant velocity one frame from now, ignore place/hitvel, hard knockdown after hitstun is done
                if (!locked) {
                    log(true, "nage but not locked?");
                }
                if (dmgType == 21 && destTime != 0) {
                    // those aren't actually used but they're set in game so it quiets some warnings
                    // there's a race condition with getting them right, because the hit is applied
                    // from RunFrame -> lockkey, and the velocity will be off by one frame depending
                    // on who's RunFrame runs first
                    hitVelX = Fixed(hitVelDirection.i() * destX * -2) / Fixed(destTime);
                    hitAccelX = fixDivWithBias(Fixed(hitVelDirection.i() * destX * 2) , Fixed(destTime * destTime));

                    velocityX = Fixed(destX) / Fixed(destTime);
                }

                nageKnockdown = true;
                knockDown = true;
                knockDownFrames = downTime;
                // the bounddest for this is always positive
                if (!groundBounce) {
                    groundBounceVelX = Fixed(direction.i() * -1 * hitVelDirection.i() * pHitEffect->boundDest) / Fixed(downTime);
                }

                // commit current place offset
                posX = posX + (posOffsetX * direction);
                posOffsetX = Fixed(0);
                bgOffsetX = Fixed(0);

                if (hitStun > 1) {
                    hitStun--;
                }

                if (hitStun == 0) {
                    hitStun = 1; // so AdvanceFrame knocks us down
                }

                if (dmgType == 22 && hitStun == 1) {
                    AdvanceFrame(false);
                    log(logHits, "advanced after nage unlock?");
                    appliedAction = true;
                }

                if (destY != 0) {
                    if (destY > 0) {
                        velocityY = Fixed(destY * 4) / Fixed(destTime);
                        accelY = fixDivWithBias(Fixed(destY * -8) , Fixed(destTime * destTime));
                        velocityY -= accelY;
                    } else {
                        velocityY = Fixed(destY) / Fixed(destTime);
                        accelY = Fixed(0);
                    }
                }
            } else if (!isDomain && destTime != 0 && moveType != 9) {
                // generic pushback/airborne knock
                if (!airborne) {
                    // if (parrying && !pAttacker->isProjectile) {
                    //     destX -= 1; // ??????? in the higuchi replay it's for sure like that
                    // }
                    hitVelX = Fixed(hitVelDirection.i() * destX * -2) / Fixed(destTime);
                    if (destX < 0 && hitVelX.data & 63) {
                        hitVelX.data -= 1;
                    }
                    hitAccelX = Fixed(hitVelDirection.i() * destX * 2) / Fixed(destTime * destTime);
                    if (hitAccelX < 0 && hitAccelX.data & 127 && destX > 0) {
                        hitAccelX.data -= 1;
                    } else if (hitAccelX > 0 && hitAccelX.data & 127 && destX > 0) {
                        hitAccelX.data += 1;
                    }
                    velocityX = Fixed(0);
                    accelX = Fixed(0);
                } else {
                    hitVelX = Fixed(0);
                    hitAccelX = Fixed(0);
                    velocityX = Fixed(-destX) / Fixed(destTime);
                    // todo not just on unlock?
                    if (recoverReverse) {
                        velocityX *= Fixed(-1);
                    }
                    if (destX < 0 && velocityX.data & 63) {
                        // this bias not like the others?
                        velocityX.data += 1;
                    }
                    accelX = Fixed(0);
                }

                if (destY != 0) {
                    if (destY > 0) {
                        velocityY = Fixed(destY * 4) / Fixed(destTime);
                        accelY = fixDivWithBias(Fixed(destY * -8) , Fixed(destTime * destTime));
                        velocityY -= accelY;
                    } else {
                        velocityY = fixDivWithBias(Fixed(destY) , Fixed(destTime));
                        accelY = Fixed(0);
                    }
                }
            }
        }
    }

    if (!isDomain && applyHit && !appliedAction && !isGrab) {
        int prevNextAction = nextAction;
        if (blocking) {
            if (parrying) {
                nextAction = 483; // todo script depends on attack height?
                if (pAttacker->hasBeenPerfectParriedThisFrame) {
                    nextAction = 486;
                    hitStun = 1;
                    hitVelX = Fixed(0);
                    hitAccelX = Fixed(0);
                }
            } else {
                nextAction = 160 + attackStrength;
                if (currentInput & DOWN) {
                    nextAction = 174 + attackStrength;
                }
            }
        } else {
            //if (dmgType & 3) { // crumple? :/

            // haven't found a situation where dm_info_tbl helped as-is yet
            // bool foundAirborne = false;
            // std::string moveTypeString = to_string_leading_zeroes(moveType, 2);
            // if (staticPlayer["dm_info_tbl"].contains(moveTypeString)) {
            //     nlohmann::json &dmEntryJson = staticPlayer["dm_info_tbl"][moveTypeString];
            //     int dmType = dmEntryJson["type"];
            //     int dmPose = dmEntryJson["pose"];
            //     int dmActID = dmEntryJson["actID"];
            //     if (dmActID != -1 && dmType == dmgType && (dmPose == 255 || dmPose == origPoseStatus)) {
            //         if (dmPose == 2) foundAirborne = true;
            //         nextAction = dmActID;
            //     }

            if (moveType == 72) {
                if (attackStrength >= 2) {
                    nextAction = 268;
                } else {
                    nextAction = 267;
                }
            } else if (moveType == 71) {
                if (attackStrength >= 2) {
                    nextAction = 262;
                } else {
                    nextAction = 261;
                }
                if (crouching || getCrouching()) {
                    nextAction += 3;
                    crouching = true;
                }
            } else if (moveType == 18) {
                nextAction = 274;
            } else if (moveType == 15) {
                if (pHitEffect->throwRelease != 3) {
                    nextAction = noBackRecovery ? 339 : 338;
                }
            } else if (moveType == 13) { // set on wall bounce
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
            } else if (!locked) {
                if (pHurtBox && pHurtBox->flags & hurtBoxFlags::head) {
                    nextAction = 200 + attackStrength;
                } else if (pHurtBox && pHurtBox->flags & hurtBoxFlags::legs) {
                    nextAction = 208 + attackStrength;
                } else {
                    nextAction = 204 + attackStrength;
                }
                if ( crouching ) {
                    nextAction = 212 + attackStrength;
                }
            }

            if (getAirborne() || posY > Fixed(0) || destY > 0) {
                // those specific scripts apply even for airborne/launch
                if (moveType == 22) {
                    nextAction = 232;
                } else if (moveType == 17) {
                    nextAction = 282;
                } else if (moveType == 16) {
                    nextAction = 280;
                } else if (moveType == 15) {
                    if (destTime != 0) {
                        nextAction = backDamage ? 351 : 350;
                    }
                } else if (moveType == 12 && !knockDown) {
                    nextAction = 222;
                } else if (!locked) {
                    // generic angle-based launch
                    float angle = std::fmod(std::atan2(destY,destX)/std::numbers::pi*180.0,360);
                    //log(logHits, "launch angle " + std::to_string(angle));

                    if (angle >= 57.5) {
                        nextAction = 251; // 90
                    } else if (angle >= 22.5) {
                        nextAction = 252; // 45
                    } else if (angle >= -22.5) {
                        nextAction = 253; // 00
                    } else if (angle >= -57.5) {
                        nextAction = 254; // -45
                    } else {
                        nextAction = 255; // -90
                    }

                    if (ext0 != 0) {
                        nextAction = 250 + ext0; // weird override ??
                    }
                }
            }
        }

        if (prevNextAction != nextAction) {
            appliedAction = true;
        }
    }

    if (appliedAction) {
        NextAction(false, false);
        WorldPhysics(true);

        // if you get hit out of an od move?
        if (focusRegenCooldownFrozen) {
            focusRegenCooldownFrozen = false;
            if (focusRegenCooldown) {
                focusRegenCooldown++;
            }
            log(logResources, "regen cooldown unfrozen (hit out of freeze move)");
        }

        if (appliedHitStun && hitStun) {
            actionInitialFrame = pHitEffect->curveTargetID;
            if (isTrade) {
                actionInitialFrame = 1; // ????
            }
            while (actionInitialFrame > 3) {
                actionInitialFrame -= 3; // everyone knows you do that
            }
            if (actionInitialFrame < 1) {
                actionInitialFrame = 1;
            }
            if (pHitEffect->curveTargetID == 10) {
                // is it just a table? ....
                actionInitialFrame = 3;
            }
            log(logTransitions, "action initial frame " + std::to_string(actionInitialFrame));
            int frameCount = hitStun - 2;
            if (resetHitStunOnTransition) {
                frameCount = pHitEffect->hitStun - 2;
            }
            if (resetHitStunOnLand) {
                frameCount = 0;
            }
            if (frameCount < 0) { 
                frameCount = 0;
            }
            if (frameCount == 0) {
                actionSpeed = Fixed(1);
            } else {
                actionSpeed = fixDivWithBias(Fixed(pCurrentAction->recoveryEndFrame - 1 - actionInitialFrame), Fixed(frameCount));
            }
        }
    }

    if (jimenBound && !airborne) {
        AdvanceFrame(false);
        //posX += velocityX * direction;
    }

    // if (pAttacker->pendingUnlockHit && floorTime) {
    //     noAccelNextFrame = true;
    //     noVelNextFrame = true;
    // }

    // fire/elec/psychopower effect
    // the two that seem to matter for gameplay are 9 for poison and 11 for mine
    if (dmgKind == 11) {
        // todo change hit marker color
        debuffTimer = 300;
    }
}

bool Guy::CheckHitBoxCondition(int conditionFlag)
{
    if (conditionFlag == 0) {
        return true;
    }
    bool conditionMet = false;
    bool anyHitThisMove = hitThisMove || hitCounterThisMove || hitPunishCounterThisMove;
    if ( conditionFlag & (1<<0) && hitThisMove) {
        // just normal hit ?
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
    if ( conditionFlag & (1<<3) && (hitCounterThisMove || hitPunishCounterThisMove)) {
        conditionMet = true;
    }
    // 1<<4 parry?
    if ( conditionFlag & (1<<5) && hitAtemiThisMove) {
        conditionMet = true;
    }
    if ( conditionFlag & (1<<6) && hitArmorThisMove) {
        conditionMet = true;
    }
    // 1<<7 PP
    // todo hit parry

    return conditionMet;
}

void Guy::DoBranchKey(bool preHit)
{
    int maxBranchType = -1;

    // ignore deferral here. weird
    if (hitStop) {
        return;
    }

    if (!pCurrentAction) {
        return;
    }

    for (auto& branchKey : pCurrentAction->branchKeys)
    {
        if (branchKey.startFrame > currentFrame || branchKey.endFrame <= currentFrame) {
            continue;
        }

        bool doBranch = false;
        int branchType = branchKey.type;
        int64_t branchParam0 = branchKey.param00;
        int64_t branchParam1 = branchKey.param01;
        int64_t branchParam2 = branchKey.param02;
        int64_t branchParam3 = branchKey.param03;
        int64_t branchParam4 = branchKey.param04;
        int branchAction = branchKey.branchAction;
        int branchFrame = branchKey.branchFrame;
        bool keepFrame = branchKey.keepFrame;

        if (branchType <= maxBranchType) {
            // todo high priority field maybe overrides that?
            continue;
        }

        switch (branchType) {
            case 0: // always?
                doBranch = true;
                break;
            case 1:
                    // else.. we could have 'denied' the condition because prehit, so avoid those there
                    // it seems like a hack, maybe some else branches are meant to work prehit - if so,
                    // we'll need to keep more careful track of why we might have not taken a branch
                    if (deniedLastBranch && !preHit) {
                        doBranch = true;
                    }
                    break;
                case 2:
                    // any hit ever? surely that's a flag - can't find a counterexcample rn
                    if (!preHit && pOpponent && pOpponent->comboHits) {
                        doBranch = true;
                    }

                    break;
                case 4:
                    if (hasBeenBlockedThisMove) {
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
                    if (getPosY() == Fixed(0) && !preHit) { // it's technically landed, but like heavy buttslam checks one frame
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
                        if (recordFrameTriggers) {
                            dc.frameTriggers.insert(ActionRef(-branchParam1, 0));
                        }
                    }
                    if (hitStun) {
                        // guard scripts have branches to switch guard? but theyre not supposed to work?
                        doBranch = false;
                    }
                    if (!preHit) {
                        // we take new input before advanceframe, check command post-bump only
                        doBranch = false;
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
                    {
                        if (branchParam1 >= 0 && branchParam1 < uniqueParamCount) {
                            doBranch = conditionOperator(branchParam2, uniqueParam[branchParam1], branchParam3, "unique param");
                        }
                        // _only_ do the branch in prehit, opposite from usual
                        // needs checked before we've evaluated the eventkey that can bump unique
                        if (!preHit) {
                            doBranch = false;
                        }
                    }
                    break;
                case 30: // unique timer
                    if (uniqueTimer) {
                        if (conditionOperator(branchParam1, uniqueTimerCount, branchParam2, "unique timer")) {
                            doBranch = true;
                            // probably incorrect but would keep counting up forever for dhalsim
                            uniqueTimer = false;
                        }
                    } else {
                        log(logUnknowns, "unique timer branch not in timer?");
                    }
                    break;
                case 31: // todo loop count
                    break;
                case 37:
                    // hit catch - maybe will need a separate 'hitgrabbed this frame' later?
                case 36:
                    if (grabbedThisFrame) {
                        if (branchParam0 == 2 && hitPunishCounterThisMove) {
                            doBranch = true;
                        }
                        if (branchParam0 == 0 && !hitPunishCounterThisMove) {
                            doBranch = true;
                        }
                        if (branchParam0 != 0 && branchParam0 != 2) {
                            log(logUnknowns, "unknown catch branch kind");
                        }
                    }
                    break;
                case 40: // area target
                    if (pOpponent) {
                        int distX = branchParam2 & 0xFFFF;
                        int distY = (branchParam2 & 0xFFFF0000) >> 16;

                        int offsetX = branchParam1 & 0xFFFF;
                        if (offsetX > 0x8000) offsetX = -(0xFFFF - offsetX);

                        offsetX *= direction.i();

                        int offsetY = (branchParam1 & 0xFFFF0000) >> 16;
                        if (offsetY > 0x8000) offsetY = -(0xFFFF - offsetY);

                        // this is weird, but some ghosts are off by one if not for it
                        Fixed opponentPosX = pOpponent->getPosX() + (pOpponent->velocityX * pOpponent->direction);
                        Fixed opponentPosY = pOpponent->getPosY() + pOpponent->velocityY;

                        // and?
                        if (branchParam0 == 0 &&
                            (fixAbs(opponentPosX - Fixed(offsetX) - getPosX()) < Fixed(distX) &&
                            fixAbs(opponentPosY - Fixed(offsetY) - getPosY()) < Fixed(distY))) {
                            doBranch = true;
                        }

                        // or? no idea
                        // there's also branchParam3 that's 0 or 1 - they're both called AREA_ALL?
                        if (branchParam0 == 1 &&
                            (fixAbs(opponentPosX - Fixed(offsetX) - getPosX()) < Fixed(distX) ||
                            fixAbs(opponentPosY - Fixed(offsetY) - getPosY()) < Fixed(distY))) {
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
                        Guy *pGuyMinions = this;
                        if (isProjectile && pParent) {
                            pGuyMinions = pParent;
                        }
                        for ( auto minion : pGuyMinions->dc.minions ) {
                            if (branchParam0 == minion->limitShotCategory) {
                                count++;
                            }
                        }
                        doBranch = conditionOperator(branchParam1, count, branchParam2, "shot count");

                        if (preHit) {
                            doBranch = false;
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

                    if (preHit) {
                        doBranch = false;
                    }
                    break;
                case 63:
                    // what's the difference between this and 20?
                    // this is used for backthrow, the other thing for held buttons
                    if (branchParam2 == 1 && lastTriggerInput & branchParam1) {
                        doBranch = true;
                    }
                    // todo fix this maybe at some point..
                    // if (recordFrameTriggers) {
                    //     dc.frameTriggers.insert(ActionRef(-branchParam1, 0));
                    // }
                    break;
                default:
                    log(logUnknowns, "unsupported branch id " + std::to_string(branchType) + " type " + branchKey.typeName);
                    break;
        }

        if (doBranch) {

            if (keepFrame) {
                int frameBias = branchFrame;
                if (preHit) {
                    branchFrame = currentFrame + frameBias;
                } else {
                    branchFrame = currentFrame + 1 + frameBias;
                }
                if (branchFrame < 0) {
                    branchFrame = 0;
                }
            }

            if (branchAction == currentAction && keepFrame) {
                log(true, "noop branch - branch type inhibit?");
            } else {
                if (branchAction == currentAction) {
                    log(logBranches, "branching to frame " + std::to_string(branchFrame));
                    currentFrame = (branchFrame && !preHit) ? branchFrame - 1 : branchFrame;
                    currentFrameFrac = Fixed(currentFrame);
                    actionSpeed = Fixed(1); // todo right?
                    actionInitialFrame = -1;
                } else {
                    log(logBranches, "branching to action " + std::to_string(branchAction) + " type " + std::to_string(branchType));
                    nextAction = branchAction;
                    nextActionFrame = branchFrame;
                    if (opponentAction) {
                        nextActionOpponentAction = true;
                    }
                }
                deniedLastBranch = false;
                keepPlace = branchKey.keepPlace;
            }

            // FALL THROUGH - there might be another branch that works later for this frame
            // it should take precedence (if branchType higher?) - see mai's charged fan projectile
            maxBranchType = branchType;
        } else {
            if (branchType != 1) {
                deniedLastBranch = true;
            }
        }
    }
}

void Guy::DoFocusRegen()
{
    bool doRegen = (!warudo || tokiWaUgokidasu) && focusRegenCooldown == 0 && !focusRegenCooldownTicking;
    if ((!superAnimation || !pSim->match) && (driveRushCancel || (pOpponent && pOpponent->driveScaling))) {
        doRegen = false;
    }
    if (superFreeze || (pOpponent && pOpponent->warudo)) {
        doRegen = false;
    }

    if (doRegen) {
        int focusRegenAmount = burnout ? 50 : 40;

        if (getAirborne()) {
            focusRegenAmount = 20;
            //log(logResources, "focus regen airborne");
        }
        if (hitStun && !blocking && (currentAction != 39)) {
            bool burnoutState = burnout;
            if (opponentAction) {
                burnoutState = false;
            }
            focusRegenAmount = burnoutState ? 25 : 20;
            //log(logResources, "focus regen hitstun");
        }
        // magic walking forward for 10f rule - there might be a 'walk forward' tag we can use instead?
        if (currentAction == 10 || (currentAction == 9 && currentFrame >= 10)) {
            focusRegenAmount *= 2;
        }

        setFocus(focus + focusRegenAmount);
        log(logResources, "focus regen +" + std::to_string(focusRegenAmount) + " (clamp), total " + std::to_string(focus));
    }
}

bool Guy::AdvanceFrame(bool advancingTime, bool endHitStopFrame, bool endWarudoFrame)
{
    if (pOpponent && pOpponent->wallStopped && pOpponent->stunned) {
        return true;
    }

    if (parryFreeze) {
        parryFreeze--;
    }

    if (parryFreeze) {
        return true;
    }
    // if this is the frame that was stolen from beginning hitstop when it ends, don't
    // add pending hitstop yet, so we can play it out fully, in case hitstop got added
    // again just now - lots of DR cancels want one frame to play out when they add
    // more screen freeze at the exact end of hitstop
    if (advancingTime) {
        //bool appliedPendingHitstop = false;
        if (!endHitStopFrame && !endWarudoFrame) {
            if (pendingHitStop) {
                if (pendingHitStop > hitStop) {
                    hitStop = pendingHitStop;
                    //appliedPendingHitstop = true;
                }
                pendingHitStop = 0;
            }
        }
        if (!endHitStopFrame && !endWarudoFrame) {
            // time stops
            if (tokiYoTomare) {
                warudo = true;
                tokiYoTomare = false;
            }
        }

        bool tickRegenCooldown = true;
        if ((getWarudo() && !tokiWaUgokidasu) || superFreeze) {
            tickRegenCooldown = false;
        }
        if (pOpponent && (pOpponent->getWarudo() || pOpponent->superFreeze)) {
            tickRegenCooldown = false;
        }
        if (focusRegenCooldownFrozen) {
            tickRegenCooldown = false;
        }
        if (endHitStopFrame || endWarudoFrame) {
            tickRegenCooldown = false;
        }
        if (deferredFocusCost) {
            tickRegenCooldown = false;
        }
        if (tickRegenCooldown) {
            if (focusRegenCooldown > 0) {
                focusRegenCooldown--;
                focusRegenCooldownTicking = true;
                log(logResources, "regen cooldown tick down " + std::to_string(focusRegenCooldown));
                // if (getHitStop() > 1 && focusRegenCooldown == 0) {
                //     focusRegenCooldown = 1;
                //     log(logResources, "final regen cooldown tick down undone bc hitstop");
                // }
            } else {
                focusRegenCooldownTicking = false;
            }
        }
    }

    if (advancingTime && deferredFocusCost > 0) {
        log(logResources, "inverting deferred cost");
        deferredFocusCost = -deferredFocusCost;
    }

    if (getHitStop() || getWarudo()) {
        if (tokiWaUgokidasu) {
            // time has begun to move again
            warudo = false;
            // leave tokiWaUgokidasu set for RunFrame to know this happened
        }
        // if we just entered hitstop, don't go to next frame right now
        // we want to have a chance to get hitstop input before triggers
        // we'll re-run it in RunFrame
        return true;
    }

    bool doTriggers = true;
    if (jumpLandingDisabledFrames) {
        doTriggers = false;
    }

    int curNextAction = nextAction;
    didTrigger = false;
    if (doTriggers) {
        DoTriggers(1);
    }
    if (nextAction != curNextAction) {
        didTrigger = true;
    }

    curNextAction = nextAction;
    bool didBranch = false;
    if (!didTrigger && nextAction == -1) {
        DoBranchKey();
        if (nextAction != curNextAction) {
            didBranch = true;
        }
    }
    Fixed currentSpeed = actionSpeed;
    if (currentFrame == 0 && actionInitialFrame != -1) {
        currentSpeed = Fixed(actionInitialFrame);
        actionInitialFrame = -1;
    } else if (currentFrame >= (pCurrentAction->recoveryEndFrame - 1)) {
        currentSpeed = Fixed(1);
    }
    currentFrameFrac += currentSpeed;
    //log(true, "currentFrameFrac " + std::to_string(currentFrameFrac.f()) + " " + std::to_string(currentFrameFrac.data));
    currentFrame = currentFrameFrac.i();

    // evaluate branches after the frame bump, branch frames are meant to be elided afaict
    if (!didTrigger && !didBranch && nextAction == -1) {
        curNextAction = nextAction;
        DoBranchKey(true);
        if (nextAction != curNextAction) {
            didBranch = true;
        }
    }

    if (advancingTime) {
        if (isProjectile) {
            if (!didBranch && projHitCount == 0) {
                return false; // die
            }

            projLifeTime--;
            if (projLifeTime <= 0) {
                return false;
            }

            if (pParent && pOpponent && pCurrentAction && pCurrentAction->pProjectileData) {
                int rangeB = pCurrentAction->pProjectileData->rangeB;

                if (rangeB) {
                    Fixed bothPlayerPos = pParent->pOpponent->getLastPosX(true) + pParent->getLastPosX(true);
                    Fixed screenCenterX = bothPlayerPos / Fixed(2);
                    int fixedRemainder = bothPlayerPos.data - screenCenterX.data * 2;
                    screenCenterX.data += fixedRemainder;
                    screenCenterX = fixMax(-maxScreenCenterDisplacement, screenCenterX);
                    screenCenterX = fixMin(maxScreenCenterDisplacement, screenCenterX);

                    if (getPosX() > screenCenterX + maxProjectileDistance + Fixed(rangeB) ||
                        getPosX() < screenCenterX - maxProjectileDistance - Fixed(rangeB)) {
                        return false;
                    }
                }
            }
        }

        if (die) {
            return false;
        }
    }

    bool doHitStun = advancingTime || hitStun == 1;

    if (landed) {
        if (currentAction != 39 && currentAction != 40 && currentAction != 41 &&
            nextAction != 39 && nextAction != 40 && nextAction != 41 && !hitStun) {
            // non-empty jump landing
            log(logTriggers, "disabling actions due to non-empty landing");
            jumpLandingDisabledFrames = 3 + 1; // 3, but we decrement in RunFrame
        }

        jumped = false;

        DoInstantAction(587); // IMM_LANDING - after style thing below or before?
        if ( resetHitStunOnLand ) {
            hitStun = 1;
            resetHitStunOnLand = false;
            // process hitstun from 1>0 below to make the transition happen
            doHitStun = true;
        }
        if ( airActionCounter ) {
            airActionCounter = 0;
        }
        if (styleInstall >= 0 && (size_t)styleInstall < pCharData->styles.size()) {
            StyleData &style = pCharData->styles[styleInstall];
            if (style.terminateState == 0x3ff1fffffff) {
                // that apparently means landing... figure out deeper meaning later
                ExitStyle();
            }
        }
    }

    if (currentAction == 294 && (landed || !airborne)) {
        nextAction = 353;
        resetHitStunOnTransition = true;
        knockDown = true;
    }

    if (wallStopped) {
        velocityX = Fixed(0);
        velocityY = Fixed(0);
        accelX = Fixed(0);
        accelY = Fixed(0);
    }

    bool triggerWallStop = touchedWall || wasOnWall();

    if ((wallBounce || wallSplat) && triggerWallStop && !stunned) {
        if (wallSplat && stunSplat && burnout) {
            stunned = true;
        }
        wallStopped = wallBounce || airborne;
        if (wallSplat) {
            if (airborne) {
                // todo what's the burnout stun sequence there?
                nextAction = 285;
            } else {
                if (stunned) {
                    nextAction = 293;
                } else {
                    nextAction = 145;
                    resetHitStunOnTransition = true;
                    knockDown = true;
                    wallSplat = false;
                }
                // crumple
                blocking = false;
                hitStun = 50000;
                stunSplat = false;
            }
        } else {
            nextAction = 256;
        }
    }

    if (stunned && currentAction == 293 && currentFrame == 6) {
        wallStopped = true;
    }

    if (wallStopped) {
        wallStopFrames--;
        if (wallStopFrames <= 0) {
            if (wallSplat) {
                if (stunned) {
                    nextAction = 294;
                } else {
                    nextAction = 287;
                }
                wallSplat = false;
                if (airborne) {
                    accelY.data = -39497; // todo -0.6ish ? magic gravity constant?
                }
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

    bool doSlideBounce = false;

    if (bounced || !airborne) {
        if (groundBounce) {
            nextAction = 350; // combo/bounce state
            if (backDamage) {
                nextAction = 351;
            }

            velocityX = groundBounceVelX;
            accelX = groundBounceAccelX;
            velocityY = groundBounceVelY;
            accelY = groundBounceAccelY;

            hitStun += 500000;
            resetHitStunOnLand = true;

            if (recoverForward && needsTurnaround()) {
                // todo do we need to consume something here? or leave recoverForward until final landing
                switchDirection();
            }

            groundBounce = false;
            groundBounceVelX = 0.0f;
            groundBounceAccelX = 0.0f;
            groundBounceVelY = 0.0f;
            groundBounceAccelY = 0.0f;
        } else if (tumble) {
            nextAction = 289;
            tumble = false;
            hitStun = knockDownFrames + 1;
            knockDownFrames = 27 - 1;

            velocityX = groundBounceVelX;
            groundBounceVelX = 0.0f;
        } else if (slide) {
            doSlideBounce = true;
        }

        bounced = false;
    }

    if (slide && (doSlideBounce || !airborne)) {
        nextAction = 333;
        slide = false;
        hitStun = knockDownFrames;
        knockDownFrames = 9;

        velocityX = groundBounceVelX;
        accelX = groundBounceAccelX;

        groundBounceVelX = Fixed(0);
        groundBounceAccelX = Fixed(0);
    }

    if (currentAction == 33 && jumpDirection == 0 && currentInput & 1 ) {
        // one opportunity to adjust neutral jump direction during prejump
        if (currentInput & 4) {
            jumpDirection = -1;
        } else if (currentInput & 8) {
            jumpDirection = 1;
        }
    }

    bool disableMovement = false;
    if (pCurrentAction && currentFrame >= pCurrentAction->actionFrameDuration && nextAction == -1)
    {
        if (currentAction == 332) {
            nextAction = 333;
            knockDown = true;
            hitStun = knockDownFrames;
            knockDownFrames = 8;

            velocityX = groundBounceVelX;
            accelX = groundBounceAccelX;

            groundBounceVelX = Fixed(0);
            groundBounceAccelX = Fixed(0);

            noVelNextFrame = true;
            velocityX -= accelX;
        } else if (currentAction == 320 || currentAction == 321) {
            disableMovement = true; // movement disabled the frame you come out of tech?
            nextAction = 1;
        } else if (currentAction >= 251 && currentAction <= 253) {
            nextAction = currentAction - 21;
        } else if ( currentAction == 33 || currentAction == 34 || currentAction == 35 ) {
            // If done with pre-jump, transition to jump
            if ( currentAction == 33) {
                if (jumpDirection == 0) {
                    nextAction = 36;
                } else if (jumpDirection == 1) {
                    nextAction = 37;
                } else {
                    nextAction = 38;
                }
            } else {
                nextAction = currentAction + 3;
            }
            // magic that seems specific to normal jumps
            noVelNextFrame = true;
            // because of it, we won't actually be y>0, but we still need to be counted airborne
            airborne = true;
        } else if (currentAction == 5) {
            nextAction = 4; // finish transition to crouch
        } else if (!getAirborne() && !isProjectile && ((pCurrentAction->loopPoint != -1 && (loopCount == -1 || loopCount > 0)) || hitStun)) {
            currentFrame = pCurrentAction->loopPoint;
            if (currentFrame == -1) {
                currentFrame = 0;
            }
            currentFrameFrac = Fixed(currentFrame);
            hasLooped = true;
            if (loopCount > 0) {
                loopCount--;
            }
        } else if ((isProjectile && loopCount == 0) || (pParent && !isProjectile)) {
            if (advancingTime) {
                return false; // die if minion at end of script
            }
        } else if (blocking) {
            // ???
            nextAction = currentAction + 1;
        } else if (wallStopped || locked || airborne || (isProjectile && loopCount == -1)) {
            // freeze time at the end there, hopefully a branch will get us when we land :/
            // should this apply in general, not just airborne?
            currentFrame--;
            currentFrameFrac = Fixed(currentFrame);
        } else if (parrying) {
            nextAction = 481;
        } else {
            nextAction = 1;
        }

        if (resetHitStunOnTransition) {
            hitStun = 1;
            resetHitStunOnTransition = false;
            nextAction = 1;
        }
    }

    if (advancingTime && throwProtectionFrames) {
        throwProtectionFrames--;
    }

    if (advancingTime && strikeProtectionFrames) {
        strikeProtectionFrames--;
    }

    curNextAction = nextAction;

    if (doHitStun && hitStun > 0)
    {
        hitStun--;
        if (hitStun == 0)
        {
            if (crouching) {
                if (!(currentInput & DOWN)) {
                    nextAction = 6;
                } else {
                    // todo figure out how to preserve fluff frames there?
                    nextAction = 4;
                }
            }
            if (knockDown) {
                Fixed prevVelX = velocityX;
                accelX = Fixed(0);
                accelY = Fixed(0);
                velocityX = Fixed(0);
                velocityY = Fixed(0);

                // can't believe we have to do that, looking at you spds
                posY = Fixed(0);
                airborne = false;

                if (knockDownFrames) {
                    hitStun = knockDownFrames;
                    knockDownFrames = 0;
                    nextAction = 330;
                    knockDownFrameCounter = pSim->frameCounter;
                    if (groundBounceVelX != Fixed(0)) {
                        // slide
                        velocityX = groundBounceVelX;
                        groundBounceVelX = Fixed(0);
                        if (nageKnockdown) {
                            // one last slide tic from whatever previous vel
                            posX = posX + (prevVelX * direction);
                            // we slide one tic early, so go against the next frame worth's :/
                            posX = posX + (velocityX * direction * -1);
                        }
                    }
                    isDown = true;
                } else {
                    int searchWindow = 11;
                    if (knockDownFrameCounter != 0) {
                        searchWindow += pSim->frameCounter - knockDownFrameCounter;
                        knockDownFrameCounter = 0;
                    }
                    bool backroll = false;
                    for (int i = 0; i < searchWindow && !noBackRecovery; i++) {
                        int input = dc.inputBuffer[i] & (LP+MP+HP+LK+MK+HK);
                        if (dc.inputBuffer[i] & FROZEN) {
                            searchWindow++;
                            //log(true, "extending backroll window frozen " + std::to_string(searchWindow));
                        }
                        if (std::bitset<32>(input).count() >= 2) {
                            backroll = true;
                        }
                    }
                    if (backroll) {
                        nextAction = !recoverReverse ? 344 : 342;
                        if (backDamage) {
                            nextAction += 1;
                        }
                    } else {
                        nextAction = 340;
                    }
                    if (prevVelX != Fixed(0)) {
                        // one last slide tic
                        posX = posX + (prevVelX * direction);
                    }
                    if (backroll && needsTurnaround()) {
                        switchDirection();
                    }
                    isDown = false;
                    knockDown = false;

                    resetComboCount = true;
                }

                nageKnockdown = false;

                if (recoverForward && needsTurnaround()) {
                    switchDirection();
                    velocityX *= Fixed(-1);
                }
            } else {
                blocking = false;
                if (parrying) {
                    if (burnout || (currentInput & (32+256)) != 32+256) {
                        nextAction = 482;
                        parrying = false;
                        if (successfulParry) {
                            // // override to lower amount without helper
                            // focusRegenCooldown = 20;
                            // focusRegenCooldownFrozen = false;
                        }
                    } else {
                        // entering a new parry, clear successful bit
                        if (!parryHoldFreebieFrames) {
                            subjectToParryRecovery = true;
                        }
                    }
                }

                if (pOpponent) {
                    pOpponent->driveRushCancel = false;
                    pOpponent->parryDriveRush = false;
                }

                throwProtectionFrames = 2;
                log (logTransitions, "2f throw protection applied!");
            }
        }
    }

    if (!hitStun && parrying && (burnout || (currentInput & (32+256)) != 32+256)) {
        bool underMinimumTime = true;
        if ((currentAction != 480 && currentAction != 489) || currentFrame >= 12) {
            underMinimumTime = false;
        }
        if (nextAction == 480 || nextAction == 489) {
            underMinimumTime = true;
        }
        if (burnout || !underMinimumTime) {
            parrying = false;
            nextAction = 482; // DPA_STD_END
        }
    }

    if (parrying && parryHoldFreebieFrames) {
        parryHoldFreebieFrames--;
        if (!parryHoldFreebieFrames) {
            subjectToParryRecovery = true;
        }
    }

    bool canProxGuard = !hitStun && (canAct() || currentAction == 39 || currentAction == 40 || currentAction == 41);
    // positionProxGuarded lets uncrouchable protection work immediately on wakeup
    // it's possible this isn't strictly correct but in practice if there are hurtboxes the position is in there
    bool proxGuarded = hurtBoxProxGuarded || positionProxGuarded;
    if (proxGuarded && canProxGuard && (currentInput & BACK) && !(currentInput & UP)) {
        // it seems to branch as needed between stand/crouch?
        nextAction = 171;
        blocking = true;

        log(logHits, "proximity guard!");
    }

    if (!didTrigger && blocking && !hitStun && (!proxGuarded || !(currentInput & BACK) || (currentInput & UP))) {
        // was proximity guard, can act now
        blocking = false;
        nextAction = 1;
    }

    bool crouchingNow = false;
    bool movingForward = false;
    bool movingBackward = false;
    bool canMoveNow = false;

    canMoveNow = canMove(crouchingNow, movingForward, movingBackward);
    bool applyFreeMovement = freeMovement && !didTrigger && !jumpLandingDisabledFrames && !hitStun && !blocking;
    if (currentAction == 39 || currentAction == 40 || currentAction == 41) {
        applyFreeMovement = false;
    }
    if (nextAction != -1) {
        applyFreeMovement = false;
    }
    if (pSim->match && !pSim->timerStarted) {
        applyFreeMovement = false;
    }

    if (disableMovement) {
        canMoveNow = false;
        applyFreeMovement = false;
    }
    
    if (canMoveNow) {
        crouching = crouchingNow;
    }

    bool moveTurnaround = false;

    // process movement if any
    if ( canMoveNow || applyFreeMovement)
    {
        if ( !couldMove ) {
            recoveryTiming = pSim->frameCounter;
        }
        // reset status - recovered control to neutral
        jumped = false;
        blocking = false;
        if (canMoveNow) {
            isDrive = false;
            wasDrive = false;
            moveTurnaround = needsTurnaround(Fixed(10));
        }

        int moveInput = currentInput;
        bool crouchingFluffFrames = forcedPoseStatus == 2 && fluffFrames();

        if ( moveInput & 1 ) {

            jumped = true;
            // jump is trigger-ish - anything else we should do here?
            uniqueOpsAppliedMask = 0;

            if ( moveInput & 4 ) {
                jumpDirection = -1;
                nextAction = 35; // BAS_JUMP_B_START
            } else if ( moveInput & 8 ) {
                jumpDirection = 1;
                nextAction = 34; // BAS_JUMP_F_START
            } else {
                jumpDirection = 0;
                nextAction = 33; // BAS_JUMP_N_START
            }
        } else if ( moveInput & 2 ) {
            if ( !crouchingFluffFrames && !crouching ) {
                crouching = true;
                if (forcedPoseStatus == 2 && currentAction != 6) {
                    nextAction = 4; // crouch loop after the first sitting down anim if already crouched
                } else {
                    nextAction = 5; // BAS_STD_CRH
                }
            }
        } else if ( moveInput & 4 || moveInput & 8 ) {
            if ( moveInput & 4 && !movingBackward ) {
                nextAction = 13; // BAS_BACKWARD_START
            }
            if ( moveInput & 8 && !movingForward) {
                nextAction = 9; // BAS_FORWARD_START
            }
        } else {
            if ( !(moveInput & 4) && movingBackward ) {
                nextAction = 15; // BAS_BACKWARD_END
            }
            if ( !(moveInput & 8) && movingForward ) {
                nextAction = 11; // BAS_FORWARD_END
            }
            if (!(moveInput & 2) && (crouching || getCrouching()) && currentAction != 6) {
                nextAction = 6;
            }
        }
    }

    if (((canMoveNow || canProxGuard) && comboHits) || resetComboCount) {
        if ( comboHits) {
            log(true, " combo hits " + std::to_string(comboHits) + " damage " + std::to_string(comboDamage));
        }
        comboDamage = 0;
        comboHits = 0;
        currentScaling = 0;
        driveScaling = false;
        superGainScaling = 100;
        resetComboCount = false;
        lastDamageScale = 0;
        if (pOpponent) {
            pOpponent->driveRushCancel = false;
            pOpponent->parryDriveRush = false;
            pOpponent->appliedScaling = false;
            for (Guy *pMinion : pOpponent->getMinions()) {
                pMinion->appliedScaling = false;
            }
        }
    }

    if (canMoveNow) {
        perfectScaling = false;
    }

    if (canMoveNow && wasHit) {
        int advantage = pSim->frameCounter - pOpponent->recoveryTiming;
        std::string message = "recovered! adv " + std::to_string(advantage);
        log(true, message );

        juggleCounter = 0;

        pAttacker = nullptr;
        wallBounce = false; // just in case we didn't reach a wall
        wallSplat = false;
        stunSplat = false;

        wasHit = false;

        if (!throwProtectionFrames) {
            // if we didnt just recover out of hitstun, apply the generic 1f 
            throwProtectionFrames = 1;
            log (logTransitions, "1f throw protection applied!");
        }

        if (neutralMove != 0) {
            std::string moveString = pCharData->vecMoveList[neutralMove];
            int actionID = atoi(moveString.substr(0, moveString.find(" ")).c_str());
            nextAction = actionID;
        }
    }

    if (curNextAction != nextAction) {
        nextActionFrame = 0;
    }

    bool didTransition = false;

    if (!didTrigger && canMoveNow && nextAction == -1) {
        // can run deferred trigger if going into fluff frames now
        DoTriggers();
        if (nextAction != -1) {
            didTrigger = true;
        }
    }

    if (nextAction == -1 && moveTurnaround) {
        nextAction = crouching ? 8 : 7;
    }

    bool doingThrowTech = false;
    Fixed throwTechOrigin = Fixed(0);
    if (nextAction == 321) {
        // if we're about to get throw teched.. do placekey at the bumped frame to determine origin
        DoPlaceKey();
        doingThrowTech = true;
        throwTechOrigin = getPosX();
    }

    // transition point!
    if (nextAction != -1) {
        NextAction(didTrigger, didBranch);
        didTransition = true;
    }

    if (doingThrowTech) {
        posX = throwTechOrigin + (Fixed(50) * -direction);
        if (pOpponent) {
            // think this should be pretty ordering impervious at this stage?
            pOpponent->posX = throwTechOrigin + (Fixed(50) * direction);
            pOpponent->posOffsetX = Fixed(0);
            pOpponent->bgOffsetX = Fixed(0);
        }
    }

    // throw tech delayed pushback
    if ((currentAction == 321 || currentAction == 320) && currentFrame == 1) {
        hitVelX = Fixed(-13) * direction;
        hitAccelX = Fixed(1) * direction;
    }

    if (moveTurnaround || (needsTurnaround() && (didTrigger && !airborne && !wasDrive))) {
        switchDirection();
    }

    if (getHitStop() == 0) {
        timeInHitStop = 0;
    }

    // give the first frame an opportunity ot branch
    DoBranchKey(true);
    if (nextAction != -1) {
        didBranch = true;
    }

    bool didRecoveryTrigger = false;

    if (!didTrigger && didTransition && (canMoveNow || proxGuard())) {
        DoTriggers();
        if (nextAction != -1) {
            didTrigger = true;
            didRecoveryTrigger = true;
        }
    }

    // if successful, eat this frame away and go right now
    if (nextAction != -1) {
        NextAction(didTrigger, didBranch, true);
        didTransition = true;
            if (didRecoveryTrigger) {
            DoBranchKey(true);
            if (nextAction != -1) {
                didBranch = true;
                NextAction(didTrigger, didBranch, true);
                didTransition = true;
            }
        }
    }

    if (((!didTrigger && !didBranch && didTransition && currentAction != 482) || canMoveNow) && focusRegenCooldownFrozen && advancingTime) {
        // if we recovered out of an OD move - trigger handled directly in execute
        focusRegenCooldownFrozen = false;
        log(logResources, "regen cooldown unfrozen (recovered out of freeze move)");
    }

    // training mode refill, immediately start regen on recovery
    // if (!couldMove && canMoveNow) {
    //     focusRegenCooldown = 3;
    // }

    if (didTrigger && didTransition) {
        AdvanceScalingTriggerID();
        appliedScaling = false;
    }

    // if we need landing adjust/etc during hitStop, need this updated now
    DoStatusKey();
    WorldPhysics(true); // only floor

    couldMove = canMoveNow;
    lastPosX = getPosX();
    lastPosY = getPosY();
    lastBGPlaceX = bgOffsetX;
    lastDirection = direction;
    wasIgnoreHitStop = ignoreHitStop;

    forcedTrigger = ActionRef(0, 0);

    return true;
}

void Guy::AdvanceScalingTriggerID()
{
    Guy *pStart = this;
    if (isProjectile && pParent) {
        pStart = pParent;
    }
    int maxTeamScalingTriggerID = pStart->scalingTriggerID;
    for (Guy *pMinion : pStart->getMinions()) {
        if (pMinion->scalingTriggerID > maxTeamScalingTriggerID) {
            maxTeamScalingTriggerID = pMinion->scalingTriggerID;
        }
    }
    scalingTriggerID = maxTeamScalingTriggerID + 1;
}

void Guy::NextAction(bool didTrigger, bool didBranch, bool bElide)
{
    if ( nextAction != -1 )
    {
        ProjectileData *oldProjData = pCurrentAction ? pCurrentAction->pProjectileData : nullptr;

        if (currentAction != nextAction) {
            prevAction = currentAction;
            currentAction = nextAction;
            std::string prefix = "current action ";
            if (bElide) prefix = "nvm! " + prefix;
            log (logTransitions, prefix + std::to_string(currentAction));

            if (styleInstallFrames && !countingDownInstall) {
                // start counting down on wakeup after install super?
                countingDownInstall = true;
            }
        }

        if (nextActionOpponentAction) {
            opponentAction = true;
        } else {
            opponentAction = false;
        }

        if (didBranch) {
            // kinda crazy, but do EventKey for the bumped branch frame before the transition
            // steering moves will apply twice, on purpose. is it the same for triggers? :thonk:
            DoEventKey(pCurrentAction, currentFrame);
        }

        // if we transition after landing frame, reset action restriction
        if (jumpLandingDisabledFrames < 4) {
            jumpLandingDisabledFrames = 0;
        }

        currentFrame = nextActionFrame != -1 ? nextActionFrame : 0;
        actionSpeed = Fixed(1);
        actionInitialFrame = -1;
        currentFrameFrac = Fixed(currentFrame);

        if (!nextActionOpponentAction) {
            locked = false;
        }
        nextActionOpponentAction = false;

        currentArmor = nullptr; // uhhh

        nextAction = -1;
        nextActionFrame = -1;

        freeMovement = false;

        UpdateActionData();

        if (pCurrentAction && currentFrame >= pCurrentAction->actionFrameDuration) {
            currentFrame = pCurrentAction->actionFrameDuration - 1;
            currentFrameFrac = Fixed(currentFrame);
        }

        int inheritFlags = pCurrentAction ? pCurrentAction->inheritKindFlag : 0;

        if (!(inheritFlags & (1<<0)) && !keepPlace) {
            posX = posX + (posOffsetX * direction);
            posOffsetX = Fixed(0);
        } else {
            noPlaceXNextFrame = true;
        }
        if (!(inheritFlags & (1<<1)) && !keepPlace) {
            posY = posY + posOffsetY;
            posOffsetY = Fixed(0);
        } else {
            noPlaceYNextFrame = true;
        }

        setPlaceX = false;
        setPlaceY = false;
        keepPlace = false;
        bgOffsetX = Fixed(0);

        bool inheritHitID = pCurrentAction ? pCurrentAction->inheritHitID : false;
        // see eg. 2MP into light crusher - light crusher has inherit hitID true..
        // assuming it's supposed to be for branches only
        if (!didBranch) {
            inheritHitID = false;
        }

        if (!inheritHitID) {
            canHitID = 0;
        }

        // only reset on doing a trigger - skull diver trigger is on hit, but the hit
        // happens on a prior script, and it doesn't have inherit HitInfo, so it's not that
        // todo SPA_HEADPRESS_P_OD_HIT(2) has both inherit hitinfo and a touch branch, check what that's about
        if (!didBranch) {
            hitThisMove = false;
            hitCounterThisMove = false;
            hitPunishCounterThisMove = false;
            hasBeenBlockedThisMove = false;
            hasBeenParriedThisMove = false;
            hasBeenPerfectParriedThisMove = false;
            hitArmorThisMove = false;
            hitAtemiThisMove = false;
        }

        if (cancelInheritVelX != Fixed(0) || cancelInheritVelY != Fixed(0) ||
            cancelInheritAccelX != Fixed(0) || cancelInheritAccelY != Fixed(0)) {
            velocityX = velocityX * cancelInheritVelX;
            velocityY = velocityY * cancelInheritVelY;
            accelX = accelX * cancelInheritAccelX;
            accelY = accelY * cancelInheritAccelY;
            noAccelNextFrame = true; // see kim TP into normal..
            // not sure if we should pick and choose here, assume the steer-driven one wins for now
        } else if (!hitStun || blocking) {
            // should this use airborne status from previous or new action? currently previous
            if (isDrive || getAirborne() || isProjectile || didBranch) {
                if (pCurrentAction) {
                    accelX = accelX * pCurrentAction->inheritAccelX;
                    accelY = accelY * pCurrentAction->inheritAccelY;
                    Fixed inheritVelXRatio = pCurrentAction->inheritVelX;
                    velocityX = inheritVelXRatio * velocityX;
                    if (velocityX != Fixed(0) && inheritVelXRatio.data & 1) {
                        velocityX.data |= 1;
                    }
                    velocityY = velocityY * pCurrentAction->inheritVelY;
                }
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

        if (isDrive && wasDrive) {
            isDrive = false;
        }

        if (isProjectile) {
            ProjectileData *newProjData = pCurrentAction ? pCurrentAction->pProjectileData : nullptr;
            if (oldProjData != newProjData) {
                projDataInitialized = false;
            }
        }

        forcedPoseStatus = 0;
        actionStatus = 0;
        jumpStatus = 0;
        landingAdjust = 0;
        DoStatusKey();
    }
}

void Guy::DoSwitchKey(void)
{
    if (!pCurrentAction) {
        return;
    }

    for (auto& switchKey : pCurrentAction->switchKeys)
    {
        if (switchKey.startFrame > currentFrame || switchKey.endFrame <= currentFrame) {
            continue;
        }

        // only in ExtSwitchKey
        int validStyles = switchKey.validStyle;
        if ( validStyles != 0 && !(validStyles & (1 << styleInstall)) ) {
            continue;
        }

        int flag = switchKey.systemFlag;

        if (flag & 0x40000000) {
            canBlock = true;
        }
        if (flag & 0x10000000) {
            ignoreCornerPushback = true;
        }
        if (flag & 0x8000000) {
            isDrive = true;
            if (pOpponent && !pOpponent->driveScaling && pOpponent->currentScaling) {
                pOpponent->driveScaling = true;
            }
        }
        if (flag & 0x800000) {
            punishCounterState = true;
        }
        if (flag & 0x400000) {
            forceKnockDownState = true;
        }
        if (flag & 0x80000) {
            ignoreBodyPush = true;
        }
        if (flag & 0x40) {
            ignoreScreenPush = true;
        }
        if (flag & 0x8) {
            ignoreHitStop = true;
        }
        if (flag & 0x2) {
            counterState = true;
        }
        if (flag & 0x1) {
            throwTechable = true;
        }

        int operation = switchKey.operationFlag;

        if (operation & 1) {
            log (logTransitions, "force landing op");
            forceLanding = true;
        }
    }
}

void Guy::DoStatusKey(void)
{
    if (!pCurrentAction) {
        return;
    }

    for (auto& statusKey : pCurrentAction->statusKeys)
    {
        if (statusKey.startFrame > currentFrame || statusKey.endFrame <= currentFrame) {
            continue;
        }

        landingAdjust = statusKey.landingAdjust;
        forcedPoseStatus = statusKey.poseStatus;
        actionStatus = statusKey.actionStatus;
        jumpStatus = statusKey.jumpStatus;

        switch (statusKey.side) {
            default:
                log (logUnknowns, "unknown side op " + std::to_string(statusKey.side));
                break;
            case 0:
                break;
            case 1:
                if (needsTurnaround()) {
                    switchDirection();
                }
                break;
            case 3:
                switchDirection();
                break;
            case 9:
                if (direction != 1) {
                    switchDirection();
                }
                break;
            case 10:
                if (direction != -1) {
                    switchDirection();
                }
                break;
            case 11:
                if (pParent && direction != pParent->direction) {
                    switchDirection();
                }
                break;
        }
    }
}

void Guy::DoSteerKey(void)
{
    if (!pCurrentAction) {
        return;
    }

    for (auto& steerKey : pCurrentAction->steerKeys)
    {
        if (steerKey.startFrame > currentFrame || steerKey.endFrame <= currentFrame) {
            continue;
        }

        if (steerKey.isDrive && !wasDrive) {
            continue;
        }

        int operationType = steerKey.operationType;
        int valueType = steerKey.valueType;
        Fixed fixValue = steerKey.fixValue;
        Fixed targetOffsetX = steerKey.targetOffsetX;
        Fixed targetOffsetY = steerKey.targetOffsetY;
        int shotCategory = steerKey.shotCategory;
        int targetType = steerKey.targetType;
        int calcValueFrame = steerKey.calcValueFrame;
        int multiValueType = steerKey.multiValueType;
        int param = steerKey.param;

        switch (operationType) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                switch (valueType) {
                    case 0: DoSteerKeyOperation(velocityX, fixValue,operationType); break;
                    case 1: DoSteerKeyOperation(velocityY, fixValue,operationType); break;
                    case 3: DoSteerKeyOperation(accelX, fixValue,operationType); break;
                    case 4: DoSteerKeyOperation(accelY, fixValue,operationType); break;
                }
                break;
            case 9:
                switch (valueType) {
                    case 0: if (velocityX < fixValue) velocityX = fixValue; break;
                    case 1: if (velocityY < fixValue) velocityY = fixValue; break;
                    case 3: if (accelX < fixValue) accelX = fixValue; break;
                    case 4: if (accelY < fixValue) accelY = fixValue; break;
                }
                break;
            case 10:
                switch (valueType) {
                    case 0: if (velocityX > fixValue) velocityX = fixValue; break;
                    case 1: if (velocityY > fixValue) velocityY = fixValue; break;
                    case 3: if (accelX > fixValue) accelX = fixValue; break;
                    case 4: if (accelY > fixValue) accelY = fixValue; break;
                }
                break;
            case 11:
                if (ignoreSteerType != -1) {
                    log(logUnknowns, "two ignore at same time need more code");
                } else {
                    ignoreSteerType = valueType;
                }
                break;
            case 12:
                // set on cancel from current action
                // guessing there's also one to add on cancel too? not sure
                operationType = 1;
                switch (valueType) {
                    case 0: DoSteerKeyOperation(cancelVelocityX, fixValue,operationType); break;
                    case 1: DoSteerKeyOperation(cancelVelocityY, fixValue,operationType); break;
                    case 3: DoSteerKeyOperation(cancelAccelX, fixValue,operationType); break;
                    case 4: DoSteerKeyOperation(cancelAccelY, fixValue,operationType); break;
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
                        for ( auto minion : dc.minions ) {
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
                        homeTargetX = pGuy->getPosX() + (targetOffsetX * pGuy->direction * Fixed(-1));
                        homeTargetX = getPosX() - homeTargetX;
                        homeTargetY = pGuy->getPosY() + targetOffsetY;
                        homeTargetY = getPosY() - homeTargetY;
                    } else if (targetType == 13) {
                        homeTargetY = targetOffsetY;
                        if (targetOffsetX != Fixed(0)) {
                            log(logUnknowns, "don't know what to do with target X offset in ease to ground");
                        }
                    } else {
                        log(logUnknowns, "unknown/not found set teleport/home target type " + std::to_string(targetType));
                    }
                    homeTargetType = targetType;
                    homeTargetFrame = currentFrame;

                    if (param != 0) {
                        log(logUnknowns, "unknown param in set home target " + std::to_string(param));
                    }
                }
                break;
            case 15:
                // teleport/lerp position
                if (calcValueFrame == 0) {
                    calcValueFrame = 1;
                }
                if (homeTargetType == 13) {
                    // ease to ground over n frames - is multiValueType used for this?
                    // if (velocityY <= Fixed(0) || calcValueFrame < 2) {
                    //     log(logUnknowns, "unhandled case for ease to ground? vely " + std::to_string(velocityY.f()) + " t " + std::to_string(calcValueFrame));
                    // } else
                    {
                        // backsolve for acceleration over time.
                        int firstFrameTerm = calcValueFrame - (currentFrame - homeTargetFrame);
                        accelY = Fixed(-2) * (getPosY() + velocityY * Fixed(firstFrameTerm) - homeTargetY) / Fixed(calcValueFrame * calcValueFrame);
                    }
                } else {
                    if (multiValueType & 1) {
                        velocityX = -(homeTargetX) / Fixed(calcValueFrame) * direction;
                    }
                    if (multiValueType & 2) {
                        velocityY = -(homeTargetY) / Fixed(calcValueFrame);
                    }
                }
                break;
            case 16:
                // set cancel inherit %
                // there's some weirdness here - kim setting 1 then 9 undoes velX in-game
                // this will work in practice, but there's some subtle bug to maybe match
                if (multiValueType & 1) {
                    cancelInheritVelX = fixValue;
                }
                if (multiValueType & 2) {
                    cancelInheritVelY = fixValue;
                }
                if (multiValueType & 8) {
                    cancelInheritAccelX = fixValue;
                }
                if (multiValueType & 16) {
                    cancelInheritAccelY = fixValue;
                }
                break;
            default:
                log(logUnknowns, "unknown steer keyoperation " + std::to_string(operationType));
                break;
        }
    }
}

void Guy::DoWorldKey(void)
{
    if (!pCurrentAction) {
        return;
    }

    for (auto& worldKey : pCurrentAction->worldKeys)
    {
        if (worldKey.startFrame > currentFrame || worldKey.endFrame <= currentFrame) {
            continue;
        }

        int type = worldKey.type;

        switch (type) {
            case 1:
                superAnimation = true;
                // todo make a set of mutually exclusive scratch variables
                // for now use anything existing to not bloat the size up
                groundBounceVelX = getPosX();
                //groundBounceVelY = getPosY();
                posX = Fixed(0);
                //posY = Fixed(0);
                velocityX = Fixed(0);
                velocityY = Fixed(0);
                accelX = Fixed(0);
                accelY = Fixed(0);
                if (pOpponent) {
                    pOpponent->superFreeze = true;
                    setFocusRegenCooldown(90 + 1 + 1); // one for the unfreeze frame
                    otherGuyLog(pOpponent, logResources, "regen cooldown " + std::to_string(pOpponent->focusRegenCooldown) + " (worldkey)");
                }
                focusRegenCooldown = 0;
                focusRegenCooldownFrozen = false;
                focusRegenCooldownTicking = false;
                break;
            case 0:
                // type 1 is sa3 vs normal? why does that matter?
                // todo is timer timer deduction?
                if (pOpponent) {
                    pOpponent->tokiYoTomare = true;
                    for ( auto minion : pOpponent->dc.minions ) {
                        minion->tokiYoTomare = true;
                    }
                }
                for ( auto minion : dc.minions ) {
                    minion->tokiYoTomare = true;
                }
                break;
            case 5:
                // resume
                if (superAnimation) {
                    superAnimation = false;
                    posX = groundBounceVelX - (posOffsetX*direction);
                    //posY = groundBounceVelY;
                    if (pOpponent && pOpponent->locked) {
                        pOpponent->posX = posX;
                        //pOpponent->posY = posY;
                    }
                    groundBounceVelX = Fixed(0);
                    //groundBounceVelY = Fixed(0);
                }
                if (pOpponent) {
                    if (pOpponent->superFreeze) {
                        pOpponent->superFreeze = false;
                    }
                    if (pOpponent->warudo) {
                        pOpponent->tokiWaUgokidasu = true;
                    }
                    for ( auto minion : pOpponent->dc.minions ) {
                        if (minion->warudo) {
                            minion->tokiWaUgokidasu = true;
                        }
                    }
                }
                for ( auto minion : dc.minions ) {
                    if (minion->warudo) {
                        minion->tokiWaUgokidasu = true;
                    }
                }
                break;
            default:
                log(logUnknowns, "unknown worldkey type " + std::to_string(type));
                break;
        }
    }
}

void Guy::DoLockKey(void)
{
    if (!pCurrentAction) {
        return;
    }

    for (auto& lockKey : pCurrentAction->lockKeys)
    {
        if (lockKey.startFrame > currentFrame || lockKey.endFrame <= currentFrame) {
            continue;
        }

        int type = lockKey.type;
        int param01 = lockKey.param01;

        if (type == 1) {
            if (pOpponent) {
                pOpponent->nextAction = param01;
                pOpponent->nextActionOpponentAction = true;
                pOpponent->hitStun = 50000;
                pOpponent->juggleCounter = 0;
                pOpponent->resetHitStunOnLand = false;
                pOpponent->knockDown = false;
                if (!pOpponent->locked) {
                    // only snap position if this isn't a followup lock
                    pOpponent->direction = direction;
                    if (pOpponent->grabAdjust) {
                        pOpponent->posX = getPosX();
                        pOpponent->posY = getPosY();
                        pOpponent->posOffsetX = Fixed(0);
                        pOpponent->posOffsetY = Fixed(0);
                        pOpponent->bgOffsetX = Fixed(0);
                        pOpponent->airborne = airborne;
                    }
                    pOpponent->velocityX = Fixed(0);
                    pOpponent->velocityY = Fixed(0);
                    pOpponent->accelX = Fixed(0);
                    pOpponent->accelY = Fixed(0);
                }
                // for transition
                pOpponent->AdvanceFrame(false);
                // for placekey/etc
                pOpponent->RunFrame(false);
                pOpponent->locked = true;
                // our position + their placekey might be in a wall
                pOpponent->WorldPhysics();
            }
        } else if (type == 2) {
            // apply hit DT param 02 after RunFrame, since we dont know if other guy RunFrame
            // has run or not yet and it introduces ordering issues
            if (pendingUnlockHit) {
                log(true, "weird!");
            }
            pendingUnlockHit = lockKey.param02;
        }
    }
}

void Guy::DoPlaceKey(void)
{
    if (!pCurrentAction) {
        return;
    }

    bool setNormalXThisFrame = false;
    bool setNormalYThisFrame = false;
    bool setBGXThisFrame = false;
    bool setBGYThisFrame = false;

    for (auto& placeKey : pCurrentAction->placeKeys)
    {
        if (placeKey.startFrame > currentFrame || placeKey.endFrame <= currentFrame) {
            continue;
        }

        // seems like we stop obeying palcekey after a nage hit?
        if (nageKnockdown) {
            break;
        }

        Fixed offsetMatch = Fixed(0);
        int flag = placeKey.optionFlag;
        // todo there's a bunch of other flags
        bool cosmeticOffset = flag & 1;
        Fixed ratio = placeKey.ratio;

        if (cosmeticOffset) {
            continue;
        }

        bool *setXThisFrame = placeKey.bgOnly ? &setBGXThisFrame : &setNormalXThisFrame;
        bool *setYThisFrame = placeKey.bgOnly ? &setBGYThisFrame : &setNormalYThisFrame;
        Fixed *offsetX = placeKey.bgOnly ? &bgOffsetX : &posOffsetX;
        Fixed *offsetY = &posOffsetY;

        for (auto& pos : placeKey.posList) {
            int keyStartFrame = placeKey.startFrame;
            if (pos.frame == currentFrame - keyStartFrame) {
                offsetMatch = pos.offset;
                // // do we need to disambiguate which axis doesn't push? that'd be annoying
                // // is vertical pushback even a thing
                // if (curPlaceKeyDoesNotPush) {
                //     offsetDoesNotPush = true;
                // }
                break;
            }
            offsetMatch = pos.offset;
        }

        offsetMatch *= ratio;
        if (offsetMatch == Fixed(0) && !placeKey.bgOnly && !setPlaceX && !setPlaceY) {
            continue;
        }

        if (placeKey.axis == 0) {
            if (*setXThisFrame) {
                *offsetX += offsetMatch;
            } else {
                *offsetX = offsetMatch;
            }
            if (!placeKey.bgOnly) {
                setPlaceX = true;
            }
            *setXThisFrame = true;
        } else if (placeKey.axis == 1) {
            if (*setYThisFrame) {
                *offsetY += offsetMatch;
            } else {
                *offsetY = offsetMatch;
            }
            if (!placeKey.bgOnly) {
                setPlaceY = true;
            }
            *setYThisFrame = true;
        }
    }
}

void Guy::DoEventKey(Action *pAction, int frameID)
{
    if (!pAction) {
        return;
    }

    for (auto& eventKey : pAction->eventKeys)
    {
        if (eventKey.startFrame > frameID || eventKey.endFrame <= frameID) {
            continue;
        }

        int validStyles = eventKey.validStyle;
        if ( validStyles != 0 && !(validStyles & (1 << styleInstall)) ) {
            continue;
        }

        int eventType = eventKey.type;
        int eventID = eventKey.id;
        int64_t param1 = eventKey.param01;
        int64_t param2 = eventKey.param02;
        int64_t param3 = eventKey.param03;
        int64_t param4 = eventKey.param04;
        int64_t param5 = eventKey.param05;

        switch (eventType)
        {
            case 0:
                    switch (eventID) {
                        case 3:
                        case 4:
                        case 5:
                        {
                            Fixed posOffset = Fixed((int)param1);
                            Guy *pOffsetGuy = nullptr;

                            if (eventID == 3) {
                                pOffsetGuy = this;
                            }
                            if (eventID == 4) {
                                pOffsetGuy = pParent;
                            }
                            if (eventID == 5) {
                                pOffsetGuy = pOpponent;
                            }
                            if (pOffsetGuy) {
                                posX = pOffsetGuy->getPosX() + posOffset * direction;
                                teleported = true;
                            } else {
                                log(true, "offset broken");
                            }
                            if (param2 || param3 || param4 || param5 ) {
                                log(logUnknowns, "unknown offset param");
                            }
                            break;
                        }
                        case 49:
                        {
                            Fixed steerForward;
                            steerForward.data = param2;
                            Fixed steerBackward;
                            steerBackward.data = param3;

                            if (currentInput & FORWARD) {
                                posX += steerForward * direction;
                            } else if (currentInput & BACK) {
                                posX += steerBackward * direction;
                            }
                            if (param1 != 0) {
                                // todo there's rightward and upward/etc too
                                log(logUnknowns, "unimplemented move steer direction");
                            }
                            break;
                        }
                        default:
                            log(logUnknowns, "unknown owner event id " + std::to_string(eventID));
                            break;
                    }
                    break;
                case 1:
                    log(logUnknowns, "system event, id " + std::to_string(eventID));
                    break;
                case 2:
                    switch (eventID) {
                        case 26: // style change
                            if (param1 == 0) {
                                ChangeStyle(param2);
                            } else if (param1 == 1) {
                                ChangeStyle(styleInstall + param2);
                            } else {
                                log(logUnknowns, "unknown operator in chara event style change");
                            }
                            if (param3 == 1) {
                                styleInstallFrames = param4;
                            }
                            break;
                        case 41: // air action counter
                            if (param1 == 0) {
                                airActionCounter = param2;
                            } else if (param1 == 1) {
                                airActionCounter += param2;
                            } else {
                                log(logUnknowns, "unknown operator in chara event air action counter");
                            }
                            if (airActionCounter < 0) {
                                airActionCounter = 0;
                            }
                            break;
                        case 50: // style install timer
                            if (param1 == 0) {
                                styleInstallFrames = param2;
                            } else if (param1 == 1) {
                                styleInstallFrames += param2;
                            } else {
                                log(logUnknowns, "unknown operator in chara event style install timer");
                            }
                            break;
                        case 52: // bomb?
                            // used by ex crusher bombed scripts but unclear what it does
                            // if (pOpponent) {
                            //     pOpponent->debuffTimer = 0;
                            // }
                            break;
                        default:
                            log(logUnknowns, "unknown chara event id " + std::to_string(eventID));
                            break;
                        case 36:
                            if (param1 == 2) {
                                setGauge(gauge + param2);
                            } else if (param1 == 4) {
                                setFocus(focus + param2);
                                log(logResources, "focus " + std::to_string(param2) + " (eventkey), total " + std::to_string(focus));
                                if (param2 < 0 && (!parrying || !successfulParry)) {
                                    // same as spending bar on od move
                                    if (!setFocusRegenCooldown(parrying?240:120)) {
                                        //focusRegenCooldown--;
                                    }
                                    focusRegenCooldownFrozen = true;
                                    log(logResources, "regen cooldown " + std::to_string(focusRegenCooldown) + " (eventkey, frozen)");
                                }
                            }
                            break;
                    }
                    break;
                case 7:
                    switch (eventID) {
                        case 60: // unique param
                            if (param3 == 0) {
                                uniqueParam[param2] = param4;
                            } else if (param3 == 1) { 
                                uniqueParam[param2] += param4;
                                // param5 appears to be the limit
                                if (param4 > 0 && uniqueParam[param2] > param5) {
                                    uniqueParam[param2] = param5;
                                }
                                if (param4 < 0 && uniqueParam[param2] < param5) {
                                    uniqueParam[param2] = param5;
                                }
                            } else {
                                log(logUnknowns, "unknown operator in chara event unique param");
                            }
                            break;
                        case 62: // unique timer
                            if (param3 == 0) { // set??
                                uniqueTimer = true;
                                uniqueTimerCount = param4;
                            } else {
                                log(logUnknowns, "unknown operator in chara event unique timer");
                            }
                            break;
                        default:
                            log(logUnknowns, "unknown unique event id " + std::to_string(eventID));
                            break;
                    }
                    break;
                default:
                    log(logUnknowns, "unhandled event, type " + std::to_string(eventType) + " id " + std::to_string(eventID));
                case 11: // commentary
                case 5: // camera
                    break;
        }
    }
}

void Guy::DoShotKey(Action *pAction, int frameID)
{
    if (!pAction) {
        return;
    }

    for (auto& shotKey : pAction->shotKeys)
    {
        if (shotKey.startFrame > frameID || shotKey.endFrame <= frameID) {
            continue;
        }

        if (shotKey.validStyle != 0 && !(shotKey.validStyle & (1 << styleInstall))) {
            continue;
        }

        if (shotKey.operation == 2) {
            if (pParent == nullptr) {
                log(logUnknowns, "shotkey despawn but no parent?");
            }
            die = true;
        } else {
            Fixed posOffsetX = shotKey.posOffsetX * direction;
            Fixed posOffsetY = shotKey.posOffsetY;

            bool spawnInBounds = false;
            if (shotKey.flags & 2) {
                spawnInBounds = true;
            }
            if (shotKey.flags & ~(2|4|16)) {
                log(logUnknowns, "unknown shotkey flag " + std::to_string(shotKey.flags));
            }

            // spawn new guy
            Guy *pNewGuy = new Guy(*this, posOffsetX, posOffsetY, shotKey.actionId, shotKey.styleIdx, true);
            if (shotKey.flags & 4) {
                pNewGuy->ignoreWarudo = true;
            }
            if (spawnInBounds) {
                pNewGuy->WorldPhysics(false, true);
            }
            if (shotKey.flags & 16) {
                AdvanceScalingTriggerID();
                pNewGuy->appliedScaling = false;
            }
            // run initial frame on behalf of sim loop here because it wasn't included this frame
            pNewGuy->DoStatusKey();
            pNewGuy->RunFrame(true);
            pNewGuy->RunFramePostPush();
            if (pParent) {
                pParent->dc.minions.push_back(pNewGuy);
            } else {
                dc.minions.push_back(pNewGuy);
            }
        }
    }
}

void Guy::DoInstantAction(int actionID)
{
    Action *pInstantAction = FindMove(actionID, styleInstall);
    if (pInstantAction) {
        // only ones i've seen used in those kinds of actions so far
        DoEventKey(pInstantAction, 0);
        DoShotKey(pInstantAction, 0);
    } else {
        log(true, "couldn't find instant action " + std::to_string(actionID));
    }
}

void Guy::ChangeStyle(int newStyleID) {
    // todo exit action from previous style?
    styleInstall = newStyleID;

    if (newStyleID < 0 || (size_t)newStyleID >= pCharData->styles.size()) {
        return;
    }

    StyleData &style = pCharData->styles[newStyleID];
    if (style.hasStartAction) {
        DoInstantAction(style.startActionID);
        if (style.startActionStyle != -1 && style.startActionStyle != newStyleID) {
            styleInstall = style.startActionStyle; // not sure if correct - like jamie's exit action naming a diff style
        }
    }
}

void Guy::ExitStyle() {
    if (styleInstall < 0 || (size_t)styleInstall >= pCharData->styles.size()) {
        return;
    }

    StyleData &style = pCharData->styles[styleInstall];

    if (style.hasExitAction) {
        DoInstantAction(style.exitActionID);
        if (style.exitActionStyle != -1 && style.exitActionStyle != styleInstall) {
            styleInstall = style.exitActionStyle;
        } else {
            styleInstall = style.parentStyleID;
        }
    } else {
        styleInstall = style.parentStyleID;
    }
}

void Guy::FixRefs(std::map<int,Guy*> &guysByID) {
    pOpponent.FixRef(guysByID);
    pParent.FixRef(guysByID);
    pAttacker.FixRef(guysByID);

    for (GuyRef & minion : dc.minions) {
        minion.FixRef(guysByID);
    }
}
