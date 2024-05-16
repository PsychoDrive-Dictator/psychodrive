#include "guy.hpp"
#include "ui.hpp"
#include "main.hpp"

#include <string>

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
    ImGui::End();

    int minionID = 1;
    for (auto minion : pGuy->getMinions() ) {
        std::string minionWinName = std::string(windowName) + "'s Minion " + std::to_string(minionID++);
        drawGuyStatusWindow(minionWinName.c_str(), minion);
    }
}

void renderUI(int currentInput, float frameRate, std::deque<std::string> *pLogQueue)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("Psycho Drive", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground );
    static float newCharColor[3] = { 0.7f, 0.6f, 0.5f};
    static char newCharTextField[512];
    static bool init = true;
    if (init) {
        strcpy(newCharTextField, "ryu");
        init = false;
    }
    float newCharPos = 0.5;
    ImGui::SliderFloat("##newcharpos", &newCharPos, 0.0, 1.0);
    ImGui::ColorEdit3("##newcharcolor", newCharColor);
    ImGui::InputText("##newcharchar", newCharTextField, IM_ARRAYSIZE(newCharTextField));
    ImGui::SameLine();
    if ( ImGui::Button("new guy") ) {
        color col = {newCharColor[0], newCharColor[1], newCharColor[2]};
        Guy *pNewGuy = new Guy(newCharTextField, newCharPos * 700.0, 0.0, 1, col );
        if (guys.size()) {
            pNewGuy->setOpponent(guys[0]);
            if (guys.size() == 1) {
                guys[0]->setOpponent(pNewGuy);
            }
        }
        guys.push_back(pNewGuy);
    }
    ImGui::Text("currentInput %d", currentInput);
    ImGui::SameLine();
    resetpos = resetpos || ImGui::Button("reset");
    ImGui::SliderInt("hitstun adder", &hitStunAdder, -10, 10);
    ImGui::Checkbox("force counter", &forceCounter);
    ImGui::SameLine();
    ImGui::Checkbox("force PC", &forcePunishCounter);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / frameRate, frameRate);
    for (auto logLine : *pLogQueue) {
        ImGui::Text(logLine.c_str());
    }
    ImGui::End();

    int guyID = 1;
    for (auto guy : guys) {
        std::string windowName = "Guy " + std::to_string(guyID++);
        drawGuyStatusWindow( windowName.c_str(), guy );
    }

    ImGui::Render();
}