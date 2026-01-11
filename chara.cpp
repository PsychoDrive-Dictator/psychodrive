#include "chara.hpp"
#include "main.hpp"
#include <string>
#include <cstdlib>

void loadCharges(nlohmann::json* pChargeJson, CharacterData* pRet)
{
    if (!pChargeJson) {
        return;
    }

    pRet->charges.reserve(pChargeJson->size());

    for (auto& [keyID, key] : pChargeJson->items()) {
        Charge charge;

        nlohmann::json *pResource = &key;
        if (key.contains("resource")) {
            pResource = &key["resource"];
        }

        charge.id = (*pResource)["charge_id"];
        charge.okKeyFlags = (*pResource)["ok_key_flags"];
        charge.okCondFlags = (*pResource)["ok_key_cond_check_flags"];
        charge.chargeFrames = (*pResource)["ok_frame"];
        charge.keepFrames = (*pResource)["keep_frame"];

        pRet->charges.push_back(charge);
    }
}

void loadCommands(nlohmann::json* pCommandsJson, CharacterData* pRet)
{
    if (!pCommandsJson) {
        return;
    }

    pRet->commands.reserve(pCommandsJson->size());

    for (auto& [commandIDStr, commandGroup] : pCommandsJson->items()) {
        int commandID = std::atoi(commandIDStr.c_str());

        Command command;
        command.id = commandID;
        command.variants.reserve(commandGroup.size());

        for (auto& [variantIDStr, variant] : commandGroup.items()) {
            int inputNum = variant["input_num"].get<int>();
            nlohmann::json *pInputs = &variant["inputs"];

            CommandVariant commandVariant;
            commandVariant.totalMaxFrames = variant["max_frame"];
            commandVariant.inputs.reserve(inputNum);

            for (int i = 0; i < inputNum; i++) {
                char inputIDStr[16];
                snprintf(inputIDStr, sizeof(inputIDStr), "%02d", i);

                nlohmann::json *pInput = &(*pInputs)[inputIDStr];
                CommandInput input;

                input.type = (InputType)(*pInput)["type"].get<int>();
                input.numFrames = (*pInput)["frame_num"];

                nlohmann::json *pInputNorm = &(*pInput)["normal"];
                input.okKeyFlags = (*pInputNorm)["ok_key_flags"];
                input.okCondFlags = (*pInputNorm)["ok_key_cond_check_flags"];
                input.ngKeyFlags = (*pInputNorm)["ng_key_flags"];
                input.ngCondFlags = (*pInputNorm)["ng_key_cond_check_flags"];

                if (input.type == InputType::Rotation) {
                    input.rotatePointsNeeded = (*pInput)["rotate"]["point"];
                } else {
                    input.rotatePointsNeeded = 0;
                }

                if (input.type == InputType::ChargeRelease) {
                    int chargeID = (*pInput)["charge"]["id"];
                    input.pCharge = nullptr;
                    for (auto& charge : pRet->charges) {
                        if (charge.id == chargeID) {
                            input.pCharge = &charge;
                            break;
                        }
                    }
                } else {
                    input.pCharge = nullptr;
                }

                commandVariant.inputs.push_back(input);
            }

            command.variants.push_back(commandVariant);
        }

        pRet->commands.push_back(command);
    }
}

void loadTriggers(nlohmann::json* pTriggersJson, CharacterData* pRet)
{
    if (!pTriggersJson) {
        return;
    }

    size_t triggerCount = 0;
    for (auto& [actionIDStr, actionGroup] : pTriggersJson->items()) {
        triggerCount += actionGroup.size();
    }
    pRet->triggers.reserve(triggerCount);

    for (auto& [actionIDStr, actionGroup] : pTriggersJson->items()) {
        for (auto& [triggerIDStr, triggerData] : actionGroup.items()) {
            Trigger trigger;

            trigger.id = std::atoi(triggerIDStr.c_str());
            trigger.actionID = triggerData["action_id"];

            trigger.validStyles = triggerData["fightstyle_flags"];

            nlohmann::json *pNorm = &triggerData["norm"];
            trigger.okKeyFlags = (*pNorm)["ok_key_flags"];
            trigger.okCondFlags = (*pNorm)["ok_key_cond_flags"];
            trigger.ngKeyFlags = (*pNorm)["ng_key_flags"];
            trigger.dcExcFlags = (*pNorm)["dc_exc_flags"];
            trigger.dcIncFlags = (*pNorm)["dc_inc_flags"];
            trigger.precedingTime = (*pNorm)["preceding_time"];

            int commandNo = (*pNorm)["command_no"];
            trigger.pCommandClassic = nullptr;
            if (commandNo != -1) {
                for (auto& command : pRet->commands) {
                    if (command.id == commandNo) {
                        trigger.pCommandClassic = &command;
                        break;
                    }
                }
            }

            trigger.useUniqueParam = triggerData["_UseUniqueParam"].get<bool>();
            trigger.condParamID = triggerData["cond_param_id"];
            trigger.condParamOp = triggerData["cond_param_ope"];
            trigger.condParamValue = triggerData["cond_param_value"];

            trigger.limitShotCount = triggerData["cond_limit_shot_num"];
            trigger.limitShotCategory = triggerData["limit_shot_category"];

            trigger.airActionCountLimit = triggerData["cond_jump_cmd_count"];

            trigger.vitalOp = triggerData["cond_vital_ope"];
            trigger.vitalRatio = triggerData["cond_vital_ratio"];

            trigger.rangeCondition = triggerData["cond_range"];
            trigger.rangeParam = Fixed(triggerData["cond_range_param"].get<double>());

            trigger.stateCondition = triggerData["cond_owner_state_flags"];

            trigger.needsFocus = triggerData["focus_need"].get<int>() != 0;
            trigger.focusCost = triggerData["focus_consume"];

            trigger.needsGauge = triggerData["gauge_need"].get<int>() != 0;
            trigger.gaugeCost = triggerData["gauge_consume"];

            trigger.comboInst = triggerData["combo_inst"];

            trigger.flags = triggerData["category_flags"];

            pRet->triggers.push_back(trigger);
        }
    }
}

void loadTriggerGroups(nlohmann::json* pTriggerGroupsJson, CharacterData* pRet)
{
    if (!pTriggerGroupsJson) {
        return;
    }

    pRet->triggerGroups.reserve(pTriggerGroupsJson->size());

    for (auto& [groupIDStr, group] : pTriggerGroupsJson->items()) {
        TriggerGroup triggerGroup;
        triggerGroup.id = std::atoi(groupIDStr.c_str());
        triggerGroup.entries.reserve(group.size());

        for (auto& [entryIDStr, entry] : group.items()) {
            int triggerID = std::atoi(entryIDStr.c_str());
            std::string actionString = entry.get<std::string>();
            int actionID = std::atoi(actionString.substr(0, actionString.find(" ")).c_str());

            TriggerGroupEntry tgEntry;
            tgEntry.actionID = actionID;
            tgEntry.triggerID = triggerID;
            tgEntry.pTrigger = nullptr;
            for (auto& trigger : pRet->triggers) {
                if (trigger.id == triggerID) {
                    tgEntry.pTrigger = &trigger;
                    break;
                }
            }

            triggerGroup.entries.push_back(tgEntry);
        }

        pRet->triggerGroups.push_back(triggerGroup);
    }
}

size_t countRects(nlohmann::json* pRectsJson)
{
    if (!pRectsJson) {
        return 0;
    }

    size_t rectsCount = 0;
    for (auto& [rectsListIDStr, rectsList] : pRectsJson->items()) {
        rectsCount += rectsList.size();
    }
    return rectsCount;
}

void loadRects(nlohmann::json* pRectsJson, std::vector<Rect>* pOutputVector)
{
    if (!pRectsJson || !pOutputVector) {
        return;
    }

    for (auto& [rectsListIDStr, rectsList] : pRectsJson->items()) {
        int listID = atoi(rectsListIDStr.c_str());
        for (auto& [rectIDStr, rect] : rectsList.items()) {
            Rect newRect;

            newRect.listID = listID;
            newRect.id = atoi(rectIDStr.c_str());

            newRect.xOrig = rect["OffsetX"];
            newRect.yOrig = rect["OffsetY"];
            newRect.xRadius = rect["SizeX"];
            newRect.yRadius = rect["SizeY"];

            pOutputVector->push_back(newRect);
        }
    }
}

void loadHurtBoxKeys(nlohmann::json* pHurtBoxJson, std::vector<HurtBoxKey>* pOutputVector, std::map<std::pair<int, int>, Rect*>& rectsByIDs, std::map<int, AtemiData*>& atemiByID)
{
    if (!pHurtBoxJson) {
        return;
    }

    int magicHurtBoxID = 8; // i hate you magic array of boxes
    int magicThrowBoxID = 7;

    for (auto& [hurtBoxID, hurtBox] : pHurtBoxJson->items()) {
        if (!hurtBox.contains("_StartFrame")) {
            continue;
        }
        HurtBoxKey newKey;
        newKey.startFrame = hurtBox["_StartFrame"];
        newKey.endFrame = hurtBox["_EndFrame"];

        newKey.condition = hurtBox["Condition"];
        newKey.offsetX = Fixed(0);
        newKey.offsetY = Fixed(0);
        if (hurtBox.contains("RootOffset")) {
            newKey.offsetX = Fixed(hurtBox["RootOffset"].value("X", 0));
            newKey.offsetY = Fixed(hurtBox["RootOffset"].value("Y", 0));
        }
        newKey.isArmor = hurtBox["_isArm"];
        int armorID = hurtBox["AtemiDataListIndex"];
        auto atemiIt = atemiByID.find(armorID);
        if (atemiIt != atemiByID.end()) {
            newKey.pAtemiData = atemiIt->second;
        }
        newKey.isAtemi = hurtBox["_isAtm"];
        newKey.immunity = hurtBox["Immune"];
        newKey.flags = hurtBox["TypeFlag"];

        newKey.headRects.reserve(hurtBox["HeadList"].size());
        for (auto& [boxNumber, boxID] : hurtBox["HeadList"].items()) {
            auto it = rectsByIDs.find(std::make_pair(magicHurtBoxID, boxID));
            if (it != rectsByIDs.end()) {
                newKey.headRects.push_back(it->second);
            }
        }
        newKey.bodyRects.reserve(hurtBox["BodyList"].size());
        for (auto& [boxNumber, boxID] : hurtBox["BodyList"].items()) {
            auto it = rectsByIDs.find(std::make_pair(magicHurtBoxID, boxID));
            if (it != rectsByIDs.end()) {
                newKey.bodyRects.push_back(it->second);
            }
        }
        newKey.legRects.reserve(hurtBox["LegList"].size());
        for (auto& [boxNumber, boxID] : hurtBox["LegList"].items()) {
            auto it = rectsByIDs.find(std::make_pair(magicHurtBoxID, boxID));
            if (it != rectsByIDs.end()) {
                newKey.legRects.push_back(it->second);
            }
        }
        newKey.throwRects.reserve(hurtBox["ThrowList"].size());
        for (auto& [boxNumber, boxID] : hurtBox["ThrowList"].items()) {
            auto it = rectsByIDs.find(std::make_pair(magicThrowBoxID, boxID));
            if (it != rectsByIDs.end()) {
                newKey.throwRects.push_back(it->second);
            }
        }

        pOutputVector->push_back(newKey);
    }
}

void loadPushBoxKeys(nlohmann::json* pPushBoxJson, std::vector<PushBoxKey>* pOutputVector, std::map<std::pair<int, int>, Rect*>& rectsByIDs)
{
    if (!pPushBoxJson) {
        return;
    }

    for (auto& [pushBoxID, pushBox] : pPushBoxJson->items()) {
        if (!pushBox.contains("_StartFrame")) {
            continue;
        }
        PushBoxKey newKey;
        newKey.startFrame = pushBox["_StartFrame"];
        newKey.endFrame = pushBox["_EndFrame"];
        newKey.condition = pushBox["Condition"];
        newKey.offsetX = Fixed(0);
        newKey.offsetY = Fixed(0);
        if (pushBox.contains("RootOffset")) {
            newKey.offsetX = Fixed(pushBox["RootOffset"].value("X", 0));
            newKey.offsetY = Fixed(pushBox["RootOffset"].value("Y", 0));
        }

        int boxID = pushBox["BoxNo"];
        auto it = rectsByIDs.find(std::make_pair(5, boxID));
        if (it != rectsByIDs.end()) {
            newKey.rect = it->second;
            pOutputVector->push_back(newKey);
        }
    }
}

void loadHitBoxKeys(nlohmann::json* pHitBoxJson, std::vector<HitBoxKey>* pOutputVector, std::map<std::pair<int, int>, Rect*>& rectsByIDs, bool isOther, std::map<int, HitData*>& hitByID)
{
    if (!pHitBoxJson) {
        return;
    }

    for (auto& [hitBoxID, hitBox] : pHitBoxJson->items()) {
        if (!hitBox.contains("_StartFrame")) {
            continue;
        }
        HitBoxKey newKey;
        newKey.startFrame = hitBox["_StartFrame"];
        newKey.endFrame = hitBox["_EndFrame"];
        newKey.condition = hitBox["Condition"];
        newKey.offsetX = Fixed(0);
        newKey.offsetY = Fixed(0);
        if (hitBox.contains("RootOffset")) {
            newKey.offsetX = Fixed(hitBox["RootOffset"].value("X", 0));
            newKey.offsetY = Fixed(hitBox["RootOffset"].value("Y", 0));
        }

        int validStyles = hitBox["_ValidStyle"];
        if (validStyles != 0) {
            newKey.hasValidStyle = true;
            newKey.validStyle = validStyles;
        }

        int collisionType = hitBox["CollisionType"];
        hitBoxType type = hit;
        int rectListID = collisionType;
        if (isOther) {
            if (collisionType == 7) {
                type = domain;
            } else if (collisionType == 10) {
                type = destroy_projectile;
            } else if (collisionType == 11) {
                type = direct_damage;
            }
            rectListID = 9;
        } else {
            if (collisionType == 3) {
                type = proximity_guard;
            } else if (collisionType == 2) {
                type = grab;
            } else if (collisionType == 1) {
                type = projectile;
            } else if (collisionType == 0) {
                type = hit;
            }
        }

        newKey.type = type;
        int hitEntryID = hitBox["AttackDataListIndex"];
        auto hitIt = hitByID.find(hitEntryID);
        if (hitIt != hitByID.end()) {
            newKey.pHitData = hitIt->second;
        }

        int hitID = hitBox["HitID"];
        bool hasHitID = hitBox.value("_IsHitID", hitBox.value("_UseHitID", false));
        if (type == domain || type == direct_damage) {
            hasHitID = false;
        }
        if (hitID < 0) {
            hitID = 15;
        }
        if (hasHitID == false) {
            hitID = -1;
        }
        if (hitID == 15 || type == domain) {
            hitID = 15 + atoi(hitBoxID.c_str());
            if (hitID >= 64) {
                log("hitID overflow for domain boxes!");
            }
        }
        newKey.hasHitID = hasHitID;
        newKey.hitID = hitID;

        newKey.flags = (hitBoxFlags)0;
        if (hitBox.value("_IsGuardBit", false)) {
            int guardBit = hitBox["GuardBit"];
            if ((guardBit & 3) == 1) {
                newKey.flags = (hitBoxFlags)(newKey.flags | overhead);
            }
            if ((guardBit & 3) == 2) {
                newKey.flags = (hitBoxFlags)(newKey.flags | low);
            }
        }

        int flags = hitBox["KindFlag"];
        // ty gelly the homie
        if (flags & 0x10) {
            newKey.flags = (hitBoxFlags)(newKey.flags | avoids_standing);
        }
        if (flags & 0x20) {
            newKey.flags = (hitBoxFlags)(newKey.flags | avoids_crouching);
        }
        if (flags & 0x40) {
            newKey.flags = (hitBoxFlags)(newKey.flags | avoids_airborne);
        }
        if (flags & 0x100) {
            newKey.flags = (hitBoxFlags)(newKey.flags | only_hits_behind);
        }
        if (flags & 0x200) {
            newKey.flags = (hitBoxFlags)(newKey.flags | only_hits_front);
        }
        if (flags & 0x40000) {
            newKey.flags = (hitBoxFlags)(newKey.flags | only_hits_in_juggle);
        }
        if (flags & 0x100000) {
            newKey.flags = (hitBoxFlags)(newKey.flags | multi_counter);
        }
        if (type == domain) {
            pOutputVector->push_back(newKey);
        } else {
            newKey.rects.reserve(hitBox["BoxList"].size());
            for (auto& [boxNumber, boxID] : hitBox["BoxList"].items()) {
                auto it = rectsByIDs.find(std::make_pair(rectListID, boxID));
                if (it != rectsByIDs.end()) {
                    newKey.rects.push_back(it->second);
                }
            }
            if (!newKey.rects.empty() || type == proximity_guard) {
                pOutputVector->push_back(newKey);
            }
        }
    }
}

void loadSteerKeys(nlohmann::json* pSteerJson, std::vector<SteerKey>* pOutputVector, bool isDrive)
{
    if (!pSteerJson) {
        return;
    }

    for (auto& [steerKeyID, steerKey] : pSteerJson->items()) {
        if (!steerKey.contains("_StartFrame")) {
            continue;
        }
        SteerKey newKey;
        newKey.startFrame = steerKey["_StartFrame"];
        newKey.endFrame = steerKey["_EndFrame"];
        newKey.operationType = steerKey["OperationType"];
        newKey.valueType = steerKey["ValueType"];
        newKey.fixValue = Fixed(steerKey["FixValue"].get<double>());
        newKey.targetOffsetX = Fixed(steerKey.value("FixTargetOffsetX", 0.0));
        newKey.targetOffsetY = Fixed(steerKey.value("FixTargetOffsetY", 0.0));
        newKey.shotCategory = steerKey.value("_ShotCategory", 0);
        newKey.targetType = steerKey.value("TargetType", 0);
        newKey.calcValueFrame = steerKey.value("CalcValueFrame", 0);
        newKey.multiValueType = steerKey.value("MultiValueType", 0);
        newKey.param = steerKey.value("Param", 0);
        newKey.isDrive = isDrive;

        pOutputVector->push_back(newKey);
    }
}

void loadPlaceKeys(nlohmann::json* pPlaceJson, std::vector<PlaceKey>* pOutputVector)
{
    if (!pPlaceJson) {
        return;
    }

    for (auto& [placeKeyID, placeKey] : pPlaceJson->items()) {
        if (!placeKey.contains("_StartFrame")) {
            continue;
        }
        PlaceKey newKey;
        newKey.startFrame = placeKey["_StartFrame"];
        newKey.endFrame = placeKey["_EndFrame"];
        newKey.optionFlag = placeKey["OptionFlag"];
        newKey.ratio = Fixed(placeKey["Ratio"].get<double>());
        newKey.axis = placeKey["Axis"];

        if (placeKey.contains("PosList")) {
            newKey.posList.reserve(placeKey["PosList"].size());
            for (auto& [frame, offset] : placeKey["PosList"].items()) {
                PlaceKeyPos pos;
                pos.frame = atoi(frame.c_str());
                pos.offset = Fixed(offset.get<double>());
                newKey.posList.push_back(pos);
            }
        }

        pOutputVector->push_back(newKey);
    }
}

void loadSwitchKeys(nlohmann::json* pSwitchJson, std::vector<SwitchKey>* pOutputVector)
{
    if (!pSwitchJson) {
        return;
    }

    for (auto& [switchKeyID, switchKey] : pSwitchJson->items()) {
        if (!switchKey.contains("_StartFrame")) {
            continue;
        }
        SwitchKey newKey;
        newKey.startFrame = switchKey["_StartFrame"];
        newKey.endFrame = switchKey["_EndFrame"];
        newKey.systemFlag = switchKey["SystemFlag"];
        newKey.operationFlag = switchKey["OperationFlag"];
        newKey.validStyle = switchKey.value("_ValidStyle", 0);

        pOutputVector->push_back(newKey);
    }
}

void loadEventKeys(nlohmann::json* pEventJson, std::vector<EventKey>* pOutputVector)
{
    if (!pEventJson) {
        return;
    }

    for (auto& [eventKeyID, eventKey] : pEventJson->items()) {
        if (!eventKey.contains("_StartFrame")) {
            continue;
        }
        EventKey newKey;
        newKey.startFrame = eventKey["_StartFrame"];
        newKey.endFrame = eventKey["_EndFrame"];
        newKey.validStyle = eventKey.value("_ValidStyle", 0);
        newKey.type = eventKey["Type"];
        newKey.id = eventKey["ID"];
        newKey.param01 = eventKey["Param01"];
        newKey.param02 = eventKey["Param02"];
        newKey.param03 = eventKey["Param03"];
        newKey.param04 = eventKey["Param04"];
        newKey.param05 = eventKey["Param05"];

        pOutputVector->push_back(newKey);
    }
}

void loadWorldKeys(nlohmann::json* pWorldJson, std::vector<WorldKey>* pOutputVector)
{
    if (!pWorldJson) {
        return;
    }

    for (auto& [worldKeyID, worldKey] : pWorldJson->items()) {
        if (!worldKey.contains("_StartFrame")) {
            continue;
        }
        WorldKey newKey;
        newKey.startFrame = worldKey["_StartFrame"];
        newKey.endFrame = worldKey["_EndFrame"];
        newKey.type = worldKey["Type"];

        pOutputVector->push_back(newKey);
    }
}

void loadLockKeys(nlohmann::json* pLockJson, std::vector<LockKey>* pOutputVector, std::map<int, HitData*>& hitByID)
{
    if (!pLockJson) {
        return;
    }

    for (auto& [lockKeyID, lockKey] : pLockJson->items()) {
        if (!lockKey.contains("_StartFrame")) {
            continue;
        }
        LockKey newKey;
        newKey.startFrame = lockKey["_StartFrame"];
        newKey.endFrame = lockKey["_EndFrame"];
        newKey.type = lockKey["Type"];
        newKey.param01 = lockKey["Param01"];
        newKey.param02 = lockKey["Param02"];

        if (newKey.type == 2) {
            auto hitIt = hitByID.find(newKey.param02);
            if (hitIt != hitByID.end()) {
                newKey.pHitEntry = &hitIt->second->common[0];
            }
        }

        pOutputVector->push_back(newKey);
    }
}

void loadBranchKeys(nlohmann::json* pBranchJson, std::vector<BranchKey>* pOutputVector)
{
    if (!pBranchJson) {
        return;
    }

    for (auto& [branchKeyID, branchKey] : pBranchJson->items()) {
        if (!branchKey.contains("_StartFrame")) {
            continue;
        }
        BranchKey newKey;
        newKey.startFrame = branchKey["_StartFrame"];
        newKey.endFrame = branchKey["_EndFrame"];
        newKey.type = branchKey["Type"];
        newKey.param00 = branchKey["Param00"];
        newKey.param01 = branchKey["Param01"];
        newKey.param02 = branchKey["Param02"];
        newKey.param03 = branchKey["Param03"];
        newKey.param04 = branchKey["Param04"];
        newKey.branchAction = branchKey["Action"];
        newKey.branchFrame = branchKey["ActionFrame"];
        newKey.keepFrame = branchKey["_InheritFrameX"];
        newKey.keepPlace = branchKey["_KeepPlace"];
        newKey.typeName = branchKey.value("_TypesName", "");

        pOutputVector->push_back(newKey);
    }
}

void loadShotKeys(nlohmann::json* pShotJson, std::vector<ShotKey>* pOutputVector)
{
    if (!pShotJson) {
        return;
    }

    for (auto& [shotKeyID, shotKey] : pShotJson->items()) {
        if (!shotKey.contains("_StartFrame")) {
            continue;
        }
        ShotKey newKey;
        newKey.startFrame = shotKey["_StartFrame"];
        newKey.endFrame = shotKey["_EndFrame"];
        newKey.validStyle = shotKey["_ValidStyle"];
        newKey.operation = shotKey["Operation"];
        newKey.flags = shotKey["SpawnFlag"];
        newKey.posOffsetX = Fixed(shotKey["PosOffset"]["x"].get<double>());
        newKey.posOffsetY = Fixed(shotKey["PosOffset"]["y"].get<double>());
        newKey.actionId = shotKey["ActionId"];
        newKey.styleIdx = shotKey["StyleIdx"];

        pOutputVector->push_back(newKey);
    }
}

void loadTriggerKeys(nlohmann::json* pTriggerJson, std::vector<TriggerKey>* pOutputVector, std::map<int, TriggerGroup*>& triggerGroupByID)
{
    if (!pTriggerJson) {
        return;
    }

    for (auto& [triggerKeyID, triggerKey] : pTriggerJson->items()) {
        if (!triggerKey.contains("_StartFrame")) {
            continue;
        }
        TriggerKey newKey;
        newKey.startFrame = triggerKey["_StartFrame"];
        newKey.endFrame = triggerKey["_EndFrame"];
        newKey.validStyle = triggerKey["_ValidStyle"];
        newKey.other = triggerKey["_Other"];
        newKey.condition = triggerKey["_Condition"];
        newKey.state = triggerKey["_State"];

        int triggerGroupID = triggerKey["TriggerGroup"];
        auto it = triggerGroupByID.find(triggerGroupID);
        if (it != triggerGroupByID.end()) {
            newKey.pTriggerGroup = it->second;
        }

        pOutputVector->push_back(newKey);
    }
}

void loadStatusKeys(nlohmann::json* pStatusJson, std::vector<StatusKey>* pOutputVector)
{
    if (!pStatusJson) {
        return;
    }

    for (auto& [statusKeyID, statusKey] : pStatusJson->items()) {
        if (!statusKey.contains("_StartFrame")) {
            continue;
        }
        StatusKey newKey;
        newKey.startFrame = statusKey["_StartFrame"];
        newKey.endFrame = statusKey["_EndFrame"];
        newKey.landingAdjust = statusKey["LandingAdjust"];
        newKey.poseStatus = statusKey["PoseStatus"];
        newKey.actionStatus = statusKey["ActionStatus"];
        newKey.jumpStatus = statusKey["JumpStatus"];
        newKey.side = statusKey["Side"];

        pOutputVector->push_back(newKey);
    }
}

void loadStyles(nlohmann::json* pCharInfoJson, std::vector<StyleData>* pOutputVector)
{
    if (!pCharInfoJson || !pCharInfoJson->contains("Styles")) {
        return;
    }

    nlohmann::json &stylesJson = (*pCharInfoJson)["Styles"];

    // Find max style ID to size vector
    int maxStyleID = -1;
    for (auto& [styleIDStr, styleJson] : stylesJson.items()) {
        int styleID = std::stoi(styleIDStr);
        if (styleID > maxStyleID) {
            maxStyleID = styleID;
        }
    }

    if (maxStyleID >= 0) {
        pOutputVector->reserve(maxStyleID + 1);
        pOutputVector->resize(maxStyleID + 1);
    }

    for (auto& [styleIDStr, styleJson] : stylesJson.items()) {
        int styleID = std::stoi(styleIDStr);
        StyleData &newStyle = (*pOutputVector)[styleID];

        newStyle.id = styleID;
        newStyle.parentStyleID = styleJson["ParentStyleID"];
        newStyle.terminateState = 0;

        if (styleJson.contains("StyleData")) {
            if (styleJson["StyleData"].contains("State") &&
                styleJson["StyleData"]["State"].contains("TerminateState")) {
                newStyle.terminateState = styleJson["StyleData"]["State"]["TerminateState"];
            }

            if (styleJson["StyleData"].contains("Action")) {
                nlohmann::json &actionJson = styleJson["StyleData"]["Action"];
                if (actionJson.contains("Start")) {
                    newStyle.hasStartAction = true;
                    newStyle.startActionID = actionJson["Start"]["Action"];
                    newStyle.startActionStyle = actionJson["Start"]["Style"];
                }
                if (actionJson.contains("Exit")) {
                    newStyle.hasExitAction = true;
                    newStyle.exitActionID = actionJson["Exit"]["Action"];
                    newStyle.exitActionStyle = actionJson["Exit"]["Style"];
                }
            }
            if (styleJson["StyleData"].contains("Basic")) {
                nlohmann::json &basicJson = styleJson["StyleData"]["Basic"];
                newStyle.attackScale = basicJson.value("OffensiveScale", 100);
                newStyle.defenseScale = basicJson.value("DefensiveScale", 100);
                newStyle.gaugeGainRatio = basicJson.value("GaugeGainRatio", 100);
            }
        }
    }
}

void loadHitEntry(nlohmann::json* pHitEntryJson, HitEntry* pEntry)
{
    pEntry->comboAdd = (*pHitEntryJson)["ComboAdd"];
    pEntry->juggleFirst = (*pHitEntryJson)["Juggle1st"];
    pEntry->juggleAdd = (*pHitEntryJson)["JuggleAdd"];
    pEntry->juggleLimit = (*pHitEntryJson)["JuggleLimit"];
    pEntry->hitStun = (*pHitEntryJson)["HitStun"];
    pEntry->moveDestX = (*pHitEntryJson)["MoveDest"]["x"];
    pEntry->moveDestY = (*pHitEntryJson)["MoveDest"]["y"];
    pEntry->moveTime = (*pHitEntryJson)["MoveTime"];
    pEntry->curveTargetID = (*pHitEntryJson)["CurveTgtID"];
    pEntry->curveOwnID = (*pHitEntryJson)["CurveOwnID"];
    pEntry->dmgValue = (*pHitEntryJson)["DmgValue"];
    pEntry->focusGainOwn = (*pHitEntryJson)["FocusOwn"];
    pEntry->focusGainTarget = (*pHitEntryJson)["FocusTgt"];
    pEntry->superGainOwn = (*pHitEntryJson)["SuperOwn"];
    pEntry->superGainTarget = (*pHitEntryJson)["SuperTgt"];
    pEntry->dmgType = (*pHitEntryJson)["DmgType"];
    pEntry->dmgKind = (*pHitEntryJson)["DmgKind"];
    pEntry->dmgPower = (*pHitEntryJson)["DmgPower"];
    pEntry->moveType = (*pHitEntryJson)["MoveType"];
    pEntry->floorTime = (*pHitEntryJson)["FloorTime"];
    pEntry->downTime = (*pHitEntryJson)["DownTime"];
    pEntry->boundDest = (*pHitEntryJson)["BoundDest"];
    pEntry->throwRelease = (*pHitEntryJson)["ThrowRelease"];
    pEntry->jimenBound = (*pHitEntryJson)["_jimen_bound"];
    pEntry->kabeBound = (*pHitEntryJson)["_kabe_bound"];
    pEntry->kabeTataki = (*pHitEntryJson)["_kabe_tataki"];
    pEntry->bombBurst = pHitEntryJson->value("_bomb_burst", false);
    pEntry->attr0 = (*pHitEntryJson)["Attr0"];
    pEntry->attr1 = (*pHitEntryJson)["Attr1"];
    pEntry->attr2 = (*pHitEntryJson)["Attr2"];
    pEntry->attr3 = (*pHitEntryJson)["Attr3"];
    pEntry->ext0 = (*pHitEntryJson)["Ext0"];
    pEntry->hitStopOwner = (*pHitEntryJson)["HitStopOwner"];
    pEntry->hitStopTarget = (*pHitEntryJson)["HitStopTarget"];
    pEntry->hitmark = (*pHitEntryJson)["Hitmark"];
    pEntry->floorDestX = (*pHitEntryJson)["FloorDest"]["x"];
    pEntry->floorDestY = (*pHitEntryJson)["FloorDest"]["y"];
    pEntry->wallTime = (*pHitEntryJson)["WallTime"];
    pEntry->wallStop = (*pHitEntryJson)["WallStop"];
    pEntry->wallDestX = (*pHitEntryJson)["WallDest"]["x"];
    pEntry->wallDestY = (*pHitEntryJson)["WallDest"]["y"];
}

void loadHits(nlohmann::json* pHitJson, std::vector<HitData>* pOutputVector)
{
    if (!pHitJson) {
        return;
    }

    for (auto& [hitID, hitData] : pHitJson->items()) {
        HitData newHit;
        newHit.id = std::stoi(hitID);

        if (hitData.contains("common")) {
            for (auto& [slotID, slotData] : hitData["common"].items()) {
                int slot = std::stoi(slotID);
                if (slot >= 0 && slot < 5) {
                    loadHitEntry(&slotData, &newHit.common[slot]);
                }
            }
        }

        if (hitData.contains("param")) {
            for (auto& [slotID, slotData] : hitData["param"].items()) {
                int slot = std::stoi(slotID);
                if (slot >= 0 && slot < 20) {
                    loadHitEntry(&slotData, &newHit.param[slot]);
                }
            }
        }

        pOutputVector->push_back(newHit);
    }
}

int countAtemis(nlohmann::json* pAtemiJson)
{
    if (!pAtemiJson) {
        return 0;
    }
    return pAtemiJson->size();
}

void loadAtemis(nlohmann::json* pAtemiJson, std::vector<AtemiData>* pOutputVector)
{
    if (!pAtemiJson) {
        return;
    }

    for (auto& [atemiID, atemiData] : pAtemiJson->items()) {
        AtemiData newAtemi;
        newAtemi.id = std::stoi(atemiID);
        newAtemi.targetStop = atemiData["TargetStop"];
        newAtemi.ownerStop = atemiData["OwnerStop"];
        newAtemi.targetStopShell = atemiData["TargetStopShell"];
        newAtemi.ownerStopShell = atemiData["OwnerStopShell"];
        newAtemi.resistLimit = atemiData["ResistLimit"];

        pOutputVector->push_back(newAtemi);
    }
}

void loadProjectileDatas(nlohmann::json* pMovesJson, std::map<int, ProjectileData>* pUniqueProjectiles)
{
    if (!pMovesJson) {
        return;
    }

    for (auto& [keyID, key] : pMovesJson->items()) {
        nlohmann::json *pFab = &key["fab"];

        if (pFab->contains("Projectile")) {
            int dataIndex = (*pFab)["Projectile"]["DataIndex"];

            if (pUniqueProjectiles->find(dataIndex) == pUniqueProjectiles->end()) {
                if (key.contains("pdata")) {
                    nlohmann::json *pProjData = &key["pdata"];
                    ProjectileData newProj;
                    newProj.id = dataIndex;
                    newProj.hitCount = (*pProjData)["HitCount"];
                    newProj.hitFlagToParent = (*pProjData)["_HitFlagToPlayer"];
                    newProj.hitStopToParent = (*pProjData)["_HitStopToPlayer"];
                    newProj.rangeB = (*pProjData)["RangeB"];
                    newProj.wallBoxForward = Fixed((*pProjData)["WallRangeF"].get<int>());
                    newProj.wallBoxBack = Fixed((*pProjData)["WallRangeB"].get<int>());
                    newProj.airborne = (*pProjData)["_AirStatus"];
                    newProj.flags = (*pProjData)["AttrX"];
                    newProj.category = (*pProjData)["Category"];
                    newProj.clashPriority = (*pProjData)["ShotLevel"];
                    newProj.noPush = (*pProjData)["_NoPush"];
                    newProj.lifeTime = (*pProjData)["LifeTime"];

                    (*pUniqueProjectiles)[dataIndex] = newProj;
                }
            }
        }
    }
}

void loadActionsFromMoves(nlohmann::json* pMovesJson, CharacterData* pRet, std::map<std::pair<int, int>, Rect*>& rectsByIDs, std::map<int, AtemiData*>& atemiByID, std::map<int, HitData*>& hitByID, std::map<int, TriggerGroup*>& triggerGroupByID)
{
    if (!pMovesJson) {
        return;
    }

    for (auto& [keyID, key] : pMovesJson->items()) {
        Action newAction;

        nlohmann::json *pFab = &key["fab"];

        newAction.actionID = (*pFab)["ActionID"];
        newAction.styleID = key.value("_PL_StyleID", 0);
        newAction.name = keyID;

        newAction.activeFrame = (*pFab)["ActionFrame"]["MainFrame"];
        newAction.recoveryStartFrame = (*pFab)["ActionFrame"]["FollowFrame"];
        newAction.recoveryEndFrame = (*pFab)["ActionFrame"]["MarginFrame"];
        newAction.actionFlags = (*pFab)["Category"]["Flags"];
        newAction.actionFrameDuration = (*pFab)["Frame"];
        newAction.loopPoint = (*pFab)["State"]["EndStateParam"];
        // if (newAction.loopPoint == -1) {
        //     newAction.loopPoint = 0;
        // }
        newAction.loopCount = (*pFab)["State"]["LoopCount"];
        newAction.startScale = (*pFab)["Combo"]["_StartScaling"];
        if (newAction.startScale == -1) {
            newAction.startScale = 0;
        }
        newAction.comboScale = (*pFab)["Combo"]["ComboScaling"];
        if (newAction.comboScale == -1) {
            newAction.comboScale = 10;
        }
        newAction.instantScale = (*pFab)["Combo"]["InstScaling"];
        if (newAction.instantScale == -1) {
            newAction.instantScale = 0;
        }

        if (pFab->contains("Projectile")) {
            int dataIndex = (*pFab)["Projectile"]["DataIndex"];
            for (auto& projData : pRet->projectileDatas) {
                if (projData.id == dataIndex) {
                    newAction.pProjectileData = &projData;
                    break;
                }
            }
        }

        if (pFab->contains("Inherit")) {
            nlohmann::json *pInherit = &(*pFab)["Inherit"];
            newAction.inheritKindFlag = (*pInherit)["KindFlag"];
            newAction.inheritHitID = (*pInherit)["_HitID"];
            if (pInherit->contains("Accelaleration")) {
                newAction.inheritAccelX = Fixed((*pInherit)["Accelaleration"]["x"].get<double>());
                newAction.inheritAccelY = Fixed((*pInherit)["Accelaleration"]["y"].get<double>());
            }
            if (pInherit->contains("Velocity")) {
                newAction.inheritVelX = Fixed((*pInherit)["Velocity"]["x"].get<double>());
                newAction.inheritVelY = Fixed((*pInherit)["Velocity"]["y"].get<double>());
            }
        }

        bool exists = false;
        for (const auto& existingAction : pRet->actions) {
            if (existingAction.actionID == newAction.actionID && existingAction.styleID == newAction.styleID) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        if (key.contains("DamageCollisionKey")) {
            newAction.hurtBoxKeys.reserve(key["DamageCollisionKey"].size() - 1);
            loadHurtBoxKeys(&key["DamageCollisionKey"], &newAction.hurtBoxKeys, rectsByIDs, atemiByID);
        }

        if (key.contains("PushCollisionKey")) {
            newAction.pushBoxKeys.reserve(key["PushCollisionKey"].size() - 1);
            loadPushBoxKeys(&key["PushCollisionKey"], &newAction.pushBoxKeys, rectsByIDs);
        }

        size_t hitBoxKeyCount = 0;
        if (key.contains("AttackCollisionKey")) {
            hitBoxKeyCount += key["AttackCollisionKey"].size() - 1;
        }
        if (key.contains("OtherCollisionKey")) {
            hitBoxKeyCount += key["OtherCollisionKey"].size() - 1;
        }
        newAction.hitBoxKeys.reserve(hitBoxKeyCount);

        if (key.contains("AttackCollisionKey")) {
            loadHitBoxKeys(&key["AttackCollisionKey"], &newAction.hitBoxKeys, rectsByIDs, false, hitByID);
        }
        if (key.contains("OtherCollisionKey")) {
            loadHitBoxKeys(&key["OtherCollisionKey"], &newAction.hitBoxKeys, rectsByIDs, true, hitByID);
        }

        size_t steerKeyCount = 0;
        if (key.contains("SteerKey")) {
            steerKeyCount += key["SteerKey"].size() - 1;
        }
        if (key.contains("DriveSteerKey")) {
            steerKeyCount += key["DriveSteerKey"].size() - 1;
        }
        newAction.steerKeys.reserve(steerKeyCount);

        if (key.contains("SteerKey")) {
            loadSteerKeys(&key["SteerKey"], &newAction.steerKeys, false);
        }
        if (key.contains("DriveSteerKey")) {
            loadSteerKeys(&key["DriveSteerKey"], &newAction.steerKeys, true);
        }

        if (key.contains("PlaceKey")) {
            newAction.placeKeys.reserve(key["PlaceKey"].size() - 1);
            loadPlaceKeys(&key["PlaceKey"], &newAction.placeKeys);
        }

        size_t switchKeyCount = 0;
        if (key.contains("SwitchKey")) {
            switchKeyCount += key["SwitchKey"].size() - 1;
        }
        if (key.contains("ExtSwitchKey")) {
            switchKeyCount += key["ExtSwitchKey"].size() - 1;
        }
        newAction.switchKeys.reserve(switchKeyCount);

        if (key.contains("SwitchKey")) {
            loadSwitchKeys(&key["SwitchKey"], &newAction.switchKeys);
        }
        if (key.contains("ExtSwitchKey")) {
            loadSwitchKeys(&key["ExtSwitchKey"], &newAction.switchKeys);
        }

        if (key.contains("EventKey")) {
            newAction.eventKeys.reserve(key["EventKey"].size() - 1);
            loadEventKeys(&key["EventKey"], &newAction.eventKeys);
        }

        if (key.contains("WorldKey")) {
            newAction.worldKeys.reserve(key["WorldKey"].size() - 1);
            loadWorldKeys(&key["WorldKey"], &newAction.worldKeys);
        }

        if (key.contains("LockKey")) {
            newAction.lockKeys.reserve(key["LockKey"].size() - 1);
            loadLockKeys(&key["LockKey"], &newAction.lockKeys, hitByID);
        }

        if (key.contains("BranchKey")) {
            newAction.branchKeys.reserve(key["BranchKey"].size() - 1);
            loadBranchKeys(&key["BranchKey"], &newAction.branchKeys);
        }

        if (key.contains("ShotKey")) {
            newAction.shotKeys.reserve(key["ShotKey"].size() - 1);
            loadShotKeys(&key["ShotKey"], &newAction.shotKeys);
        }

        if (key.contains("TriggerKey")) {
            newAction.triggerKeys.reserve(key["TriggerKey"].size() - 1);
            loadTriggerKeys(&key["TriggerKey"], &newAction.triggerKeys, triggerGroupByID);
        }

        if (key.contains("StatusKey")) {
            newAction.statusKeys.reserve(key["StatusKey"].size() - 1);
            loadStatusKeys(&key["StatusKey"], &newAction.statusKeys);
        }

        pRet->actions.push_back(newAction);
    }
}

CharacterData *loadCharacter(std::string charName, int charVersion)
{
    nlohmann::json *pMovesDictJson = loadCharFile(charName, charVersion, "moves");
    nlohmann::json *pRectsJson = loadCharFile(charName, charVersion, "rects");
    nlohmann::json *pTriggerGroupsJson = loadCharFile(charName, charVersion, "trigger_groups");
    nlohmann::json *pTriggersJson = loadCharFile(charName, charVersion, "triggers");
    nlohmann::json *pCommandsJson = loadCharFile(charName, charVersion, "commands");
    nlohmann::json *pChargeJson = loadCharFile(charName, charVersion, "charge");
    nlohmann::json *pHitJson = loadCharFile(charName, charVersion, "hit");
    nlohmann::json *pAtemiJson = loadCharFile(charName, charVersion, "atemi");
    nlohmann::json *pCharInfoJson = loadCharFile(charName, charVersion, "charinfo");

    nlohmann::json *pCommonMovesJson = loadCharFile("common", charVersion, "moves");
    nlohmann::json *pCommonRectsJson = loadCharFile("common", charVersion, "rects");
    nlohmann::json *pCommonAtemiJson = loadCharFile("common", charVersion, "atemi");

    CharacterData *pRet = new CharacterData;

    pRet->charName = charName;
    pRet->charVersion = charVersion;
    pRet->charID = getCharIDFromName(charName.c_str());

    if (pCharInfoJson) {
        pRet->vitality = (*pCharInfoJson)["PlData"]["Vitality"];
        pRet->gauge = (*pCharInfoJson)["PlData"]["Gauge"].get<int>();
    }

    loadStyles(pCharInfoJson, &pRet->styles);

    loadCharges(pChargeJson, pRet);
    loadCommands(pCommandsJson, pRet);
    loadTriggers(pTriggersJson, pRet);
    loadTriggerGroups(pTriggerGroupsJson, pRet);

    for (auto& triggerGroup : pRet->triggerGroups) {
        pRet->triggerGroupByID[triggerGroup.id] = &triggerGroup;
    }

    pRet->rects.reserve(countRects(pCommonRectsJson) + countRects(pRectsJson));
    loadRects(pCommonRectsJson, &pRet->rects);
    loadRects(pRectsJson, &pRet->rects);

    for (auto& rect : pRet->rects) {
        pRet->rectsByIDs[std::make_pair(rect.listID, rect.id)] = &rect;
    }

    // for UI dropdown selector
    pRet->vecMoveList.push_back(strdup("1 no action (obey input)"));
    if (pTriggerGroupsJson) {
        auto triggerGroupString = to_string_leading_zeroes(0, 3);
        for (auto& [keyID, key] : (*pTriggerGroupsJson)[triggerGroupString].items())
        {
            std::string actionString = key;
            pRet->vecMoveList.push_back(strdup(actionString.c_str()));
        }
    }


    std::map<int, ProjectileData> uniqueProjectiles;
    loadProjectileDatas(pMovesDictJson, &uniqueProjectiles);
    loadProjectileDatas(pCommonMovesJson, &uniqueProjectiles);

    pRet->projectileDatas.reserve(uniqueProjectiles.size());
    for (auto& [id, projData] : uniqueProjectiles) {
        pRet->projectileDatas.push_back(projData);
    }

    pRet->atemis.reserve(countAtemis(pAtemiJson) + countAtemis(pCommonAtemiJson));
    loadAtemis(pAtemiJson, &pRet->atemis);
    loadAtemis(pCommonAtemiJson, &pRet->atemis);

    for (auto& atemi : pRet->atemis) {
        pRet->atemiByID[atemi.id] = &atemi;
    }

    if (pHitJson) {
        pRet->hits.reserve(pHitJson->size());
    }
    loadHits(pHitJson, &pRet->hits);

    for (auto& hit : pRet->hits) {
        pRet->hitByID[hit.id] = &hit;
    }


    loadActionsFromMoves(pMovesDictJson, pRet, pRet->rectsByIDs, pRet->atemiByID, pRet->hitByID, pRet->triggerGroupByID);
    loadActionsFromMoves(pCommonMovesJson, pRet, pRet->rectsByIDs, pRet->atemiByID, pRet->hitByID, pRet->triggerGroupByID);

    for (auto& action : pRet->actions) {
        pRet->actionsByID[ActionRef(action.actionID, action.styleID)] = &action;
    }

    // delete pMovesDictJson;
    // delete pRectsJson;
    // delete pTriggerGroupsJson;
    // delete pTriggersJson;
    // delete pCommandsJson;
    // delete pChargeJson;
    // delete pHitJson;
    // delete pAtemiJson;
    // delete pCharInfoJson;
    // delete pCommonMovesJson;
    // delete pCommonRectsJson;
    // delete pCommonAtemiJson;

    return pRet;
}