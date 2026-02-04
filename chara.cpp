#include "chara.hpp"
#include "main.hpp"
#include <string>
#include <cstdlib>
#include <fstream>

#include "zip.h"

std::unordered_map<std::string, struct zip_t*> charZipFiles;
std::unordered_map<std::string, nlohmann::json> mapCharFileLoader;

bool charFileExists(const std::string &path, const std::string &charName, const std::string &charFileName)
{
    // check loose files first
    std::string filePath = path + charFileName;
    if (std::filesystem::exists(filePath))
        return true;

    // check in possible char zip next

    // try loading char zip if we haven't tried yet
    if (charZipFiles.find(charName) == charZipFiles.end()) {
        std::string zipFileName = path + charName + ".zip";
        struct zip_t *zip = zip_open(zipFileName.c_str(), 0, 'r');
        charZipFiles[charName] = zip;
    }

    struct zip_t *zip = charZipFiles[charName];
    if (zip) {
        // check for file in char zip
        int err = zip_entry_open(zip, charFileName.c_str());
        if (err == 0) {
            zip_entry_close(zip);
            return true;
        }
    }

    return false;
}

void closeZipFiles()
{
    for (auto zip : charZipFiles) {
        zip_close(zip.second);
    }
    charZipFiles.clear();
}

int findCharVersionSlot(int version)
{
    int versionSlot = charVersionCount - 1;
    while (versionSlot >= 0) {
        if (atoi(charVersions[versionSlot]) == version) {
            break;
        }
        versionSlot--;
    }

    return versionSlot;
}

nlohmann::json *loadCharFile(const std::string &charName, int version, const std::string &jsonName)
{
    std::string charPath = "data/chars/" + charName + "/";
    std::string charFileName;
    bool foundFile = false;

    // find initial version slot for passed version number
    int versionSlot = findCharVersionSlot(version);
    if (versionSlot < 0) {
        return nullptr;
    }
    while (versionSlot >= 0) {
        charFileName = charName + std::to_string(atoi(charVersions[versionSlot])) + "_" + jsonName + ".json";
        if (charFileExists(charPath, charName, charFileName)) {
            foundFile = true;
            break;
        }
        versionSlot--;
    }
    if (!foundFile) {
        return nullptr;
    }

    if (mapCharFileLoader.find(charFileName) == mapCharFileLoader.end()) {
        bool loaded = false;
        // try loading from loose file first
        std::string looseFileName = charPath + charFileName;
        if (std::filesystem::exists(looseFileName)) {
            mapCharFileLoader[charFileName] = parse_json_file(looseFileName);
            loaded = true;
        } else {
            // now zip
            struct zip_t *zip = charZipFiles[charName];
            if (zip) {
                size_t bufSize;
                int err = zip_entry_open(zip, charFileName.c_str());
                if (err == 0) {
                    std::string destString;
                    bufSize = zip_entry_size(zip);
                    destString.resize(bufSize);
                    zip_entry_noallocread(zip, (void*)destString.c_str(), bufSize);
                    mapCharFileLoader[charFileName] = nlohmann::json::parse(destString);
                    loaded = true;
                    zip_entry_close(zip);
                }
            }
        }
        if (loaded == false) {
            // really shouldn't happen since charFileExists said there was something
            mapCharFileLoader[charFileName] = nullptr;
        }
    }

    return &mapCharFileLoader[charFileName];
}

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
                input.failKeyFlags = (*pInputNorm)["ignore_key_flags"];
                input.failCondFlags = (*pInputNorm)["ignore_key_cond_check_flags"];

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
            trigger.advanceCombo = !triggerData["_NotResetComboID"].get<bool>();
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
            trigger.comboSuperScaling = triggerData["combo_sp_gain"];

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

        int validStyles = hitBox.value("_ValidStyle", 0);
        if (validStyles != 0) {
            newKey.hasValidStyle = true;
            newKey.validStyle = validStyles;
        }

        hitBoxType type = hit;
        int collisionType = hitBox["CollisionType"];
        int rectListID = collisionType;

        if (isOther) {
            if (collisionType == 7) {
                type = domain;
            } else if (collisionType == 8) {
                type = screen_freeze;
            } else if (collisionType == 9) {
                type = clash;
            } else if (collisionType == 10) {
                type = destroy_projectile;
            } else if (collisionType == 11) {
                type = direct_damage;
            } else {
                continue;
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
            } else {
                continue;
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
            newKey.flags = (hitBoxFlags)(newKey.flags | only_hits_in_combo);
        }
        if (flags & 0x100000) {
            newKey.flags = (hitBoxFlags)(newKey.flags | multi_counter);
        }
        if (flags & 0x800000) {
            newKey.flags = (hitBoxFlags)(newKey.flags | fixed_position);
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
            if (!newKey.rects.empty()) {
                pOutputVector->push_back(newKey);
            }
        }
    }
}

void loadUniqueBoxKeys(nlohmann::json* pHitBoxJson, std::vector<UniqueBoxKey>* pOutputVector, std::map<std::pair<int, int>, Rect*>& rectsByIDs)
{
    if (!pHitBoxJson) {
        return;
    }

    for (auto& [hitBoxID, hitBox] : pHitBoxJson->items()) {
        if (!hitBox.contains("_StartFrame")) {
            continue;
        }
        UniqueBoxKey newKey;
        newKey.startFrame = hitBox["_StartFrame"];
        newKey.endFrame = hitBox["_EndFrame"];
        newKey.condition = hitBox["Condition"];
        newKey.offsetX = Fixed(0);
        newKey.offsetY = Fixed(0);
        if (hitBox.contains("RootOffset")) {
            newKey.offsetX = Fixed(hitBox["RootOffset"].value("X", 0));
            newKey.offsetY = Fixed(hitBox["RootOffset"].value("Y", 0));
        }

        int rectListID = 6;

        newKey.checkMask = hitBox["CheckFlags"];
        newKey.uniquePitcher = hitBox["CheckType"] == 0;
        newKey.applyOpToTarget = hitBox["PropHolder"] == 1;

        newKey.ops.reserve(hitBox["Datas"].size());
        for (auto& [dataID, dataOp] : hitBox["Datas"].items()) {
            UniqueBoxOp newOp;
            newOp.op = dataOp["OpeType"];
            newOp.opParam0 = dataOp["Param00"];
            newOp.opParam1 = dataOp["Param01"];
            newOp.opParam2 = dataOp["Param02"];
            newOp.opParam3 = dataOp["Param03"];
            newOp.opParam4 = dataOp["Param04"];
            newKey.ops.push_back(newOp);
        }

        newKey.rects.reserve(hitBox["BoxList"].size());
        for (auto& [boxNumber, boxID] : hitBox["BoxList"].items()) {
            auto it = rectsByIDs.find(std::make_pair(rectListID, boxID));
            if (it != rectsByIDs.end()) {
                newKey.rects.push_back(it->second);
            }
        }
        if (!newKey.rects.empty()) {
            pOutputVector->push_back(newKey);
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

void loadPlaceKeys(nlohmann::json* pPlaceJson, std::vector<PlaceKey>* pOutputVector, bool isBG)
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
        newKey.optionFlag = placeKey.value("OptionFlag", 0);
        newKey.ratio = Fixed(placeKey.value<double>("Ratio", 1.0));
        newKey.axis = placeKey.value("Axis", 0);
        newKey.bgOnly = isBG;

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
    pEntry->parryGain = (*pHitEntryJson)["DriveNorm"];
    pEntry->perfectParryGain = (*pHitEntryJson)["DriveJust"];
    if (pEntry->parryGain == 65535) {
        pEntry->parryGain = 10000;
    }
    if (pEntry->perfectParryGain == 65535) {
        pEntry->perfectParryGain = 10000;
    }
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
        newAtemi.targetStopAdd = atemiData["TargetStopAdd"];
        newAtemi.ownerStopAdd = atemiData["OwnerStopAdd"];
        newAtemi.resistLimit = atemiData["ResistLimit"];
        newAtemi.damageRatio = atemiData["DamageRatio"];
        newAtemi.recoverRatio = atemiData["RecoverRatio"];
        newAtemi.superRatio = atemiData["GaugeRatio"];

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
                    newProj.extraHitStop = (*pProjData)["HitAfterFrame"];
                    // newProj.extraHitStop -= 2;
                    // if (newProj.extraHitStop < 0) {
                    //     newProj.extraHitStop = 0;
                    // }
                    newProj.hitFlagToParent = (*pProjData)["_HitFlagToPlayer"];
                    newProj.hitStopToParent = (*pProjData)["_HitStopToPlayer"];
                    newProj.rangeB = (*pProjData)["RangeB"];
                    newProj.wallBoxForward = Fixed((*pProjData)["WallRangeF"].get<int>());
                    newProj.wallBoxBack = Fixed((*pProjData)["WallRangeB"].get<int>());
                    newProj.airborne = (*pProjData)["_AirStatus"];
                    newProj.flags = (*pProjData)["Attr0"];
                    newProj.flagsExt = (*pProjData)["AttrX"];
                    newProj.category = (*pProjData)["Category"];
                    newProj.clashPriority = (*pProjData)["ShotLevel"];
                    newProj.noPush = (*pProjData)["_NoPush"];
                    newProj.lifeTime = (*pProjData)["LifeTime"];
                    newProj.hitSpan = (*pProjData)["HitSpan"];

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

        if (key.contains("OtherCollisionKey")) {
            loadHitBoxKeys(&key["OtherCollisionKey"], &newAction.hitBoxKeys, rectsByIDs, true, hitByID);
        }
        if (key.contains("AttackCollisionKey")) {
            loadHitBoxKeys(&key["AttackCollisionKey"], &newAction.hitBoxKeys, rectsByIDs, false, hitByID);
        }

        if (key.contains("UniqueCollisionKey")) {
            newAction.uniqueBoxKeys.reserve(key["UniqueCollisionKey"].size() - 1);
            loadUniqueBoxKeys(&key["UniqueCollisionKey"], &newAction.uniqueBoxKeys, rectsByIDs);
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

        size_t placeKeyCount = 0;
        if (key.contains("PlaceKey")) {
            placeKeyCount += key["PlaceKey"].size() - 1;
        }
        if (key.contains("BGPlaceKey")) {
            placeKeyCount += key["BGPlaceKey"].size() - 1;
        }
        newAction.placeKeys.reserve(placeKeyCount);

        if (key.contains("PlaceKey")) {
            loadPlaceKeys(&key["PlaceKey"], &newAction.placeKeys, false);
        }
        if (key.contains("BGPlaceKey")) {
            loadPlaceKeys(&key["BGPlaceKey"], &newAction.placeKeys, true);
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
    int versionSlot = findCharVersionSlot(charVersion);
    while (versionSlot >= 0) {
        int version = atoi(charVersions[versionSlot]);
        std::string cookedPath = "data/cooked/" + charName + std::to_string(version) + ".bin";
        if (std::filesystem::exists(cookedPath)) {
            return loadCookedCharacter(cookedPath, charVersion);
        }
        versionSlot--;
    }

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

    mapCharFileLoader.clear();
    closeZipFiles();

    return pRet;
}

static void writeU32(std::ofstream &f, uint32_t v) { f.write((char*)&v, 4); }
static void writeU64(std::ofstream &f, uint64_t v) { f.write((char*)&v, 8); }
static void writeI32(std::ofstream &f, int32_t v) { f.write((char*)&v, 4); }
static void writeI64(std::ofstream &f, int64_t v) { f.write((char*)&v, 8); }
static void writeBool(std::ofstream &f, bool v) { uint8_t b = v ? 1 : 0; f.write((char*)&b, 1); }
static void writeFixed(std::ofstream &f, Fixed v) { writeI64(f, v.data); }
static void writeString(std::ofstream &f, const std::string &s) {
    writeU32(f, s.size());
    f.write(s.data(), s.size());
}

static uint32_t readU32(std::ifstream &f) { uint32_t v; f.read((char*)&v, 4); return v; }
static uint64_t readU64(std::ifstream &f) { uint64_t v; f.read((char*)&v, 8); return v; }
static int32_t readI32(std::ifstream &f) { int32_t v; f.read((char*)&v, 4); return v; }
static int64_t readI64(std::ifstream &f) { int64_t v; f.read((char*)&v, 8); return v; }
static bool readBool(std::ifstream &f) { uint8_t b; f.read((char*)&b, 1); return b != 0; }
static Fixed readFixed(std::ifstream &f) { Fixed v; v.data = readI64(f); return v; }
static std::string readString(std::ifstream &f) {
    uint32_t len = readU32(f);
    std::string s(len, '\0');
    f.read(s.data(), len);
    return s;
}

template<typename T>
static int32_t ptrToIndex(T* ptr, const std::vector<T>& vec) {
    if (!ptr) return -1;
    for (size_t i = 0; i < vec.size(); i++) {
        if (&vec[i] == ptr) return (int32_t)i;
    }
    return -1;
}

template<typename T>
static T* indexToPtr(int32_t idx, std::vector<T>& vec) {
    if (idx < 0 || idx >= (int32_t)vec.size()) return nullptr;
    return &vec[idx];
}

static int32_t rectPtrToIndex(Rect* ptr, const std::vector<Rect>& rects) {
    return ptrToIndex(ptr, rects);
}

static int32_t hitEntryPtrToIndex(HitEntry* ptr, const std::vector<HitData>& hits) {
    if (!ptr) return -1;
    for (size_t i = 0; i < hits.size(); i++) {
        for (int j = 0; j < 5; j++) {
            if (&hits[i].common[j] == ptr) return (int32_t)(i * 25 + j);
        }
        for (int j = 0; j < 20; j++) {
            if (&hits[i].param[j] == ptr) return (int32_t)(i * 25 + 5 + j);
        }
    }
    return -1;
}

static HitEntry* indexToHitEntryPtr(int32_t idx, std::vector<HitData>& hits) {
    if (idx < 0) return nullptr;
    int hitIdx = idx / 25;
    int slot = idx % 25;
    if (hitIdx >= (int)hits.size()) return nullptr;
    if (slot < 5) return &hits[hitIdx].common[slot];
    return &hits[hitIdx].param[slot - 5];
}

bool cookCharacter(CharacterData* pData, const std::string& path)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    writeString(f, pData->charName);
    writeI32(f, pData->charID);
    writeI32(f, pData->vitality);
    writeI32(f, pData->gauge);

    writeU32(f, pData->charges.size());
    for (auto& charge : pData->charges) {
        writeI32(f, charge.id);
        writeU32(f, charge.okKeyFlags);
        writeU32(f, charge.okCondFlags);
        writeI32(f, charge.chargeFrames);
        writeI32(f, charge.keepFrames);
    }

    writeU32(f, pData->commands.size());
    for (auto& cmd : pData->commands) {
        writeI32(f, cmd.id);
        writeU32(f, cmd.variants.size());
        for (auto& variant : cmd.variants) {
            writeI32(f, variant.totalMaxFrames);
            writeU32(f, variant.inputs.size());
            for (auto& input : variant.inputs) {
                writeI32(f, (int32_t)input.type);
                writeI32(f, input.numFrames);
                writeU32(f, input.okKeyFlags);
                writeU32(f, input.okCondFlags);
                writeU32(f, input.ngKeyFlags);
                writeU32(f, input.ngCondFlags);
                writeU32(f, input.failKeyFlags);
                writeU32(f, input.failCondFlags);
                writeI32(f, input.rotatePointsNeeded);
                writeI32(f, ptrToIndex(input.pCharge, pData->charges));
            }
        }
    }

    writeU32(f, pData->triggers.size());
    for (auto& trigger : pData->triggers) {
        writeI32(f, trigger.id);
        writeI32(f, trigger.actionID);
        writeI32(f, trigger.validStyles);
        writeI32(f, ptrToIndex(trigger.pCommandClassic, pData->commands));
        writeI32(f, trigger.okKeyFlags);
        writeI32(f, trigger.okCondFlags);
        writeI32(f, trigger.ngKeyFlags);
        writeI32(f, trigger.dcExcFlags);
        writeI32(f, trigger.dcIncFlags);
        writeI32(f, trigger.precedingTime);
        writeBool(f, trigger.useUniqueParam);
        writeBool(f, trigger.advanceCombo);
        writeI32(f, trigger.condParamID);
        writeI32(f, trigger.condParamOp);
        writeI32(f, trigger.condParamValue);
        writeI32(f, trigger.limitShotCount);
        writeI32(f, trigger.limitShotCategory);
        writeI32(f, trigger.airActionCountLimit);
        writeI32(f, trigger.vitalOp);
        writeI32(f, trigger.vitalRatio);
        writeI32(f, trigger.rangeCondition);
        writeFixed(f, trigger.rangeParam);
        writeI32(f, trigger.stateCondition);
        writeBool(f, trigger.needsFocus);
        writeI32(f, trigger.focusCost);
        writeBool(f, trigger.needsGauge);
        writeI32(f, trigger.gaugeCost);
        writeI32(f, trigger.comboInst);
        writeI32(f, trigger.comboSuperScaling);
        writeI64(f, trigger.flags);
    }

    writeU32(f, pData->triggerGroups.size());
    for (auto& tg : pData->triggerGroups) {
        writeI32(f, tg.id);
        writeU32(f, tg.entries.size());
        for (auto& entry : tg.entries) {
            writeI32(f, entry.actionID);
            writeI32(f, entry.triggerID);
            writeI32(f, ptrToIndex(entry.pTrigger, pData->triggers));
        }
    }

    writeU32(f, pData->rects.size());
    for (auto& rect : pData->rects) {
        writeI32(f, rect.listID);
        writeI32(f, rect.id);
        writeI32(f, rect.xOrig);
        writeI32(f, rect.yOrig);
        writeI32(f, rect.xRadius);
        writeI32(f, rect.yRadius);
    }

    writeU32(f, pData->projectileDatas.size());
    for (auto& proj : pData->projectileDatas) {
        writeI32(f, proj.id);
        writeI32(f, proj.hitCount);
        writeI32(f, proj.extraHitStop);
        writeBool(f, proj.hitFlagToParent);
        writeBool(f, proj.hitStopToParent);
        writeI32(f, proj.rangeB);
        writeFixed(f, proj.wallBoxForward);
        writeFixed(f, proj.wallBoxBack);
        writeBool(f, proj.airborne);
        writeI32(f, proj.flags);
        writeI32(f, proj.flagsExt);
        writeI32(f, proj.category);
        writeI32(f, proj.clashPriority);
        writeBool(f, proj.noPush);
        writeI32(f, proj.lifeTime);
        writeI32(f, proj.hitSpan);
    }

    writeU32(f, pData->atemis.size());
    for (auto& atemi : pData->atemis) {
        writeI32(f, atemi.id);
        writeI32(f, atemi.targetStop);
        writeI32(f, atemi.ownerStop);
        writeI32(f, atemi.targetStopShell);
        writeI32(f, atemi.ownerStopShell);
        writeI32(f, atemi.targetStopAdd);
        writeI32(f, atemi.ownerStopAdd);
        writeI32(f, atemi.resistLimit);
        writeI32(f, atemi.damageRatio);
        writeI32(f, atemi.recoverRatio);
        writeI32(f, atemi.superRatio);
    }

    auto writeHitEntry = [&](const HitEntry& e) {
        writeI32(f, e.comboAdd);
        writeI32(f, e.juggleFirst);
        writeI32(f, e.juggleAdd);
        writeI32(f, e.juggleLimit);
        writeI32(f, e.hitStun);
        writeI32(f, e.moveDestX);
        writeI32(f, e.moveDestY);
        writeI32(f, e.moveTime);
        writeI32(f, e.curveOwnID);
        writeI32(f, e.curveTargetID);
        writeI32(f, e.dmgValue);
        writeI32(f, e.focusGainOwn);
        writeI32(f, e.focusGainTarget);
        writeI32(f, e.superGainOwn);
        writeI32(f, e.superGainTarget);
        writeI32(f, e.parryGain);
        writeI32(f, e.perfectParryGain);
        writeI32(f, e.dmgType);
        writeI32(f, e.dmgKind);
        writeI32(f, e.dmgPower);
        writeI32(f, e.moveType);
        writeI32(f, e.floorTime);
        writeI32(f, e.downTime);
        writeI32(f, e.boundDest);
        writeI32(f, e.throwRelease);
        writeBool(f, e.jimenBound);
        writeBool(f, e.kabeBound);
        writeBool(f, e.kabeTataki);
        writeBool(f, e.bombBurst);
        writeI32(f, e.attr0);
        writeI32(f, e.attr1);
        writeI32(f, e.attr2);
        writeI32(f, e.attr3);
        writeI32(f, e.ext0);
        writeI32(f, e.hitStopOwner);
        writeI32(f, e.hitStopTarget);
        writeI32(f, e.hitmark);
        writeI32(f, e.floorDestX);
        writeI32(f, e.floorDestY);
        writeI32(f, e.wallTime);
        writeI32(f, e.wallStop);
        writeI32(f, e.wallDestX);
        writeI32(f, e.wallDestY);
    };

    writeU32(f, pData->hits.size());
    for (auto& hit : pData->hits) {
        writeI32(f, hit.id);
        for (int i = 0; i < 5; i++) writeHitEntry(hit.common[i]);
        for (int i = 0; i < 20; i++) writeHitEntry(hit.param[i]);
    }

    writeU32(f, pData->styles.size());
    for (auto& style : pData->styles) {
        writeI32(f, style.id);
        writeI32(f, style.parentStyleID);
        writeI64(f, style.terminateState);
        writeBool(f, style.hasStartAction);
        writeI32(f, style.startActionID);
        writeI32(f, style.startActionStyle);
        writeBool(f, style.hasExitAction);
        writeI32(f, style.exitActionID);
        writeI32(f, style.exitActionStyle);
        writeI32(f, style.attackScale);
        writeI32(f, style.defenseScale);
        writeI32(f, style.gaugeGainRatio);
    }

    auto writeBoxKeyBase = [&](const BoxKey& k) {
        writeI32(f, k.startFrame);
        writeI32(f, k.endFrame);
        writeI32(f, k.condition);
        writeFixed(f, k.offsetX);
        writeFixed(f, k.offsetY);
    };

    auto writeRectPtrVec = [&](const std::vector<Rect*>& rects) {
        writeU32(f, rects.size());
        for (auto* r : rects) writeI32(f, rectPtrToIndex(r, pData->rects));
    };

    writeU32(f, pData->actions.size());
    for (auto& action : pData->actions) {
        writeI32(f, action.actionID);
        writeI32(f, action.styleID);
        writeString(f, action.name);

        writeU32(f, action.hurtBoxKeys.size());
        for (auto& k : action.hurtBoxKeys) {
            writeBoxKeyBase(k);
            writeBool(f, k.isArmor);
            writeBool(f, k.isAtemi);
            writeI32(f, ptrToIndex(k.pAtemiData, pData->atemis));
            writeI32(f, k.immunity);
            writeI32(f, k.flags);
            writeRectPtrVec(k.headRects);
            writeRectPtrVec(k.bodyRects);
            writeRectPtrVec(k.legRects);
            writeRectPtrVec(k.throwRects);
        }

        writeU32(f, action.pushBoxKeys.size());
        for (auto& k : action.pushBoxKeys) {
            writeBoxKeyBase(k);
            writeI32(f, rectPtrToIndex(k.rect, pData->rects));
        }

        writeU32(f, action.hitBoxKeys.size());
        for (auto& k : action.hitBoxKeys) {
            writeBoxKeyBase(k);
            writeI32(f, (int32_t)k.type);
            writeI32(f, (int32_t)k.flags);
            writeI32(f, ptrToIndex(k.pHitData, pData->hits));
            writeBool(f, k.hasValidStyle);
            writeI32(f, k.validStyle);
            writeBool(f, k.hasHitID);
            writeI32(f, k.hitID);
            writeRectPtrVec(k.rects);
        }

        writeU32(f, action.uniqueBoxKeys.size());
        for (auto& k : action.uniqueBoxKeys) {
            writeBoxKeyBase(k);
            writeI32(f, k.checkMask);
            writeBool(f, k.uniquePitcher);
            writeBool(f, k.applyOpToTarget);
            writeU32(f, k.ops.size());
            for (auto& op : k.ops) {
                writeI32(f, op.op);
                writeI32(f, op.opParam0);
                writeI32(f, op.opParam1);
                writeI32(f, op.opParam2);
                writeI32(f, op.opParam3);
                writeI32(f, op.opParam4);
            }
            writeRectPtrVec(k.rects);
        }

        writeU32(f, action.steerKeys.size());
        for (auto& k : action.steerKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.operationType);
            writeI32(f, k.valueType);
            writeFixed(f, k.fixValue);
            writeFixed(f, k.targetOffsetX);
            writeFixed(f, k.targetOffsetY);
            writeI32(f, k.shotCategory);
            writeI32(f, k.targetType);
            writeI32(f, k.calcValueFrame);
            writeI32(f, k.multiValueType);
            writeI32(f, k.param);
            writeBool(f, k.isDrive);
        }

        writeU32(f, action.placeKeys.size());
        for (auto& k : action.placeKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.optionFlag);
            writeFixed(f, k.ratio);
            writeI32(f, k.axis);
            writeBool(f, k.bgOnly);
            writeU32(f, k.posList.size());
            for (auto& pos : k.posList) {
                writeI32(f, pos.frame);
                writeFixed(f, pos.offset);
            }
        }

        writeU32(f, action.switchKeys.size());
        for (auto& k : action.switchKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.systemFlag);
            writeI32(f, k.operationFlag);
            writeI32(f, k.validStyle);
        }

        writeU32(f, action.eventKeys.size());
        for (auto& k : action.eventKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.validStyle);
            writeI32(f, k.type);
            writeI32(f, k.id);
            writeI64(f, k.param01);
            writeI64(f, k.param02);
            writeI64(f, k.param03);
            writeI64(f, k.param04);
            writeI64(f, k.param05);
        }

        writeU32(f, action.worldKeys.size());
        for (auto& k : action.worldKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.type);
        }

        writeU32(f, action.lockKeys.size());
        for (auto& k : action.lockKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.type);
            writeI32(f, k.param01);
            writeI32(f, k.param02);
            writeI32(f, hitEntryPtrToIndex(k.pHitEntry, pData->hits));
        }

        writeU32(f, action.branchKeys.size());
        for (auto& k : action.branchKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.type);
            writeI64(f, k.param00);
            writeI64(f, k.param01);
            writeI64(f, k.param02);
            writeI64(f, k.param03);
            writeI64(f, k.param04);
            writeI32(f, k.branchAction);
            writeI32(f, k.branchFrame);
            writeBool(f, k.keepFrame);
            writeBool(f, k.keepPlace);
            writeString(f, k.typeName);
        }

        writeU32(f, action.shotKeys.size());
        for (auto& k : action.shotKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.validStyle);
            writeI32(f, k.operation);
            writeI32(f, k.flags);
            writeFixed(f, k.posOffsetX);
            writeFixed(f, k.posOffsetY);
            writeI32(f, k.actionId);
            writeI32(f, k.styleIdx);
        }

        writeU32(f, action.triggerKeys.size());
        for (auto& k : action.triggerKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.validStyle);
            writeI32(f, k.other);
            writeI32(f, k.condition);
            writeI32(f, k.state);
            writeI32(f, ptrToIndex(k.pTriggerGroup, pData->triggerGroups));
        }

        writeU32(f, action.statusKeys.size());
        for (auto& k : action.statusKeys) {
            writeI32(f, k.startFrame);
            writeI32(f, k.endFrame);
            writeI32(f, k.landingAdjust);
            writeI32(f, k.poseStatus);
            writeI32(f, k.actionStatus);
            writeI32(f, k.jumpStatus);
            writeI32(f, k.side);
        }

        writeI32(f, action.activeFrame);
        writeI32(f, action.recoveryStartFrame);
        writeI32(f, action.recoveryEndFrame);
        writeU64(f, action.actionFlags);
        writeI32(f, action.actionFrameDuration);
        writeI32(f, action.loopPoint);
        writeI32(f, action.loopCount);
        writeI32(f, action.startScale);
        writeI32(f, action.comboScale);
        writeI32(f, action.instantScale);
        writeI32(f, ptrToIndex(action.pProjectileData, pData->projectileDatas));
        writeI32(f, action.inheritKindFlag);
        writeBool(f, action.inheritHitID);
        writeFixed(f, action.inheritAccelX);
        writeFixed(f, action.inheritAccelY);
        writeFixed(f, action.inheritVelX);
        writeFixed(f, action.inheritVelY);
    }

    writeU32(f, pData->vecMoveList.size());
    for (auto* str : pData->vecMoveList) {
        writeString(f, str ? str : "");
    }

    return true;
}

CharacterData* loadCookedCharacter(const std::string& path, int charVersion)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return nullptr;

    CharacterData* pRet = new CharacterData;

    pRet->charName = readString(f);
    pRet->charID = readI32(f);
    pRet->charVersion = charVersion;
    pRet->vitality = readI32(f);
    pRet->gauge = readI32(f);

    uint32_t chargeCount = readU32(f);
    pRet->charges.resize(chargeCount);
    for (auto& charge : pRet->charges) {
        charge.id = readI32(f);
        charge.okKeyFlags = readU32(f);
        charge.okCondFlags = readU32(f);
        charge.chargeFrames = readI32(f);
        charge.keepFrames = readI32(f);
    }

    uint32_t commandCount = readU32(f);
    pRet->commands.resize(commandCount);
    for (auto& cmd : pRet->commands) {
        cmd.id = readI32(f);
        uint32_t variantCount = readU32(f);
        cmd.variants.resize(variantCount);
        for (auto& variant : cmd.variants) {
            variant.totalMaxFrames = readI32(f);
            uint32_t inputCount = readU32(f);
            variant.inputs.resize(inputCount);
            for (auto& input : variant.inputs) {
                input.type = (InputType)readI32(f);
                input.numFrames = readI32(f);
                input.okKeyFlags = readU32(f);
                input.okCondFlags = readU32(f);
                input.ngKeyFlags = readU32(f);
                input.ngCondFlags = readU32(f);
                input.failKeyFlags = readU32(f);
                input.failCondFlags = readU32(f);
                input.rotatePointsNeeded = readI32(f);
                input.pCharge = indexToPtr(readI32(f), pRet->charges);
            }
        }
    }

    uint32_t triggerCount = readU32(f);
    pRet->triggers.resize(triggerCount);
    std::vector<int32_t> triggerCommandIndices(triggerCount);
    for (size_t i = 0; i < triggerCount; i++) {
        auto& trigger = pRet->triggers[i];
        trigger.id = readI32(f);
        trigger.actionID = readI32(f);
        trigger.validStyles = readI32(f);
        triggerCommandIndices[i] = readI32(f);
        trigger.okKeyFlags = readI32(f);
        trigger.okCondFlags = readI32(f);
        trigger.ngKeyFlags = readI32(f);
        trigger.dcExcFlags = readI32(f);
        trigger.dcIncFlags = readI32(f);
        trigger.precedingTime = readI32(f);
        trigger.useUniqueParam = readBool(f);
        trigger.advanceCombo = readBool(f);
        trigger.condParamID = readI32(f);
        trigger.condParamOp = readI32(f);
        trigger.condParamValue = readI32(f);
        trigger.limitShotCount = readI32(f);
        trigger.limitShotCategory = readI32(f);
        trigger.airActionCountLimit = readI32(f);
        trigger.vitalOp = readI32(f);
        trigger.vitalRatio = readI32(f);
        trigger.rangeCondition = readI32(f);
        trigger.rangeParam = readFixed(f);
        trigger.stateCondition = readI32(f);
        trigger.needsFocus = readBool(f);
        trigger.focusCost = readI32(f);
        trigger.needsGauge = readBool(f);
        trigger.gaugeCost = readI32(f);
        trigger.comboInst = readI32(f);
        trigger.comboSuperScaling = readI32(f);
        trigger.flags = readI64(f);
    }
    for (size_t i = 0; i < triggerCount; i++) {
        pRet->triggers[i].pCommandClassic = indexToPtr(triggerCommandIndices[i], pRet->commands);
    }

    uint32_t triggerGroupCount = readU32(f);
    pRet->triggerGroups.resize(triggerGroupCount);
    for (auto& tg : pRet->triggerGroups) {
        tg.id = readI32(f);
        uint32_t entryCount = readU32(f);
        tg.entries.resize(entryCount);
        for (auto& entry : tg.entries) {
            entry.actionID = readI32(f);
            entry.triggerID = readI32(f);
            entry.pTrigger = indexToPtr(readI32(f), pRet->triggers);
        }
    }

    for (auto& tg : pRet->triggerGroups) {
        pRet->triggerGroupByID[tg.id] = &tg;
    }

    uint32_t rectCount = readU32(f);
    pRet->rects.resize(rectCount);
    for (auto& rect : pRet->rects) {
        rect.listID = readI32(f);
        rect.id = readI32(f);
        rect.xOrig = readI32(f);
        rect.yOrig = readI32(f);
        rect.xRadius = readI32(f);
        rect.yRadius = readI32(f);
    }

    for (auto& rect : pRet->rects) {
        pRet->rectsByIDs[std::make_pair(rect.listID, rect.id)] = &rect;
    }

    uint32_t projCount = readU32(f);
    pRet->projectileDatas.resize(projCount);
    for (auto& proj : pRet->projectileDatas) {
        proj.id = readI32(f);
        proj.hitCount = readI32(f);
        proj.extraHitStop = readI32(f);
        proj.hitFlagToParent = readBool(f);
        proj.hitStopToParent = readBool(f);
        proj.rangeB = readI32(f);
        proj.wallBoxForward = readFixed(f);
        proj.wallBoxBack = readFixed(f);
        proj.airborne = readBool(f);
        proj.flags = readI32(f);
        proj.flagsExt = readI32(f);
        proj.category = readI32(f);
        proj.clashPriority = readI32(f);
        proj.noPush = readBool(f);
        proj.lifeTime = readI32(f);
        proj.hitSpan = readI32(f);
    }

    uint32_t atemiCount = readU32(f);
    pRet->atemis.resize(atemiCount);
    for (auto& atemi : pRet->atemis) {
        atemi.id = readI32(f);
        atemi.targetStop = readI32(f);
        atemi.ownerStop = readI32(f);
        atemi.targetStopShell = readI32(f);
        atemi.ownerStopShell = readI32(f);
        atemi.targetStopAdd = readI32(f);
        atemi.ownerStopAdd = readI32(f);
        atemi.resistLimit = readI32(f);
        atemi.damageRatio = readI32(f);
        atemi.recoverRatio = readI32(f);
        atemi.superRatio = readI32(f);
    }

    for (auto& atemi : pRet->atemis) {
        pRet->atemiByID[atemi.id] = &atemi;
    }

    auto readHitEntry = [&](HitEntry& e) {
        e.comboAdd = readI32(f);
        e.juggleFirst = readI32(f);
        e.juggleAdd = readI32(f);
        e.juggleLimit = readI32(f);
        e.hitStun = readI32(f);
        e.moveDestX = readI32(f);
        e.moveDestY = readI32(f);
        e.moveTime = readI32(f);
        e.curveOwnID = readI32(f);
        e.curveTargetID = readI32(f);
        e.dmgValue = readI32(f);
        e.focusGainOwn = readI32(f);
        e.focusGainTarget = readI32(f);
        e.superGainOwn = readI32(f);
        e.superGainTarget = readI32(f);
        e.parryGain = readI32(f);
        e.perfectParryGain = readI32(f);
        e.dmgType = readI32(f);
        e.dmgKind = readI32(f);
        e.dmgPower = readI32(f);
        e.moveType = readI32(f);
        e.floorTime = readI32(f);
        e.downTime = readI32(f);
        e.boundDest = readI32(f);
        e.throwRelease = readI32(f);
        e.jimenBound = readBool(f);
        e.kabeBound = readBool(f);
        e.kabeTataki = readBool(f);
        e.bombBurst = readBool(f);
        e.attr0 = readI32(f);
        e.attr1 = readI32(f);
        e.attr2 = readI32(f);
        e.attr3 = readI32(f);
        e.ext0 = readI32(f);
        e.hitStopOwner = readI32(f);
        e.hitStopTarget = readI32(f);
        e.hitmark = readI32(f);
        e.floorDestX = readI32(f);
        e.floorDestY = readI32(f);
        e.wallTime = readI32(f);
        e.wallStop = readI32(f);
        e.wallDestX = readI32(f);
        e.wallDestY = readI32(f);
    };

    uint32_t hitCount = readU32(f);
    pRet->hits.resize(hitCount);
    for (auto& hit : pRet->hits) {
        hit.id = readI32(f);
        for (int i = 0; i < 5; i++) readHitEntry(hit.common[i]);
        for (int i = 0; i < 20; i++) readHitEntry(hit.param[i]);
    }

    for (auto& hit : pRet->hits) {
        pRet->hitByID[hit.id] = &hit;
    }

    uint32_t styleCount = readU32(f);
    pRet->styles.resize(styleCount);
    for (auto& style : pRet->styles) {
        style.id = readI32(f);
        style.parentStyleID = readI32(f);
        style.terminateState = readI64(f);
        style.hasStartAction = readBool(f);
        style.startActionID = readI32(f);
        style.startActionStyle = readI32(f);
        style.hasExitAction = readBool(f);
        style.exitActionID = readI32(f);
        style.exitActionStyle = readI32(f);
        style.attackScale = readI32(f);
        style.defenseScale = readI32(f);
        style.gaugeGainRatio = readI32(f);
    }

    auto readBoxKeyBase = [&](BoxKey& k) {
        k.startFrame = readI32(f);
        k.endFrame = readI32(f);
        k.condition = readI32(f);
        k.offsetX = readFixed(f);
        k.offsetY = readFixed(f);
    };

    auto readRectPtrVec = [&](std::vector<Rect*>& rects) {
        uint32_t count = readU32(f);
        rects.resize(count);
        for (auto& r : rects) r = indexToPtr(readI32(f), pRet->rects);
    };

    uint32_t actionCount = readU32(f);
    pRet->actions.resize(actionCount);
    std::vector<int32_t> actionProjIndices(actionCount);
    std::vector<std::vector<int32_t>> actionLockKeyHitEntryIndices(actionCount);
    std::vector<std::vector<int32_t>> actionTriggerKeyGroupIndices(actionCount);

    for (size_t ai = 0; ai < actionCount; ai++) {
        auto& action = pRet->actions[ai];
        action.actionID = readI32(f);
        action.styleID = readI32(f);
        action.name = readString(f);

        uint32_t hurtBoxKeyCount = readU32(f);
        action.hurtBoxKeys.resize(hurtBoxKeyCount);
        for (auto& k : action.hurtBoxKeys) {
            readBoxKeyBase(k);
            k.isArmor = readBool(f);
            k.isAtemi = readBool(f);
            k.pAtemiData = indexToPtr(readI32(f), pRet->atemis);
            k.immunity = readI32(f);
            k.flags = readI32(f);
            readRectPtrVec(k.headRects);
            readRectPtrVec(k.bodyRects);
            readRectPtrVec(k.legRects);
            readRectPtrVec(k.throwRects);
        }

        uint32_t pushBoxKeyCount = readU32(f);
        action.pushBoxKeys.resize(pushBoxKeyCount);
        for (auto& k : action.pushBoxKeys) {
            readBoxKeyBase(k);
            k.rect = indexToPtr(readI32(f), pRet->rects);
        }

        uint32_t hitBoxKeyCount = readU32(f);
        action.hitBoxKeys.resize(hitBoxKeyCount);
        for (auto& k : action.hitBoxKeys) {
            readBoxKeyBase(k);
            k.type = (hitBoxType)readI32(f);
            k.flags = (hitBoxFlags)readI32(f);
            k.pHitData = indexToPtr(readI32(f), pRet->hits);
            k.hasValidStyle = readBool(f);
            k.validStyle = readI32(f);
            k.hasHitID = readBool(f);
            k.hitID = readI32(f);
            readRectPtrVec(k.rects);
        }

        uint32_t uniqueBoxKeyCount = readU32(f);
        action.uniqueBoxKeys.resize(uniqueBoxKeyCount);
        for (auto& k : action.uniqueBoxKeys) {
            readBoxKeyBase(k);
            k.checkMask = readI32(f);
            k.uniquePitcher = readBool(f);
            k.applyOpToTarget = readBool(f);
            uint32_t opCount = readU32(f);
            k.ops.resize(opCount);
            for (auto& op : k.ops) {
                op.op = readI32(f);
                op.opParam0 = readI32(f);
                op.opParam1 = readI32(f);
                op.opParam2 = readI32(f);
                op.opParam3 = readI32(f);
                op.opParam4 = readI32(f);
            }
            readRectPtrVec(k.rects);
        }

        uint32_t steerKeyCount = readU32(f);
        action.steerKeys.resize(steerKeyCount);
        for (auto& k : action.steerKeys) {
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.operationType = readI32(f);
            k.valueType = readI32(f);
            k.fixValue = readFixed(f);
            k.targetOffsetX = readFixed(f);
            k.targetOffsetY = readFixed(f);
            k.shotCategory = readI32(f);
            k.targetType = readI32(f);
            k.calcValueFrame = readI32(f);
            k.multiValueType = readI32(f);
            k.param = readI32(f);
            k.isDrive = readBool(f);
        }

        uint32_t placeKeyCount = readU32(f);
        action.placeKeys.resize(placeKeyCount);
        for (auto& k : action.placeKeys) {
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.optionFlag = readI32(f);
            k.ratio = readFixed(f);
            k.axis = readI32(f);
            k.bgOnly = readBool(f);
            uint32_t posCount = readU32(f);
            k.posList.resize(posCount);
            for (auto& pos : k.posList) {
                pos.frame = readI32(f);
                pos.offset = readFixed(f);
            }
        }

        uint32_t switchKeyCount = readU32(f);
        action.switchKeys.resize(switchKeyCount);
        for (auto& k : action.switchKeys) {
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.systemFlag = readI32(f);
            k.operationFlag = readI32(f);
            k.validStyle = readI32(f);
        }

        uint32_t eventKeyCount = readU32(f);
        action.eventKeys.resize(eventKeyCount);
        for (auto& k : action.eventKeys) {
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.validStyle = readI32(f);
            k.type = readI32(f);
            k.id = readI32(f);
            k.param01 = readI64(f);
            k.param02 = readI64(f);
            k.param03 = readI64(f);
            k.param04 = readI64(f);
            k.param05 = readI64(f);
        }

        uint32_t worldKeyCount = readU32(f);
        action.worldKeys.resize(worldKeyCount);
        for (auto& k : action.worldKeys) {
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.type = readI32(f);
        }

        uint32_t lockKeyCount = readU32(f);
        action.lockKeys.resize(lockKeyCount);
        actionLockKeyHitEntryIndices[ai].resize(lockKeyCount);
        for (size_t li = 0; li < lockKeyCount; li++) {
            auto& k = action.lockKeys[li];
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.type = readI32(f);
            k.param01 = readI32(f);
            k.param02 = readI32(f);
            actionLockKeyHitEntryIndices[ai][li] = readI32(f);
        }

        uint32_t branchKeyCount = readU32(f);
        action.branchKeys.resize(branchKeyCount);
        for (auto& k : action.branchKeys) {
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.type = readI32(f);
            k.param00 = readI64(f);
            k.param01 = readI64(f);
            k.param02 = readI64(f);
            k.param03 = readI64(f);
            k.param04 = readI64(f);
            k.branchAction = readI32(f);
            k.branchFrame = readI32(f);
            k.keepFrame = readBool(f);
            k.keepPlace = readBool(f);
            k.typeName = readString(f);
        }

        uint32_t shotKeyCount = readU32(f);
        action.shotKeys.resize(shotKeyCount);
        for (auto& k : action.shotKeys) {
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.validStyle = readI32(f);
            k.operation = readI32(f);
            k.flags = readI32(f);
            k.posOffsetX = readFixed(f);
            k.posOffsetY = readFixed(f);
            k.actionId = readI32(f);
            k.styleIdx = readI32(f);
        }

        uint32_t triggerKeyCount = readU32(f);
        action.triggerKeys.resize(triggerKeyCount);
        actionTriggerKeyGroupIndices[ai].resize(triggerKeyCount);
        for (size_t ti = 0; ti < triggerKeyCount; ti++) {
            auto& k = action.triggerKeys[ti];
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.validStyle = readI32(f);
            k.other = readI32(f);
            k.condition = readI32(f);
            k.state = readI32(f);
            actionTriggerKeyGroupIndices[ai][ti] = readI32(f);
        }

        uint32_t statusKeyCount = readU32(f);
        action.statusKeys.resize(statusKeyCount);
        for (auto& k : action.statusKeys) {
            k.startFrame = readI32(f);
            k.endFrame = readI32(f);
            k.landingAdjust = readI32(f);
            k.poseStatus = readI32(f);
            k.actionStatus = readI32(f);
            k.jumpStatus = readI32(f);
            k.side = readI32(f);
        }

        action.activeFrame = readI32(f);
        action.recoveryStartFrame = readI32(f);
        action.recoveryEndFrame = readI32(f);
        action.actionFlags = readU64(f);
        action.actionFrameDuration = readI32(f);
        action.loopPoint = readI32(f);
        action.loopCount = readI32(f);
        action.startScale = readI32(f);
        action.comboScale = readI32(f);
        action.instantScale = readI32(f);
        actionProjIndices[ai] = readI32(f);
        action.inheritKindFlag = readI32(f);
        action.inheritHitID = readBool(f);
        action.inheritAccelX = readFixed(f);
        action.inheritAccelY = readFixed(f);
        action.inheritVelX = readFixed(f);
        action.inheritVelY = readFixed(f);
    }

    for (size_t ai = 0; ai < actionCount; ai++) {
        pRet->actions[ai].pProjectileData = indexToPtr(actionProjIndices[ai], pRet->projectileDatas);
        for (size_t li = 0; li < pRet->actions[ai].lockKeys.size(); li++) {
            pRet->actions[ai].lockKeys[li].pHitEntry = indexToHitEntryPtr(actionLockKeyHitEntryIndices[ai][li], pRet->hits);
        }
        for (size_t ti = 0; ti < pRet->actions[ai].triggerKeys.size(); ti++) {
            pRet->actions[ai].triggerKeys[ti].pTriggerGroup = indexToPtr(actionTriggerKeyGroupIndices[ai][ti], pRet->triggerGroups);
        }
    }

    for (auto& action : pRet->actions) {
        pRet->actionsByID[ActionRef(action.actionID, action.styleID)] = &action;
    }

    uint32_t moveListCount = readU32(f);
    pRet->vecMoveList.resize(moveListCount);
    for (auto& str : pRet->vecMoveList) {
        std::string s = readString(f);
        str = strdup(s.c_str());
    }

    return pRet;
}