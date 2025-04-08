#include "simulation.hpp"
#include "guy.hpp"

void Simulation::CreateGuy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color)
{
    Guy *pNewGuy = new Guy(charName, charVersion, x, y, startDir, color);

    if (simGuys.size()) {
        pNewGuy->setOpponent(simGuys[0]);
        if (simGuys.size() == 1) {
            simGuys[0]->setOpponent(pNewGuy);
        }
    }

    pNewGuy->setSim(this);

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

void Simulation::AdvanceFrame(void)
{
    if (!replayingGameStateDump) {
        frameCounter++;
    }

    for (auto guy : vecGuysToDelete) {
        delete guy;
    }
    vecGuysToDelete.clear();

    std::vector<Guy *> everyone;

    for (auto guy : simGuys) {
        everyone.push_back(guy);
        for ( auto minion : guy->getMinions() ) {
            everyone.push_back(minion);
        }
    }

    for (auto guy : everyone) {
        guy->PreFrame();
    }

    // gather everyone again in case of deletions/additions in PreFrame
    everyone.clear();
    for (auto guy : simGuys) {
        everyone.push_back(guy);
        for ( auto minion : guy->getMinions() ) {
            everyone.push_back(minion);
        }
    }

    for (auto guy : everyone) {
        guy->WorldPhysics();
    }
    for (auto guy : everyone) {
        guy->Push(guy->getOpponent());
    }
    for (auto guy : everyone) {
        guy->CheckHit(guy->getOpponent());
    }

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
                CompareGameStateFixed(Fixed(players[i]["posX"].get<double>()), simGuys[i]->getPosX(), i, targetDumpFrame, ePos, desc + " pos X");
                CompareGameStateFixed(Fixed(players[i]["posY"].get<double>()), simGuys[i]->getPosY(), i, targetDumpFrame, ePos, desc + " pos Y");
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

                if (players[i]["hitStop"] == 0) {
                    CompareGameStateInt(players[i]["actionID"], simGuys[i]->getCurrentAction(), i, targetDumpFrame, eActionID, desc + " action ID");
                    CompareGameStateInt(players[i]["actionFrame"], simGuys[i]->getCurrentFrame(), i, targetDumpFrame, eActionFrame, desc + " action frame");
                }

                // swap players here, we track combo hits on the opponent
                CompareGameStateInt(players[i]["comboCount"], simGuys[!i]->getComboHits(), i, targetDumpFrame, eComboCount, desc + " combo");

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

    for (auto guy : everyone) {
        bool die = !guy->Frame();

        if (die) {
            vecGuysToDelete.push_back(guy);
        }
    }
}