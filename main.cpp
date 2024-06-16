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

const char* charNames[] = {
    "ryu",
    "ken",
    "luke",
    "honda",
    "zangief",
    "manon",
    "ed",
    "jp",
    "guile",
    "rashid",
    "aki",
    "chunli",
    "lily",
    "deejay",
    "cammy",
    "dhalsim",
    "kimberly",
    "jamie",
    "marisa",
    "blanka",
    "akuma"
};
const int charNameCount = IM_ARRAYSIZE(charNames);

const char* charVersions[] = {
    { "19 - pre-S2" },
    { "20 - S2 (akuma patch)" },
    { "21 - S2 hotfix 1" },
    { "22 - S2 hotfix 2" }
};
const int charVersionCount = IM_ARRAYSIZE(charVersions);

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
int firstGameStateFrame = 0;
int replayErrors = 0;

void compareGameState( float dumpValue, float realValue, bool fatal, std::string description )
{
    float valueDiff = realValue - dumpValue;
    const float epsilon = 0.01;

    if (fabsf(valueDiff) > epsilon)
    {
        log("replay error: " + description + " dump: " + std::to_string(dumpValue) + " real: " + std::to_string(realValue) + " diff: " + std::to_string(valueDiff));
        if (fatal) {
            replayErrors++;
        }
    }
}

void compareGameState( int dumpValue, int realValue, bool fatal, std::string description )
{
    if (dumpValue != realValue)
    {
        log("replay error: " + description + " dump: " + std::to_string(dumpValue) + " real: " + std::to_string(realValue));
        if (fatal) {
            replayErrors++;
        }
    }
}

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

nlohmann::json parseCharFile(const std::string &path, const std::string &charName, int version, const std::string &jsonName)
{
    std::string fileName;
    bool foundFile = false;

    // find initial version slot for passed version number
    int versionSlot = charVersionCount - 1;
    while (versionSlot >= 0) {
        if (atoi(charVersions[versionSlot]) == version) {
            break;
        }
        versionSlot--;
    }
    if (versionSlot < 0) {
        return nullptr;
    }
    while (versionSlot >= 0) {
        fileName = path + charName + std::to_string(atoi(charVersions[versionSlot])) + "_" + jsonName + ".json";
        if (std::filesystem::exists(fileName)) {
            foundFile = true;
            break;
        }
        versionSlot--;
    }
    if (!foundFile) {
        return nullptr;
    }

    return parse_json_file(fileName);
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

void extractCharVersion(char *cmdLine, std::string &charName, int &version)
{
    uint32_t i = 0;
    while (i < strlen(cmdLine)) {
        if (cmdLine[i] >= '0' && cmdLine[i] <= '9') {
            version = atoi(&cmdLine[i]);
            cmdLine[i] = 0;
            break;
        }
        i++;
    }
    charName = cmdLine;
}

int main(int argc, char**argv)
{
    srand(time(NULL));
    auto window = initWindowRender();
    initUI();
    ImGuiIO& io = ImGui::GetIO(); // why doesn't the one from initUI work? who knows
    initRenderUI();

    int maxVersion = atoi(charVersions[charVersionCount - 1]);

    std::string charNameLeft = (char*)charNames[rand() % charNameCount];
    int versionLeft = maxVersion;
    std::string charNameRight = (char*)charNames[rand() % charNameCount];
    int versionRight = maxVersion;

    if ( argc > 1 ) {
        extractCharVersion( argv[1], charNameLeft, versionLeft );
    }
    if ( argc > 2 ) {
        extractCharVersion( argv[2], charNameRight, versionRight );
    }
    Guy *pNewGuy = new Guy(charNameLeft, versionLeft, -150.0, 0.0, 1, {randFloat(), randFloat(), randFloat()} );
    *pNewGuy->getInputIDPtr() = keyboardID;
    *pNewGuy->getInputListIDPtr() = 1; // its spot in the UI, or it'll override it :/
    guys.push_back(pNewGuy);

    pNewGuy = new Guy(charNameRight, versionRight, 150.0, 0.0, -1, {randFloat(), randFloat(), randFloat()} );
    guys.push_back(pNewGuy);

    int curChar = 3;
    while (curChar < argc) {
        std::string charName;
        int charVersion = maxVersion;
        extractCharVersion( argv[curChar], charName, charVersion );
        pNewGuy = new Guy(charName, charVersion, 0.0, 0.0, 1, {randFloat(), randFloat(), randFloat()} );
        guys.push_back(pNewGuy);
        curChar++;
    }

    pNewGuy->setOpponent(guys[0]);
    guys[0]->setOpponent(pNewGuy);

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
            guys[0]->setPos(gameStateDump[i]["players"][0]["posX"], gameStateDump[0]["players"][0]["posY"]);
            *guys[0]->getInputIDPtr() = replayLeft;
            *guys[0]->getInputListIDPtr() = replayLeft;
            guys[1]->setPos(gameStateDump[i]["players"][1]["posX"], gameStateDump[0]["players"][1]["posY"]);
            *guys[1]->getInputIDPtr() = replayRight;
            *guys[1]->getInputListIDPtr() = replayRight;

            gameStateFrame = gameStateDump[i]["frameCount"];
            firstGameStateFrame = gameStateFrame;
            replayingGameState = true;
            currentInputMap[replayLeft] = 0;
            currentInputMap[replayRight] = 0;
        }
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
                int targetDumpFrame = gameStateFrame - firstGameStateFrame;
                if (firstFrame) {
                    firstFrame = false;
                } else {
                    auto players = gameStateDump[targetDumpFrame]["players"];
                    int i = 0;
                    while (i < 2) {
                        std::string desc = "player " + std::to_string(i);
                        compareGameState(players[i]["posX"], guys[i]->getPosX(), true, desc + " pos X");
                        compareGameState(players[i]["posY"], guys[i]->getPosY(), true, desc + " pos Y");
                        float velX, velY, accelX, accelY;
                        guys[i]->getVel(velX, velY, accelX, accelY);
                        compareGameState(players[i]["velX"], velX, true, desc + " vel X");
                        compareGameState(players[i]["velY"], velY, true, desc + " vel Y");
                        //compareGameState(players[i]["accelX"], accelX, true, desc + " accel X");
                        compareGameState(players[i]["accelY"], accelY, true, desc + " accel Y");

                        compareGameState(players[i]["actionID"], guys[i]->getCurrentAction(), false, desc + " action ID");
                        compareGameState(players[i]["actionFrame"], guys[i]->getCurrentFrame(), false, desc + " action frame");

                        i++;
                    }

                    gameStateFrame++;
                }
            }
        }

        if (replayingGameState) {
            if (oneframe || !paused) {
                int targetDumpFrame = gameStateFrame - firstGameStateFrame;
                if (targetDumpFrame >= (int)gameStateDump.size()) {
                    replayingGameState = false;
                    gameStateFrame = 0;
                    firstGameStateFrame = 0;
                    log("game replay finished, errors: " + std::to_string(replayErrors));
                } else {
                    int inputLeft = gameStateDump[targetDumpFrame]["players"][0]["currentInput"];
                    inputLeft = addPressBits( inputLeft, currentInputMap[replayLeft] );
                    currentInputMap[replayLeft] = inputLeft;

                    // training dumps don't have input for player 2
                    if (gameStateDump[targetDumpFrame]["players"][1].contains("currentInput")) {
                        int inputRight = gameStateDump[targetDumpFrame]["players"][1]["currentInput"];
                        inputRight = addPressBits( inputRight, currentInputMap[replayRight] );
                        currentInputMap[replayRight] = inputRight;
                    }
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

        // find camera position if we have 2 guys
        if (guys.size() >= 2) {
            float distGuys = fabs( guys[1]->getPosX() - guys[0]->getPosX() );
            distGuys += 50.0; // account for some buffer behind
            float angleRad = 90 - (fov / 2.0);
            angleRad *= (M_PI / 360.0);
            zoom = distGuys / 2.0 / tanf( angleRad );
            translateX = (guys[1]->getPosX() + guys[0]->getPosX()) / 2.0;
            zoom = fmax(zoom, 360.0);
            translateX = fmin(translateX, 550.0);
            translateX = fmax(translateX, -550.0);
            //log("zoom " + std::to_string(zoom) + " translateX " + std::to_string(translateX) + " translateY " + std::to_string(translateY));
        }

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
            uint32_t i = 0;
            while (i < guys.size()) {
                guys[i]->resetPos();
                i++;
            }
        }

        SDL_GL_SwapWindow(window);

        resetpos = false;
        oneframe = false;
        static int lasterrorcount = replayErrors;
        if (lasterrorcount != replayErrors) {
            paused = true; // cant pause in the middle above
            lasterrorcount = replayErrors;
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
