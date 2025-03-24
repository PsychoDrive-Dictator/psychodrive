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

void Simulation::CreateGuyFromDumpedPlayer(nlohmann::json &playerJson)
{
    std::string charName = getCharNameFromID(playerJson["charID"]);
    // todo currently max version, read from test file or something
    int charVersion = atoi(charVersions[charVersionCount - 1]);
    Fixed posX = Fixed(playerJson["posX"].get<double>());
    Fixed posY = Fixed(playerJson["posY"].get<double>());
    int bitValue = playerJson["bitValue"];
    int charDirection = (bitValue & 1<<7) ? 1 : -1;
    color charColor = {randFloat(), randFloat(), randFloat()};

    CreateGuy(charName, charVersion, posX, posY, charDirection, charColor);
}

bool Simulation::SetupFromGameDump(std::string dumpPath)
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

    nlohmann::json &players = gameStateDump[i]["players"];
    CreateGuyFromDumpedPlayer(players[0]);
    CreateGuyFromDumpedPlayer(players[1]);

    gameStateFrame = gameStateDump[i]["frameCount"];
    firstGameStateFrame = gameStateFrame;

    if (gameStateDump[i].contains("stageTimer")) {
        frameCounter = gameStateDump[i]["stageTimer"];
    }

    replayingGameStateDump = true;

    return true;
}

void Simulation::CompareGameStateFixed( Fixed dumpValue, Fixed realValue, bool fatal, std::string description )
{
    if (dumpValue != realValue)
    {
        float valueDiff = realValue.f() - dumpValue.f();
        std::string dumpValueStr = std::to_string(dumpValue.data) + " / " + std::to_string(dumpValue.f());
        std::string simValueStr = std::to_string(realValue.data) + " / " + std::to_string(realValue.f());
        Log("replay error: " + description + " dump: " + dumpValueStr + " sim: " + simValueStr + " diff: " + std::to_string(valueDiff));
        if (fatal) {
            replayErrors++;
        }
    }
}

void Simulation::CompareGameStateInt( int64_t dumpValue, int64_t realValue, bool fatal, std::string description )
{
    if (dumpValue != realValue)
    {
        Log("replay error: " + description + " dump: " + std::to_string(dumpValue) + " real: " + std::to_string(realValue));
        if (fatal) {
            replayErrors++;
        }
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
                CompareGameStateFixed(Fixed(players[i]["posX"].get<double>()), simGuys[i]->getPosX(), true, desc + " pos X");
                CompareGameStateFixed(Fixed(players[i]["posY"].get<double>()), simGuys[i]->getPosY(), true, desc + " pos Y");
                Fixed velX, velY, accelX, accelY;
                simGuys[i]->getVel(velX, velY, accelX, accelY);
                CompareGameStateFixed(Fixed(players[i]["velX"].get<double>()), velX, true, desc + " vel X");
                CompareGameStateFixed(Fixed(players[i]["velY"].get<double>()), velY, true, desc + " vel Y");
                if (players[i].contains("accelX")) {
                    CompareGameStateFixed(Fixed(players[i]["accelX"].get<double>()), accelX, true, desc + " accel X");
                }
                CompareGameStateFixed(Fixed(players[i]["accelY"].get<double>()), accelY, true, desc + " accel Y");

                if (players[i].contains("hitVelX")) {
                    CompareGameStateFixed(Fixed(players[i]["hitVelX"].get<double>()), simGuys[i]->getHitVelX(), true, desc + " hitAccel X");
                    CompareGameStateFixed(Fixed(players[i]["hitAccelX"].get<double>()), simGuys[i]->getHitAccelX(), true, desc + " hitAccel X");
                }

                CompareGameStateInt(players[i]["actionID"], simGuys[i]->getCurrentAction(), false, desc + " action ID");
                CompareGameStateInt(players[i]["actionFrame"], simGuys[i]->getCurrentFrame(), false, desc + " action frame");

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
            Log("game replay finished, errors: " + std::to_string(replayErrors));
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