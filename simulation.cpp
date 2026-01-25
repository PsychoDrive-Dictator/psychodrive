#include "simulation.hpp"
#include "guy.hpp"
#include "ui.hpp"
#include "render.hpp"

Simulation::~Simulation() {
    if (!enableCleanup) {
        return;
    }
    gatherEveryone(simGuys, everyone);
    for (auto guy: everyone) {
        // don't try to massage the guys list or opponent pointers, we're deleting everything
        guy->enableCleanup = false;
        delete guy;
    }
}

void Simulation::Clone(Simulation *pOtherSim, ObjectPool<Guy> *pGuyPool)
{
    gatherEveryone(simGuys, everyone);
    gatherEveryone(pOtherSim->simGuys, pOtherSim->everyone);

    if (everyone.size() < pOtherSim->everyone.size()) {
        int guysToAllocate = pOtherSim->everyone.size() - everyone.size();
        for (int i = 0; i < guysToAllocate; i++) {
            Guy *newGuy = pGuyPool ? pGuyPool->allocate() : new Guy;
            if (pGuyPool) {
                newGuy->enableCleanup = false;
            }
            everyone.push_back(newGuy);
        }
    }
    if (everyone.size() > pOtherSim->everyone.size()) {
        int guysToFree = everyone.size() - pOtherSim->everyone.size();
        for (int i = 0; i < guysToFree; i++) {
            if (pGuyPool) {
                pGuyPool->release(everyone.back());
            } else {
                everyone.back()->enableCleanup = false;
                delete everyone.back();
            }
            everyone.pop_back();
        }
    }

    std::map<int,Guy*> guysByID;
    std::map<Guy*,Guy*> ourGuyByTheirGuy;

    assert(everyone.size() == pOtherSim->everyone.size());

    for (uint64_t i = 0; i < everyone.size(); i++) {
        *everyone[i] = *pOtherSim->everyone[i];
        everyone[i]->setSim(this);
        assert(guysByID.find(everyone[i]->getUniqueID()) == guysByID.end());
        guysByID[everyone[i]->getUniqueID()] = everyone[i];
        ourGuyByTheirGuy[pOtherSim->everyone[i]] = everyone[i];
    }

    for (uint64_t i = 0; i < everyone.size(); i++) {
        everyone[i]->FixRefs(guysByID);
        assert(everyone[i]->getUniqueID() == pOtherSim->everyone[i]->getUniqueID());
    }

    simGuys.clear();
    for (Guy *pGuy : pOtherSim->simGuys) {
        simGuys.push_back(ourGuyByTheirGuy[pGuy]);
    }

    vecGuysToDelete.clear();
    for (Guy *pGuy : pOtherSim->vecGuysToDelete) {
        assert(ourGuyByTheirGuy.find(pGuy) != ourGuyByTheirGuy.end());
        vecGuysToDelete.push_back(ourGuyByTheirGuy[pGuy]);
    }

    guyIDCounter = pOtherSim->guyIDCounter;
    frameCounter = pOtherSim->frameCounter;
}

void Simulation::CreateGuy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color)
{
    Guy *pNewGuy = new Guy(this, charName, charVersion, x, y, startDir, color);

    if (simGuys.size()) {
        pNewGuy->setOpponent(simGuys[0]);
        if (simGuys.size() == 1) {
            simGuys[0]->setOpponent(pNewGuy);
        }
    }

    simGuys.push_back(pNewGuy);
}

void Simulation::CreateGuyFromDumpedPlayer(nlohmann::json &playerJson, int version)
{
    std::string charName = getCharNameFromID(playerJson["charID"]);
    Fixed posX = Fixed(playerJson["posX"].get<double>());
    Fixed posY = Fixed(playerJson["posY"].get<double>());
    int bitValue = playerJson["bitValue"];
    int charDirection = (bitValue & 1<<7) ? 1 : -1;
    color charColor = {randFloat(), randFloat(), randFloat()};

    CreateGuy(charName, version, posX, posY, charDirection, charColor);
}

void Simulation::CreateGuyFromCharController(CharacterUIController &controller)
{
    std::string charName = charNames[controller.character];

    int version = atoi(charVersions[controller.charVersion]);
    CreateGuy(charName, version, controller.startPosX, Fixed(0), controller.rightSide ? -1 : 1, controller.charColor);
}


bool Simulation::SetupFromGameDump(std::string dumpPath, int version)
{
    gameStateDump = parse_json_file(dumpPath);
    if (gameStateDump == nullptr) {
        return false;
    }

    int i = 0;
    while (i < (int)gameStateDump.size()) {
        if (gameStateDump[i]["playTimer"] != 0 && gameStateDump[i]["players"][0]["actionID"] != 0 &&
            gameStateDump[i+1]["players"][0]["actionFrame"] != 0) {
            break;
        }
        i++;
    }

    if (version == -1) {
        // fall back to latest
        version = atoi(charVersions[charVersionCount - 1]);
    }

    nlohmann::json &players = gameStateDump[i]["players"];
    CreateGuyFromDumpedPlayer(players[0], version);
    CreateGuyFromDumpedPlayer(players[1], version);

    int playerID = 0;
    while (playerID < 2) {
        nlohmann::json &playerJson = players[playerID];
        int actionID = playerJson["actionID"];
        int actionFrame = playerJson["actionFrame"];
        if (actionID == 17) {
            // guile66hit combo has p2 in mid-dash and we don't capture posoffset
            actionID = 1;
            actionFrame = 0;
        }
        simGuys[playerID]->setAction(actionID, actionFrame - 1);
        simGuys[playerID]->setHealth(playerJson["hp"]);
        simGuys[playerID]->setFocus(playerJson.value("driveGauge", maxFocus));
        simGuys[playerID]->setFocusRegenCooldown(-1);
        simGuys[playerID]->setGauge(playerJson.value("superGauge", simGuys[playerID]->getCharData()->gauge));
        playerID++;
    }

    gameStateFrame = i;

    if (gameStateDump[i].contains("stageTimer")) {
        frameCounter = gameStateDump[i]["stageTimer"];
    }

    replayingGameStateDump = true;

    return true;
}

void Simulation::CompareGameStateFixed( Fixed dumpValue, Fixed realValue, int player, int frame, ErrorType errorType, std::string description )
{
    if (dumpValue != realValue)
    {
        float valueDiff = realValue.f() - dumpValue.f();
        uint64_t valueDiffData = realValue.data - dumpValue.data;
        std::string dumpValueStr = std::to_string(dumpValue.data) + " / " + std::to_string(dumpValue.f());
        std::string simValueStr = std::to_string(realValue.data) + " / " + std::to_string(realValue.f());
        std::string diffStr = std::to_string(valueDiffData) + " / " + std::to_string(valueDiff);
        std::string headerStr = "E;" + std::to_string(player) + ";" + std::to_string(frame) + ";" + std::to_string(errorType) + ";";
        Log(headerStr + description + " dump: " + dumpValueStr + " sim: " + simValueStr + " diff: " + diffStr);

        replayErrors++;
    }
}

void Simulation::CompareGameStateInt( int64_t dumpValue, int64_t realValue, int player, int frame, ErrorType errorType, std::string description )
{
    if (dumpValue != realValue)
    {
        std::string headerStr = "E;" + std::to_string(player) + ";" + std::to_string(frame) + ";" + std::to_string(errorType) + ";";
        Log(headerStr + description + " dump: " + std::to_string(dumpValue) + " sim: " + std::to_string(realValue));

        replayErrors++;
    }
}

void Simulation::Log(std::string logLine)
{
    fprintf(stderr, "%s\n", logLine.c_str());
}

void Simulation::RunFrame(void) {
    if (!replayingGameStateDump) {
        frameCounter++;
    }

    for (auto guy : vecGuysToDelete) {
        delete guy;
    }
    vecGuysToDelete.clear();

    gatherEveryone(simGuys, everyone);

    for (auto guy : everyone) {
        guy->RunFrame();
    }

    // gather everyone again in case of deletions/additions in RunFrame
    gatherEveryone(simGuys, everyone);

    for (auto guy : everyone) {
        guy->WorldPhysics();
    }
    for (auto guy : everyone) {
        guy->Push(guy->getOpponent());
    }
    for (auto guy : everyone) {
        guy->RunFramePostPush();
    }

    // gather everyone again in case of deletions/additions in RunFramePostPush
    gatherEveryone(simGuys, everyone);

    std::vector<PendingHit> pendingHitList;

    for (auto guy : everyone) {
        guy->CheckHit(guy->getOpponent(), pendingHitList);
    }

    ResolveHits(this, pendingHitList);

    if (replayingGameStateDump) {
        static bool firstFrame = true;
        int targetDumpFrame = gameStateFrame;

        if (firstFrame) {
            firstFrame = false;
        } else {
            nlohmann::json &players = gameStateDump[targetDumpFrame]["players"];
            int i = 0;
            while (i < 2) {
                std::string desc = "player " + std::to_string(i);

                if (!finished && players[i]["hp"] == 0 && players[i]["hp"] == simGuys[i]->getHealth()) {
                    finished = true;
                }

                if (finished) {
                    break;
                }

                // some tests have a first frame from before state reset?
                if (targetDumpFrame > 1) {
                    CompareGameStateFixed(Fixed(players[i]["posX"].get<double>()), simGuys[i]->getPosX(), i, targetDumpFrame, ePos, desc + " pos X");
                    CompareGameStateFixed(Fixed(players[i]["posY"].get<double>()), simGuys[i]->getPosY(), i, targetDumpFrame, ePos, desc + " pos Y");
                }
                Fixed velX, velY, accelX, accelY;
                simGuys[i]->getVel(velX, velY, accelX, accelY);
                CompareGameStateFixed(Fixed(players[i]["velX"].get<double>()), velX, i, targetDumpFrame, eVel, desc + " vel X");
                CompareGameStateFixed(Fixed(players[i]["velY"].get<double>()), velY, i, targetDumpFrame, eVel, desc + " vel Y");
                if (players[i].contains("accelX")) {
                    CompareGameStateFixed(Fixed(players[i]["accelX"].get<double>()), accelX, i, targetDumpFrame, eAccel, desc + " accel X");
                }
                CompareGameStateFixed(Fixed(players[i]["accelY"].get<double>()), accelY, i, targetDumpFrame, eAccel, desc + " accel Y");

                if (players[i].contains("hitVelX")) {
                    CompareGameStateFixed(Fixed(players[i]["hitVelX"].get<double>()), simGuys[i]->getHitVelX(), i, targetDumpFrame, eHitVel, desc + " hitAccel X");
                    CompareGameStateFixed(Fixed(players[i]["hitAccelX"].get<double>()), simGuys[i]->getHitAccelX(), i, targetDumpFrame, eHitAccel, desc + " hitAccel X");
                }

                CompareGameStateInt(players[i]["actionID"], simGuys[i]->getCurrentAction(), i, targetDumpFrame, eActionID, desc + " action ID");
                CompareGameStateInt(players[i]["actionFrame"], simGuys[i]->getCurrentFrame(), i, targetDumpFrame, eActionFrame, desc + " action frame");

                // swap players here, we track combo hits on the opponent
                CompareGameStateInt(players[i]["comboCount"], simGuys[!i]->getComboHits(), i, targetDumpFrame, eComboCount, desc + " combo");

                CompareGameStateInt((players[i]["bitValue"].get<int>() & (1<<7)) ? 1 : -1, simGuys[i]->getDirection(), i, targetDumpFrame, eDirection, desc + " direction");

                if (targetDumpFrame > 0) {
                    nlohmann::json &prevPlayers = gameStateDump[targetDumpFrame-1]["players"];
                    nlohmann::json &firstPlayers = gameStateDump[0]["players"];
                    if (!match && detectTrainingAutoRegen(prevPlayers[i], players[i], firstPlayers[i], "hp", simGuys[i]->getCharData()->vitality)) {
                        simGuys[i]->setHealth(players[i]["hp"]);
                    }
                    if (!match && detectTrainingAutoRegen(prevPlayers[i], players[i], firstPlayers[i], "driveGauge", maxFocus)) {
                        simGuys[i]->guyLog(simGuys[i]->getLogResources(), "focus training refill detected, focus to " + std::to_string(players[i]["driveGauge"].get<int>()) + " was " + std::to_string(simGuys[i]->getFocus()));
                        simGuys[i]->setFocus(players[i]["driveGauge"]);
                        simGuys[i]->setFocusRegenCooldown(-1);
                    }
                    if (!match && detectTrainingAutoRegen(prevPlayers[i], players[i], firstPlayers[i], "superGauge", simGuys[i]->getCharData()->gauge)) {
                        simGuys[i]->setGauge(players[i]["superGauge"]);
                    }
                }

                CompareGameStateInt(players[i]["hp"], simGuys[i]->getHealth(), i, targetDumpFrame, eHealth, desc + " health");
                CompareGameStateInt(players[i]["hitStop"], simGuys[i]->getHitStopForDump(), i, targetDumpFrame, eHitStop, desc + " hitstop");

                if (players[i].contains("driveGauge")) {
                    CompareGameStateInt(players[i]["driveGauge"], simGuys[i]->getFocus(), i, targetDumpFrame, eGauge, desc + " drive gauge");
                    // if (targetDumpFrame < 240 && (targetDumpFrame + 1) < (int)gameStateDump.size()) {
                    //     nlohmann::json &nextPlayers = gameStateDump[targetDumpFrame+1]["players"];
                    //     int driveDiff = nextPlayers[i]["driveGauge"].get<int>() - players[i]["driveGauge"].get<int>();
                    //     if (driveDiff == 40 || driveDiff == 50 || driveDiff == 20 || driveDiff == 25 || driveDiff == 60 || driveDiff == 70) {
                    //         if (!simGuys[i]->getHasFocusRegenCooldowned()) {
                    //             simGuys[i]->setFocusRegenCooldown(1); // start regenning next frame
                    //         }
                    //     }
                    // }
                }
                if (players[i].contains("superGauge")) {
                    CompareGameStateInt(players[i]["superGauge"], simGuys[i]->getGauge(), i, targetDumpFrame, eGauge, desc + " super gauge");
                }

                i++;
            }

            gameStateFrame++;
            targetDumpFrame++;
        }

        if (targetDumpFrame >= (int)gameStateDump.size()) {
            replayingGameStateDump = false;
            gameStateFrame = 0;
            frameCounter = 0;
            Log("F;game replay finished, errors: " + std::to_string(replayErrors));
        } else {
            int inputLeft = gameStateDump[targetDumpFrame]["players"][0]["currentInput"];
            int prevInputLeft = 0;
            if (targetDumpFrame > 0) {
                prevInputLeft = gameStateDump[targetDumpFrame-1]["players"][0]["currentInput"];
            }
            inputLeft = addPressBits( inputLeft, prevInputLeft );

            int inputRight = 0;
            // training dumps don't have input for player 2
            if (gameStateDump[targetDumpFrame]["players"][1].contains("currentInput")) {
                inputRight = gameStateDump[targetDumpFrame]["players"][1]["currentInput"];
                int prevInputRight = 0;
                if (targetDumpFrame > 0) {
                    prevInputRight = gameStateDump[targetDumpFrame-1]["players"][1]["currentInput"];
                }
                inputRight = addPressBits( inputRight, prevInputRight );
            }

            simGuys[0]->Input(inputLeft);
            simGuys[1]->Input(inputRight);

            if (gameStateDump[targetDumpFrame].contains("stageTimer")) {
                frameCounter = gameStateDump[targetDumpFrame]["stageTimer"];
            }

            int playTimerSeconds = gameStateDump[targetDumpFrame].value("playTimer", 99);
            int playTimerFrames = gameStateDump[targetDumpFrame].value("playTimerMS", 60);

            if (match && (playTimerSeconds != 99 || (playTimerFrames != 60 && playTimerFrames != 59))) {
                timerStarted = true;
            }
        }
    }
}

void Simulation::AdvanceFrame(void)
{
    for (auto guy : everyone) {
        bool die = !guy->AdvanceFrame();

        if (die) {
            // don't delete guys before other guys might be done with them this frame
            // and also not before the start of next frame so we may still render them
            vecGuysToDelete.push_back(guy);
        }
    }
}

void Simulation::Render(float z /* = 0.0 */)
{
    for (auto guy : everyone) {
        guy->Render(z);
    }
}

