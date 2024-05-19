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
    "jamie"
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
    if difference != expectedDiff and findMoveByHitID(movesJson, hitID) != None:
        # that's a followup guaranteed hit attack
        if "block" in description and findMoveByHitID(movesJson, hitID)[0] == "ATK_CTA(1)":
            return
        # knockdown
        if hitParam[right]["MoveDest"]["y"] > 0:
            return
        # todo filter out crumples and grounded knockdowns too
        print(char + " " + findMoveByHitID(movesJson, hitID)[0] + " hit " + findMoveByHitID(movesJson, hitID)[1] + " (dt " + hitID + "):")
        print(description + " should be " + str(expectedDiff) + ", but is " + str(difference))


for char in characters:
    hitsJson = json.load(open(dataPath + char + "_hit.json"))
    movesJson = json.load(open(dataPath + char + "_moves.json"))
    for hitID in hitsJson:
        hitParam = hitsJson[hitID]["param"]
        # check movetime vs hitstun
        for param in hitParam:
            if hitParam[param]["HitStun"] != hitParam[param]["MoveTime"]:
                print("movetime != hitstun")
        # check burnout block is -4 compared to normal block
        checkHitParamHitStun(hitParam, "04", "16", -4, "burnout stand block vs stand block")
        checkHitParamHitStun(hitParam, "16", "17", 0, "stand block vs crouch block")
        checkHitParamHitStun(hitParam, "04", "05", 0, "burnout stand block vs burnout crouch block")
        checkHitParamHitStun(hitParam, "00", "01", 0, "stand hit vs crouch hit")
        checkHitParamHitStun(hitParam, "08", "09", 0, "stand counter vs crouch counter")
        checkHitParamHitStun(hitParam, "12", "13", 0, "stand PC vs crouch PC")
        checkHitParamHitStun(hitParam, "00", "08", 2, "stand hit vs stand counter")
        checkHitParamHitStun(hitParam, "00", "12", 4, "stand hit vs stand PC")
