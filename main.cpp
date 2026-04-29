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

#include "simulation.hpp"
#include "guy.hpp"
#include "main.hpp"
#include "ui.hpp"
#include "input.hpp"
#include "render.hpp"
#include "combogen.hpp"
#include "chara.hpp"

EGameMode gameMode = Training;

bool forceCounter = false;
bool forcePunishCounter = false;

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
    charEntries.push_back({31, "alex", "Alex"});

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
    "40 - S3 Alex Update",
    "41 - S3 Alex Hotfix",
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

struct PendingViewerLoad {
    bool active = false;
    bool isReplay = false;
    std::string path;
    std::vector<std::string> neededChars;
    int version;
};
PendingViewerLoad pendingViewerLoad;

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

bool isCharLoaded(const std::string& charName, int charVersion)
{
    if (charName == "") {
        return false;
    }
#ifdef __EMSCRIPTEN__
    std::string charSpec = charName + std::to_string(charVersion);
    return setCharsLoaded.find(charSpec) != setCharsLoaded.end();
#else
    (void)charVersion;
    return true;
#endif
}

void requestCharDownload(const std::string& charName, int charVersion)
{
    if (charName == "") {
        return;
    }
#ifndef __EMSCRIPTEN__
    (void)charVersion;
#else
    std::string charSpec = charName + std::to_string(charVersion);
    if (setCharsLoaded.find(charSpec) != setCharsLoaded.end()) {
        return;
    }
    if (setCharsStarted.find(charSpec) != setCharsStarted.end()) {
        return;
    }
    log("downloading " + charSpec);
    setCharsStarted.insert(charSpec);
    std::string strVersion = std::string(STRINGIZE_VALUE_OF(PROJECT_VERSION));
    EM_ASM({
        var charSpec = UTF8ToString($0);
        var url = charSpec + '.bin?version=' + UTF8ToString($1);

        fetch(url).then(function(response) {
            return response.arrayBuffer();
        }).then(function(buffer) {
            var data = new Uint8Array(buffer);
            try { FS.mkdir('data'); } catch(e) {}
            try { FS.mkdir('data/cooked'); } catch(e) {}
            FS.writeFile('data/cooked/' + charSpec + '.bin', data);
            Module.ccall('jsCharLoadCallback', null, ['string'], [charSpec]);
        });
    }, charSpec.c_str(), strVersion.c_str());
#endif
}

void createGuy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color)
{
#ifdef __EMSCRIPTEN__
    std::string charSpec = charName + std::to_string(charVersion);
    if (setCharsLoaded.find(charSpec) == setCharsLoaded.end()) {
        bool needDownload = true;
        for (auto& deferred : vecDeferredCreateGuys) {
            if (deferred.charName == charName && deferred.charVersion == charVersion)
                needDownload = false;
        }

        vecDeferredCreateGuys.push_back({charName,charVersion,x,y,startDir,color});

        if (!needDownload)
            return;

        requestCharDownload(charName, charVersion);
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
        for (auto& deferredCreate : vecDeferredCreateGuys) {
            std::string charSpec = deferredCreate.charName + std::to_string(deferredCreate.charVersion);
            if (setCharsLoaded.find(charSpec) != setCharsLoaded.end()) {
                createGuyNow(deferredCreate.charName, deferredCreate.charVersion, deferredCreate.x, deferredCreate.y, deferredCreate.startDir, deferredCreate.col);
            } else {
                vecDeferredNotDoneYet.push_back(deferredCreate);
            }
        }
        vecDeferredCreateGuys = vecDeferredNotDoneYet;
        newCharLoaded = false;
    }

    if (pendingViewerLoad.active) {
        bool allReady = true;
        for (auto &charSpec : pendingViewerLoad.neededChars) {
            if (setCharsLoaded.find(charSpec) == setCharsLoaded.end()) {
                allReady = false;
                break;
            }
        }
        if (allReady) {
            simController.charCount = 2;
            gameMode = Viewer;
            if (pendingViewerLoad.isReplay) {
                simController.viewerDumpPath.clear();
                simController.ValidateAllRounds();
                simController.LoadReplayRound(0);
            } else {
                simController.replayRoundCount = 0;
                simController.replayCurrentRound = -1;
                simController.replayRoundResults.clear();
                simController.ReloadViewer();
            }
            simInputsChanged = false;
            pendingViewerLoad.active = false;
        }
    }
}

static bool isOldReplayFormat(const nlohmann::json &replayInfo);
static void fixupOldReplayInfo(nlohmann::json &replayInfo);
static const nlohmann::json &replayInputBytes(const nlohmann::json &inputData);

extern "C" {

void jsCharLoadCallback(char *charVerKey)
{
    log(std::string(charVerKey) + " download complete");
    if (setCharsLoaded.find(charVerKey) == setCharsLoaded.end()) {
        setCharsLoaded.insert(charVerKey);
        newCharLoaded = true;
    }
}

void jsLoadFile(char *filePath)
{
    int maxVersion = atoi(charVersions[charVersionCount - 1]);
    std::string path(filePath);

    nlohmann::json parsed = parse_json_file(path);
    if (parsed == nullptr) {
        fprintf(stderr, "failed to parse dropped file\n");
        return;
    }

    pendingViewerLoad = {};
    pendingViewerLoad.active = true;
    pendingViewerLoad.path = path;
    pendingViewerLoad.version = maxVersion;

    // check if it's a replay (has InputData/ReplayInfo)
    nlohmann::json *data = &parsed;
    if (parsed.contains("BattleReplayData")) {
        data = &parsed["BattleReplayData"];
    }
    if (data->contains("InputData") && data->contains("ReplayInfo")) {
        pendingViewerLoad.isReplay = true;
        simController.replayInfo = (*data)["ReplayInfo"];
        simController.replayIsOldFormat = isOldReplayFormat(simController.replayInfo);
        if (simController.replayIsOldFormat) {
            fixupOldReplayInfo(simController.replayInfo);
        }
        simController.replayInputData = replayInputBytes((*data)["InputData"]).get<std::vector<uint8_t>>();
        simController.replayVersion = maxVersion;

        for (int i = 0; i < 2; i++) {
            int fighterID = simController.replayInfo["Fighters"][i]["FighterID"];
            std::string charName = getCharNameFromID(fighterID);
            std::string charSpec = charName + std::to_string(maxVersion);
            pendingViewerLoad.neededChars.push_back(charSpec);
            requestCharDownload(charName, maxVersion);
        }
    } else {
        // otherwise treat as dump — extract char names from first frame's players
        pendingViewerLoad.isReplay = false;
        if (path.find("forcepc") != std::string::npos) {
            forcePunishCounter = true;
        }
        simController.viewerDumpPath = path;
        simController.viewerDumpVersion = maxVersion;
        simController.viewerDumpIsMatch = path.find("match") != std::string::npos;

        for (int i = 0; i < (int)parsed.size() && i < 1; i++) {
            if (parsed[i].contains("players")) {
                for (auto &p : parsed[i]["players"]) {
                    std::string charName = getCharNameFromID(p["charID"]);
                    std::string charSpec = charName + std::to_string(maxVersion);
                    pendingViewerLoad.neededChars.push_back(charSpec);
                    requestCharDownload(charName, maxVersion);
                }
                break;
            }
        }
    }

}

}

bool done = false;
bool paused = false;
bool oneframe = false;
int runUntilFrame = 0;
bool lockCamera = true;
bool toggleRenderUI = true;
bool toggleDebugUI = false;

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


std::vector<normalRangePlotEntry> vecPlotEntries;
int curPlotEntryID = -1;
int curPlotEntryStartFrame = 0;
int curPlotEntryNormalStartFrame = 0;
int curPlotActionID = 0;

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

static const nlohmann::json &replayInputBytes(const nlohmann::json &inputData)
{
    if (inputData.is_object() && inputData.contains("mValue")) {
        return inputData["mValue"];
    }
    return inputData;
}

static bool isOldReplayFormat(const nlohmann::json &replayInfo)
{
    if (replayInfo.contains("RoundInfo") && replayInfo["RoundInfo"].is_object()) {
        return true;
    }
    if (replayInfo.contains("Fighters") && replayInfo["Fighters"].is_object()
        && replayInfo["Fighters"].contains("FighterID")
        && replayInfo["Fighters"]["FighterID"].is_array()) {
        return true;
    }
    return false;
}

static void fixupOldReplayInfo(nlohmann::json &replayInfo)
{
    // fix up everything except input data, which still needs mValue accessed by hand

    if (replayInfo["Fighters"].is_object() && replayInfo["Fighters"].contains("FighterID")
        && replayInfo["Fighters"]["FighterID"].is_array()) {
        nlohmann::json oldFighters = replayInfo["Fighters"];
        nlohmann::json newFighters = nlohmann::json::array();
        size_t n = oldFighters["FighterID"].size();
        for (size_t i = 0; i < n; i++) {
            nlohmann::json f = nlohmann::json::object();
            for (auto it = oldFighters.begin(); it != oldFighters.end(); ++it) {
                if (it.value().is_array() && it.value().size() > i) {
                    f[it.key()] = it.value()[i];
                }
            }
            newFighters.push_back(f);
        }
        replayInfo["Fighters"] = newFighters;
    }

    if (replayInfo["RoundInfo"].is_object()) {
        nlohmann::json old = replayInfo["RoundInfo"];

        nlohmann::json saGauge = nlohmann::json::array({0, 0});
        if (old.contains("SAGaugeStart")) {
            const auto &sg = old["SAGaugeStart"];
            if (sg.is_object() && sg.contains("mValue")) saGauge = sg["mValue"];
            else if (sg.is_array()) saGauge = sg;
        }
        nlohmann::json styleNo = nlohmann::json::array({0, 0});
        if (old.contains("StyleNo")) {
            const auto &sn = old["StyleNo"];
            if (sn.is_object() && sn.contains("mValue")) styleNo = sn["mValue"];
            else if (sn.is_array()) styleNo = sn;
        }

        int roundCount = 0;
        if (old.contains("RandomSeed") && old["RandomSeed"].is_array()) {
            roundCount = (int)old["RandomSeed"].size();
        }

        nlohmann::json newRounds = nlohmann::json::array();
        for (int r = 0; r < roundCount; r++) {
            nlohmann::json ri = nlohmann::json::object();
            ri["RandomSeed"] = old["RandomSeed"][r];
            ri["WinPlayerType"] = 0;
            if (old.contains("WinPlayerType") && old["WinPlayerType"].is_array() && (int)old["WinPlayerType"].size() > r) {
                ri["WinPlayerType"] = old["WinPlayerType"][r];
            }
            ri["FinishType"] = 0;
            if (old.contains("FinishType") && old["FinishType"].is_array() && (int)old["FinishType"].size() > r) {
                ri["FinishType"] = old["FinishType"][r];
            }
            if (r == roundCount - 1) {
                ri["SAGaugeStart"] = saGauge;
            } else {
                ri["SAGaugeStart"] = nlohmann::json::array({0, 0});
            }
            ri["StyleNo"] = styleNo;
            if (old.contains("UniqueParam")) ri["UniqueParam"] = old["UniqueParam"];
            if (old.contains("BgmInfo")) ri["BgmInfo"] = old["BgmInfo"];
            newRounds.push_back(ri);
        }
        replayInfo["RoundInfo"] = newRounds;
    }
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
ImGuiIO *io;
SDL_Window *sdlwindow;
color clearColor = {0.0,0.0,0.0};

static void mainloop(void)
{
    if (done) {
        stopComboFinder();

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
    if ((limitRate || !playingBackInput) && currentTime - frameStartTime < desiredFrameTimeMS) {
        const float timeToSleepMS = (desiredFrameTimeMS - (currentTime - frameStartTime));
        usleep(timeToSleepMS * 1000 - 100);
    }
    frameStartTime = SDL_GetTicks();

    defaultSim.frameCounter++;

    int sizeX, sizeY;
    SDL_GetWindowSize(sdlwindow, &sizeX, &sizeY);

    updateInputs(sizeX, sizeY);

    updateComboFinder();

    if (runComboFinder) {
        findCombos(comboFinderDoLights, comboFinderDoLateCancels, comboFinderDoWalk, comboFinderDoKaras);
        runComboFinder = false;
    }

    if (gameMode != Training) {
        setRenderState(clearColor, sizeX, sizeY);
        int curFrame = simController.scrubberFrame; // before ui changes it
        if (simInputsChanged && simController.NewSim()) {
            simController.AdvanceUntilComplete();
            // in case it got clamped
            curFrame = simController.scrubberFrame;

            simInputsChanged = false;
        }

        if (!simInputsChanged) {
            Simulation *pFrameSim = simController.getSnapshotAtFrame(curFrame);
            renderComboFinder();
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
                translateX = (pRightGuy->getLastPosX(true).f() + pLeftGuy->getLastPosX(true).f()) / 2.0f;
                translateX = fmin(translateX, 550.0);
                translateX = fmax(translateX, -550.0);

                // first find required camera distance to have both guys in view horizontally
                float distGuys = fabs( pRightGuy->getLastPosX(true).f() - pLeftGuy->getLastPosX(true).f() );
                distGuys += 200.0; // account for some buffer behind
                float angleRad = fov / 2.0 * std::numbers::pi / 180.0;
                // zoom is adjacent edge, equals opposite over tan(ang)
                float zoomToFitGuys = distGuys / 2.0 / tanf( angleRad );

                float vertFovRad = 2.0 * atanf( tanf( ( fov * std::numbers::pi / 180.0 ) / 2.0 ) * ((float)sizeY / (float)sizeX) );
                // we want to see a point 25 units below the chars to clearly see their feet
                float zoomToFitFloor = (translateY + 150.0) / tanf( vertFovRad / 2.0 );

                zoom = fmax( zoomToFitGuys, zoomToFitFloor );
                zoom = fmax( zoom, 250.0f );
            }
        }
        renderUI(io->Framerate, &logQueue, sizeX, sizeY);
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

        if (runUntilFrame) {
            if (runUntilFrame > defaultSim.frameCounter) {
                limitRate = false;
                paused = false;
            } else {
                runUntilFrame = 0;
                paused = true;
                limitRate = true;
            }
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
            const DoneRoute &playingRoute = **finder.doneRoutes.rbegin();
            int targetFrame = defaultSim.frameCounter + 1;
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

        if (runFrame) {
            for (auto guy : vecGuysToDelete) {
                delete guy;
            }
            vecGuysToDelete.clear();
        }

        defaultSim.gatherEveryone();

        if (runFrame) {
            for (auto guy : defaultSim.everyone) {
                if (guy->RunFrame()) {
                }
            }
        }

        // gather everyone again in case of deletions/additions in RunFrame
        defaultSim.gatherEveryone();

        if (!runFrame) {
            defaultSim.frameCounter--;
        }

        if (runFrame) {
            for (auto guy : defaultSim.everyone) {
                guy->WorldPhysics();
            }
            for (auto guy : defaultSim.everyone) {
                guy->Push(guy->getOpponent());
            }

            for (auto guy : defaultSim.everyone) {
                guy->RunFramePostPush();
            }

            // gather everyone again in case of deletions/additions in RunFramePostPush
            defaultSim.gatherEveryone();

            std::vector<PendingHit> pendingHitList;

            for (auto guy : defaultSim.everyone) {
                guy->CheckHit(guy->getOpponent(), pendingHitList);
            }

            ResolveHits(&defaultSim, pendingHitList);

            // any hit spawns
            defaultSim.gatherEveryone();

            static bool hasAddedData = false;
            // update plot range if we're doing that
            if (guys.size() > 0 ) {
                if (curPlotActionID == 0 && guys[0]->getIsDrive() == true)
                {
                    curPlotEntryStartFrame = defaultSim.frameCounter;
                    curPlotActionID = guys[0]->getCurrentAction();
                    vecPlotEntries.push_back({});
                    curPlotEntryID++;
                }
                if (curPlotActionID != 0 && !guys[0]->canAct() && guys[0]->getIsDrive() == false)
                {
                    if (curPlotEntryNormalStartFrame == 0) {
                        curPlotEntryNormalStartFrame = defaultSim.frameCounter - curPlotEntryStartFrame;
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
                        vecPlotEntries[curPlotEntryID].hitBoxRangePlotX.push_back(defaultSim.frameCounter - curPlotEntryStartFrame);
                        vecPlotEntries[curPlotEntryID].hitBoxRangePlotY.push_back(maxXHitBox.f());
                    }
                    if (vecPlotEntries[curPlotEntryID].strName == "") {
                        vecPlotEntries[curPlotEntryID].strName = guys[0]->getCharacter() + " " + guys[0]->getActionName(guys[0]->getCurrentAction()) + " (" + std::to_string(curPlotEntryNormalStartFrame) + "f cancel)";
                        vecPlotEntries[curPlotEntryID].col = guys[0]->getColor();
                    }
                }
                if (curPlotActionID != 0 && guys[0]->canAct()) {
                    hasAddedData = false;
                    curPlotActionID = 0;
                    curPlotEntryStartFrame = 0;
                    curPlotEntryNormalStartFrame = 0;
                }
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
            for (auto guy : defaultSim.everyone) {
                bool die = !guy->AdvanceFrame();

                if (die) {
                    vecGuysToDelete.push_back(guy);
                }
            }
        }

        // find camera position if we have 2 guys
        if (lockCamera && guys.size() >= 2) {
            translateX = (guys[1]->getLastPosX(true).f() + guys[0]->getLastPosX(true).f()) / 2.0f;
            translateX = fmin(translateX, 550.0);
            translateX = fmax(translateX, -550.0);

            // first find required camera distance to have both guys in view horizontally
            float distGuys = fabs( guys[1]->getLastPosX(true).f() - guys[0]->getLastPosX(true).f() );
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
        defaultSim.everyone.clear(); 
        for (auto guy : guys) {
            defaultSim.everyone.push_back(guy);
            for ( auto minion : guy->getMinions() ) {
                defaultSim.everyone.push_back(minion);
            }
        }

        // gather everyone again in case of deletions/additions in renderUI
        defaultSim.everyone.clear();
        for (auto guy : guys) {
            defaultSim.everyone.push_back(guy);
            for ( auto minion : guy->getMinions() ) {
                defaultSim.everyone.push_back(minion);
            }
        }

        setRenderState(clearColor, sizeX, sizeY);
        renderUI(io->Framerate, &logQueue, sizeX, sizeY);

        renderComboFinder();
        defaultSim.Render();

        if (resetpos) {
            uint32_t i = 0;
            while (i < guys.size()) {
                guys[i]->resetPos();
                i++;
            }
        }

        resetpos = false;
        oneframe = false;
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
        strDumpLoadPath = argv[2];
        dumpSim.SetupFromGameDump(strDumpLoadPath, version);

        if (strDumpLoadPath.find("forcepc") != std::string::npos) {
            forcePunishCounter = true;
        }
        if (strDumpLoadPath.find("match") != std::string::npos) {
            dumpSim.match = true;
        }

        while (true) {
            dumpSim.RunFrame();
            dumpSim.AdvanceFrame();
            if (dumpSim.replayingGameStateDump == false) {
                exit(0);
            }
        }
    }

    if ( argc > 2 && std::string(argv[1]) == "run_replay") {
        gameMode = Batch;
        int version = -1;
        if ( argc > 3 ) {
            version = atoi(argv[3]);
        }
        if (version == -1) {
            version = atoi(charVersions[charVersionCount - 1]);
        }

        nlohmann::json replayJson = parse_json_file(argv[2]);
        if (replayJson == nullptr) {
            fprintf(stderr, "failed to load replay %s\n", argv[2]);
            exit(1);
        }
        nlohmann::json &data = replayJson.contains("BattleReplayData") ? replayJson["BattleReplayData"] : replayJson;
        if (!data.contains("InputData") || !data.contains("ReplayInfo")) {
            fprintf(stderr, "replay missing InputData or ReplayInfo\n");
            exit(1);
        }
        nlohmann::json replayInfo = data["ReplayInfo"];
        bool oldFormat = isOldReplayFormat(replayInfo);
        if (oldFormat) {
            fixupOldReplayInfo(replayInfo);
        }

        ReplayDecoder decoder;
        decoder.inputData = replayInputBytes(data["InputData"]).get<std::vector<uint8_t>>();

        int totalErrors = 0;
        int roundCount = (int)replayInfo["RoundInfo"].size();
        int carryGauges[2] = {0, 0};

        for (int round = 0; round < roundCount; round++) {
            nlohmann::json &roundInfo = replayInfo["RoundInfo"][round];

            // online replays have all five round slots but zero out missing ones
            if (roundInfo["RandomSeed"] == 0) {
                break;
            }

            Simulation roundSim;
            const int *startGauges = (oldFormat && round > 0) ? carryGauges : nullptr;
            roundSim.SetupReplayRound(replayInfo, round, version, decoder, startGauges);

            int prevHealth[2] = {0, 0};
            while (roundSim.replayingReplay) {
                prevHealth[0] = roundSim.simGuys[0]->getHealth();
                prevHealth[1] = roundSim.simGuys[1]->getHealth();
                roundSim.RunFrame();
                roundSim.AdvanceFrame();
            }

            int winPlayer = roundInfo["WinPlayerType"];

            fprintf(stderr, "R;round %d finished at frame %d\n", round + 1, roundSim.frameCounter);

            if (winPlayer == 0 || winPlayer == 1) {
                int losePlayer = winPlayer ^ 1;
                int loserHealth = roundSim.simGuys[losePlayer]->getHealth();
                if (loserHealth != 0 || prevHealth[losePlayer] == 0) {
                    fprintf(stderr, "E;round %d loser (P%d) health %d expected zero prev health %d expected nonzero\n", round + 1, losePlayer + 1, loserHealth, prevHealth[losePlayer]);
                    totalErrors++;
                }
            } else {
                for (int p = 0; p < 2; p++) {
                    int h = roundSim.simGuys[p]->getHealth();
                    if (h != 0 || prevHealth[p] == 0) {
                        fprintf(stderr, "E;round %d draw P%d health %d expected zero prev health %d expected nonzero\n", round + 1, p + 1, h, prevHealth[p]);
                        totalErrors++;
                    }
                }
            }

            bool checkGauge = !oldFormat || round == roundCount - 1;
            for (int p = 0; p < 2; p++) {
                int simGauge = roundSim.simGuys[p]->getGauge();
                carryGauges[p] = simGauge;
                if (checkGauge) {
                    int expectedGauge = roundInfo["SAGaugeStart"][p];
                    if (expectedGauge != simGauge) {
                        fprintf(stderr, "E;round %d P%d end gauge %d expected %d\n", round + 1, p + 1, simGauge, expectedGauge);
                        totalErrors++;
                    }
                }
            }

            // todo check win condition

            // reset for next round
            decoder.inputState[0] = 0;
            decoder.inputState[1] = 0;
            decoder.prevInputState[0] = 0;
            decoder.prevInputState[1] = 0;
        }

        fprintf(stderr, "F;replay finished, total errors: %d\n", totalErrors);
        exit(0);
    }

    if (argc > 1 && std::string(argv[1]) == "printversions") {
        for (int i = 0; i < charVersionCount; i++) {
            printf("%d\n", atoi(charVersions[i]));
        }
        exit(0);
    }

    if (argc > 3 && std::string(argv[1]) == "cook") {
        char* charSpec = argv[2];
        std::string outFile = argv[3];

        std::string charName;
        int version = atoi(charVersions[charVersionCount - 1]);
        extractCharVersion(charSpec, charName, version);

        CharacterData* pData = loadCharacter(charName, version);
        if (!pData) {
            fprintf(stderr, "failed to load %s v%d\n", charName.c_str(), version);
            exit(1);
        }

        if (!cookCharacter(pData, outFile)) {
            fprintf(stderr, "failed to cook %s v%d\n", charName.c_str(), version);
            exit(1);
        }

        delete pData;
        exit(0);
    }

    if ( argc > 2 && std::string(argv[1]) == "load_dump") {
        strDumpLoadPath = argv[2];
        loadingDump = true;
        if (argc > 3) {
            dumpVersion = atoi(argv[3]);
        }
        if (argc > 4) {
            int filterType = atoi(argv[4]);
            for (int i = 0; i < 14; i++) simController.viewerErrorTypeFilter[i] = false;
            simController.viewerErrorTypeFilter[filterType] = true;
        }
    }

    bool loadingReplay = false;
    std::string strReplayLoadPath;
    int replayVersion = -1;
    if ( argc > 2 && std::string(argv[1]) == "load_replay") {
        strReplayLoadPath = argv[2];
        loadingReplay = true;
        if (argc > 3) {
            replayVersion = atoi(argv[3]);
        }
    }

    sdlwindow = initWindowRender();
    initUI();
    io = &ImGui::GetIO(); // why doesn't the one from initUI work? who knows
    initRenderUI();

    int maxVersion = atoi(charVersions[charVersionCount - 1]);
    if (argc > 1)
        gameMode = Training;
    
    if (gameMode == Training && loadingDump == false && loadingReplay == false) {
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

    if (loadingDump) {
        if (dumpVersion == -1) {
            dumpVersion = maxVersion;
        }
        if (strDumpLoadPath.find("forcepc") != std::string::npos) {
            forcePunishCounter = true;
        }

        simController.viewerDumpPath = strDumpLoadPath;
        simController.viewerDumpVersion = dumpVersion;
        simController.viewerDumpIsMatch = strDumpLoadPath.find("match") != std::string::npos;
        simController.charCount = 2;
        gameMode = Viewer;
        simController.ReloadViewer();
        simInputsChanged = false;
    }

    if (loadingReplay) {
        nlohmann::json replayJson = parse_json_file(strReplayLoadPath);
        if (replayJson == nullptr) {
            fprintf(stderr, "failed to load replay %s\n", strReplayLoadPath.c_str());
            exit(1);
        }
        nlohmann::json &data = replayJson.contains("BattleReplayData") ? replayJson["BattleReplayData"] : replayJson;
        if (!data.contains("InputData") || !data.contains("ReplayInfo")) {
            fprintf(stderr, "replay missing InputData or ReplayInfo\n");
            exit(1);
        }

        simController.replayInfo = data["ReplayInfo"];
        simController.replayIsOldFormat = isOldReplayFormat(simController.replayInfo);
        if (simController.replayIsOldFormat) {
            fixupOldReplayInfo(simController.replayInfo);
        }
        simController.replayInputData = replayInputBytes(data["InputData"]).get<std::vector<uint8_t>>();
        simController.replayVersion = (replayVersion == -1) ? maxVersion : replayVersion;

        simController.charCount = 2;
        gameMode = Viewer;
        simController.ValidateAllRounds();
        simController.LoadReplayRound(0);
        simInputsChanged = false;
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
