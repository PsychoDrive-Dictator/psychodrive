#include "chara.hpp"
#include "main.hpp"
#include <string>
#include <cstdlib>

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

    CharacterData *pRet = new CharacterData;

    pRet->charName = charName;
    pRet->charVersion = charVersion;

    size_t chargeCount = 0;
    size_t commandCount = 0;
    size_t triggerCount = 0;
    size_t triggerGroupCount = pTriggerGroupsJson ? pTriggerGroupsJson->size() : 0;

    if (pChargeJson) {
        chargeCount = pChargeJson->size();
    }

    if (pCommandsJson) {
        commandCount = pCommandsJson->size();
    }

    if (pTriggersJson) {
        for (auto& [actionIDStr, actionGroup] : pTriggersJson->items()) {
            triggerCount += actionGroup.size();
        }
    }

    pRet->charges.reserve(chargeCount);
    pRet->commands.reserve(commandCount);
    pRet->triggers.reserve(triggerCount);
    pRet->triggerGroups.reserve(triggerGroupCount);

    if (pChargeJson) {
        for (auto& [keyID, key] : pChargeJson->items()) {
            Charge charge;

            // compat with old charge format
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

    if (pCommandsJson) {
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

    if (pTriggersJson) {
        for (auto& [actionIDStr, actionGroup] : pTriggersJson->items()) {
            for (auto& [triggerIDStr, triggerData] : actionGroup.items()) {
                Trigger trigger;

                trigger.id = std::atoi(triggerIDStr.c_str());
                trigger.actionID = triggerData["action_id"];

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

    if (pTriggerGroupsJson) {
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

    for (auto& triggerGroup : pRet->triggerGroups) {
        pRet->triggerGroupByID[triggerGroup.id] = &triggerGroup;
    }

    size_t rectsCount = 0;

    if (pRectsJson) {
        for (auto& [rectsListIDStr, rectsList] : pRectsJson->items()) {
            rectsCount = rectsList.size();
        }
    }

    pRet->rects.reserve(rectsCount);

    if (pRectsJson) {
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

                pRet->rects.push_back(newRect);
            }
        }
    }

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

    return pRet;
}