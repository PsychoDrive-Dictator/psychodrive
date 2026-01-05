#!/usr/bin/python

import json

characters = [
    "ryu",
    "luke",
    "kimberly",
    "chunli",
    "manon",
    "zangief",
    "jp",
    "dhalsim",
    "cammy",
    "ken",
    "deejay",
    "lily",
    "aki",
    "rashid",
    "blanka",
    "juri",
    "marisa",
    "guile",
    "ed",
    "honda",
    "jamie",
    "akuma",
    "dictator",
    "terry",
    "mai",
    "elena",
    "sagat",
    "viper"
]

dataPath = "./data/chars/"

def findMoveByHitID(movesJson, hitID):
    for moveID in movesJson:
        move = movesJson[moveID]
        if "AttackCollisionKey" in move:
            for key in move["AttackCollisionKey"]:
                if isinstance(move["AttackCollisionKey"][key], dict) and "CollisionType" in move["AttackCollisionKey"][key]:
                    attackKey = move["AttackCollisionKey"][key]
                    if attackKey["CollisionType"] == 0 and attackKey["AttackDataListIndex"] == int(hitID):
                        return moveID, str(attackKey["HitID"])
    return None

def checkHitParamHitStun(hitParam, left, right, expectedDiff, description):
    burnoutBlockHitStun = hitParam[left]["HitStun"]
    blockHitStun = hitParam[right]["HitStun"]
    difference = blockHitStun - burnoutBlockHitStun
    if difference != expectedDiff and findMoveByHitID(moves21Json, hitID) != None:
        # that's a followup guaranteed hit attack
        if "block" in description and findMoveByHitID(moves21Json, hitID)[0] == "ATK_CTA(1)":
            return
        # knockdown
        if hitParam[right]["MoveDest"]["y"] > 0:
            return
        # todo filter out crumples and grounded knockdowns too
        print(char + " " + findMoveByHitID(moves21Json, hitID)[0] + " hit " + findMoveByHitID(moves21Json, hitID)[1] + " (dt " + hitID + "):")
        print(description + " should be " + str(expectedDiff) + ", but is " + str(difference))

def countActiveFrames(move):
    minActive = -1
    maxActive = -1
    if "AttackCollisionKey" in move:
        for key in move["AttackCollisionKey"]:
            if isinstance(move["AttackCollisionKey"][key], dict) and "CollisionType" in move["AttackCollisionKey"][key]:
                attackKey = move["AttackCollisionKey"][key]
                if (attackKey["CollisionType"] == 0 or attackKey["CollisionType"] == 1):
                    if minActive == -1:
                        minActive = attackKey["_StartFrame"]
                    if maxActive == -1:
                        maxActive = attackKey["_EndFrame"]
                    minActive = min(minActive, attackKey["_StartFrame"])
                    maxActive = max(maxActive, attackKey["_EndFrame"])
    return minActive,maxActive

def getHitInfoDict(move, hitsJson):
    hitInfoDict = {}
    if "AttackCollisionKey" in move:
        for key in move["AttackCollisionKey"]:
            if isinstance(move["AttackCollisionKey"][key], dict) and "CollisionType" in move["AttackCollisionKey"][key]:
                attackKey = move["AttackCollisionKey"][key]
                if (attackKey["CollisionType"] == 0 or attackKey["CollisionType"] == 1) and attackKey["AttackDataListIndex"] != -1:
                    hitEntry = hitsJson[str(attackKey["AttackDataListIndex"]).zfill(3)]
                    hitInfoDict[attackKey["HitID"]] = hitEntry
    return hitInfoDict

def compareHitInfo(hitInfoLeft, hitInfoRight, descHeader, paramEntry, paramDesc):
    for hitRight in hitInfoRight:
        for hitKey, hitValue in hitInfoRight[hitRight]["param"][paramEntry].items():
            if hitKey != "CurveOwnID" and hitKey != "CurveTgtID":
                if hitRight in hitInfoLeft and hitKey in hitInfoLeft[hitRight]["param"][paramEntry]:
                    if not isinstance(hitValue, dict):
                        if hitValue != hitInfoLeft[hitRight]["param"][paramEntry][hitKey]:
                            print(descHeader + " hit " + str(hitRight) + " on " + paramDesc + " " + hitKey + " was " +
                                str(hitInfoLeft[hitRight]["param"][paramEntry][hitKey]) + ", now " + str(hitValue))
                    else:
                        for subHitKey, subHitValue in hitInfoRight[hitRight]["param"][paramEntry][hitKey].items():
                            if subHitValue != hitInfoLeft[hitRight]["param"][paramEntry][hitKey][subHitKey]:
                                print(descHeader + " hit " + str(hitRight) + " on " + paramDesc + " " + hitKey + "." + subHitKey + " was " +
                                    str(hitInfoLeft[hitRight]["param"][paramEntry][hitKey][subHitKey]) + ", now " + str(subHitValue))

def compareScaling(moveLeft, moveRight, scalingString, descHeader):
    if "fab" in moveLeft:
        if moveLeft["fab"]["Combo"][scalingString] != moveRight["fab"]["Combo"][scalingString]:
            print(descHeader + " " + scalingString + " was " + str(moveLeft["fab"]["Combo"][scalingString]), ", now " + str(moveRight["fab"]["Combo"][scalingString])) 
               

for char in characters:
    dataPathWithChar = dataPath + char + "/"
    # charWithVersion = char + "37"
    # hitsJson = json.load(open(dataPathWithChar + charWithVersion + "_hit.json"))
    # movesJson = json.load(open(dataPathWithChar + charWithVersion + "_moves.json"))
    charWithVersion = char + "39"
    hits21Json = json.load(open(dataPathWithChar + charWithVersion + "_hit.json"))
    moves21Json = json.load(open(dataPathWithChar + charWithVersion + "_moves.json"))
    tgroups21Json = json.load(open(dataPathWithChar + charWithVersion + "_trigger_groups.json"))

    for hitID in hits21Json:
        hitParam = hits21Json[hitID]["param"]
        # check movetime vs hitstun
        for param in hitParam:
            if hitParam[param]["HitStun"] != hitParam[param]["MoveTime"]:
                print("movetime != hitstun")
        # check burnout block is -4 compared to normal block
        #checkHitParamHitStun(hitParam, "04", "16", -4, "burnout stand block vs stand block")
        #checkHitParamHitStun(hitParam, "16", "17", 0, "stand block vs crouch block")
        #checkHitParamHitStun(hitParam, "04", "05", 0, "burnout stand block vs burnout crouch block")
        # checkHitParamHitStun(hitParam, "00", "01", 0, "stand hit vs crouch hit")
        # checkHitParamHitStun(hitParam, "08", "09", 0, "stand counter vs crouch counter")
        # checkHitParamHitStun(hitParam, "12", "13", 0, "stand PC vs crouch PC")
        # checkHitParamHitStun(hitParam, "00", "08", 2, "stand hit vs stand counter")
        # checkHitParamHitStun(hitParam, "00", "12", 4, "stand hit vs stand PC")

    # find hitboxes with certain flags
    # for moveID in moves21Json:
    #     move = moves21Json[moveID]
    #     if "AttackCollisionKey" in move:
    #         for key in move["AttackCollisionKey"]:
    #             if isinstance(move["AttackCollisionKey"][key], dict) and "CollisionType" in move["AttackCollisionKey"][key]:
    #                 attackKey = move["AttackCollisionKey"][key]
    #                 if attackKey["Condition"] & 2048:
    #                     print(char + " " + moveID + " type " + str(attackKey["CollisionType"]) +
    #                           " hit " + str(attackKey["HitID"]) + " frames " + str(attackKey["_StartFrame"]) + "-" + str(attackKey["_EndFrame"]) +
    #                           " condition " + str(attackKey["Condition"]))

    # find certain branches
    # for moveID in moves21Json:
    #     move = moves21Json[moveID]
    #     if "BranchKey" in move:
    #         for key in move["BranchKey"]:
    #             if isinstance(move["BranchKey"][key], dict) and "Type" in move["BranchKey"][key]:
    #                 branchKey = move["BranchKey"][key]
    #                 if branchKey["Type"] == 1:
    #                     print(char + " " + moveID + " ELSE branch params " + str(branchKey["Param00"]) +
    #                           " " + str(branchKey["Param01"]) + " " + str(branchKey["Param02"]) + " " +
    #                           str(branchKey["Param03"]) + " " + str(branchKey["Param04"]))

    # find all key types in IMM_ actions
    # for moveID in moves21Json:
    #     if "IMM_" in moveID:
    #         move = moves21Json[moveID]
    #         for key in move:
    #             if "Key" in key:
    #                 print(char + " " + moveID + " " + key)

    # find certain StatusKey Side bits
    # for moveID in moves21Json:
    #     move = moves21Json[moveID]
    #     if "StatusKey" in move:
    #         for key in move["StatusKey"]:
    #             if isinstance(move["StatusKey"][key], dict) and "Side" in move["StatusKey"][key]:
    #                 statusKey = move["StatusKey"][key]
    #                 if statusKey["Side"] == 1:
    #                     print(char + " " + moveID + " StatusKey Side " + str(statusKey["Side"]) + " frames " + str(statusKey["_StartFrame"]) +
    #                           "-" + str(statusKey["_EndFrame"]) + " pose " + str(statusKey["PoseStatus"]))

    # find certain SwitchKey OperationFlag bits
    # for moveID in moves21Json:
    #     move = moves21Json[moveID]
    #     if "SwitchKey" in move:
    #         for key in move["SwitchKey"]:
    #             if isinstance(move["SwitchKey"][key], dict) and "OperationFlag" in move["SwitchKey"][key]:
    #                 switchKey = move["SwitchKey"][key]
    #                 if switchKey["OperationFlag"] != 0:
    #                     print(char + " " + moveID + " switchkey OperationFlag " +
    #                     str(switchKey["OperationFlag"]) + " frames " + str(switchKey["_StartFrame"]) +
    #                     "-" + str(switchKey["_EndFrame"]))

    # find SwitchKey timer stop bits
    # for moveID in moves21Json:
    #     move = moves21Json[moveID]
    #     if "SwitchKey" in move:
    #         for key in move["SwitchKey"]:
    #             if isinstance(move["SwitchKey"][key], dict) and "_SystemBit" in move["SwitchKey"][key]:
    #                 switchKey = move["SwitchKey"][key]
    #                 if switchKey["_SystemBit"] & 4096:
    #                     moveDuration = move["fab"]["ActionFrame"]["MarginFrame"]
    #                     if moveDuration == -1:
    #                         moveDuration = move["fab"]["Frame"]
    #                     mismatch = ""
    #                     if moveDuration != switchKey["_EndFrame"]:
    #                         mismatch = " (end frame mismatch!)"
    #                     print(char + " " + moveID + " (" + str(moveDuration) + " frames) timer stop frames " + str(switchKey["_StartFrame"]) +
    #                     "-" + str(switchKey["_EndFrame"]) + mismatch)

    # find certain SteerKey target types 
    # nextSteerKey = 0
    # for moveID in moves21Json:
    #     move = moves21Json[moveID]
    #     if "SteerKey" in move:
    #         for key in move["SteerKey"]:
    #             if isinstance(move["SteerKey"][key], dict) and "TargetType" in move["SteerKey"][key]:
    #                 steerKey = move["SteerKey"][key]
    #                 # if nextSteerKey == 1:
    #                 #     print(char + " " + moveID + " SteerKey TargetType op " + str(steerKey["OperationType"]) + " " + str(steerKey["FixValue"]) + " " + str(steerKey["MultiValueType"]))
    #                 #     nextSteerKey = 0
    #                 if steerKey["OperationType"] == 3:
    #                     nextSteerKey = 1
    #                     print(char + " " + moveID + " SteerKey TargetType op " + str(steerKey["TargetType"]) + " " + str(steerKey["FixValue"]) + " " + str(steerKey["ValueType"]))

    # Find moves with missing kara cancels
    # for key, value in tgroups21Json["000"].items():
    #     moveName = value.split(" ")[1]
    #     moveNoKara = False
    #     if "TriggerKey" not in moves21Json[moveName]:
    #         moveNoKara = True
    #     else:
    #         moveNoKara = True
    #         for triggernum, triggerkey in moves21Json[moveName]["TriggerKey"].items():
    #             if isinstance(triggerkey, dict):
    #                 if triggerkey["_StartFrame"] == 0 and triggerkey["_EndFrame"] == 1:
    #                     moveNoKara = False
    #     if moveNoKara:
    #         print(char, moveName)

    # Find moves with end buffers that don't match move recovery
    # for moveID in moves21Json:
    #     move = moves21Json[moveID]
    #     if "TriggerKey" in move:
    #         for key in move["TriggerKey"]:
    #             if isinstance(move["TriggerKey"][key], dict):
    #                 trigger = move["TriggerKey"][key]
    #                 if trigger["TriggerGroup"] == 0 and trigger["_StartFrame"] != 0 and not trigger["_Other"] & 0x20000 and trigger["_NotDefer"] == True and trigger["_StartFrame"] < move["fab"]["ActionFrame"]["MarginFrame"]:
    #                     print(char + " " + moveID + " margin " + str(move["fab"]["ActionFrame"]["MarginFrame"]) + " end buffer " + str(trigger["_StartFrame"]))

    # Find triggers with certain IDs
    # for key, value in tgroups21Json.items():
    #     for tkey, tvalue in value.items():
    #         moveName = tvalue.split(" ")[1]
    #         triggerID = int(tkey)
    #         if triggerID < 50:
    #             print(char, moveName, str(triggerID))

    # for moveID in moves21Json:
    #     moveIDLeft = moveID
    #     # if "_Y2" in moveID:
    #     #     moveIDLeft = moveIDLeft.replace("_Y2", "")
    #     if moveIDLeft in movesJson:
    #         moveLeft = movesJson[moveIDLeft]
    #         moveRight = moves21Json[moveID]
    #         descHeader = char + " " + moveID
    #         minActiveLeft, maxActiveLeft = countActiveFrames(moveLeft)
    #         minActiveRight, maxActiveRight = countActiveFrames(moveRight)
    #         if minActiveLeft != minActiveRight or maxActiveLeft != maxActiveRight:
    #             print(descHeader + " was active from " + str(minActiveLeft) + "-" + str(maxActiveLeft) + 
    #                   " now active from " + str(minActiveRight) + "-" + str(maxActiveRight))
    #         hitInfoLeft = getHitInfoDict(moveLeft, hitsJson)
    #         hitInfoRight = getHitInfoDict(moveRight, hits21Json)
    #         if len(hitInfoLeft.keys()) != len(hitInfoRight.keys()):
    #             print(descHeader + " different hit count")
    #         compareHitInfo(hitInfoLeft, hitInfoRight, descHeader, "00", "hit")
    #         compareHitInfo(hitInfoLeft, hitInfoRight, descHeader, "16", "block")
    #         compareHitInfo(hitInfoLeft, hitInfoRight, descHeader, "02", "air hit")
    #         compareHitInfo(hitInfoLeft, hitInfoRight, descHeader, "04", "burnout block")
    #         compareHitInfo(hitInfoLeft, hitInfoRight, descHeader, "08", "counter")
    #         compareHitInfo(hitInfoLeft, hitInfoRight, descHeader, "12", "punish counter")
    #         compareScaling(moveLeft, moveRight, "ComboScaling", descHeader)
    #         compareScaling(moveLeft, moveRight, "InstScaling", descHeader)
    #         compareScaling(moveLeft, moveRight, "_StartScaling", descHeader)