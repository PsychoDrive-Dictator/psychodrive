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
int currentInput = 0;

bool done = false;
bool paused = false;
bool oneframe = false;
int globalFrameCount = 0;

bool recordingInput = false;
bool initialLoading = false;
std::vector<int> recordedInput;
int recordingStartFrame = 0;

bool playingBackInput = false;
std::deque<int> playBackInputBuffer;
int playBackFrame = 0;

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

int main(int argc, char**argv)
{
    srand(time(NULL));
    auto window = initWindowRender();
    initUI();
    ImGuiIO& io = ImGui::GetIO(); // why doesn't the one from initUI work? who knows
    initRenderUI();

    if ( argc > 1 ) {
        Guy *pNewGuy = new Guy(argv[1], 50.0, 0.0, 1, {randFloat(), randFloat(), randFloat()} );
        guys.push_back(pNewGuy);

        if ( argc > 2 ) {
            pNewGuy = new Guy(argv[2], 250.0, 0.0, -1, {randFloat(), randFloat(), randFloat()} );
            guys.push_back(pNewGuy);

            pNewGuy->setOpponent(guys[0]);
            guys[0]->setOpponent(pNewGuy);
        }
    }

    nlohmann::json inputTimeline = parse_json_file("timeline.json");
    if (inputTimeline != nullptr) {
        recordedInput = inputTimeline.get<std::vector<int>>();
        initialLoading = true;
    }

    uint32_t frameStartTime = SDL_GetTicks();

    while (!done)
    {
        const float desiredFrameTimeMS = 1000.0 / 60.0f;
        uint32_t currentTime = SDL_GetTicks();
        if (currentTime - frameStartTime < desiredFrameTimeMS) {
            const float timeToSleepMS = (desiredFrameTimeMS - (currentTime - frameStartTime));
            usleep(timeToSleepMS * 1000 - 100);
        }
        frameStartTime = SDL_GetTicks();

        globalFrameCount++;

        currentInput = getInput( currentInput );

        if (recordingInput) {
            recordedInput.push_back(currentInput);
        }

        bool hasInput = true;

        if (playingBackInput) {
            if (oneframe || !paused) {
                if (playBackFrame >= (int)playBackInputBuffer.size()) {
                    playingBackInput = false;
                    playBackFrame = 0;
                } else {
                    currentInput = playBackInputBuffer[playBackFrame++];
                    //log ("input from playback! " + std::to_string(currentInput));
                }
            } else {
                hasInput = false;
            }
        }

        for (auto guy : guys) {
            if ( hasInput ) {
                guy->Input( currentInput);
            }
            hasInput = false;
        }

        std::vector<Guy *> guysWhoFrame;

        if (oneframe || !paused) {
            for (auto guy : guys) {
                if (guy->PreFrame()) {
                    guysWhoFrame.push_back(guy);
                }
            }
        }

        if (guysWhoFrame.size() == 0) {
            globalFrameCount--; // don't count that frame, useful for comparing logs to frame data
        }

        bool push = true; // only push the first guy until this actually works
        if (oneframe || !paused) {
            // everyone gets world physics
            for (auto guy : guys) {
                guy->WorldPhysics();
            }
            for (auto guy : guysWhoFrame) {
                guy->CheckHit(guy->getOpponent());
            }
            // push after hit for fbs to work, but i think it's really that there's some pushboxes on hit only
            // see all air normals?
            for (auto guy : guysWhoFrame) {
                if ( push ) {
                    guy->Push(guy->getOpponent());
                    push = false;
                }
            }
            for (auto guy : guysWhoFrame) {
                guy->Frame();
            }
        }
        oneframe = false;

        color clearColor = {0.0,0.0,0.0};

        bool ticktock = false;
        for (auto guy : guys) {
            ticktock = ticktock || guy->getWarudo();
        }
        if (ticktock) {
            clearColor = {0.075,0.075,0.075};
        }

        setRenderState(clearColor, (int)io.DisplaySize.x, (int)io.DisplaySize.y);

        renderUI(currentInput, io.Framerate, &logQueue);

        for (auto guy : guys) {
            guy->Render();
        }

        if (resetpos) {
            if (guys.size() > 0 ) {
                guys[0]->resetPosDebug(50.0f, 0.0f);
            }
            if (guys.size() > 1 ) {
                guys[1]->resetPosDebug(250.0f, 0.0f);
            }
        }

        SDL_GL_SwapWindow(window);

        resetpos = false;
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
