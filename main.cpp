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
#include <unordered_set>
#include <numbers>

#include <SDL.h>
#include <SDL_opengl.h>

#include "json.hpp"
#include "zip.h"

#include "simulation.hpp"
#include "guy.hpp"
#include "main.hpp"
#include "ui.hpp"
#include "input.hpp"
#include "render.hpp"
#include "combogen.hpp"

EGameMode gameMode = Training;

bool forceCounter = false;
bool forcePunishCounter = false;
int hitStunAdder = 0;

struct charEntry {
    int charID;
    const char *name;
    const char *niceName;
};

std::vector<charEntry> charEntries;
std::vector<const char *> charNames;
std::vector<const char *> charNiceNames;

void makeCharEntries(void)
{
    charEntries.push_back({1, "ryu", "Ryu"});
    charEntries.push_back({2, "luke", "Luke"});
    charEntries.push_back({3, "kimberly", "Kimberly"});
    charEntries.push_back({4, "chunli", "Chun-Li"});
    charEntries.push_back({5, "manon", "Manon"});
    charEntries.push_back({6, "zangief", "Zangief"});
    charEntries.push_back({7, "jp", "JP"});
    charEntries.push_back({8, "dhalsim", "Dhalsim"});
    charEntries.push_back({9, "cammy", "Cammy"});
    charEntries.push_back({10, "ken", "Ken"});
    charEntries.push_back({11, "deejay", "Dee Jay"});
    charEntries.push_back({12, "lily", "Lily"});
    charEntries.push_back({13, "aki", "A.K.I."});
    charEntries.push_back({14, "rashid", "Rashid"});
    charEntries.push_back({15, "blanka", "Blanka"});
    charEntries.push_back({16, "juri", "Juri"});
    charEntries.push_back({17, "marisa", "Marisa"});
    charEntries.push_back({18, "guile", "Guile"});
    charEntries.push_back({19, "ed", "Ed"});
    charEntries.push_back({20, "honda", "E. Honda"});
    charEntries.push_back({21, "jamie", "Jamie"});
    charEntries.push_back({22, "akuma", "Akuma"});
    charEntries.push_back({25, "sagat", "Sagat"});
    charEntries.push_back({26, "dictator", "M. Bison"});
    charEntries.push_back({27, "terry", "Terry"});
    charEntries.push_back({28, "mai", "Mai"});
    charEntries.push_back({29, "elena", "Elena"});
    charEntries.push_back({30, "viper", "C. Viper"});

    for (charEntry &entry : charEntries ) {
        charNames.push_back(entry.name);
        charNiceNames.push_back(entry.niceName);
    }
}

const char *getCharNameFromID(int charID)
{
    for (charEntry &entry : charEntries ) {
        if (entry.charID == charID) {
            return entry.name;
        }
    }

    return nullptr;
}

int getCharIDFromName(const char *charName)
{
    for (charEntry &entry : charEntries) {
        if (strcmp(entry.name, charName) == 0) {
            return entry.charID;
        }
    }

    return 0;
}

const char* charVersions[] = {
    "19 - S1 February Update",
    "20 - S2 Akuma Update",
    "21 - S2 Hotfix 1",
    "22 - S2 Hotfix 2",
    "23 - S2 M. Bison Update",
    "24 - S2 Terry Update",
    "25 - S2 December Update",
    "26 - S2 Mai Update",
    "30 - S3 Elena Update + Hotfix 1",
    "31 - S3 Hotfix 2",
    "32 - S3 Hotfix 3",
    "33 - S3 Hotfix 4",
    "34 - S3 Sagat Update",
    "35 - S3 Sagat Hotfix 1",
    "36 - S3 Viper Update",
    "37 - S3 Viper Hotfix 1",
    "38 - S3 Viper Hotfix 2",
    "39 - S3 December Update",
};
const int charVersionCount = IM_ARRAYSIZE(charVersions);

bool resetpos = false;

Simulation defaultSim;
Simulation snapShotSim;
std::vector<Guy *> &guys = defaultSim.simGuys;

std::unordered_set<std::string> setCharsLoaded;
std::unordered_set<std::string> setCharsStarted;

struct guyCreateInfo_t {
    std::string charName;
    int charVersion;
    Fixed x = Fixed(0);
    Fixed y = Fixed(0);
    int startDir;
    color col;
};

std::vector<guyCreateInfo_t> vecDeferredCreateGuys;
bool newCharLoaded = false;

void createGuyNow(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color)
{
    Guy *pNewGuy = new Guy(&defaultSim, charName, charVersion, x, y, startDir, color);

    if (guys.size()) {
        pNewGuy->setOpponent(guys[0]);
        if (guys.size() == 1) {
            guys[0]->setOpponent(pNewGuy);
        }
    } else if (gameMode == Training) {
        *pNewGuy->getInputIDPtr() = keyboardID;
        *pNewGuy->getInputListIDPtr() = 1; // its spot in the UI, or it'll override it :/
    }
    defaultSim.simGuys.push_back(pNewGuy);
}

bool isCharLoaded(std::string charName)
{
    if (charName == "") {
        return false;
    }
#ifdef __EMSCRIPTEN__
    return setCharsLoaded.find(charName) != setCharsLoaded.end();
#else
    return true;
#endif
}

void requestCharDownload(std::string charName)
{
    if (charName == "") {
        return;
    }
#ifdef __EMSCRIPTEN__
    if (isCharLoaded(charName)) {
        return;
    }
    if (setCharsStarted.find(charName) != setCharsStarted.end()) {
        return;
    }
    log("downloading " + std::string(charName));
    setCharsStarted.insert(charName);
    EM_ASM({
        var script = document.createElement('script');
        var charName = UTF8ToString($0);
        script.onload = function () {
            Module.ccall('jsCharLoadCallback',
            null,
            ['string'],
            [charName]);
        };
        script.src = './psychodrive_char_' + charName + '.js';
        script.async = true;

        document.head.appendChild(script);
    }, charName.c_str());
#endif
}

void createGuy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color)
{
#ifdef __EMSCRIPTEN__
    if (setCharsLoaded.find(charName) == setCharsLoaded.end()) {
        bool needDownload = true;
        for (auto deferred : vecDeferredCreateGuys) {
            if (deferred.charName == charName)
                needDownload = false;
        }

        vecDeferredCreateGuys.push_back({charName,charVersion,x,y,startDir,color});

        if (!needDownload)
            return;

        requestCharDownload(charName);
    } else {
        createGuyNow(charName, charVersion, x, y, startDir, color);
    }
#else
    createGuyNow(charName, charVersion, x, y, startDir, color);
#endif
}

void doDeferredCreateGuys(void)
{
    std::vector<guyCreateInfo_t> vecDeferredNotDoneYet;
    if (newCharLoaded) {
        for (auto deferredCreate : vecDeferredCreateGuys) {
            if (setCharsLoaded.find(deferredCreate.charName) != setCharsLoaded.end()) {
                createGuyNow(deferredCreate.charName, deferredCreate.charVersion, deferredCreate.x, deferredCreate.y, deferredCreate.startDir, deferredCreate.col);
            } else {
                vecDeferredNotDoneYet.push_back(deferredCreate);
            }
        }
        vecDeferredCreateGuys = vecDeferredNotDoneYet;
        newCharLoaded = false;
    }
}

extern "C" {

void jsCharLoadCallback(char *charName)
{
    log(std::string(charName) + " download complete");
    if (setCharsLoaded.find(charName) == setCharsLoaded.end()) {
        setCharsLoaded.insert(charName);
        newCharLoaded = true;
    }
}

}

bool done = false;
bool paused = false;
bool oneframe = false;
int globalFrameCount = 0;
int replayFrameNumber = 0;
bool lockCamera = true;
bool toggleRenderUI = true;

bool saveState = false;
bool restoreState = false;

bool comboFinderDoLights = false;
bool comboFinderDoLateCancels = false;
bool comboFinderDoWalk = false;
bool comboFinderDoKaras = false;
bool showComboFinder = false;
bool runComboFinder = false;

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
int fatalErrorKind = -1;

std::vector<normalRangePlotEntry> vecPlotEntries;
int curPlotEntryID = -1;
int curPlotEntryStartFrame = 0;
int curPlotEntryNormalStartFrame = 0;
int curPlotActionID = 0;

void compareGameStateFixed( Fixed dumpValue, Fixed realValue, int errorKind, std::string description )
{
    if (dumpValue != realValue)
    {
        float valueDiff = realValue.f() - dumpValue.f();
        std::string dumpValueStr = std::to_string(dumpValue.data) + " / " + std::to_string(dumpValue.f());
        std::string simValueStr = std::to_string(realValue.data) + " / " + std::to_string(realValue.f());
        log("replay error: " + description + " dump: " + dumpValueStr + " sim: " + simValueStr + " diff: " + std::to_string(valueDiff));
        if (errorKind == fatalErrorKind) {
            replayErrors++;
        }
    }
}

void compareGameStateInt( int64_t dumpValue, int64_t realValue, int errorKind, std::string description )
{
    if (dumpValue != realValue)
    {
        log("replay error: " + description + " dump: " + std::to_string(dumpValue) + " sim: " + std::to_string(realValue));
        if (errorKind == fatalErrorKind) {
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

    static std::unordered_map<std::string, nlohmann::json> mapCharFileLoader;

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

std::string to_string_leading_zeroes(unsigned int number, unsigned int length)
{
    
     std::string num_str = std::to_string(number);
    
    if(num_str.length() >= length) return num_str;
    
     std::string leading_zeros(length - num_str.length(), '0');
    
    return leading_zeros + num_str;
}

std::deque<std::string> logQueue;

extern "C" {

void jsLog(char *logLine)
{
    log("JS:" + std::string(logLine));
}

}

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
color clearColor = {0.0,0.0,0.0};

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

    doDeferredCreateGuys();

    const float desiredFrameTimeMS = 1000.0 / 60.0f;
    uint32_t currentTime = SDL_GetTicks();
    if ((limitRate || (!playingBackInput && !replayingGameState)) && currentTime - frameStartTime < desiredFrameTimeMS) {
        const float timeToSleepMS = (desiredFrameTimeMS - (currentTime - frameStartTime));
        usleep(timeToSleepMS * 1000 - 100);
    }
    frameStartTime = SDL_GetTicks();

    globalFrameCount++;

    int sizeX, sizeY;
    SDL_GetWindowSize(sdlwindow, &sizeX, &sizeY);

    updateInputs(sizeX, sizeY);

    if (gameMode != Training) {
        setRenderState(clearColor, sizeX, sizeY);
        int curFrame = simController.scrubberFrame; // before ui changes it
        renderUI(io->Framerate, &logQueue, sizeX, sizeY);
        if (simInputsChanged && simController.NewSim()) {
            simController.AdvanceUntilComplete();
            // in case it got clamped
            curFrame = simController.scrubberFrame;

            simInputsChanged = false;
        }

        if (!simInputsChanged) {
            Simulation *pFrameSim = simController.getSnapshotAtFrame(curFrame);
            if (pFrameSim) {
                pFrameSim->Render();
            }
            simController.renderRecordedHitMarkers(curFrame);

            if (simController.playing) {
                simController.scrubberFrame += simController.playSpeed;
                if (simController.scrubberFrame == simController.playUntilFrame) {
                    simController.playing = false;
                    simController.playUntilFrame = 0;
                }
                if (simController.scrubberFrame >= simController.simFrameCount) {
                    simController.scrubberFrame = simController.simFrameCount - 1;
                    simController.playing = false;
                    simController.playUntilFrame = 0;
                }
                if (simController.scrubberFrame < 0) {
                    simController.scrubberFrame = 0;
                    simController.playing = false;
                    simController.playUntilFrame = 0;
                }
            }

            // for next frame
            Guy *pLeftGuy = simController.getRecordedGuy(simController.scrubberFrame, 0);
            Guy *pRightGuy = simController.getRecordedGuy(simController.scrubberFrame, 1);
            if (!pRightGuy) {
                pRightGuy = pLeftGuy;
            }
            if (lockCamera && pLeftGuy) {
                translateX = (pRightGuy->getLastPosX().f() + pLeftGuy->getLastPosX().f()) / 2.0f;
                translateX = fmin(translateX, 550.0);
                translateX = fmax(translateX, -550.0);

                // first find required camera distance to have both guys in view horizontally
                float distGuys = fabs( pRightGuy->getLastPosX().f() - pLeftGuy->getLastPosX().f() );
                distGuys += 200.0; // account for some buffer behind
                float angleRad = fov / 2.0 * std::numbers::pi / 180.0;
                // zoom is adjacent edge, equals opposite over tan(ang)
                float zoomToFitGuys = distGuys / 2.0 / tanf( angleRad );

                float vertFovRad = 2.0 * atanf( tanf( ( fov * std::numbers::pi / 180.0 ) / 2.0 ) * ((float)sizeY / (float)sizeX) );
                // we want to see a point 25 units below the chars to clearly see their feet
                float zoomToFitFloor = (translateY + 75.0) / tanf( vertFovRad / 2.0 );

                zoom = fmax( zoomToFitGuys, zoomToFitFloor );
                zoom = fmax( zoom, 250.0f );
            }
        }
    } else {
        if (recordingInput) {
            recordedInput.push_back(currentInputMap[keyboardID]);
        }

        if (saveState) {
            snapShotSim.Clone(&defaultSim);
            saveState = false;
        }

        if (restoreState) {
            defaultSim.Clone(&snapShotSim);
            restoreState = false;
        }

        updateComboFinder();

        if (runComboFinder) {
            findCombos(comboFinderDoLights, comboFinderDoLateCancels, comboFinderDoWalk, comboFinderDoKaras);
            runComboFinder = false;
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

        if (finder.playing && finder.doneRoutes.size() && guys.size()) {
            const DoneRoute &playingRoute = *finder.doneRoutes.rbegin();
            int targetFrame = globalFrameCount - finder.startFrame + 1;
            auto frameTrigger = playingRoute.timelineTriggers.find(targetFrame);
            if (frameTrigger != playingRoute.timelineTriggers.end()) {
                if (frameTrigger->second.actionID() > 0) {
                    guys[0]->getForcedTrigger() = frameTrigger->second;
                    log("forced trigger " + std::to_string(targetFrame));
                } else {
                    int input = -frameTrigger->second.actionID();
                    if (guys[0]->getDirection() < 0) {
                        input = invertDirection(input);
                    }
                    guys[0]->Input(input);
                    hasInput = false; // todo better system for this
                }
            }
        }

        static std::vector<Guy *> everyone;

        if (runFrame) {
            for (auto guy : vecGuysToDelete) {
                delete guy;
            }
            vecGuysToDelete.clear();
        }

        gatherEveryone(guys, everyone);

        if (runFrame) {
            for (auto guy : everyone) {
                if (guy->RunFrame()) {
                }
            }
        }

        // gather everyone again in case of deletions/additions in RunFrame
        gatherEveryone(guys, everyone);

        // time stands still if paused - surely there's a better way to do that
        if (!runFrame) {
            globalFrameCount--;
        }

        if (runFrame) {
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
            gatherEveryone(guys, everyone);

            std::vector<PendingHit> pendingHitList;

            for (auto guy : everyone) {
                guy->CheckHit(guy->getOpponent(), pendingHitList);
            }

            ResolveHits(pendingHitList);

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
                    std::vector<HitBox> hitBoxes;
                    guys[0]->getHitBoxes(&hitBoxes);
                    Fixed maxXHitBox = Fixed(0);
                    for (auto hitbox : hitBoxes) {
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
                        vecPlotEntries[curPlotEntryID].strName = guys[0]->getCharacter() + " " + guys[0]->getActionName(guys[0]->getCurrentAction()) + " (" + std::to_string(curPlotEntryNormalStartFrame) + "f cancel)";
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
                        if (targetDumpFrame > 1) {
                            compareGameStateFixed(Fixed(players[i]["posX"].get<double>()), guys[i]->getPosX(), Simulation::ePos, desc + " pos X");
                            compareGameStateFixed(Fixed(players[i]["posY"].get<double>()), guys[i]->getPosY(), Simulation::ePos, desc + " pos Y");
                        }
                        Fixed velX, velY, accelX, accelY;
                        guys[i]->getVel(velX, velY, accelX, accelY);
                        compareGameStateFixed(Fixed(players[i]["velX"].get<double>()), velX, Simulation::eVel, desc + " vel X");
                        compareGameStateFixed(Fixed(players[i]["velY"].get<double>()), velY, Simulation::eVel, desc + " vel Y");
                        if (players[i].contains("accelX")) {
                            compareGameStateFixed(Fixed(players[i]["accelX"].get<double>()), accelX, Simulation::eAccel, desc + " accel X");
                        }
                        compareGameStateFixed(Fixed(players[i]["accelY"].get<double>()), accelY, Simulation::eAccel, desc + " accel Y");

                        if (players[i].contains("hitVelX")) {
                            compareGameStateFixed(Fixed(players[i]["hitVelX"].get<double>()), guys[i]->getHitVelX(), Simulation::eHitVel, desc + " hitVel X");
                            compareGameStateFixed(Fixed(players[i]["hitAccelX"].get<double>()), guys[i]->getHitAccelX(), Simulation::eHitAccel, desc + " hitAccel X");
                        }

                        compareGameStateInt(players[i]["actionID"], guys[i]->getCurrentAction(), Simulation::eActionID, desc + " action ID");
                        compareGameStateInt(players[i]["actionFrame"], guys[i]->getCurrentFrame(), Simulation::eActionFrame, desc + " action frame");

                        // swap players here, we track combo hits on the opponent
                        compareGameStateInt(players[i]["comboCount"], guys[!i]->getComboHits(), Simulation::eComboCount, desc + " combo");

                        compareGameStateInt((players[i]["bitValue"].get<int>() & (1<<7)) ? 1 : -1, guys[i]->getDirection(), Simulation::eDirection, desc + " direction");

                        if (targetDumpFrame > 0) {
                            nlohmann::json &prevPlayers = gameStateDump[targetDumpFrame-1]["players"];
                            nlohmann::json &firstPlayers = gameStateDump[0]["players"];
                            if (detectTrainingAutoRegen(prevPlayers[i], players[i], firstPlayers[i], "hp", guys[i]->getCharData()->vitality)) {
                                guys[i]->setHealth(players[i]["hp"]);
                            }
                            if (detectTrainingAutoRegen(prevPlayers[i], players[i], firstPlayers[i], "driveGauge", maxFocus)) {
                                guys[i]->guyLog(guys[i]->getLogResources(), "focus training refill detected, focus to " + std::to_string(players[i]["driveGauge"].get<int>()) + " was " + std::to_string(guys[i]->getFocus()));
                                guys[i]->setFocus(players[i]["driveGauge"]);
                                guys[i]->setFocusRegenCooldown(-1);
                            }
                            if (detectTrainingAutoRegen(prevPlayers[i], players[i], firstPlayers[i], "superGauge", guys[i]->getCharData()->gauge)) {
                                guys[i]->setGauge(players[i]["superGauge"]);
                            }
                        }



                        compareGameStateInt(players[i]["hp"], guys[i]->getHealth(), Simulation::eHealth, desc + " health");
                        compareGameStateInt(players[i]["hitStop"], guys[i]->getHitStopForDump(), Simulation::eHitStop, desc + " hitstop");

                        if (players[i].contains("driveGauge")) {
                            compareGameStateInt(players[i]["driveGauge"], guys[i]->getFocus(), Simulation::eGauge, desc + " drive gauge");
                            // if (targetDumpFrame < 240 && (targetDumpFrame + 1) < (int)gameStateDump.size()) {
                            //     nlohmann::json &nextPlayers = gameStateDump[targetDumpFrame+1]["players"];
                            //     int driveDiff = nextPlayers[i]["driveGauge"].get<int>() - players[i]["driveGauge"].get<int>();
                            //     if (driveDiff == 40 || driveDiff == 50 || driveDiff == 20 || driveDiff == 25 || driveDiff == 60 || driveDiff == 70) {
                            //         if (!guys[i]->getHasFocusRegenCooldowned()) {
                            //             guys[i]->setFocusRegenCooldown(1); // start regenning next frame
                            //         }
                            //     }
                            // }
                        }
                        if (players[i].contains("superGauge")) {
                            compareGameStateInt(players[i]["superGauge"], guys[i]->getGauge(), Simulation::eGauge, desc + " super gauge");
                        }

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
                bool die = !guy->AdvanceFrame();

                if (die) {
                    vecGuysToDelete.push_back(guy);
                }
            }
        }

        // find camera position if we have 2 guys
        if (lockCamera && guys.size() >= 2) {
            translateX = (guys[1]->getLastPosX().f() + guys[0]->getLastPosX().f()) / 2.0f;
            translateX = fmin(translateX, 550.0);
            translateX = fmax(translateX, -550.0);

            // first find required camera distance to have both guys in view horizontally
            float distGuys = fabs( guys[1]->getLastPosX().f() - guys[0]->getLastPosX().f() );
            distGuys += 200.0; // account for some buffer behind
            float angleRad = fov / 2.0 * std::numbers::pi / 180.0;
            // zoom is adjacent edge, equals opposite over tan(ang)
            float zoomToFitGuys = distGuys / 2.0 / tanf( angleRad );

            float vertFovRad = 2.0 * atanf( tanf( ( fov * std::numbers::pi / 180.0 ) / 2.0 ) * ((float)sizeY / (float)sizeX) );
            // we want to see a point 25 units below the chars to clearly see their feet
            float zoomToFitFloor = (translateY + 25.0) / tanf( vertFovRad / 2.0 );

            zoom = fmax( zoomToFitGuys, zoomToFitFloor );
            zoom = fmax( zoom, 250.0f );
        }
        
        //log("zoom " + std::to_string(zoom) + " translateX " + std::to_string(translateX) + " translateY " + std::to_string(translateY));

        // gather everyone again in case of deletions/additions in renderUI
        everyone.clear(); 
        for (auto guy : guys) {
            everyone.push_back(guy);
            for ( auto minion : guy->getMinions() ) {
                everyone.push_back(minion);
            }
        }

        // gather everyone again in case of deletions/additions in renderUI
        everyone.clear();
        for (auto guy : guys) {
            everyone.push_back(guy);
            for ( auto minion : guy->getMinions() ) {
                everyone.push_back(minion);
            }
        }

        setRenderState(clearColor, sizeX, sizeY);
        renderUI(io->Framerate, &logQueue, sizeX, sizeY);

        renderComboFinder();
        for (auto guy : everyone) {
            guy->Render();
        }

        if (resetpos) {
            uint32_t i = 0;
            while (i < guys.size()) {
                guys[i]->resetPos();
                i++;
            }
        }

        resetpos = false;
        oneframe = false;
        static int lasterrorcount = replayErrors;
        if (lasterrorcount != replayErrors) {
            paused = true; // cant pause in the middle above
            lasterrorcount = replayErrors;
        }
    }

    renderMarkersAndStuff();

    setScreenSpaceRenderState(sizeX, sizeY);
    renderTouchControls(sizeX, sizeY);

    SDL_GL_SwapWindow(sdlwindow);
}

int main(int argc, char**argv)
{
    srand(time(NULL));
    makeCharEntries();

    bool loadingDump = false;
    std::string strDumpLoadPath;
    int dumpVersion = -1;

    if ( argc > 2 && std::string(argv[1]) == "run_dump") {
        gameMode = Batch;
        Simulation dumpSim;
        int version = -1;
        if ( argc > 3 ) {
            version = atoi(argv[3]);
        }
        dumpSim.SetupFromGameDump(argv[2], version);

        if (std::string(argv[2]).find("guile_jp_66_hit_guile_combo") != std::string::npos) {
            forcePunishCounter = true;
        }
        if (std::string(argv[2]).find("viper_sa1_dump_route") != std::string::npos) {
            forcePunishCounter = true;
        }

        while (true) {
            dumpSim.RunFrame();
            dumpSim.AdvanceFrame();
            if (dumpSim.replayingGameStateDump == false) {
                exit(0);
            }
        }
    }

    if ( argc > 2 && std::string(argv[1]) == "load_dump") {
        strDumpLoadPath = argv[2];
        loadingDump = true;
        if (argc > 3) {
            dumpVersion = atoi(argv[3]);
            if (argc > 4) {
                fatalErrorKind = atoi(argv[4]);
            }
        }
    }

    sdlwindow = initWindowRender();
    initUI();
    io = &ImGui::GetIO(); // why doesn't the one from initUI work? who knows
    initRenderUI();

    int maxVersion = atoi(charVersions[charVersionCount - 1]);
    if (argc > 1)
        gameMode = Training;
    
    if (gameMode == Training && loadingDump == false) {
        std::string charNameLeft = (char*)charNames[rand() % charNames.size()];
        int versionLeft = maxVersion;
        std::string charNameRight = (char*)charNames[rand() % charNames.size()];
        int versionRight = maxVersion;

        if ( argc > 1 ) {
            extractCharVersion( argv[1], charNameLeft, versionLeft );
        }
        if ( argc > 2 ) {
            extractCharVersion( argv[2], charNameRight, versionRight );
        }
        createGuy(charNameLeft, versionLeft, Fixed(-150.0f), Fixed(0.0f), 1, {randFloat(), randFloat(), randFloat()} );
        createGuy(charNameRight, versionRight, Fixed(150.0f), Fixed(0.0f), -1, {randFloat(), randFloat(), randFloat()} );

        int curChar = 3;
        while (curChar < argc) {
            std::string charName;
            int charVersion = maxVersion;
            extractCharVersion( argv[curChar], charName, charVersion );
            createGuy(charName, charVersion, Fixed(0), Fixed(0), 1, {randFloat(), randFloat(), randFloat()} );
            curChar++;
        }
    }

    nlohmann::json inputTimeline = parse_json_file("timeline.json");
    if (inputTimeline != nullptr) {
        recordedInput = inputTimeline.get<std::vector<int>>();
    }

    gameStateDump = nullptr;
    if (loadingDump) {
        gameStateDump = parse_json_file(strDumpLoadPath);
    }
    if (gameStateDump != nullptr) {
        if (strDumpLoadPath.find("guile_jp_66_hit_guile_combo") != std::string::npos) {
            forcePunishCounter = true;
        }
        if (strDumpLoadPath.find("viper_sa1_dump_route") != std::string::npos) {
            forcePunishCounter = true;
        }
        int i = 0;
        while (i < (int)gameStateDump.size()) {
            if (gameStateDump[i]["playTimer"] != 0 && gameStateDump[i]["players"][0]["actionID"] != 0) {
                break;
            }
            i++;
        }
        if (dumpVersion == -1) {
            // fall back to latest
            dumpVersion = maxVersion;
        }
        int playerID = 0;
        while (playerID < 2) {
            nlohmann::json &playerJson = gameStateDump[i]["players"][playerID];
            std::string charName = getCharNameFromID(playerJson["charID"]);
            Fixed posX = Fixed(playerJson["posX"].get<double>());
            Fixed posY = Fixed(playerJson["posY"].get<double>());
            int bitValue = playerJson["bitValue"];
            int charDirection = (bitValue & 1<<7) ? 1 : -1;
            color charColor = {randFloat(), randFloat(), randFloat()};
            createGuy(charName, dumpVersion, posX, posY, charDirection, charColor );
            playerID++;
        }

        while (guys.size() < 2) {
            usleep(200 * 1000);
            doDeferredCreateGuys();
        }

        playerID = 0;
        while (playerID < 2) {
            nlohmann::json &playerJson = gameStateDump[i]["players"][playerID];
            int actionID = playerJson["actionID"];
            int actionFrame = playerJson["actionFrame"];
            if (actionID == 17) {
                // guile66hit combo has p2 in mid-dash and we don't capture posoffset
                actionID = 1;
                actionFrame = 0;
            }
            guys[playerID]->setAction(actionID, actionFrame - 1);
            guys[playerID]->setHealth(playerJson["hp"]);
            guys[playerID]->setFocus(playerJson.value("driveGauge", maxFocus));
            guys[playerID]->setFocusRegenCooldown(-1, false);
            guys[playerID]->setGauge(playerJson.value("superGauge", guys[playerID]->getCharData()->gauge));

            playerID++;
        }

        *guys[0]->getInputIDPtr() = replayLeft;
        *guys[0]->getInputListIDPtr() = replayLeft;
        *guys[1]->getInputIDPtr() = replayRight;
        *guys[1]->getInputListIDPtr() = replayRight;

        gameStateFrame = gameStateDump[i]["frameCount"];
        firstGameStateFrame = gameStateFrame;
        replayingGameState = true;
        currentInputMap[replayLeft] = 0;
        currentInputMap[replayRight] = 0;

        globalFrameCount = firstGameStateFrame;

        if (gameStateDump[i].contains("stageTimer")) {
            replayFrameNumber = gameStateDump[i]["stageTimer"];
        }
    } else if (loadingDump) {
        fprintf(stderr, "failed to load dump %s\n", strDumpLoadPath.c_str());
        exit(1);
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
