#include "guy.hpp"
#include "ui.hpp"
#include "main.hpp"

#include <string>

Guy *pGuyToDelete = nullptr;

void drawGuyStatusWindow(const char *windowName, Guy *pGuy)
{
    ImGui::Begin(windowName);
    ImGui::Text("name %s moveset %s", pGuy->getName().c_str(), pGuy->getCharacter().c_str());
    ImGui::Text("action %s frame %i", pGuy->getActionName().c_str(), pGuy->getCurrentFrame());
    float posX, posY, posOffsetX, posOffsetY, velX, velY, accelX, accelY;
    pGuy->getPosDebug(posX, posY, posOffsetX, posOffsetY);
    pGuy->getVel(velX, velY, accelX, accelY);
    ImGui::Text("pos %f %f", posX, posY);
    ImGui::Text("posOffset %f %f", posOffsetX, posOffsetY);
    ImGui::Text("vel %f %f", velX, velY);
    ImGui::Text("accel %f %f", accelX, accelY);
    ImGui::Text("push %li hit %li hurt %li", pGuy->getPushBoxes()->size(), pGuy->getHitBoxes()->size(), pGuy->getHurtBoxes()->size());
    ImGui::Text("COMBO HITS %i damage %i hitstun %i juggle %i", pGuy->getComboHits(), pGuy->getComboDamage(), pGuy->getHitStun(), pGuy->getJuggleCounter());
    if ( ImGui::Button("destroy") ) { pGuyToDelete = pGuy; }
    ImGui::End();

    int minionID = 1;
    for (auto minion : pGuy->getMinions() ) {
        std::string minionWinName = std::string(windowName) + "'s Minion " + std::to_string(minionID++);
        drawGuyStatusWindow(minionWinName.c_str(), minion);
    }
}

static inline float randFloat()
{
    float ret = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    return ret;
}

void renderUI(int currentInput, float frameRate, std::deque<std::string> *pLogQueue)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("Psycho Drive", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground );
    static float newCharColor[3] = { randFloat(), randFloat(), randFloat() };
    const char* chars[] = { "ryu", "honda", "ed", "jp", "guile" };
    static int charID = rand() % IM_ARRAYSIZE(chars);
    static float newCharPos = randFloat();
    ImGui::SliderFloat("##newcharpos", &newCharPos, 0.0, 1.0);
    ImGui::ColorEdit3("##newcharcolor", newCharColor);
    ImGui::Combo("##newcharchar", &charID, chars, IM_ARRAYSIZE(chars));
    ImGui::SameLine();
    if ( ImGui::Button("new guy") ) {
        color col = {newCharColor[0], newCharColor[1], newCharColor[2]};
        Guy *pNewGuy = new Guy(chars[charID], newCharPos * 700.0, 0.0, 1, col );
        if (guys.size()) {
            pNewGuy->setOpponent(guys[0]);
            if (guys.size() == 1) {
                guys[0]->setOpponent(pNewGuy);
            }
        }
        guys.push_back(pNewGuy);
        newCharColor[0] = randFloat();
        newCharColor[1] = randFloat();
        newCharColor[2] = randFloat();
        charID = rand() % IM_ARRAYSIZE(chars);
        newCharPos = randFloat();
    }
    ImGui::Text("currentInput %d", currentInput);
    ImGui::SameLine();
    resetpos = resetpos || ImGui::Button("reset positions");
    ImGui::SameLine();
    ImGui::Text("frame %d", globalFrameCount);
    ImGui::SameLine();
    if (paused) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "PAUSED");
    }
    ImGui::SameLine();
    if ( ImGui::Button("pause") ) {
        paused = !paused;
    }
    ImGui::SameLine();
    if ( ImGui::Button("step one") ) {
        oneframe = true;
    }
    ImGui::SliderInt("hitstun adder", &hitStunAdder, -10, 10);
    ImGui::Checkbox("force counter", &forceCounter);
    ImGui::SameLine();
    ImGui::Checkbox("force PC", &forcePunishCounter);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / frameRate, frameRate);
    for (auto logLine : *pLogQueue) {
        ImGui::Text(logLine.c_str());
    }
    ImGui::End();

    pGuyToDelete = nullptr;
    int guyID = 1;
    for (auto guy : guys) {
        std::string windowName = "Guy " + std::to_string(guyID++);
        drawGuyStatusWindow( windowName.c_str(), guy );
    }

    if (pGuyToDelete) {
        if (pGuyToDelete->getParent()) {
            std::vector<Guy *> &vec = pGuyToDelete->getParent()->getMinions();
            vec.erase(std::remove(vec.begin(), vec.end(), pGuyToDelete), vec.end());
        } else {
            guys.erase(std::remove(guys.begin(), guys.end(), pGuyToDelete), guys.end());
        }
        for (auto guy : guys) {
            if (guy->getOpponent() == pGuyToDelete) {
                guy->setOpponent(nullptr);
            }
            for (auto minion : guy->getMinions()) {
                if (minion->getOpponent() == pGuyToDelete) {
                    minion->setOpponent(nullptr);
                }
            }
        }
        delete pGuyToDelete;
        pGuyToDelete = nullptr;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

ImGuiIO& initUI(void)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    return io;
}

void destroyUI(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}