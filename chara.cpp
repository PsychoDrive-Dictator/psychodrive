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

            std::vector<CommandInput> vecInputs;
            vecInputs.reserve(inputNum);

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

                vecInputs.push_back(input);
            }

            command.variants.push_back(vecInputs);
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

void loadActionsFromMoves(nlohmann::json* pMovesJson, CharacterData* pRet, std::map<std::pair<int, int>, Rect*>& rectsByIDs)
{
    if (!pMovesJson) {
        return;
    }

    for (auto& [keyID, key] : pMovesJson->items()) {
        Action newAction;

        newAction.actionID = key["fab"]["ActionID"];
        newAction.styleID = key.value("_PL_StyleID", 0);
        newAction.name = keyID;

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

        int keyCount = 0;

        if (key.contains("DamageCollisionKey")) {
            keyCount = key["DamageCollisionKey"].size() - 1;
        }

        newAction.hurtBoxKeys.reserve(keyCount);

        if (key.contains("DamageCollisionKey")) {
            for (auto& [hurtBoxID, hurtBox] : key["DamageCollisionKey"].items()) {
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
                newKey.armorID = hurtBox["AtemiDataListIndex"];
                newKey.isAtemi = hurtBox["_isAtm"];
                newKey.immunity = hurtBox["Immune"];
                newKey.flags = hurtBox["TypeFlag"];

                int magicHurtBoxID = 8; // i hate you magic array of boxes
                int magicThrowBoxID = 7;

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

                newAction.hurtBoxKeys.push_back(newKey);
            }
        }

        if (key.contains("PushCollisionKey")) {
            for (auto& [pushBoxID, pushBox] : key["PushCollisionKey"].items()) {
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
                    newAction.pushBoxKeys.push_back(newKey);
                }
            }
        }

        if (key.contains("AttackCollisionKey") || key.contains("OtherCollisionKey")) {
            for (auto& keyName : {"AttackCollisionKey", "OtherCollisionKey"}) {
                if (!key.contains(keyName)) {
                    continue;
                }

                bool isOther = strcmp(keyName, "OtherCollisionKey") == 0;

                for (auto& [hitBoxID, hitBox] : key[keyName].items()) {
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
                    newKey.hitEntryID = hitBox["AttackDataListIndex"];

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

                    if (type == domain) {
                        newAction.hitBoxKeys.push_back(newKey);
                    } else {
                        newKey.rects.reserve(hitBox["BoxList"].size());
                        for (auto& [boxNumber, boxID] : hitBox["BoxList"].items()) {
                            auto it = rectsByIDs.find(std::make_pair(rectListID, boxID));
                            if (it != rectsByIDs.end()) {
                                newKey.rects.push_back(it->second);
                            }
                        }
                        if (!newKey.rects.empty() || type == proximity_guard) {
                            newAction.hitBoxKeys.push_back(newKey);
                        }
                    }
                }
            }
        }

        pRet->actions.push_back(newAction);
    }
}

CharacterData *loadCharacter(std::string charName, int charVersion)
{
    nlohmann::json *pMovesDictJson = loadCharFile(charName, charVersion, "moves");
    nlohmann::json *pRectsJson = loadCharFile(charName, charVersion, "rects");
    nlohmann::json *pNamesJson = loadCharFile(charName, charVersion, "names");
    nlohmann::json *pTriggerGroupsJson = loadCharFile(charName, charVersion, "trigger_groups");
    nlohmann::json *pTriggersJson = loadCharFile(charName, charVersion, "triggers");
    nlohmann::json *pCommandsJson = loadCharFile(charName, charVersion, "commands");
    nlohmann::json *pChargeJson = loadCharFile(charName, charVersion, "charge");
    nlohmann::json *pHitJson = loadCharFile(charName, charVersion, "hit");
    nlohmann::json *pAtemiJson = loadCharFile(charName, charVersion, "atemi");
    nlohmann::json *pCharInfoJson = loadCharFile(charName, charVersion, "charinfo");

    nlohmann::json *pCommonMovesJson = loadCharFile("common", charVersion, "moves");
    nlohmann::json *pCommonRectsJson = loadCharFile("common", charVersion, "rects");

    CharacterData *pRet = new CharacterData;

    pRet->charName = charName;
    pRet->charVersion = charVersion;

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

    if (pMovesDictJson) {
        for (auto& [keyID, key] : pMovesDictJson->items())
        {
            int actionID = key["fab"]["ActionID"];
            int styleID = 0;
            if (key.contains("_PL_StyleID")) {
                styleID = key["_PL_StyleID"];
            }
            auto mapIndex = std::make_pair(actionID, styleID);
            pRet->mapMoveStyle[mapIndex] = std::make_pair(keyID, false);
        }
    }

    if (pCommonMovesJson) {
        for (auto& [keyID, key] : pCommonMovesJson->items())
        {
            int actionID = key["fab"]["ActionID"];
            int styleID = 0;
            if (key.contains("_PL_StyleID")) {
                styleID = key["_PL_StyleID"];
            }
            auto mapIndex = std::make_pair(actionID, styleID);
            if (pRet->mapMoveStyle.find(mapIndex) == pRet->mapMoveStyle.end()) {
                pRet->mapMoveStyle[mapIndex] = std::make_pair(keyID, true);
            }
        }
    }

    pRet->actions.reserve(pRet->mapMoveStyle.size());

    loadActionsFromMoves(pMovesDictJson, pRet, pRet->rectsByIDs);
    loadActionsFromMoves(pCommonMovesJson, pRet, pRet->rectsByIDs);

    for (auto& action : pRet->actions) {
        pRet->actionsByID[std::make_pair(action.actionID, action.styleID)] = &action;
    }

    return pRet;
}