#include <stdio.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <ios>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <bitset>

#include <SDL.h>
#include <SDL_opengl.h>

#include "json.hpp"

#include "guy.hpp"
#include "main.hpp"
#include "ui.hpp"
#include "input.hpp"
#include "render.hpp"

bool forceCounter = false;
bool forcePunishCounter = false;
int hitStunAdder = 0;

uint32_t globalInputBufferLength = 4; // 4 frames of input buffering

bool resetpos = false;
std::vector<Guy *> guys;

bool done = false;
bool paused = false;
bool oneframe = false;
int globalFrameCount = 0;

bool recordingInput = false;
std::vector<int> recordedInput;
int recordingStartFrame = 0;

bool playingBackInput = false;
std::deque<int> playBackInputBuffer;
int playBackFrame = 0;

bool replayingGameState = false;
int gameStateFrame = 0;
int replayErrors = 0;

bool limitRate = true;

bool deleteInputs = false;

void writeFile(const std::string &fileName, std::string contents)
{
    std::ofstream ofs(fileName.c_str(), std::ios::out | std::ios::trunc);
    if (ofs.is_open()) {
        ofs.write(contents.c_str(), contents.size());
    }
}

std::string readFile(const std::string &fileName)
{
    std::ifstream ifs(fileName.c_str(), std::ios::in | std::ios::ate);

    std::ifstream::pos_type fileSize = ifs.tellg();
    if (fileSize < 0)                             
        return std::string();                     

    ifs.seekg(0, std::ios::beg);

    std::vector<char> bytes(fileSize);
    ifs.read(&bytes[0], fileSize);

    return std::string(&bytes[0], fileSize);
}

nlohmann::json parse_json_file(const std::string &fileName)
{
    std::string fileText = readFile(fileName);
    if (fileText == "") return nullptr;
    return nlohmann::json::parse(fileText);
}

std::string to_string_leading_zeroes(unsigned int number, unsigned int length)
{
    
     std::string num_str = std::to_string(number);
    
    if(num_str.length() >= length) return num_str;
    
     std::string leading_zeros(length - num_str.length(), '0');
    
    return leading_zeros + num_str;
}

bool doBoxesHit(Box box1, Box box2)
{
    if (box1.x + box1.w < box2.x) {
        return false;
    }
    if (box2.x + box2.w < box1.x) {
        return false;
    }
    if (box1.y + box1.h < box2.y) {
        return false;
    }
    if (box2.y + box2.h < box1.y) {
        return false;
    }
    //log ("boxes hit! " + std::to_string(box1.x + box1.w - box2.x));
    return true;
}
 
std::deque<std::string> logQueue;

void log(std::string logLine)
{
    logQueue.push_back(logLine);
    if (logQueue.size() > 15) {
        logQueue.pop_front();
    }
}

float startPos1 = -340.0;
float startPos2 = -140.0;

int main(int argc, char**argv)
{
    srand(time(NULL));
    auto window = initWindowRender();
    initUI();
    ImGuiIO& io = ImGui::GetIO(); // why doesn't the one from initUI work? who knows
    initRenderUI();

    if ( argc > 1 ) {
        Guy *pNewGuy = new Guy(argv[1], startPos1, 0.0, 1, {randFloat(), randFloat(), randFloat()} );
        *pNewGuy->getInputIDPtr() = keyboardID;
        *pNewGuy->getInputListIDPtr() = 1; // its spot in the UI, or it'll override it :/
        guys.push_back(pNewGuy);

        if ( argc > 2 ) {
            pNewGuy = new Guy(argv[2], startPos2, 0.0, -1, {randFloat(), randFloat(), randFloat()} );
            guys.push_back(pNewGuy);

            pNewGuy->setOpponent(guys[0]);
            guys[0]->setOpponent(pNewGuy);
        }
    }

    nlohmann::json inputTimeline = parse_json_file("timeline.json");
    if (inputTimeline != nullptr) {
        recordedInput = inputTimeline.get<std::vector<int>>();
    }

    nlohmann::json gameStateDump = parse_json_file("game_state_dump.json");
    if (gameStateDump != nullptr) {
        int i = 0;
        while (i < (int)gameStateDump.size()) {
            if (gameStateDump[i]["playTimer"] != 0 && gameStateDump[i]["players"][0]["actionID"] != 0) {
                break;
            }
            i++;
        }
        if (guys.size() >= 2) {
            guys[0]->resetPosDebug(gameStateDump[i]["players"][0]["posX"], gameStateDump[0]["players"][0]["posY"]);
            *guys[0]->getInputIDPtr() = replayLeft;
            *guys[0]->getInputListIDPtr() = replayLeft;
            guys[1]->resetPosDebug(gameStateDump[i]["players"][1]["posX"], gameStateDump[0]["players"][1]["posY"]);
            *guys[1]->getInputIDPtr() = replayRight;
            *guys[1]->getInputListIDPtr() = replayRight;
        }
        gameStateFrame = gameStateDump[i]["frameCount"];
        replayingGameState = true;
        currentInputMap[replayLeft] = 0;
        currentInputMap[replayRight] = 0;
    }

    uint32_t frameStartTime = SDL_GetTicks();

    currentInputMap[nullInputID] = 0;
    currentInputMap[keyboardID] = 0;
    currentInputMap[recordingID] = 0;

    while (!done)
    {
        const float desiredFrameTimeMS = 1000.0 / 60.0f;
        uint32_t currentTime = SDL_GetTicks();
        if ((limitRate || (!playingBackInput && !replayingGameState)) && currentTime - frameStartTime < desiredFrameTimeMS) {
            const float timeToSleepMS = (desiredFrameTimeMS - (currentTime - frameStartTime));
            usleep(timeToSleepMS * 1000 - 100);
        }
        frameStartTime = SDL_GetTicks();

        globalFrameCount++;

        updateInputs();

        if (recordingInput) {
            recordedInput.push_back(currentInputMap[keyboardID]);
        }

        bool hasInput = true;

        if (playingBackInput) {
            if (oneframe || !paused) {
                if (playBackFrame >= (int)playBackInputBuffer.size()) {
                    playingBackInput = false;
                    playBackFrame = 0;
                } else {
                    currentInputMap[recordingID] = playBackInputBuffer[playBackFrame++];
                    //log ("input from playback! " + std::to_string(currentInput));
                }
            } else {
                hasInput = false;
            }
        }

        std::vector<Guy *> everyone;

        for (auto guy : guys) {
            everyone.push_back(guy);
            for ( auto minion : guy->getMinions() ) {
                everyone.push_back(minion);
            }
        }

        int frameGuyCount = 0;
        if (oneframe || !paused) {
            for (auto guy : everyone) {
                if (guy->PreFrame()) {
                    frameGuyCount++;
                }
            }
        }

        // gather everyone again in case of deletions/additions in PreFrame
        everyone.clear(); 
        for (auto guy : guys) {
            everyone.push_back(guy);
            for ( auto minion : guy->getMinions() ) {
                everyone.push_back(minion);
            }
        }

        if (frameGuyCount == 0) {
            globalFrameCount--; // don't count that frame, useful for comparing logs to frame data
        }

        bool push = true; // only push the first guy until this actually works
        if (oneframe || !paused) {
            // everyone gets world physics
            for (auto guy : everyone) {
                guy->WorldPhysics();
            }
            for (auto guy : everyone) {
                guy->CheckHit(guy->getOpponent());
            }
            // push after hit for fbs to work, but i think it's really that there's some pushboxes on hit only
            // see all air normals?
            for (auto guy : everyone) {
                if ( push ) {
                    guy->Push(guy->getOpponent());
                    push = false;
                }
            }
        }

        if (replayingGameState) {
            if (oneframe || !paused) {
                static bool firstFrame = true;
                int targetDumpFrame = gameStateFrame;
                if (firstFrame) {
                    firstFrame = false;
                } else {
                    float dumpPosXLeft = gameStateDump[targetDumpFrame]["players"][0]["posX"];
                    float diffLeft = guys[0]->getPosX() - dumpPosXLeft;
                    float dumpPosXRight = gameStateDump[targetDumpFrame]["players"][1]["posX"];
                    float diffRight = guys[1]->getPosX() - dumpPosXRight;
                    if (fabsf(diffLeft) > 0.01 || fabsf(diffRight) > 0.01) {
                        log("guy 0 pos " + std::to_string(guys[0]->getPosX()) + " game pos " + std::to_string(dumpPosXLeft) + " diff " + std::to_string(diffLeft));
                        log("guy 1 pos " + std::to_string(guys[1]->getPosX()) + " game pos " + std::to_string(dumpPosXRight) + " diff " + std::to_string(diffRight));
                        replayErrors++;
                    }
                    int actionLeft = gameStateDump[gameStateFrame]["players"][0]["actionID"];
                    int actionRight = gameStateDump[gameStateFrame]["players"][1]["actionID"];
                    if (guys[0]->getCurrentAction() != actionLeft || guys[1]->getCurrentAction() != actionRight) {
                        log("guy 0 action " + std::to_string(guys[0]->getCurrentAction()) + " " + guys[0]->getActionName() + " game action " + std::to_string(actionLeft) + " " + guys[0]->getActionName(actionLeft));
                        log("guy 1 action " + std::to_string(guys[1]->getCurrentAction()) + " " + guys[1]->getActionName() + " game action " + std::to_string(actionRight) + " " + guys[1]->getActionName(actionRight));
                        // replayErrors++;
                    }
                    gameStateFrame++;
                }
            }
        }

        if (replayingGameState) {
            if (oneframe || !paused) {
                if (gameStateFrame >= (int)gameStateDump.size()) {
                    replayingGameState = false;
                    gameStateFrame = 0;
                    log("game replay finished, errors: " + std::to_string(replayErrors));
                } else {
                    int inputLeft = gameStateDump[gameStateFrame]["players"][0]["currentInput"];
                    int inputRight = gameStateDump[gameStateFrame]["players"][1]["currentInput"];
                    inputLeft = addPressBits( inputLeft, currentInputMap[replayLeft] );
                    inputRight = addPressBits( inputRight, currentInputMap[replayRight] );
                    currentInputMap[replayLeft] = inputLeft;
                    currentInputMap[replayRight] = inputRight;
                }
            } else {
                hasInput = false;
            }
        }

        for (auto guy : guys) {
            if ( hasInput ) {
                int input = 0;
                if (currentInputMap.count(*guy->getInputIDPtr())) {
                    input = currentInputMap[*guy->getInputIDPtr()];
                }
                guy->Input( input );
            }
        }

        if (oneframe || !paused) { 
            for (auto guy : everyone) {
                bool die = !guy->Frame();

                if (die) {
                    delete guy;
                }
            }
        }

        color clearColor = {0.0,0.0,0.0};

        setRenderState(clearColor, (int)io.DisplaySize.x, (int)io.DisplaySize.y);

        renderUI(io.Framerate, &logQueue);

        // gather everyone again in case of deletions in Frame
        everyone.clear(); 
        for (auto guy : guys) {
            everyone.push_back(guy);
            for ( auto minion : guy->getMinions() ) {
                everyone.push_back(minion);
            }
        }

        for (auto guy : everyone) {
            guy->Render();
        }

        renderMarkersAndStuff();

        if (resetpos) {
            if (guys.size() > 0 ) {
                guys[0]->resetPosDebug(startPos1, 0.0f);
            }
            if (guys.size() > 1 ) {
                guys[1]->resetPosDebug(startPos2, 0.0f);
            }
        }

        SDL_GL_SwapWindow(window);

        resetpos = false;
        oneframe = false;
        static int lasterrorcount = replayErrors;
        if (lasterrorcount != replayErrors) {
            paused = true; // cant pause in the middle above
        }
    }

    // save timeline buffer to disk
    timelineToInputBuffer(playBackInputBuffer);
    nlohmann::json jsonInput(playBackInputBuffer);
    writeFile("timeline.json", nlohmann::to_string(jsonInput));

    for (auto guy : guys)
    {
        delete guy;
    }
    guys.clear();

    destroyUI();
    destroyRender();

    return 0;
}
