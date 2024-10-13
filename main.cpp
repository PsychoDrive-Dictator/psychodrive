#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
#include "zip.h"

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
    "juri",
    "lily",
    "deejay",
    "cammy",
    "dhalsim",
    "kimberly",
    "jamie",
    "marisa",
    "blanka",
    "akuma",
    "dictator",
    "terry"
};
const int charNameCount = IM_ARRAYSIZE(charNames);

const char* charVersions[] = {
    { "19 - pre-S2" },
    { "20 - S2 (akuma patch)" },
    { "21 - S2 hotfix 1" },
    { "22 - S2 hotfix 2" },
    { "23 - S2 (dictator patch)" },
    { "24 - S2 (terry patch)" }
};
const int charVersionCount = IM_ARRAYSIZE(charVersions);

bool resetpos = false;

std::vector<Guy *> guys;

bool done = false;
bool paused = false;
bool oneframe = false;
int globalFrameCount = 0;
int replayFrameNumber = 0;

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

std::vector<normalRangePlotEntry> vecPlotEntries;
int curPlotEntryID = -1;
int curPlotEntryStartFrame = 0;
int curPlotEntryNormalStartFrame = 0;
int curPlotActionID = 0;

void compareGameStateFixed( Fixed dumpValue, Fixed realValue, bool fatal, std::string description )
{

    if (dumpValue != realValue)
    {
        float valueDiff = realValue.f() - dumpValue.f();
        std::string dumpValueStr = std::to_string(dumpValue.data) + " / " + std::to_string(dumpValue.f());
        std::string simValueStr = std::to_string(realValue.data) + " / " + std::to_string(realValue.f());
        log("replay error: " + description + " dump: " + dumpValueStr + " sim: " + simValueStr + " diff: " + std::to_string(valueDiff));
        if (fatal) {
            replayErrors++;
        }
    }
}

void compareGameStateInt( int64_t dumpValue, int64_t realValue, bool fatal, std::string description )
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

std::unordered_map<std::string, struct zip_t*> charZipFiles;

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

nlohmann::json *loadCharFile(const std::string &path, const std::string &charName, int version, const std::string &jsonName)
{
    std::string charFileName;
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
        charFileName = charName + std::to_string(atoi(charVersions[versionSlot])) + "_" + jsonName + ".json";
        if (charFileExists(path, charName, charFileName)) {
            foundFile = true;
            break;
        }
        versionSlot--;
    }
    if (!foundFile) {
        return nullptr;
    }

    static std::unordered_map<std::string, nlohmann::json> mapCharFileLoader;

    if (mapCharFileLoader.find(charFileName) == mapCharFileLoader.end()) {
        bool loaded = false;
        // try loading from loose file first
        std::string looseFileName = path + charFileName;
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
    //log ("boxes hit! " + std::to_string(box1.x.f() + box1.w.f() - box2.x.f()));
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

uint32_t frameStartTime;
std::vector<Guy *> vecGuysToDelete;
nlohmann::json gameStateDump;
ImGuiIO *io;
SDL_Window *sdlwindow;

static void mainloop(void)
{
    if (done) {
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

#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#else
        exit(0);
#endif
    }

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
    bool runFrame = oneframe || !paused;

    if (playingBackInput) {
        if (runFrame) {
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

    if (runFrame) {
        for (auto guy : vecGuysToDelete) {
            delete guy;
        }
        vecGuysToDelete.clear();
    }

    for (auto guy : guys) {
        everyone.push_back(guy);
        for ( auto minion : guy->getMinions() ) {
            everyone.push_back(minion);
        }
    }

    int frameGuyCount = 0;
    if (runFrame) {
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

    if (runFrame) {
        for (auto guy : everyone) {
            guy->WorldPhysics();
        }
        for (auto guy : everyone) {
            guy->Push(guy->getOpponent());
        }
        for (auto guy : everyone) {
            guy->CheckHit(guy->getOpponent());
        }

        static bool hasAddedData = false;
        // update plot range if we're doing that
        if (guys.size() > 0 ) {
            if (curPlotActionID == 0 && guys[0]->getIsDrive() == true)
            {
                curPlotEntryStartFrame = globalFrameCount;
                curPlotActionID = guys[0]->getCurrentAction();
                vecPlotEntries.push_back({});
                curPlotEntryID++;
            }
            if (curPlotActionID != 0 && guys[0]->getCurrentAction() != 1 && guys[0]->getIsDrive() == false)
            {
                if (curPlotEntryNormalStartFrame == 0) {
                    curPlotEntryNormalStartFrame = globalFrameCount - curPlotEntryStartFrame;
                }
                std::vector<HitBox> *hitBoxes = guys[0]->getHitBoxes();
                Fixed maxXHitBox = Fixed(0);
                for (auto hitbox : *hitBoxes) {
                    if (hitbox.type != hitBoxType::hit) continue;
                    Fixed hitBoxX = hitbox.box.x + hitbox.box.w;
                    if (hitBoxX > maxXHitBox) {
                        maxXHitBox = hitBoxX;
                    }
                }
                // find a better solution for two-hitting normals
                // if (maxXHitBox != Fixed(0)) {
                //     hasAddedData = true;
                // }
                if (maxXHitBox != Fixed(0) || hasAddedData) {
                    vecPlotEntries[curPlotEntryID].hitBoxRangePlotX.push_back(globalFrameCount - curPlotEntryStartFrame);
                    vecPlotEntries[curPlotEntryID].hitBoxRangePlotY.push_back(maxXHitBox.f());
                }
                if (vecPlotEntries[curPlotEntryID].strName == "") {
                    vecPlotEntries[curPlotEntryID].strName = guys[0]->getCharacter() + " " + guys[0]->getActionName() + " (" + std::to_string(curPlotEntryNormalStartFrame) + "f cancel)";
                    vecPlotEntries[curPlotEntryID].col = guys[0]->getColor();
                }
            }
            if (curPlotActionID != 0 && guys[0]->getCurrentAction() == 1) {
                hasAddedData = false;
                curPlotActionID = 0;
                curPlotEntryStartFrame = 0;
                curPlotEntryNormalStartFrame = 0;
            }
        }
    }

    if (replayingGameState) {
        if (runFrame) {
            static bool firstFrame = true;
            int targetDumpFrame = gameStateFrame - firstGameStateFrame;
            if (firstFrame) {
                firstFrame = false;
            } else {
                auto players = gameStateDump[targetDumpFrame]["players"];
                int i = 0;
                while (i < 2) {
                    std::string desc = "player " + std::to_string(i);
                    compareGameStateFixed(Fixed(players[i]["posX"].get<double>()), guys[i]->getPosX(), true, desc + " pos X");
                    compareGameStateFixed(Fixed(players[i]["posY"].get<double>()), guys[i]->getPosY(), true, desc + " pos Y");
                    Fixed velX, velY, accelX, accelY;
                    guys[i]->getVel(velX, velY, accelX, accelY);
                    compareGameStateFixed(Fixed(players[i]["velX"].get<double>()), velX, true, desc + " vel X");
                    compareGameStateFixed(Fixed(players[i]["velY"].get<double>()), velY, true, desc + " vel Y");
                    if (players[i].contains("accelX")) {
                        compareGameStateFixed(Fixed(players[i]["accelX"].get<double>()), accelX, true, desc + " accel X");
                    }
                    compareGameStateFixed(Fixed(players[i]["accelY"].get<double>()), accelY, true, desc + " accel Y");

                    if (players[i].contains("hitVelX")) {
                        compareGameStateFixed(Fixed(players[i]["hitVelX"].get<double>()), guys[i]->getHitVelX(), true, desc + " hitAccel X");
                        compareGameStateFixed(Fixed(players[i]["hitAccelX"].get<double>()), guys[i]->getHitAccelX(), true, desc + " hitAccel X");
                    }

                    compareGameStateInt(players[i]["actionID"], guys[i]->getCurrentAction(), false, desc + " action ID");
                    compareGameStateInt(players[i]["actionFrame"], guys[i]->getCurrentFrame(), false, desc + " action frame");

                    i++;
                }

                gameStateFrame++;
            }
        }
    }

    if (replayingGameState) {
        if (runFrame) {
            int targetDumpFrame = gameStateFrame - firstGameStateFrame;
            if (targetDumpFrame >= (int)gameStateDump.size()) {
                replayingGameState = false;
                gameStateFrame = 0;
                firstGameStateFrame = 0;
                replayFrameNumber = 0;
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

                if (gameStateDump[targetDumpFrame].contains("stageTimer")) {
                    replayFrameNumber = gameStateDump[targetDumpFrame]["stageTimer"];
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

    if (runFrame) {
        for (auto guy : everyone) {
            bool die = !guy->Frame();

            if (die) {
                vecGuysToDelete.push_back(guy);
            }
        }
    }

    color clearColor = {0.0,0.0,0.0};

    // find camera position if we have 2 guys
    if (guys.size() >= 2) {
        float distGuys = fabs( guys[1]->getPosX().f() - guys[0]->getPosX().f() );
        distGuys += 50.0; // account for some buffer behind
        float angleRad = 90 - (fov / 2.0);
        angleRad *= (M_PI / 360.0);
        zoom = distGuys / 2.0 / tanf( angleRad );
        translateX = (guys[1]->getPosX().f() + guys[0]->getPosX().f()) / 2.0f;
        zoom = fmax(zoom, 360.0);
        translateX = fmin(translateX, 550.0);
        translateX = fmax(translateX, -550.0);
        //log("zoom " + std::to_string(zoom) + " translateX " + std::to_string(translateX) + " translateY " + std::to_string(translateY));
    }

    setRenderState(clearColor, 1920, 1080);

    renderUI(io->Framerate, &logQueue);

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

    SDL_GL_SwapWindow(sdlwindow);

    resetpos = false;
    oneframe = false;
    static int lasterrorcount = replayErrors;
    if (lasterrorcount != replayErrors) {
        paused = true; // cant pause in the middle above
        lasterrorcount = replayErrors;
    }
}   

int main(int argc, char**argv)
{
    srand(time(NULL));
    sdlwindow = initWindowRender();
    initUI();
    io = &ImGui::GetIO(); // why doesn't the one from initUI work? who knows
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
    Guy *pNewGuy = new Guy(charNameLeft, versionLeft, Fixed(-150.0f), Fixed(0.0f), 1, {randFloat(), randFloat(), randFloat()} );
    *pNewGuy->getInputIDPtr() = keyboardID;
    *pNewGuy->getInputListIDPtr() = 1; // its spot in the UI, or it'll override it :/
    guys.push_back(pNewGuy);

    pNewGuy = new Guy(charNameRight, versionRight, Fixed(150.0f), Fixed(0.0f), -1, {randFloat(), randFloat(), randFloat()} );
    guys.push_back(pNewGuy);

    int curChar = 3;
    while (curChar < argc) {
        std::string charName;
        int charVersion = maxVersion;
        extractCharVersion( argv[curChar], charName, charVersion );
        pNewGuy = new Guy(charName, charVersion, Fixed(0), Fixed(0), 1, {randFloat(), randFloat(), randFloat()} );
        guys.push_back(pNewGuy);
        curChar++;
    }

    pNewGuy->setOpponent(guys[0]);
    guys[0]->setOpponent(pNewGuy);

    nlohmann::json inputTimeline = parse_json_file("timeline.json");
    if (inputTimeline != nullptr) {
        recordedInput = inputTimeline.get<std::vector<int>>();
    }

    gameStateDump = parse_json_file("game_state_dump.json");
    if (gameStateDump != nullptr) {
        int i = 0;
        while (i < (int)gameStateDump.size()) {
            if (gameStateDump[i]["playTimer"] != 0 && gameStateDump[i]["players"][0]["actionID"] != 0) {
                break;
            }
            i++;
        }
        if (guys.size() >= 2) {
            guys[0]->setPos(Fixed(gameStateDump[i]["players"][0]["posX"].get<double>()), Fixed(gameStateDump[0]["players"][0]["posY"].get<float>()));
            *guys[0]->getInputIDPtr() = replayLeft;
            *guys[0]->getInputListIDPtr() = replayLeft;
            guys[1]->setPos(Fixed(gameStateDump[i]["players"][1]["posX"].get<double>()), Fixed(gameStateDump[0]["players"][1]["posY"].get<float>()));
            *guys[1]->getInputIDPtr() = replayRight;
            *guys[1]->getInputListIDPtr() = replayRight;

            gameStateFrame = gameStateDump[i]["frameCount"];
            firstGameStateFrame = gameStateFrame;
            replayingGameState = true;
            currentInputMap[replayLeft] = 0;
            currentInputMap[replayRight] = 0;

            if (gameStateDump[i].contains("stageTimer")) {
                replayFrameNumber = gameStateDump[i]["stageTimer"];
            }
        }
    }

    frameStartTime = SDL_GetTicks();

    currentInputMap[nullInputID] = 0;
    currentInputMap[keyboardID] = 0;
    currentInputMap[recordingID] = 0;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainloop, 0, 1);
#else
    while (1) { mainloop(); }
#endif

    return 0;
}
