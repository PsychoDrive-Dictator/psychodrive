#include "simulation.hpp"
#include "guy.hpp"
#include "ui.hpp"
#include "render.hpp"

Simulation::~Simulation() {
    gatherEveryone(simGuys, everyone);
    for (auto guy: everyone) {
        // don't try to massage the guys list or opponent pointers, we're deleting everything
        guy->enableCleanup = false;
        delete guy;
    }
}

void Simulation::Clone(Simulation *pOtherSim)
{
    gatherEveryone(simGuys, everyone);
    gatherEveryone(pOtherSim->simGuys, pOtherSim->everyone);

    if (everyone.size() < pOtherSim->everyone.size()) {
        int guysToAllocate = pOtherSim->everyone.size() - everyone.size();
        for (int i = 0; i < guysToAllocate; i++) {
            Guy *newGuy = new Guy;
            everyone.push_back(newGuy);
        }
    }
    if (everyone.size() > pOtherSim->everyone.size()) {
        int guysToFree = everyone.size() - pOtherSim->everyone.size();
        for (int i = 0; i < guysToFree; i++) {
            everyone.back()->enableCleanup = false;
            delete everyone.back();
            everyone.pop_back();
        }
    }

    std::map<int,Guy*> guysByID;
    std::map<Guy*,Guy*> ourGuyByTheirGuy;

    assert(everyone.size() == pOtherSim->everyone.size());

    for (uint64_t i = 0; i < everyone.size(); i++) {
        *everyone[i] = *pOtherSim->everyone[i];
        everyone[i]->setSim(this);
        guysByID[everyone[i]->getUniqueID()] = everyone[i];
        ourGuyByTheirGuy[pOtherSim->everyone[i]] = everyone[i];
    }

    for (uint64_t i = 0; i < everyone.size(); i++) {
        everyone[i]->FixRefs(guysByID);
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
        if (gameStateDump[i]["playTimer"] != 0 && gameStateDump[i]["players"][0]["actionID"] != 0) {
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
        playerID++;
    }

    gameStateFrame = gameStateDump[i]["frameCount"];
    firstGameStateFrame = gameStateFrame;

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

    ResolveHits(pendingHitList);

    if (replayingGameStateDump) {
        static bool firstFrame = true;
        int targetDumpFrame = gameStateFrame - firstGameStateFrame;

        if (firstFrame) {
            firstFrame = false;
        } else {
            nlohmann::json &players = gameStateDump[targetDumpFrame]["players"];
            int i = 0;
            while (i < 2) {
                std::string desc = "player " + std::to_string(i);
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
                    // try to detect and align to training mode life auto-regen
                    nlohmann::json &prevPlayers = gameStateDump[targetDumpFrame-1]["players"];
                    if (prevPlayers[i]["hp"] < players[i]["hp"] && players[i]["hp"] == simGuys[i]->getMaxHealth()) {
                        simGuys[i]->setHealth(simGuys[i]->getMaxHealth());
                    }
                }

                CompareGameStateInt(players[i]["hp"], simGuys[i]->getHealth(), i, targetDumpFrame, eHealth, desc + " health");
                CompareGameStateInt(players[i]["hitStop"], simGuys[i]->getHitStopForDump(), i, targetDumpFrame, eHitStop, desc + " hitstop");

                i++;
            }

            gameStateFrame++;
            targetDumpFrame++;
        }

        if (targetDumpFrame >= (int)gameStateDump.size()) {
            replayingGameStateDump = false;
            gameStateFrame = 0;
            firstGameStateFrame = 0;
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
        }
    }

    if (recordingState) {
        stateRecording.emplace_back();
        RecordedFrame &frame = stateRecording[stateRecording.size()-1];
        for (auto guy : everyone) {
            if ((int)simController.recordedGuysPool.size() == simController.recordedGuysPoolIndex) {
                // let's say a half second of action, 2 guys for 300 frames?
                const int kPoolGrowSize = 600;
                Guy *newGuys = new Guy[kPoolGrowSize];
                for (int i = 0; i < kPoolGrowSize; i++) {
                    simController.recordedGuysPool.push_back(&newGuys[i]);
                }
            }
            Guy *pGuy = simController.recordedGuysPool[simController.recordedGuysPoolIndex++];
            *pGuy = *guy;
            pGuy->facSimile = true;
            // this is the canonical state of the guy for this frame, record boxes/state/etc here
            frame.guys[guy->getUniqueID()] = pGuy;
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
    if (recordingState) {
        RecordedFrame &frame = stateRecording[stateRecording.size()-1];
        for (auto guy : everyone) {
            // update frame triggers after AdvanceFrame() though
            frame.guys[guy->getUniqueID()]->getFrameTriggers() = guy->getFrameTriggers();
        }
        frame.events = currentFrameEvents;
        currentFrameEvents.clear();
    }
}

void Simulation::renderRecordedGuys(int frameIndex)
{
    if (frameIndex >= 0 && frameIndex < (int)stateRecording.size()) {
        for (auto [id, guy] : stateRecording[frameIndex].guys) {
            guy->Render();
        }
    }
}

Guy *Simulation::getRecordedGuy(int frameIndex, int guyID)
{
    if (frameIndex >= 0 && frameIndex < (int)stateRecording.size()) {
        for (auto [id, guy] : stateRecording[frameIndex].guys) {
            if (id == guyID) {
                return guy;
            }
        }
    }
    return nullptr;
}

void Simulation::renderRecordedHitMarkers(int frameIndex)
{
    int maxMarkerAge = 10;
    int startFrame = std::max(0, frameIndex - maxMarkerAge + 1);

    for (int checkFrame = startFrame; checkFrame <= frameIndex; checkFrame++) {
        auto &histFrame = stateRecording[checkFrame];
        auto &currentFrameGuys = stateRecording[frameIndex];

        for (const auto &event : histFrame.events) {
            if (event.type == FrameEvent::Hit) {
                int markerAge = frameIndex - checkFrame;

                Guy* targetGuy = currentFrameGuys.findGuyByID(event.hitEventData.targetID);
                if (targetGuy) {
                    float worldX = targetGuy->getPosX().f() + event.hitEventData.x;
                    float worldY = targetGuy->getPosY().f() + event.hitEventData.y;
                    drawHitMarker(worldX, worldY, event.hitEventData.radius,
                                 event.hitEventData.hitType, markerAge, maxMarkerAge,
                                 event.hitEventData.dirX, event.hitEventData.dirY, event.hitEventData.seed);
                }
            }
        }
    }
}