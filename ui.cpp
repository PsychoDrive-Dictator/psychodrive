#define IMGUI_DEFINE_MATH_OPERATORS

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "imgui_neo_sequencer.h"
#include "implot.h"

#include "guy.hpp"
#include "ui.hpp"
#include "main.hpp"
#include "input.hpp"
#include "combogen.hpp"

#include <cstdlib>

#include <string>
#include <unordered_map>

#include "imgui/imgui_internal.h"

#include "font_droidsans.hpp"

int renderSizeX;
int renderSizeY;

bool webWidgets = false;

ImFont *comboFont = nullptr;
ImFont *comboFontSubscript = nullptr;
ImFont *fontBigger = nullptr;
ImFont *font = nullptr;

Guy *pGuyToDelete = nullptr;

int mobileDropDownOption = -1;
ImGuiID curDropDownID = -1;

SimulationController simController;
bool simInputsChanged = true;

extern "C" {

void jsModalDropDownSelection(int selectionID)
{
    mobileDropDownOption = selectionID;
}

}

bool modalDropDown(const char *label, int *pSelection, std::vector<const char *> vecOptions, int nFixedWidth = 0)
{
    bool ret = false;
    int result = -1;
    int selectedOption = *pSelection;

    if (!webWidgets) {
        if (nFixedWidth != 0)
            ImGui::SetNextItemWidth(nFixedWidth);
        if (ImGui::BeginCombo(label, vecOptions[selectedOption])) {
            int i = 0;
            for (auto option : vecOptions) {
                const bool selected = selectedOption == i;
                if (ImGui::Selectable(option, selected))
                    result = i;
                if (selected)
                    ImGui::SetItemDefaultFocus();
                i++;
            }
            ImGui::EndCombo();
        }
    } else {
#ifdef __EMSCRIPTEN__
        std::string strButtonID = std::string(vecOptions[selectedOption]) + "##mobilemodaldropdown_" + label;
        bool showingDropDown = false;
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
        if (ImGui::Button(strButtonID.c_str(), ImVec2(nFixedWidth, 0))) {
            showingDropDown = true;
        }
        ImGui::PopStyleVar();
        if (label[0] != '#') {
            ImGui::SameLine();
            ImGui::Text("%s", label);
        }

        if (showingDropDown) {
            mobileDropDownOption = -1;
            curDropDownID = ImGui::GetCurrentWindow()->GetID(label);
            int i = 0;
            for (auto option : vecOptions) {
                int32_t selected = 0;
                if (selectedOption == i)
                    selected = 1;
                EM_ASM({
                    var option = UTF8ToString($0);
                    var selected = $1;
                    addDropDownOption(option, selected);
                }, option, selected);
                i++;
            }
            EM_ASM({
                showDropDown();
            }, 0);
        }

        if (curDropDownID == ImGui::GetCurrentWindow()->GetID(label) && mobileDropDownOption != -1) {
            result = mobileDropDownOption;
            curDropDownID = -1;
        }
#endif
    }

    if (result != -1) {
        if (result != selectedOption)
            ret = true;

        *pSelection = result;
    }

    return ret;
}

bool modalDropDown(const char *label, int *pSelection, std::vector<std::string> vecOptions, int nFixedWidth = 0)
{
    std::vector<const char *> vecActualOptions;

    for (auto& i : vecOptions) {
        vecActualOptions.push_back(i.c_str());
    }
    return modalDropDown(label, pSelection, vecActualOptions, nFixedWidth);
}


bool modalDropDown(const char *label, int *pSelection, const char** ppOptions, int nOptions, int nFixedWidth = 0)
{
    std::vector<const char *> vecOptions;
    for (int i = 0; i < nOptions; i++) {
        vecOptions.push_back(ppOptions[i]);
    }
    return modalDropDown(label, pSelection, vecOptions, nFixedWidth);
}

void renderComboMeter(bool rightSpot, int comboHits, int comboDamage, int scaling) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    // if (finder.running) {
    //     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0,0.0,0.0,1.0));
    // }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0,1.0,1.0,0.05));

    ImGui::SetNextWindowPos(ImVec2(rightSpot ? renderSizeX - 100.0f : 0, 300.0));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    const char *pComboWindowName = rightSpot ? "Right Combo Meter" : "Left Combo Meter";

    ImGui::Begin(pComboWindowName, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing );
    //ImGui::AlignTextToFramePadding();
    ImGui::PushFont(comboFont);
    if (rightSpot) {
        float offset = comboHits < 10 ? 37.0 : -4.0;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    }
    ImGui::Text("%d", comboHits);
    ImGui::PopFont();
    //ImGui::SameLine();
    ImGui::PushFont(comboFontSubscript);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY()-25.0);
    //ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 10.0);
    ImGui::Text("HIT");
    ImGui::PopFont();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY()-10.0);
    ImGui::Text("%d (%d%%)", comboDamage, scaling);
    ImGui::End();

    // if (finder.running) {
    //     ImGui::PopStyleColor();
    // }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
}

// const float frameWidth = 30;

// void drawActionTimelineKeys(nlohmann::json *pAction, const char *keyName, nlohmann::json *pTriggerGroups)
// {
//     int lineStartFrame = -1;
//     int lineEndFrame = -1;
//     float cursorX = ImGui::GetCursorPosX();
//     for (auto& [keyID, key] : (*pAction)[keyName].items()) {
//         if (!key.contains("_StartFrame")) {
//             continue;
//         }

//         int startFrame = key["_StartFrame"];
//         int endFrame = key["_EndFrame"];

//         if (lineEndFrame == -1) {
//             lineStartFrame = startFrame;
//             lineEndFrame = endFrame;
//         } else {
//             if (startFrame >= lineEndFrame || endFrame <= lineStartFrame) {
//                 ImGui::SameLine();
//                 if (startFrame < lineStartFrame) {
//                     lineStartFrame = startFrame;
//                 }
//                 if (endFrame > lineEndFrame) {
//                     lineEndFrame = endFrame;
//                 }
//             } else {
//                 // newline
//                 lineStartFrame = startFrame;
//                 lineEndFrame = endFrame;
//             }
//         }

//         std::string strButtonName = std::string(keyName) + " " + keyID;
//         log(strButtonName.c_str());

//         ImGui::SetCursorPosX(cursorX + frameWidth * startFrame);
//         if (key["_NotDefer"] == true) {
//             ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6, 0.0, 0.0, 1.0));
//         }
//         ImGui::Button(strButtonName.c_str() , ImVec2((endFrame - startFrame) * frameWidth, 0));
//         if (key["_NotDefer"] == true) {
//             ImGui::PopStyleColor();
//         }
//         if (ImGui::IsItemHovered()) {
//             ImGui::BeginTooltip();
//             if (std::string(keyName) == "TriggerKey") {
//                 for (auto& [trigGroupKeyID, trigGroupKey] : (*pTriggerGroups).items()) {
//                     if (atoi(trigGroupKeyID.c_str()) != key["TriggerGroup"]) {
//                         continue;
//                     }
//                     for (auto& [trigKeyID, trigKey] : trigGroupKey.items()) {
//                         ImGui::Text("%s", std::string(trigKey).c_str());
//                     }
//                 }
//             }
//             ImGui::EndTooltip();
//         }
//     }
// }

// void drawActionTimeline(const char *charName, int version, const char *moveName)
// {
//     nlohmann::json *pMovesDictJson = loadCharFile(charName, version, "moves");
//     nlohmann::json *pTriggerGroups = loadCharFile(charName, version, "trigger_groups");

//     nlohmann::json *pAction = &(*pMovesDictJson)[moveName];

//     if (!pAction) {
//         return;
//     }

//     drawActionTimelineKeys(pAction, "TriggerKey", pTriggerGroups);
// }

void drawGuyStatusWindow(const char *windowName, Guy *pGuy, Simulation *pSim = nullptr)
{
    //drawActionTimeline(pGuy->getName()->c_str(), pGuy->getVersion(), pGuy->getActionName().c_str());

    ImGui::Begin(windowName);
    color col = pGuy->getColor();
    ImGui::TextColored(ImVec4(col.r, col.g, col.b, 1), "%s %d", pGuy->getName().c_str(), pGuy->getVersion());
    ImGui::SameLine();
    std::vector<const char *> vecInputs;
    std::vector<std::string> vecInputLabels;
    for (auto i : currentInputMap) {
        vecInputLabels.push_back( std::to_string(i.first));
    }
    for (auto& i : vecInputLabels) {
        vecInputs.push_back(i.c_str());
    }
    modalDropDown("input", pGuy->getInputListIDPtr(), vecInputs, 50);
    *pGuy->getInputIDPtr() = atoi(vecInputLabels[*pGuy->getInputListIDPtr()].c_str());

    ImGui::SameLine();
    std::vector<std::string> vecGuyNames;
    vecGuyNames.push_back("none");
    std::map<int, Guy *> mapDropDownIDToGuyPtr;
    mapDropDownIDToGuyPtr[0] = nullptr;
    int guyID = 1, newOpponentID = 0;
    for (auto guy : pSim ? pSim->simGuys : guys) {
        if (guy == pGuy) {
            continue;
        }
        vecGuyNames.push_back( guy->getName() );
        mapDropDownIDToGuyPtr[guyID++] = guy;
    }
    guyID = 0;
    for (auto [ i, guy ] : mapDropDownIDToGuyPtr ) {
        if (guy == pGuy->getOpponent()) {
            guyID = i;
            break;
        }
    }
    vecInputs.clear();
    for (auto& i : vecGuyNames) {
        vecInputs.push_back(i.c_str());
    }
    newOpponentID = guyID;
    modalDropDown("opponent", &newOpponentID, vecInputs, 100);
    if (newOpponentID != guyID) {
        pGuy->setOpponent(mapDropDownIDToGuyPtr[newOpponentID]);
    }

    float startPosX = pGuy->getStartPosX().f();
    float newStartPosX = startPosX;
    ImGui::SetNextItemWidth( 250.0 );
    ImGui::SliderFloat("##startpos", &newStartPosX, -765.0, 765.0);
    char startPosTest[32] = {};
    snprintf(startPosTest, sizeof(startPosTest), "%.2f", newStartPosX);
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 100.0 );
    ImGui::InputText("##startpostext", startPosTest, sizeof(startPosTest));
    newStartPosX = atof(startPosTest);
    if (newStartPosX != startPosX) {
        pGuy->setStartPosX(Fixed(newStartPosX, true));
    }
    ImGui::Text("action %i frame %i name %s input %d", pGuy->getCurrentAction(), pGuy->getCurrentFrame(), pGuy->getCurrentActionPtr()->name.c_str(), pGuy->getCurrentInput());
    if (!pGuy->getProjectile()) {
        const char* states[] = { "stand", "jump", "crouch", "not you", "block", "not you", "crouch block" };
        modalDropDown("state", pGuy->getInputOverridePtr(), states, IM_ARRAYSIZE(states), 125);
        ImGui::SameLine();
        std::vector<const char *> &vecMoveList = pGuy->getMoveList();
        modalDropDown("recovery action", pGuy->getNeutralMovePtr(), vecMoveList, 300);
    }
    ImGui::Text("crouching %i airborne %i poseStatus %i landingAdjust %i", pGuy->getCrouchingDebug(), pGuy->getAirborneDebug(), pGuy->getforcedPoseStatus(), pGuy->getLandingAdjust());
    Fixed posX, posY, posOffsetX, posOffsetY, velX, velY, accelX, accelY;
    pGuy->getPosDebug(posX, posY, posOffsetX, posOffsetY);
    pGuy->getVel(velX, velY, accelX, accelY);
    ImGui::Text("pos %.2f %.2f %.2f direction %i inner %i posOffset %.2f %.2f", posX.f(), posY.f(), pGuy->getPosX().f(), pGuy->getDirection(), pGuy->getInnerDirection(), posOffsetX.f(), posOffsetY.f());
    ImGui::Text("vel %f %f %f accel %f %f %f", velX.f(), velY.f(), pGuy->getHitVelX().f(), accelX.f(), accelY.f(), pGuy->getHitAccelX().f());
    if ( !pGuy->getOpponent() ) {
        ImGui::SameLine();
        if ( ImGui::Button("switch direction") ) { pGuy->switchDirection(); }
    }
    std::vector<HitBox> hitBoxes;
    std::vector<Box> pushBoxes;
    std::vector<HurtBox> hurtBoxes;
    pGuy->getHitBoxes(&hitBoxes);
    pGuy->getPushBoxes(&pushBoxes);
    pGuy->getHurtBoxes(&hurtBoxes, nullptr);

    Fixed maxXHitBox = 0.0f;
    for (auto hitbox : hitBoxes) {
        if (hitbox.type != hitBoxType::hit && hitbox.type != hitBoxType::grab) continue;
        Fixed hitBoxX = hitbox.box.x + hitbox.box.w;
        if (hitBoxX > maxXHitBox) {
            maxXHitBox = hitBoxX;
        }
    }
    ImGui::Text("push %zd hit %zd hit extent %.2f hurt %zd regen cooldown %d scaled %d", pushBoxes.size(), hitBoxes.size(), maxXHitBox.f(), hurtBoxes.size(), pGuy->getFocusRegenCoolDown(), pGuy->getAppliedScaling());
    if (pGuy->getProjectile()) {
        ImGui::Text("limit category %i hit count %i hitstop %i", pGuy->getLimitShotCategory(), pGuy->getProjHitCount(), pGuy->getHitStop() );
        if (pGuy->getWarudo()) {
            ImGui::SameLine();
            ImGui::Text("warudo");
        }
    } else {
        int focus = pGuy->getFocus();
        if (pGuy->getBurnout()) {
            focus *= -1;
        }
        ImGui::Text("health %i focus %i gauge %i unique %s debuff %i style %i install %i timer %i", pGuy->getHealth(), focus, pGuy->getGauge(), pGuy->getUniqueParamsString().c_str(), pGuy->getDebuffTimer(), pGuy->getStyle(), pGuy->getInstallFrames(), pGuy->getUniqueTimer());
        ImGui::Text("COMBO HITS %i damage %i hitstun %i juggle %i hitstop %i down %i", pGuy->getComboHits(), pGuy->getComboDamage(), pGuy->getHitStun(), pGuy->getJuggleCounter(), pGuy->getHitStop(), pGuy->getIsDown());
        if (pGuy->getWarudo()) {
            ImGui::SameLine();
            ImGui::Text("warudo");
        }
    }
    if ( ImGui::Button("destroy") ) { pGuyToDelete = pGuy; }
    ImGui::SameLine();
    ImGui::Text("log:");
    ImGui::SameLine();
    bool logUnknowns = pGuy->getLogUnknowns();
    ImGui::Checkbox("unknowns", &logUnknowns);
    if (logUnknowns != pGuy->getLogUnknowns()) {
        pGuy->setLogUnknowns(logUnknowns);
    }
    ImGui::SameLine();
    bool logHits = pGuy->getLogHits();
    ImGui::Checkbox("hits", &logHits);
    if (logHits != pGuy->getLogHits()) {
        pGuy->setLogHits(logHits);
    }
    ImGui::SameLine();
    bool logTriggers = pGuy->getLogTriggers();
    ImGui::Checkbox("triggers", &logTriggers);
    if (logTriggers != pGuy->getLogTriggers()) {
        pGuy->setLogTriggers(logTriggers);
    }
    ImGui::SameLine();
    bool logBranches = pGuy->getLogBranches();
    ImGui::Checkbox("branches", &logBranches);
    if (logBranches != pGuy->getLogBranches()) {
        pGuy->setLogBranches(logBranches);
    }
    ImGui::SameLine();
    bool logTransitions = pGuy->getLogTransitions();
    ImGui::Checkbox("transitions", &logTransitions);
    if (logTransitions != pGuy->getLogTransitions()) {
        pGuy->setLogTransitions(logTransitions);
    }
    ImGui::SameLine();
    bool logResources = pGuy->getLogResources();
    ImGui::Checkbox("resources", &logResources);
    if (logResources != pGuy->getLogResources()) {
        pGuy->setLogResources(logResources);
    }
    auto logQueue = pGuy->getLogQueue();
    for (int i = logQueue.size() - 1; i >= 0; i--) {
        ImGui::Text("%s", logQueue[i].c_str());
    }
    ImGui::End();

    int minionID = 1;
    for (auto minion : pGuy->getMinions() ) {
        std::string minionWinName = std::string(windowName) + "'s Minion " + std::to_string(minionID++);
        drawGuyStatusWindow(minionWinName.c_str(), minion, pSim);
    }

    if (guys.size() >= 2 && (pGuy == guys[0] || pGuy == guys[1]) && pGuy->getOpponent() && pGuy->getOpponent()->getComboHits()) {
        renderComboMeter(pGuy == guys[1], pGuy->getOpponent()->getComboHits(), pGuy->getOpponent()->getComboDamage(), pGuy->getOpponent()->getLastDamageScale());
    }
}

struct timelineItem {
    int frame;
    int input;
    int uniqueID;
};

std::unordered_map<int, timelineItem*> mapTimelineItems;

void timelineToInputBuffer(std::deque<int> &inputBuffer)
{
    inputBuffer.clear();

    for (auto elem : mapTimelineItems)
    {
        while ((int)inputBuffer.size() <= elem.second->frame) {
            inputBuffer.push_back(0);
        }
        inputBuffer[elem.second->frame] |= elem.second->input;
    }

    inputBuffer.push_back(0); // add a neutral input at the end

    unsigned int i = 0;
    while (i < inputBuffer.size()) {
        int key = 4; // put down the _pressed bits on first appearance
        while (key < 10)
        {
            int input = 1<<key;
            if (inputBuffer[i] & input && (i == 0 || !(inputBuffer[i-1] & input))) {
                inputBuffer[i] |= 1<<(key+6);
            }
            key++;
        }
        i++;
    }
}

void drawHitboxExtentPlotWindow()
{
    if (vecPlotEntries.size() == 0) return;

    ImGui::Begin("Data Analysis Window");
    if (ImPlot::BeginPlot("Drive Normals Hitbox Ranges", ImVec2(-1,-1))) {
        ImPlot::SetupAxis(ImAxis_X1, "Frame", ImPlotAxisFlags_AutoFit);  
        ImPlot::SetupAxis(ImAxis_Y1, "Range", ImPlotAxisFlags_AutoFit);  
        unsigned int i = 0;
        while (i < vecPlotEntries.size()) {
            ImPlot::SetNextLineStyle(ImVec4(vecPlotEntries[i].col.r, vecPlotEntries[i].col.g, vecPlotEntries[i].col.b, 1), 3.0f);
            ImPlot::PlotLine(vecPlotEntries[i].strName.c_str(), vecPlotEntries[i].hitBoxRangePlotX.data(), vecPlotEntries[i].hitBoxRangePlotY.data(), vecPlotEntries[i].hitBoxRangePlotX.size());
            i++;
        }
        ImPlot::EndPlot();
    }
    ImGui::End();
}

void drawInputEditor()
{
    static int32_t currentFrame = 0;

    static int32_t startFrame = 0;
    static int32_t endFrame = 10000;
    //static bool transformOpen = false;

    unsigned int i = 0;
    static int uniqueID = 0;
    while (i < recordedInput.size()) {
        // in timeline frames
        int targetFrame = currentFrame + (int)i;

        int key = 0;
        while (key < 10)
        {
            if (recordedInput[i] & 1<<key) {

                if ((int)playBackInputBuffer.size() > targetFrame && playBackInputBuffer[targetFrame] & 1<<key) {
                    key++;
                    continue;
                }

                timelineItem *pNewNote = new timelineItem;
                *pNewNote = {
                    targetFrame,
                    1<<key,
                    uniqueID
                };
                mapTimelineItems[uniqueID++] = pNewNote;
            }
            key++;
        }
        i++;
    }
    recordedInput.clear();

    if (playingBackInput) {
        currentFrame = playBackFrame;
    }

    if (recordingInput) {
        currentFrame++;
    }

    ImGui::Begin("input editor");
    if (ImGui::BeginNeoSequencer("input", &currentFrame, &startFrame, &endFrame, {0, 0},
                                 ImGuiNeoSequencerFlags_AllowLengthChanging |
                                 ImGuiNeoSequencerFlags_EnableSelection |
                                 ImGuiNeoSequencerFlags_Selection_EnableDragging |
                                 ImGuiNeoSequencerFlags_Selection_EnableDeletion))
    {
        int key = 0;
        while (key < 10)
        {
            if (ImGui::BeginNeoTimelineEx(std::to_string(key).c_str()))
            {
                for (auto elem : mapTimelineItems)
                {
                    if (elem.second->input == 1<<key) {
                        ImGui::NeoKeyframe(&elem.second->frame);
                    }
                }

                if (deleteInputs)
                {
                    uint32_t count = ImGui::GetNeoKeyframeSelectionSize();

                    ImGui::FrameIndexType * toRemove = new ImGui::FrameIndexType[count];

                    ImGui::GetNeoKeyframeSelection(toRemove);

                    std::vector<int> vecUniqueIDs;
                    for (auto elem : mapTimelineItems)
                    {
                        if (elem.second->input == 1<<key) {
                            unsigned int delIndex = 0;
                            while (delIndex < count) {
                                if (elem.second->frame == toRemove[delIndex]) {
                                    vecUniqueIDs.push_back(elem.second->uniqueID);
                                }
                                delIndex++;
                            }
                        }
                    }

                    for (auto id : vecUniqueIDs)
                    {
                        delete mapTimelineItems[id];
                        mapTimelineItems.erase(id);
                    }

                    delete[] toRemove;
                }
                ImGui::EndNeoTimeLine();
            }
            key++;
        }
        ImGui::EndNeoSequencer();
    }

    deleteInputs = false;

    ImGui::End();
}

void drawComboFinderWindow()
{
    ImGui::Begin("Combo Miner");
    ImGui::Checkbox("Light normals", &comboFinderDoLights);
    ImGui::SameLine();
    ImGui::Checkbox("Late cancels", &comboFinderDoLateCancels);
    ImGui::SameLine();
    ImGui::Checkbox("Walk", &comboFinderDoWalk);
    ImGui::SameLine();
    ImGui::Checkbox("Karas", &comboFinderDoKaras);
    if (ImGui::Button("Run!")) {
        runComboFinder = true;
    }

    if (finder.running || finder.totalFrames > 0) {
        ImGui::Separator();
        ImGui::Text("threads: %d", finder.threadCount);

        uint64_t currentTotalFrames = finder.totalFrames;
        if (finder.running) {
            currentTotalFrames = 0;
            for (auto worker : finder.workerPool) {
                currentTotalFrames += worker->framesProcessed.load();
            }
        }

        uint64_t fps = finder.running ? finder.currentFPS : finder.finalFPS;

        ImGui::Text("frames: %s", formatWithCommas(currentTotalFrames).c_str());
        ImGui::Text("fps: %s", formatWithCommas(fps).c_str());
    }

    if (finder.doneRoutes.size() > 0 && guys.size() > 0) {
        ImGui::Separator();
        ImGui::Text("Top routes:");

        int routeCount = 0;
        const int maxRoutes = 10;
        for (auto it = finder.doneRoutes.rbegin(); it != finder.doneRoutes.rend() && routeCount < maxRoutes; ++it, ++routeCount) {
            std::string routeStr = routeToString(**it, guys[0]);
            ImGui::TextWrapped("%s", routeStr.c_str());
        }
    }

    if (finder.recentRoutes.size() > 0 && guys.size() > 0) {
        ImGui::Separator();
        ImGui::Text("Recent routes:");

        for (auto it = finder.recentRoutes.rbegin(); it != finder.recentRoutes.rend(); ++it) {
            std::string routeStr = routeToString(*it, guys[0]);
            ImGui::TextWrapped("%s", routeStr.c_str());
        }
    }

    ImGui::End();
}

void renderAdvancedUI(float frameRate, std::deque<std::string> *pLogQueue)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("PsychoDrive", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground );
    static float newCharColor[3] = { randFloat(), randFloat(), randFloat() };
    static int charID = rand() % charNames.size();
    static int versionID = charVersionCount - 1;
    static float newCharPos = 0.0;
    resetpos = resetpos || ImGui::Button("reset positions (Q)");
    ImGui::SameLine();
    if (ImGui::Button("combo finder (C)")) {
        showComboFinder = true;
    }
    ImGui::Text("add new guy:");
    ImGui::SliderFloat("##newcharpos", &newCharPos, -765.0, 765.0);
    ImGui::ColorEdit3("##newcharcolor", newCharColor);
    modalDropDown("##newcharchar", &charID, charNames.data(), charNames.size(), 100);
    ImGui::SameLine();
    modalDropDown("##newcharversion", &versionID, charVersions, charVersionCount, 200);
    ImGui::SameLine();
    if ( ImGui::Button("new guy") ) {
        color col = {newCharColor[0], newCharColor[1], newCharColor[2]};
        createGuy(charNames[charID], atoi(charVersions[versionID]), Fixed(newCharPos, true), Fixed(0.0f), 1, col );

        newCharColor[0] = randFloat();
        newCharColor[1] = randFloat();
        newCharColor[2] = randFloat();
        //newCharPos = randFloat() * 300 - 150.0;
    }
    for (auto i : currentInputMap) {
        if (i.first) {
            ImGui::SameLine();
        }
        ImGui::Text("input %i %i", i.first, i.second);
    }
    ImGui::Text("frame %d", defaultSim.frameCounter);
    if (paused) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "PAUSED");
    }
    if ( playingBackInput ) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "PLAY");
    }
    if ( recordingInput ) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "RECORDING");
    }
    ImGui::SameLine();
    if ( ImGui::Button("pause (P)") ) {
        paused = !paused;
    }
    ImGui::SameLine();
    if ( ImGui::Button("step one (0)") ) {
        oneframe = true;
    }
    ImGui::SameLine();
    char goToFrameText[32] = {};
    snprintf(goToFrameText, sizeof(goToFrameText), "%d", runUntilFrame);
    ImGui::SetNextItemWidth( 100.0 );
    ImGui::InputText("##startpostext", goToFrameText, sizeof(goToFrameText));
    runUntilFrame = atoi(goToFrameText);
    ImGui::Checkbox("force counter", &forceCounter);
    ImGui::SameLine();
    ImGui::Checkbox("force PC", &forcePunishCounter);
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / frameRate, frameRate);
    for (int i = pLogQueue->size() - 1; i >= 0; i--) {
        ImGui::Text("%s", (*pLogQueue)[i].c_str());
    }
    ImGui::End();

    pGuyToDelete = nullptr;
    int guyID = 1;
    for (auto guy : guys) {
        std::string windowName = "Guy " + std::to_string(guyID++);
        drawGuyStatusWindow( windowName.c_str(), guy );
    }

    if (pGuyToDelete) {
        delete pGuyToDelete;
        pGuyToDelete = nullptr;
    }

    drawInputEditor();

    drawHitboxExtentPlotWindow();

    if (showComboFinder) {
        drawComboFinderWindow();
    }
}

void renderUI(float frameRate, std::deque<std::string> *pLogQueue, int sizeX, int sizeY)
{
    renderSizeX = sizeX;
    renderSizeY = sizeY;
    if (!toggleRenderUI) {
        for (auto pGuy : guys) {
            if (guys.size() >= 2 && (pGuy == guys[0] || pGuy == guys[1]) && pGuy->getOpponent() && pGuy->getOpponent()->getComboHits()) {
                renderComboMeter(pGuy == guys[1], pGuy->getOpponent()->getComboHits(), pGuy->getOpponent()->getComboDamage(), pGuy->getOpponent()->getLastDamageScale());
            }
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return;
    }

    if (gameMode == Training) {
        renderAdvancedUI(frameRate, pLogQueue);
    }
    
    simController.RenderUI();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

ImGuiIO& initUI(void)
{
#ifdef __EMSCRIPTEN__
    webWidgets = true;
    gameMode = ComboMaker;
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsClassic();

    comboFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        Droid_Sans_compressed_data, Droid_Sans_compressed_size, 96.0
    );
    comboFontSubscript = io.Fonts->AddFontFromMemoryCompressedTTF(
        Droid_Sans_compressed_data, Droid_Sans_compressed_size, 64.0
    );
    fontBigger = io.Fonts->AddFontFromMemoryCompressedTTF(
        Droid_Sans_compressed_data, Droid_Sans_compressed_size, 24.0
    );
    font = io.Fonts->AddFontFromMemoryCompressedTTF(
        Droid_Sans_compressed_data, Droid_Sans_compressed_size, 18.0
    );

    if (webWidgets) {
        ImGui::PushFont(fontBigger);
    } else {
        ImGui::PushFont(font);
    }

    ImGui::GetStyle().TabRounding = 4.0f;
    ImGui::GetStyle().FrameRounding = 4.0f;
    ImGui::GetStyle().GrabRounding = 4.0f;
    ImGui::GetStyle().WindowRounding = 4.0f;
    ImGui::GetStyle().PopupRounding = 4.0f;

    ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(0.9f, 0.9f, 0.9f, 1.00f);

    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);

    simController.Reset();

    return io;
}

void destroyUI(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void CharacterUIController::renderCharSetup(void)
{
    if (modalDropDown("##char", &character, charNiceNames, 207)) {
        simInputsChanged = true;
        timelineTriggers.clear();
    }
    ImGui::SameLine();
    if (modalDropDown("##charversion", &charVersion, charVersions, charVersionCount, 35)) {
        simInputsChanged = true;
        // todo some actino ids might not match - make stitching code to find by name if needed?
    }

    if (gameMode == ComboMaker) {
        ImGui::SetNextItemWidth( webWidgets ? 250.0 : 170.0 );
        float newStartPosX = flStartPosX;
        ImGui::SliderFloat("##startpos", &newStartPosX, -765.0, 765.0);
        if (!webWidgets) {
            char startPosTest[32] = {};
            snprintf(startPosTest, sizeof(startPosTest), "%.2f", newStartPosX);
            ImGui::SameLine();
            ImGui::SetNextItemWidth( 70.0 );
            if (ImGui::InputText("##startpostext", startPosTest, sizeof(startPosTest))) {
                newStartPosX = atof(startPosTest);
            }
        }
        if (ImGui::Checkbox("Counter", &forceCounter) ) {
            simInputsChanged = true;
        }
        if (ImGui::Checkbox("Punish Counter", &forcePunishCounter) ) {
            simInputsChanged = true;
        }
        if (newStartPosX != flStartPosX) {
            flStartPosX = newStartPosX;
            startPosX = Fixed(flStartPosX, true);
            simInputsChanged = true;
        }

        if (ImGui::SliderInt("Health", &startHealth, 1, maxStartHealth) ) {
            simInputsChanged = true;
        }
        if (ImGui::SliderInt("Drive", &startFocus, 1, maxFocus) ) {
            simInputsChanged = true;
        }
        if (ImGui::SliderInt("Super", &startGauge, 1, maxStartGauge) ) {
            simInputsChanged = true;
        }
    }

    if (ImGui::SliderInt("Buff Level", &buffLevel, 0, 4) ) {
        simInputsChanged = true;
    }
}

void CharacterUIController::renderActionSetup(int frameIndex)
{
    if (!simController.pSim) {
        return;
    }

    Guy *pGuy = simController.getRecordedGuy(frameIndex, getSimCharSlot());

    ImGui::Text("Navigation:");
    if (ImGui::Button("<")) {
        int searchFrame = frameIndex;
        bool foundNoWindow = false;
        while (searchFrame >= 0) {
            Guy *pFrameGuy = simController.getRecordedGuy(searchFrame, getSimCharSlot());
            if (pFrameGuy && !foundNoWindow && pFrameGuy->getFrameTriggers() != pGuy->getFrameTriggers()) {
                foundNoWindow = true;
            }
            if (foundNoWindow && pFrameGuy->getFrameTriggers().size()) {
                simController.playing = true;
                simController.playUntilFrame = searchFrame;
                simController.playSpeed = -1;
                break;
            }
            searchFrame--;
        }
    }
    ImGui::SameLine();
    ImGui::Text("Action windows");
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        int searchFrame = frameIndex;
        bool foundNoWindow = false;
        while (searchFrame < simController.simFrameCount) {
            Guy *pFrameGuy = simController.getRecordedGuy(searchFrame, getSimCharSlot());
            if (pFrameGuy && !foundNoWindow && pFrameGuy->getFrameTriggers() != pGuy->getFrameTriggers()) {
                foundNoWindow = true;
            }
            if (foundNoWindow && pFrameGuy->getFrameTriggers().size()) {
                simController.playing = true;
                simController.playUntilFrame = searchFrame;
                simController.playSpeed = 1;
                break;
            }
            searchFrame++;
        }
    }
    if (timelineTriggers.find(frameIndex) != timelineTriggers.end()) {
        auto &trigger = timelineTriggers[frameIndex];
        std::string strLabel = "Delete " + timelineTriggerToString(trigger, pGuy);
        if (ImGui::Button(strLabel.c_str())) {
            timelineTriggers.erase(timelineTriggers.find(frameIndex));
            simInputsChanged = true;
            changed = true;
        }
    } else if (pGuy && pGuy->getFrameTriggers().size()) {
        vecTriggerDropDownLabels.clear();
        vecTriggers.clear();
        vecTriggerDropDownLabels.push_back("Add Action");
        for (auto &trigger : pGuy->getFrameTriggers()) {
            vecTriggerDropDownLabels.push_back(timelineTriggerToString(trigger, pGuy));
            vecTriggers.push_back(trigger);
        }
        if (modalDropDown("##moves", &pendingTriggerAdd, vecTriggerDropDownLabels, 220)) {
            if (pendingTriggerAdd != 0) {
                timelineTriggers[frameIndex] = vecTriggers[pendingTriggerAdd - 1];

                simInputsChanged = true;
                changed = true;
                pendingTriggerAdd = 0;
                triggerAdded = true;
            }
        }
    }

    static const int dirIDToInput[] = {
        5, 1, 9,
        4, 0, 8,
        6, 2, 10
    };
    static const int buttonIDToInput[] = {
        16, 32, 64,
        128, 256, 512,
    };
    int regionID = 0;
    int pendingDragID = 0;
    bool hasRegionDisplayed = false;
    for (auto &region: inputRegions) {

        // do this before continuing to have consistent enough IDs
        regionID++;

        if (activeInputDragID != regionID &&
            (frameIndex < region.frame || frameIndex >= region.frame + region.duration)) {
            continue;
        }

        if (!hasRegionDisplayed) {
            ImGui::Text("Current input regions:");
        }

        hasRegionDisplayed = true;

        float cursorX = ImGui::GetCursorPosX();
        float cursorY = ImGui::GetCursorPosY();

        ImGui::PushID(regionID);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0,0.0));
        // direction
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (j != 0) {
                    ImGui::SameLine();
                }
                int id = i * 3 + j;
                bool highlighted = dirIDToInput[id] == (region.input & 0xf);
                ImGui::PushID(id);
                if (highlighted) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45,0.15,0.1,1.0));
                }
                if (ImGui::Button("##", ImVec2(24.0,24.0))) {
                    region.input &= ~0xf;
                    region.input |= dirIDToInput[id];
                    simInputsChanged = true;
                    changed = true;
                }
                if (highlighted) {
                    ImGui::PopStyleColor();
                }
                ImGui::PopID();
            }
        }
        ImGui::PopStyleVar();
        // botans

        ImGui::SetCursorPosY(cursorY + 2.5);

        for (int i = 0; i < 2; i++) {
            ImGui::SetCursorPosX(cursorX + 85.0);
            for (int j = 0; j < 3; j++) {
                if (j != 0) {
                    ImGui::SameLine();
                }
                int id = i * 3 + j;
                bool highlighted = buttonIDToInput[id] & region.input;
                ImGui::PushID(10+id);
                if (highlighted) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45,0.15,0.1,1.0));
                }
                if (ImGui::Button("##", ImVec2(32.0,32.0))) {
                    region.input = region.input ^ buttonIDToInput[id];
                    simInputsChanged = true;
                    changed = true;
                }
                if (highlighted) {
                    ImGui::PopStyleColor();
                }
                ImGui::PopID();
            }
        }

        ImGui::SetCursorPosY(cursorY);

        ImGui::SetCursorPosX(cursorX + 205.0);
        ImGui::Text("Start frame:");

        ImGui::SameLine();
        std::string startFrameLabel = std::to_string(region.frame) + "###startframe";
        ImGui::Button(startFrameLabel.c_str());

        if (ImGui::IsItemActive()) {
            region.frame += (int)ImGui::GetMouseDragDelta(0, 0.0).x;
            simController.clampFrame(region.frame);
            ImGui::ResetMouseDragDelta();
            pendingDragID = regionID;
            simInputsChanged = true;
            changed = true;
        }

        ImGui::SetCursorPosX(cursorX + 205.0);
        ImGui::Text("Frame count:");

        ImGui::SameLine();
        std::string frameCountLabel = std::to_string(region.duration) + "###framecount";
        ImGui::Button(frameCountLabel.c_str());

        if (ImGui::IsItemActive()) {
            region.duration += (int)ImGui::GetMouseDragDelta(0, 0.0).x;
            simController.clampFrame(region.duration);
            ImGui::ResetMouseDragDelta();
            pendingDragID = regionID;
            simInputsChanged = true;
            changed = true;
        }

        ImGui::PopID();
    }

    activeInputDragID = pendingDragID;
    if (ImGui::Button(hasRegionDisplayed ? "Add Another Input" : "Add Input", ImVec2(220, 0))) {
        inputRegions.push_back({frameIndex, 1, 0});
    }
}

void CharacterUIController::RenderUI(void)
{
    if (!simController.pSim) {
        return;
    }

    Guy *pGuy = simController.getRecordedGuy(simController.scrubberFrame, getSimCharSlot());

    if (pGuy && pGuy->getOpponent()) {
        int opponentID = pGuy->getOpponent()->getUniqueID();
        Guy *pOpponent = simController.getRecordedGuy(simController.scrubberFrame, opponentID);

        if (pOpponent) {
            int comboHits = pOpponent->getComboHits();

            if (comboHits != 0) {
                renderComboMeter(rightSide, comboHits, pOpponent->getComboDamage(), pOpponent->getLastDamageScale());
            }
        }
    }
}

static ImVec4 frameMeterColors[] = {
    { 0.5,0.5,0.5,1.0 }, // default can't move grey (dash/jump)
    { 0.206,0.202,0.184,1.0 }, // can act/move very dark grey
    { 0.02,0.443,0.729,1.0 }, // recovery blue
    { 0.0,0.733,0.573,1.0 }, // startup green
    { 0.78,0.173,0.4,1.0 }, // active red
    { 1.0,0.965,0.224,1.0 }, // hitstun yellow
    { 0.25,0.11,0.11,1.0 }, // hitstop/warudo very dark red
};

void CharacterUIController::renderFrameMeterCancelWindows(int frameIndex)
{
    int frameCount = simController.stateRecording.size();

    float cursorX = ImGui::GetCursorPosX() - (frameIndex - kFrameOffset) * (kHorizSpacing + kFrameButtonWidth) + simController.frameMeterMouseDragAmount * simController.kFrameMeterDragRatio;
    float cursorY = ImGui::GetCursorPosY();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45,0.15,0.1,1.0));
    ImGui::BeginDisabled();
    for (int i = 0; i < frameCount; i++) {
        Guy *pGuy = simController.getRecordedGuy(i, getSimCharSlot());
        Guy *pGuyPrevFrame = simController.getRecordedGuy(i-1, getSimCharSlot());

        if (!pGuyPrevFrame || (pGuy->getFrameTriggers().size() && (pGuyPrevFrame->getFrameTriggers() != pGuy->getFrameTriggers()))) {
            // cancel window, figure out how far it goes
            int j = i;
            pGuyPrevFrame = pGuy;
            while (true) {
                j++;
                Guy *pGuyJ = simController.getRecordedGuy(j, getSimCharSlot());
                if (!pGuyJ || (pGuyPrevFrame->getFrameTriggers() != pGuyJ->getFrameTriggers())) {
                    break;
                }
                pGuyPrevFrame = pGuyJ;
            }

            float startOffset = (kFrameButtonWidth + kHorizSpacing) * i;
            ImGui::SetCursorPosX(cursorX + startOffset);
            ImGui::SetCursorPosY(cursorY);

            float cancelWidth = (kFrameButtonWidth + kHorizSpacing) * (j-i) - kHorizSpacing;
            ImGui::PushID(i);
            ImGui::Button("##input", ImVec2(cancelWidth,10.0));
            ImGui::PopID();

            simController.doFrameMeterDrag();
        }

        if (timelineTriggers.find(i) != timelineTriggers.end()) {
            float startOffset = (kFrameButtonWidth + kHorizSpacing) * i;
            ImGui::SetCursorPosX(cursorX + startOffset + 5.0);
            ImGui::SetCursorPosY(cursorY - 2.5);

            ImGui::PushID(i);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.5f);
            ImGui::Button("##timeline_trigger", ImVec2(15.0,15.0));
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
    }
    ImGui::EndDisabled();
    ImGui::PopStyleColor();
}

void CharacterUIController::renderFrameMeter(int frameIndex)
{
    ImGui::PushID(getSimCharSlot());

    const float kFrameButtonHeight = 35.0;
    const ImVec4 kFrameButtonBorderColor = ImVec4(0.0,0.0,0.0,1.0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kHorizSpacing,ImGui::GetStyle().ItemSpacing.y));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, kFrameButtonBorderColor);
    int sameColorCount = 0;
    int frameCount = simController.stateRecording.size();

    if (!rightSide) {
        Guy *pGuy = simController.getRecordedGuy(simController.scrubberFrame, getSimCharSlot());
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + kFrameOffset * (kHorizSpacing + kFrameButtonWidth));
        ImGui::Text("%s %d/%d", pGuy->getCurrentActionPtr()->niceNameDyn.c_str(), pGuy->getCurrentFrame(), pGuy->getCurrentActionPtr()->actionFrameDuration);
        renderFrameMeterCancelWindows(frameIndex);
    }

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - (frameIndex - kFrameOffset) * (kHorizSpacing + kFrameButtonWidth) + simController.frameMeterMouseDragAmount * simController.kFrameMeterDragRatio);
    float tronglePosY = ImGui::GetCursorPosY();

    bool foundAdvantageWindow = false;
    int endAdvantageWindowFrame = -1;
    int advantageFrames = 0;
    if (simController.charCount == 2) {
        // walk from the end and find advantage
        int i = frameCount - 1;
        while (i > 0) {
            Guy *pGuy = simController.getRecordedGuy(i, getSimCharSlot());
            Guy *pOtherGuy = simController.getRecordedGuy(i, !getSimCharSlot());
            if (pGuy->getFrameMeterColorIndex() == 1 && pOtherGuy->getFrameMeterColorIndex() != 1) {
                foundAdvantageWindow = true;
                endAdvantageWindowFrame = i;
                while (i > 0) {
                    Guy *pGuy = simController.getRecordedGuy(i, getSimCharSlot());
                    if (pGuy->getFrameMeterColorIndex() != 1) {
                        break;
                    }
                    advantageFrames++;
                    i--;
                }
                if (i == 0) {
                    foundAdvantageWindow = false;
                }
                if (foundAdvantageWindow) {
                    break;
                }
            }
            if (pGuy->getFrameMeterColorIndex() != 1 && pOtherGuy->getFrameMeterColorIndex() != 1) {
                break;
            }
            i--;
        }
    }

    for (int i = 0; i < frameCount; i++) {
        Guy *pGuy = simController.getRecordedGuy(i, getSimCharSlot());
        Guy *pGuyNextFrame = simController.getRecordedGuy(i+1, getSimCharSlot());
        if (i != 0) ImGui::SameLine();

        ImGui::PushID(i);

        int colorIndex = pGuy->getFrameMeterColorIndex();
        ImGui::PushStyleColor(ImGuiCol_Button, frameMeterColors[colorIndex]);
        bool darkText = false;
        if (colorIndex != 1 && colorIndex != 6) {
            darkText = true;
        }
        if (darkText) {
            ImGui::PushStyleColor(ImGuiCol_Text, kFrameButtonBorderColor);
        }
        std::string strButtonCaption = "";
        if (!pGuyNextFrame || colorIndex != pGuyNextFrame->getFrameMeterColorIndex()) {
            strButtonCaption = std::to_string(sameColorCount+1);
            sameColorCount = 0;
        } else {
            sameColorCount++;
        }
        bool popColor = false;
        if (foundAdvantageWindow && endAdvantageWindowFrame == i+1 && advantageFrames > 9) {
            strButtonCaption = "+";
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0,1.0,0.0,1.0));
            popColor = true;
        }
        if (foundAdvantageWindow && endAdvantageWindowFrame == i) {
            if (advantageFrames < 10) {
                strButtonCaption = "+" + std::to_string(advantageFrames);
            } else {
                strButtonCaption = std::to_string(advantageFrames);
            }
            sameColorCount = 0;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0,1.0,0.0,1.0));
            popColor = true;
        }
        if (webWidgets) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(strButtonCaption.c_str(), ImVec2(kFrameButtonWidth,kFrameButtonHeight))) {
            simController.scrubberFrame = i;
        }
        if (webWidgets) {
            ImGui::EndDisabled();
        }
        if (darkText) {
            ImGui::PopStyleColor();
        }
        if (popColor) {
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleColor();

        simController.doFrameMeterDrag();

        ImGui::PopID();
    }

    if (rightSide) {
        renderFrameMeterCancelWindows(frameIndex);
        Guy *pGuy = simController.getRecordedGuy(simController.scrubberFrame, getSimCharSlot());
        Action *pAction = pGuy->getCurrentActionPtr();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + kFrameOffset * (kHorizSpacing + kFrameButtonWidth));
        ImGui::Text("%s %d/%d", pAction->niceNameDyn.c_str(), pGuy->getCurrentFrame(), pAction->actionFrameDuration);
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

    ImVec2 trongle[] = {
        {15.0,0.0},
        {25.0,0.0},
        {20.0,8.0}
    };
    // can't just invert it as both need to be clockwise for imgui AA
    ImVec2 upsideDownTrongle[] = {
        {15.0,0.0},
        {20.0,-8.0},
        {25.0,0.0}
    };
    ImVec2 *curTrongle = rightSide ? upsideDownTrongle : trongle;
    for (int i = 0; i < IM_ARRAYSIZE(trongle); i++) {
        curTrongle[i] += ImGui::GetWindowPos();
        curTrongle[i].x += kFrameOffset * (kHorizSpacing + kFrameButtonWidth);
        curTrongle[i].y += tronglePosY;
        if (rightSide) {
            curTrongle[i].y += kFrameButtonHeight - 1.0;
        } else {
            curTrongle[i].y += 1.0;
        }
    }
    ImGui::GetCurrentWindow()->DrawList->AddConvexPolyFilled(curTrongle, IM_ARRAYSIZE(trongle), ImColor(kFrameButtonBorderColor));

    ImGui::PopID();
}

int CharacterUIController::getInput(int frameID)
{
    int input = 0;
    for (auto &region: inputRegions) {
        if ((frameID < region.frame || frameID >= region.frame + region.duration)) {
            continue;
        }
        input |= region.input;
    }
    return input;
}

SimulationController::~SimulationController()
{
    // clear stateRecording before guyPool destructs to avoid dangling pointers
    stateRecording.clear();
}

void SimulationController::Reset(void)
{
    charControllers.clear();
    charControllers.emplace_back();
    charControllers.emplace_back();

    charControllers[0].character = 28;
    charControllers[0].charVersion = charVersionCount - 1;
    charControllers[0].charColor = { 1.0, 1.0, 1.0 };
    charControllers[1].character = 0;
    charControllers[1].charVersion = charVersionCount - 1;
    charControllers[1].rightSide = true;
    charControllers[1].charColor = { 0.15, 0.5, 0.78 };

    if (gameMode == ComboMaker) {
        charControllers[0].startPosX = Fixed(-150);
        charControllers[1].startPosX = Fixed(150);
    } else {
        charControllers[0].startPosX = Fixed(0);
        charControllers[1].startPosX = Fixed(0);
    }

    for (auto &charController : charControllers) {
        charController.timelineTriggers.clear();
        charController.flStartPosX = charController.startPosX.f();
    }
}

bool triedRestoreFromURL = false;
bool hasRestored = false;

std::string pendingLoad;
bool hasPendingLoad = false;

bool SimulationController::NewSim(void)
{
#ifdef __EMSCRIPTEN__
    if (!triedRestoreFromURL) {
        std::string strCombo = (char*)EM_ASM_PTR({
            let params = new URLSearchParams(document.location.search || window.location.hash.slice(1));
            let combo = params?.get("combo");
            if (combo == null) {
                combo = "";
            }
            return stringToNewUTF8(combo);
        });
        if (strCombo != "") {
            Restore(strCombo);
            hasRestored = true;
        }
        triedRestoreFromURL = true;
    }
#endif

    if (hasPendingLoad) {
        Restore(pendingLoad);
        hasPendingLoad = false;
    }
    charCount = 1;

    if (gameMode == ComboMaker) {
        charCount = 2;
    }

    bool charsLoaded = true;

    for (int i = 0; i < charCount; i++) {
        const char *charName = charNames[charControllers[i].character];
        int charVersion = atoi(charVersions[charControllers[i].charVersion]);
        if (!isCharLoaded(charName, charVersion)) {
            requestCharDownload(charName, charVersion);
            charsLoaded = false;
        }
    }

    if (!charsLoaded) {
        return false;
    }

    if (pSim != nullptr) {
        delete pSim;
        pSim = nullptr;
    }

    pSim = new Simulation;

    if (pSim == nullptr) {
        abort();
    }

    stateRecording.clear();
    guyPool.reset();

    for (int i = 0; i < charCount; i++) {
        pSim->CreateGuyFromCharController(charControllers[i]);
    }

    for (auto &guy : pSim->simGuys) {
        guy->setRecordFrameTriggers(true, true);
    }


    maxComboCount = 0;
    maxComboDamage = 0;

    if (hasRestored) {
        playing = true;
        hasRestored = false;
    }

    return true;
}

void SimulationController::RecordFrame(void)
{
    if (!recordFrames) return;

    stateRecording.emplace_back();
    RecordedFrame &frame = stateRecording[stateRecording.size()-1];

    frame.sim.enableCleanup = false;
    frame.sim.Clone(pSim, &guyPool);
    if (gameMode == Viewer) {
        for (int i = 0; i < (int)pSim->everyone.size() && i < (int)frame.sim.everyone.size(); i++) {
            frame.sim.everyone[i]->getLogQueue() = pSim->everyone[i]->getLogQueue();
        }
    }
    frame.events = pSim->getCurrentFrameEvents();
    pSim->getCurrentFrameEvents().clear();
}

Guy *SimulationController::getRecordedGuy(int frameIndex, int guyID)
{
    if (frameIndex >= 0 && frameIndex < (int)stateRecording.size()) {
        for (auto guy : stateRecording[frameIndex].sim.everyone) {
            if (guy->getUniqueID() == guyID) {
                return guy;
            }
        }
    }
    return nullptr;
}

void SimulationController::renderRecordedHitMarkers(int frameIndex)
{
    int maxMarkerAge = 10;
    int startFrame = std::max(0, frameIndex - maxMarkerAge + 1);

    if (frameIndex >= (int)stateRecording.size()) return;

    for (int checkFrame = startFrame; checkFrame <= frameIndex; checkFrame++) {
        if (checkFrame >= (int)stateRecording.size()) continue;
        auto &histFrame = stateRecording[checkFrame];

        for (const auto &event : histFrame.events) {
            if (event.type == FrameEvent::Hit) {
                int markerAge = frameIndex - checkFrame;

                Guy* targetGuy = nullptr;
                for (auto guy : histFrame.sim.everyone) {
                    if (guy->getUniqueID() == event.hitEventData.targetID) {
                        targetGuy = guy;
                        break;
                    }
                }

                if (targetGuy) {
                    float worldX = targetGuy->getPosX().f() + event.hitEventData.x;
                    float worldY = targetGuy->getPosY().f() + event.hitEventData.y;
                    drawHitMarker(worldX, worldY, event.hitEventData.radius,
                                 event.hitEventData.hitType, markerAge, maxMarkerAge,
                                 event.hitEventData.dirX, event.hitEventData.dirY, event.hitEventData.seed);
                }
            }
        }
    }
}

Simulation *SimulationController::getSnapshotAtFrame(int frameIndex)
{
    if (frameIndex >= 0 && frameIndex < (int)stateRecording.size()) {
        return &stateRecording[frameIndex].sim;
    }
    return nullptr;
}

void SimulationController::doFrameMeterDrag(void)
{
    if (ImGui::IsItemActive()) {
        momentumActive = false;
        lastDragDelta = ImGui::GetMouseDragDelta(0, 0.0).x;
        pendingFrameMeterMouseDragAmount += lastDragDelta;

        activeDragID = ImGui::GetItemID();
        ImGui::ResetMouseDragDelta();
    } else if (activeDragID == ImGui::GetItemID()) {
        activeDragID = 0;
        momentumActive = true;
        curMomentum = lastDragDelta;
    }
}

static bool showResults = true;

void SimulationController::RenderComboMinerSetup(void)
{
    ImGui::Dummy(ImVec2(800, 0));

    ImGui::PushFont(font);
    if (finder.running) {
        if (ImGui::Button("Stop")) {
            stopComboFinder();
        }
    } else {
        if (ImGui::Button("Run!")) {
            runComboFinder = true;
        }
    }
    if (finder.doneRoutes.size()) {
        ImGui::SameLine();
        if (ImGui::Button("Toggle Results")) {
            showResults = !showResults;
        }
    }
    if (!finder.running) {
        ImGui::Checkbox("Late cancels", &comboFinderDoLateCancels);
        ImGui::SameLine();
        ImGui::Checkbox("Walk", &comboFinderDoWalk);
        ImGui::Checkbox("Light normals", &comboFinderDoLights);
        ImGui::SameLine();
        ImGui::Checkbox("Karas", &comboFinderDoKaras);
    }
    if (finder.running || finder.totalFrames > 0) {
        ImGui::Separator();
        ImGui::Text("threads: %d", finder.threadCount);
        ImGui::SameLine();
        uint64_t currentTotalFrames = finder.totalFrames;
        if (finder.running) {
            currentTotalFrames = 0;
            for (auto worker : finder.workerPool) {
                currentTotalFrames += worker->framesProcessed.load();
            }
        }

        uint64_t fps = finder.running ? finder.currentFPS : finder.finalFPS;

        ImGui::Text("frames: %s", formatWithCommas(currentTotalFrames).c_str());
        ImGui::SameLine();
        ImGui::Text("fps: %s", formatWithCommas(fps).c_str());
    }

    static int routeScrollAmount = 0;
    const int routesToDisplay = 10;
    int routeCount = 0;
    static int sortMode = 0;

    bool filterActive = filterIsActive(finder);
    size_t totalVisibleCount = 0;
    std::vector<DoneRoute *> vecVisibleRoutes;

    auto clampScroll = [&]() {
        if (routeScrollAmount > (int)totalVisibleCount - routesToDisplay) routeScrollAmount = (int)totalVisibleCount - routesToDisplay;
        if (routeScrollAmount < 0) routeScrollAmount = 0;
    };

    if (filterActive) {
        if (sortMode == 0) {
            totalVisibleCount = finder.filteredByDamage.size();
            clampScroll();
            for (auto it = std::next(finder.filteredByDamage.rbegin(), routeScrollAmount); it != finder.filteredByDamage.rend() && (int)vecVisibleRoutes.size() < routesToDisplay; ++it) {
                vecVisibleRoutes.push_back(*it);
            }
        } else if (sortMode == 1) {
            totalVisibleCount = finder.filteredByFocusGain.size();
            clampScroll();
            for (auto it = std::next(finder.filteredByFocusGain.rbegin(), routeScrollAmount); it != finder.filteredByFocusGain.rend() && (int)vecVisibleRoutes.size() < routesToDisplay; ++it) {
                vecVisibleRoutes.push_back(*it);
            }
        } else if (sortMode == 2) {
            totalVisibleCount = finder.filteredByGaugeGain.size();
            clampScroll();
            for (auto it = std::next(finder.filteredByGaugeGain.rbegin(), routeScrollAmount); it != finder.filteredByGaugeGain.rend() && (int)vecVisibleRoutes.size() < routesToDisplay; ++it) {
                vecVisibleRoutes.push_back(*it);
            }
        } else if (sortMode == 3) {
            totalVisibleCount = finder.filteredByFocusDmg.size();
            clampScroll();
            for (auto it = std::next(finder.filteredByFocusDmg.rbegin(), routeScrollAmount); it != finder.filteredByFocusDmg.rend() && (int)vecVisibleRoutes.size() < routesToDisplay; ++it) {
                vecVisibleRoutes.push_back(*it);
            }
        }
    } else {
        totalVisibleCount = finder.doneRoutes.size();
        clampScroll();
        if (sortMode == 0) {
            for (auto it = std::next(finder.doneRoutes.rbegin(), routeScrollAmount); it != finder.doneRoutes.rend() && (int)vecVisibleRoutes.size() < routesToDisplay; ++it) {
                vecVisibleRoutes.push_back(it->get());
            }
        } else if (sortMode == 1) {
            for (auto it = std::next(finder.doneRoutesByFocusGain.rbegin(), routeScrollAmount); it != finder.doneRoutesByFocusGain.rend() && (int)vecVisibleRoutes.size() < routesToDisplay; ++it) {
                vecVisibleRoutes.push_back(*it);
            }
        } else if (sortMode == 2) {
            for (auto it = std::next(finder.doneRoutesByGaugeGain.rbegin(), routeScrollAmount); it != finder.doneRoutesByGaugeGain.rend() && (int)vecVisibleRoutes.size() < routesToDisplay; ++it) {
                vecVisibleRoutes.push_back(*it);
            }
        } else if (sortMode == 3) {
            for (auto it = std::next(finder.doneRoutesByFocusDmg.rbegin(), routeScrollAmount); it != finder.doneRoutesByFocusDmg.rend() && (int)vecVisibleRoutes.size() < routesToDisplay; ++it) {
                vecVisibleRoutes.push_back(*it);
            }
        }
    }

    if (showResults && finder.doneRoutes.size() > 0 && pSim && pSim->simGuys.size() > 0) {
        ImGui::Separator();
        if (filterActive) {
            ImGui::Text("%zu / %zu routes:", totalVisibleCount, finder.doneRoutes.size());
        } else {
            ImGui::Text("%zu routes:", totalVisibleCount);
        }
        if ((int)totalVisibleCount > routesToDisplay) {
            ImGui::SameLine();
            if (ImGui::Button("<")) {
                routeScrollAmount -= routesToDisplay;
                if (routeScrollAmount < 0) {
                    routeScrollAmount = 0;
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(300);
            ImGui::SliderInt("##routescroll", &routeScrollAmount, 0, (int)totalVisibleCount - routesToDisplay);
            ImGui::SameLine();
            if (ImGui::Button(">")) {
                routeScrollAmount += routesToDisplay;
                if (routeScrollAmount > (int)totalVisibleCount - routesToDisplay) {
                    routeScrollAmount = (int)totalVisibleCount - routesToDisplay;
                }
            }
        }
        ImGui::Text("Sort:");
        ImGui::SameLine();
        bool highlighted = sortMode == 0;
        if (highlighted) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45,0.15,0.1,1.0));
        }
        if (ImGui::Button("Damage")) {
            sortMode = 0;
        }
        if (highlighted) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        highlighted = sortMode == 1;
        if (highlighted) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45,0.15,0.1,1.0));
        }
        if (ImGui::Button("Drive Gain")) {
            sortMode = 1;
        }
        if (highlighted) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        highlighted = sortMode == 2;
        if (highlighted) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45,0.15,0.1,1.0));
        }
        if (ImGui::Button("Super Gain")) {
            sortMode = 2;
        }
        if (highlighted) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        highlighted = sortMode == 3;
        if (highlighted) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45,0.15,0.1,1.0));
        }
        if (ImGui::Button("Drive Damage")) {
            sortMode = 3;
        }
        if (highlighted) {
            ImGui::PopStyleColor();
        }

        ImGui::Text("Filter:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderInt("Drive##filter", &finder.filterFocusBars, 0, 6, finder.filterFocusBars >= 6 ? "any" : "<= %d bars")) {
            finder.filterDirty = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderInt("Super##filter", &finder.filterGaugeBars, 0, 3, finder.filterGaugeBars >= 3 ? "any" : "<= %d bars")) {
            finder.filterDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Side switch", &finder.filterSideSwitchOnly)) {
            finder.filterDirty = true;
        }
        if (finder.sawImpossible) {
            ImGui::SameLine();
            if (ImGui::Checkbox("Impossible?", &finder.filterImpossibleOnly)) {
                finder.filterDirty = true;
            }
        }

        routeCount = 0;
        for (DoneRoute *route : vecVisibleRoutes) {
            std::string routeStr = routeToString(*route, pSim->simGuys[0]);
            ImGui::PushID(routeCount++);
            if (ImGui::Button("Load")) {
                for (int i = 0; i < 2; i++) {
                    simController.charControllers[i].timelineTriggers = finder.startTimelineTriggers[i];
                    simController.charControllers[i].inputRegions = finder.startInputRegions[i];
                }
                for (auto & trigger : route->timelineTriggers) {
                    simController.charControllers[0].timelineTriggers[(int)trigger.first-1] = trigger.second;
                }
                simInputsChanged = true;
                simController.charControllers[0].changed = true;
            }
            ImGui::SameLine();
            ImGui::TextWrapped("%s", routeStr.c_str());
            ImGui::PopID();
        }
    }

    if (showResults && finder.running && finder.recentRoutes.size() > 0 && pSim && pSim->simGuys.size() > 0) {
        ImGui::Separator();
        ImGui::Text("Recent routes:");

        for (auto it = finder.recentRoutes.rbegin(); it != finder.recentRoutes.rend(); ++it, ++routeCount) {
            std::string routeStr = routeToString(*it, pSim->simGuys[0]);
            ImGui::PushID(routeCount);
            if (ImGui::Button("Load")) {
                for (int i = 0; i < 2; i++) {
                    simController.charControllers[i].timelineTriggers = finder.startTimelineTriggers[i];
                    simController.charControllers[i].inputRegions = finder.startInputRegions[i];
                }
                for (auto & trigger : it->timelineTriggers) {
                    simController.charControllers[0].timelineTriggers[(int)trigger.first-1] = trigger.second;
                }
                simInputsChanged = true;
                simController.charControllers[0].changed = true;
            }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::TextWrapped("%s", routeStr.c_str());
        }
    }
    ImGui::PopFont();
}

void SimulationController::RenderUI(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0,1.0,1.0,0.05));

    bool opaqueWindow = false;
    if (viewSelect == 4 && showResults && finder.doneRoutes.size()) {
        opaqueWindow = true;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0,1.0,1.0,0.10));
    }
    if (gameMode != Training) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin("PsychoDrive Left Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus );
        if (gameMode == Viewer) {
            ImGui::Text("Log:");
            ImGui::SameLine();
            bool changed = false;
            changed |= ImGui::Checkbox("unknowns", &viewerLogUnknowns);
            ImGui::SameLine();
            changed |= ImGui::Checkbox("hits", &viewerLogHits);
            ImGui::SameLine();
            changed |= ImGui::Checkbox("triggers", &viewerLogTriggers);
            ImGui::SameLine();
            changed |= ImGui::Checkbox("branches", &viewerLogBranches);
            ImGui::SameLine();
            changed |= ImGui::Checkbox("transitions", &viewerLogTransitions);
            ImGui::SameLine();
            changed |= ImGui::Checkbox("resources", &viewerLogResources);
            if (changed) {
                ReloadViewer();
            }
            ImGui::Separator();
            if (replayRoundCount > 0) {
                for (int r = 0; r < replayRoundCount; r++) {
                    auto &result = replayRoundRecordings[r].result;
                    if (result.hasErrors) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    }
                    bool selected = (r == replayCurrentRound);
                    std::string label = result.summary;
                    if (selected) label = "> " + label;
                    if (ImGui::Selectable(label.c_str(), selected)) {
                        LoadReplayRound(r);
                        simInputsChanged = false;
                    }
                    if (result.hasErrors) {
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::Separator();
            }
            if (replayRoundCount == 0 && pSim && !pSim->errorLog.empty()) {
                static constexpr int errorTypeCount = 14;
                // warning types: vel, accel, hitVel, hitAccel
                static constexpr int warningTypes[] = {1, 2, 3, 4};
                static constexpr int warningTypeCount = IM_ARRAYSIZE(warningTypes);
                struct ErrorColumn { int type; const char *name; };
                static constexpr ErrorColumn errorColumns[] = {
                    {5, "ActionID"}, {6, "ActionFrame"}, {0, "Pos"},
                    {7, "Combo"}, {8, "Direction"}, {9, "Health"},
                    {10, "Hitstop"}, {11, "Gauge"}, {12, "FocusRegen"}, {13, "RandomSeed"}
                };
                static constexpr int errorColumnCount = IM_ARRAYSIZE(errorColumns);

                struct ParsedError {
                    int frame;
                    int type;
                    int descStart;
                };
                static std::vector<ParsedError> parsedErrors;
                static Simulation *errorFramesSim = nullptr;
                if (errorFramesSim != pSim || parsedErrors.size() != pSim->errorLog.size()) {
                    errorFramesSim = pSim;
                    parsedErrors.resize(pSim->errorLog.size());
                    for (int idx = 0; idx < (int)pSim->errorLog.size(); idx++) {
                        auto &err = pSim->errorLog[idx];
                        int semiCount = 0;
                        int fStart = -1, fEnd = -1, tStart = -1, tEnd = -1, dStart = -1;
                        for (int c = 0; c < (int)err.size(); c++) {
                            if (err[c] == ';') {
                                semiCount++;
                                if (semiCount == 2) fStart = c + 1;
                                if (semiCount == 3) { fEnd = c; tStart = c + 1; }
                                if (semiCount == 4) { tEnd = c; dStart = c + 1; break; }
                            }
                        }
                        parsedErrors[idx].frame = (fStart >= 0 && fEnd > fStart) ? atoi(err.substr(fStart, fEnd - fStart).c_str()) : -1;
                        parsedErrors[idx].type = (tStart >= 0 && tEnd > tStart) ? atoi(err.substr(tStart, tEnd - tStart).c_str()) : -1;
                        parsedErrors[idx].descStart = dStart;
                    }
                }

                int errorTypeCounts[errorTypeCount] = {};
                for (auto &pe : parsedErrors) {
                    if (pe.type >= 0 && pe.type < errorTypeCount) {
                        errorTypeCounts[pe.type]++;
                    }
                }

                auto passesFilter = [&](int type) -> bool {
                    if (type < 0 || type >= errorTypeCount) return true;
                    return viewerErrorTypeFilter[type];
                };

                int filteredCount = 0;
                for (auto &pe : parsedErrors) {
                    if (passesFilter(pe.type)) filteredCount++;
                }

                int warningCount = 0;
                for (int w = 0; w < warningTypeCount; w++) warningCount += errorTypeCounts[warningTypes[w]];

                ImGui::Text("Errors: %d (showing %d)", (int)pSim->errorLog.size(), filteredCount);
                bool filterChanged = false;
                int checkboxCount = 0;
                if (warningCount > 0) {
                    // all warning types share the first warning type's filter bit
                    std::string wLabel = "Warnings (" + std::to_string(warningCount) + ")";
                    if (ImGui::Checkbox(wLabel.c_str(), &viewerErrorTypeFilter[warningTypes[0]])) {
                        for (int w = 1; w < warningTypeCount; w++) {
                            viewerErrorTypeFilter[warningTypes[w]] = viewerErrorTypeFilter[warningTypes[0]];
                        }
                        filterChanged = true;
                    }
                    checkboxCount++;
                    ImGui::SameLine();
                }
                for (int c = 0; c < errorColumnCount; c++) {
                    int t = errorColumns[c].type;
                    if (errorTypeCounts[t] == 0) continue;
                    if (checkboxCount == 5) {
                        ImGui::NewLine();
                        checkboxCount = 0;
                    }
                    std::string label = std::string(errorColumns[c].name) + " (" + std::to_string(errorTypeCounts[t]) + ")";
                    filterChanged |= ImGui::Checkbox(label.c_str(), &viewerErrorTypeFilter[t]);
                    checkboxCount++;
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::Separator();

                ImGui::BeginChild("ErrorList", ImVec2(800, 500), false, ImGuiWindowFlags_None);
                int visibleIdx = 0;
                int scrollTargetVisible = -1;
                for (int idx = 0; idx < (int)pSim->errorLog.size(); idx++) {
                    auto &pe = parsedErrors[idx];
                    if (pe.type >= 0 && pe.type < errorTypeCount && !viewerErrorTypeFilter[pe.type]) {
                        continue;
                    }
                    auto &err = pSim->errorLog[idx];
                    std::string display;
                    if (pe.frame >= 0 && pe.descStart > 0) {
                        display = "[" + std::to_string(pe.frame) + "] " + err.substr(pe.descStart);
                    } else {
                        display = err;
                    }
                    int scrubberForError = pe.frame + 1;
                    bool isCurrentFrame = (scrubberForError == scrubberFrame);
                    if (isCurrentFrame) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    }
                    if (ImGui::Selectable(display.c_str())) {
                        if (pe.frame >= 0) {
                            scrubberFrame = scrubberForError;
                            clampFrame(scrubberFrame);
                        }
                    }
                    if (isCurrentFrame) {
                        ImGui::PopStyleColor();
                    }
                    // track first visible error at or after current frame for scroll
                    if (scrollTargetVisible == -1 && scrubberForError >= scrubberFrame) {
                        scrollTargetVisible = visibleIdx;
                    }
                    visibleIdx++;
                }
                // scroll when scrubber changed (playback, drag, or click) or filter changed
                if ((scrubberFrame != prevScrubberFrame || filterChanged) && scrollTargetVisible >= 0) {
                    float itemHeight = ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY(scrollTargetVisible * itemHeight);
                }
                prevScrubberFrame = scrubberFrame;
                ImGui::EndChild();
            }
        } else {
            switch (viewSelect) {
                default:
                case 0:
                    charControllers[0].renderCharSetup();
                    break;
                case 1:
                    charControllers[0].renderActionSetup(scrubberFrame);
                    break;
                case 2:
                    charControllers[1].renderCharSetup();
                    break;
                case 3:
                    charControllers[1].renderActionSetup(scrubberFrame);
                    break;
                case 4:
                    RenderComboMinerSetup();
                    break;
            }
            if (maxComboCount > 0) {
                std::string strComboInfo = "Current combo max damage: " + std::to_string(maxComboDamage);
                ImGui::Text("%s", strComboInfo.c_str());
            }
        }
        ImGui::End();
    }
    if (opaqueWindow) {
        ImGui::PopStyleColor();
    }

    // Top right panel
    int modeSelectorSize = 180;
    ImGui::SetNextWindowPos(ImVec2(renderSizeX - 190.0, 0));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("PsychoDrive Top Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse );

    int simFrameCount = pSim ? stateRecording.size() : 0;

    if (gameMode == Viewer) {
        ImGui::Text("Viewer");
    } else {
    const char* modes[] = { "Developer", "Move Viewer", "Combo Maker" };
    if (modalDropDown("##gamemode", (int*)&gameMode, modes, IM_ARRAYSIZE(modes), modeSelectorSize)) {
        simInputsChanged = true;
        simController.Reset();

        translateY = gameMode == Training ? 150.0 : 100.0;
    }

    if (gameMode != Training) {
        std::vector<std::string> vecViewLabels;
        vecViewLabels.push_back("P1 Setup");
        vecViewLabels.push_back("P1 Actions");
        if (gameMode == ComboMaker) {
            vecViewLabels.push_back("P2 Setup");
            vecViewLabels.push_back("P2 Actions");
            vecViewLabels.push_back("Combo Miner");
        }
        if (viewSelect >= (int)vecViewLabels.size()) {
            viewSelect = 0;
        }
        modalDropDown("##viewselect", (int*)&viewSelect, vecViewLabels, modeSelectorSize);
        int halfButtonSize = modeSelectorSize / 2 - 5;
        if (ImGui::Button("Share", ImVec2(halfButtonSize,0))) {
            std::string strSerialized;
            Serialize(strSerialized);
            //Restore(strSerialized);
#ifdef __EMSCRIPTEN__
            EM_ASM({
                var serialized = UTF8ToString($0);
                navigator.clipboard.writeText(window.location.protocol + "//" + window.location.host + window.location.pathname + '#combo=' + serialized);
                //window.location.href = '?combo=' + serialized;
            }, strSerialized.c_str());
#else
            std::string strComboURL = "https://psychodrive.gg/#combo=" + strSerialized;
            SDL_SetClipboardText(strComboURL.c_str());
#endif
        }
        ImGui::SameLine();
#ifdef __EMSCRIPTEN__
        if (ImGui::Button("Load", ImVec2(halfButtonSize,0))) {
            EM_ASM({
                navigator.clipboard.readText().then(function(text) {
                    Module.clipboardText = text;
                });
            });
        }
        std::string strClipboard = (char*)EM_ASM_PTR({
                if (Module.clipboardText) {
                    return stringToNewUTF8(Module.clipboardText);
                } else {
                    return stringToNewUTF8("");
                }
        });
        auto comboAnchor = strClipboard.find("#combo=");
        if (comboAnchor != std::string::npos) {
            pendingLoad = strClipboard.substr(comboAnchor + 7);
            hasPendingLoad = true;
            simInputsChanged = true;
            EM_ASM({
                Module.clipboardText = null;
            });
        }
#else
        if (SDL_HasClipboardText()) {
            std::string strClipboard = SDL_GetClipboardText();
            auto comboAnchor = strClipboard.find("#combo=");
            if (comboAnchor != std::string::npos) {
                if (ImGui::Button("Load", ImVec2(halfButtonSize,0))) {
                    pendingLoad = strClipboard.substr(comboAnchor + 7);
                    hasPendingLoad = true;
                    simInputsChanged = true;
                }
            }
        }
#endif
    }
    } // else (not Viewer)
    ImGui::End();

    if (gameMode == Training) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        return;
    }

    if (!opaqueWindow) {
        for (int i = 0; i < charCount; i++) {
            charControllers[i].RenderUI();
        }
    }

    if (simFrameCount) {
        ImGui::SetNextWindowPos(ImVec2(0, renderSizeY - 200));
        ImGui::SetNextWindowSize(ImVec2(renderSizeX, 0));

        ImGui::Begin("PsychoDrive Bottom Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse );
        doFrameMeterDrag();

        ImGui::PushFont(font);
        for (int i = 0; i < charCount; i++) {
            charControllers[i].renderFrameMeter(scrubberFrame);
        }
        ImGui::PopFont();

        // ImGui::SetNextItemWidth(renderSizeX - 40);
        // ImGui::SliderInt("##framedump", &scrubberFrame, 0, simFrameCount - 1);

        if (ImGui::Button("Rewind")) {
            scrubberFrame = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Play")) {
            if (simController.scrubberFrame >= simController.simFrameCount - 1) {
                simController.scrubberFrame = 0;
            }
            playing = !playing;
            simController.playSpeed = 1;
            simController.playUntilFrame = 0;
        }

        ImGui::End();

        frameMeterMouseDragAmount += pendingFrameMeterMouseDragAmount;
        pendingFrameMeterMouseDragAmount = 0.0f;
        if (simController.scrubberFrame == 0 && frameMeterMouseDragAmount > 0) {
            frameMeterMouseDragAmount = 0.0f;
            momentumActive = false;
            curMomentum = 0.0f;
        }
        if (simController.scrubberFrame == simFrameCount - 1 && frameMeterMouseDragAmount < 0) {
            frameMeterMouseDragAmount = 0.0f;
            momentumActive = false;
            curMomentum = 0.0f;
        }

        const float momentumDeceleration = 0.25;
        if (momentumActive) {
            frameMeterMouseDragAmount += curMomentum;
            float oldCurMomentum = curMomentum;
            if (curMomentum > 0.0) {
                curMomentum -= momentumDeceleration;
            } else if (curMomentum < 0.0) {
                curMomentum += momentumDeceleration;
            }
            if (oldCurMomentum * curMomentum < 0.0 || curMomentum == 0.0) {
                momentumActive = false;
                curMomentum = 0.0f;
                frameMeterMouseDragAmount = 0.0f;
#ifdef __EMSCRIPTEN__
                emscripten_vibrate(6);
#endif
            }
        }
        const float dragThresholdForMovingOneFrame = (CharacterUIController::kHorizSpacing + CharacterUIController::kFrameButtonWidth) / kFrameMeterDragRatio;
        bool changed = false;
        while (frameMeterMouseDragAmount + dragThresholdForMovingOneFrame / 2.0 > dragThresholdForMovingOneFrame) {
            simController.scrubberFrame--;
            frameMeterMouseDragAmount -= dragThresholdForMovingOneFrame;
            changed = true;
        }
        // im not fucking around with modulo of negative numbers you cant fool me
        while (frameMeterMouseDragAmount - dragThresholdForMovingOneFrame / 2.0 < -dragThresholdForMovingOneFrame) {
            simController.scrubberFrame++;
            frameMeterMouseDragAmount += dragThresholdForMovingOneFrame;
            changed = true;
        }
        bool clamped = false;
        if (simController.scrubberFrame < 0) {
            simController.scrubberFrame = 0;
            clamped = true;
        }
        if (simController.scrubberFrame >= simFrameCount) {
            simController.scrubberFrame = simFrameCount - 1;
            clamped = true;
        }
        if (clamped || (changed && (simController.scrubberFrame == 0 || simController.scrubberFrame == simFrameCount - 1))) {
            momentumActive = false;
            curMomentum = 0.0f;
            frameMeterMouseDragAmount = 0.0f;
        }
#ifdef __EMSCRIPTEN__
        if (clamped) {
            emscripten_vibrate(6);
        } else if (changed) {
            emscripten_vibrate(3);
        }
#endif

        if (toggleDebugUI) {
            int guyID = 1;
            Simulation *pSim = simController.getSnapshotAtFrame(simController.scrubberFrame);
            if (pSim) {
                for (Guy *pGuy : pSim->simGuys) {
                    std::string windowName = "Guy " + std::to_string(guyID++);
                    drawGuyStatusWindow( windowName.c_str(), pGuy, pSim );
                }
            }
        }
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
}

void SimulationController::getFinishedSnapshotAtFrame(Simulation *pSimDst, int frameIndex)
{
    if (frameIndex >= 0 && frameIndex < (int)stateRecording.size()) {
        pSimDst->Clone(&stateRecording[frameIndex].sim);

        for (int i = 0; i < charCount; i++) {
            Guy *pGuy = pSimDst->simGuys[charControllers[i].getSimCharSlot()];

            int input = charControllers[i].getInput(frameIndex);
            int prevInput = pGuy->getCurrentInput();
            pGuy->Input(addPressBits(input, prevInput));
        }
        pSimDst->AdvanceFrame();
    }
}

void SimulationController::AdvanceUntilComplete(void)
{
    int frameCount = 0;
    bool bLastFrame = false;
    while (true) {
        for (int i = 0; i < charCount; i++) {
            // put forced triggers in place before RunFrame(), because it contains the hitstop end AdvanceFrame()
            Guy *pGuy = pSim->simGuys[charControllers[i].getSimCharSlot()];
            charControllers[i].forcedInput = 0;
            auto &forcedTrigger = pGuy->getForcedTrigger();
            auto frameTrigger = charControllers[i].timelineTriggers.find(frameCount);
            if (frameTrigger != charControllers[i].timelineTriggers.end()) {
                if (frameTrigger->second.actionID() > 0) {
                    forcedTrigger = frameTrigger->second;
                } else {
                    charControllers[i].forcedInput = -frameTrigger->second.actionID();
                    if (pGuy->getDirection() < 0) {
                        charControllers[i].forcedInput = invertDirection(charControllers[i].forcedInput);
                    }
                }
            }
        }
        pSim->RunFrame();
        RecordFrame();

        for (int i = 0; i < charCount; i++) {
            Guy *pGuy = pSim->simGuys[charControllers[i].getSimCharSlot()];

            if (pGuy->getComboHits() > maxComboCount) {
                maxComboCount = pGuy->getComboHits();
            }
            if (pGuy->getComboDamage() > maxComboDamage) {
                maxComboDamage = pGuy->getComboDamage();
            }

            int input = charControllers[i].getInput(frameCount);
            if (charControllers[i].forcedInput) {
                input = charControllers[i].forcedInput;
            }
            int prevInput = pGuy->getCurrentInput();
            pGuy->Input(addPressBits(input, prevInput));
        }
        pSim->AdvanceFrame();

        // update frame triggers after AdvanceFrame
        RecordedFrame &frame = stateRecording[stateRecording.size()-1];
        for (auto guy : pSim->everyone) {
            for (auto recordedGuy : frame.sim.everyone) {
                if (recordedGuy->getUniqueID() == guy->getUniqueID()) {
                    recordedGuy->getFrameTriggers() = guy->getFrameTriggers();
                    break;
                }
            }
        }
        frameCount++;

        if (bLastFrame) {
            break;
        }

        bool bDone = true;

        for (int i = 0; i < charCount; i++) {
            Guy *pGuy = pSim->simGuys[charControllers[i].getSimCharSlot()];
            // if we're not idle, we're not done
            if (!pGuy->canAct()) {
                bDone = false;
            }
            // if we have any minions, we're not done
            if (pGuy->getMinions().size()) {
                bDone = false;
            }

            // if we're supposed to do an action sometime in the future, we're not done
            // bias a bit to have some buffer for stuff to get delayed?
            const int kBias = 10;

            for (auto &[key, trigger] : charControllers[i].timelineTriggers) {
                if (key > frameCount - kBias) {
                    bDone = false;
                }
            }
            for (auto &region : charControllers[i].inputRegions) {
                if (region.frame + region.duration > frameCount - kBias) {
                    bDone = false;
                }
            }
        }

        // failsafe
        if (frameCount > 10000) {
            bDone = true;
        }

        if (bDone) {
            // one more so they actually stand up
            if (frameCount != 1) {
                bLastFrame = true;
            } else {
                break;
            }
        }
    }

    simFrameCount = stateRecording.size();

    int controllerSearchForNextTrigger = -1;

    for (int i = 0; i < charCount; i++) {
        if (charControllers[i].triggerAdded) {
            controllerSearchForNextTrigger = i;
            break;
        }
    }

    if (scrubberFrame >= simFrameCount) {
        scrubberFrame = simFrameCount - 1;
    } else if (controllerSearchForNextTrigger != -1) {
        playing = true;
        simController.playSpeed = 1;
        int searchFrame = scrubberFrame + 2; // skip kara cancel :/
        while (searchFrame < simFrameCount) {
            Guy *pGuy = getRecordedGuy(searchFrame, charControllers[controllerSearchForNextTrigger].getSimCharSlot());
            if (pGuy->getFrameTriggers().size()) {
                playUntilFrame = searchFrame;
                break;
            }
            searchFrame++;
        }
        charControllers[controllerSearchForNextTrigger].triggerAdded = false;
    }
}

void SimulationController::AdvanceFromReplay(ReplayDecoder &decoder)
{
    while (!decoder.finished && pSim->replayingReplay) {
        pSim->RunFrame();
        RecordFrame();

        // replay decode happens at end of RunFrame() inside Simulation,
        // input is now set for AdvanceFrame
        pSim->AdvanceFrame();
    }

    simFrameCount = stateRecording.size();
    scrubberFrame = 0;
    playing = true;
    playSpeed = 1;
}

void SimulationController::ReloadViewer()
{
    bool isReload = (simFrameCount > 0);
    int savedFrame = scrubberFrame;
    bool savedPlaying = playing;
    int savedPlaySpeed = playSpeed;

    if (!replayInputData.empty()) {
        int round = (replayCurrentRound >= 0) ? replayCurrentRound : 0;
        SimulateAllReplayRounds();
        LoadReplayRound(round);
    } else if (!viewerDumpPath.empty()) {
        stateRecording.clear();
        guyPool.reset();
        if (pSim) delete pSim;
        pSim = new Simulation;
        if (viewerDumpIsMatch) pSim->match = true;

        pSim->SetupFromGameDump(viewerDumpPath, viewerDumpVersion);
        for (auto &guy : pSim->simGuys) {
            guy->setRecordFrameTriggers(true, true);
            guy->setLogTransitions(viewerLogTransitions);
            guy->setLogTriggers(viewerLogTriggers);
            guy->setLogUnknowns(viewerLogUnknowns);
            guy->setLogHits(viewerLogHits);
            guy->setLogBranches(viewerLogBranches);
            guy->setLogResources(viewerLogResources);
        }
        AdvanceFromDump();
    }

    if (isReload) {
        scrubberFrame = savedFrame;
        clampFrame(scrubberFrame);
        playing = savedPlaying;
        playSpeed = savedPlaySpeed;
    }
    prevScrubberFrame = -1;
}

void SimulationController::AdvanceFromDump()
{
    while (pSim->replayingGameStateDump) {
        pSim->RunFrame();
        RecordFrame();
        pSim->AdvanceFrame();
    }

    simFrameCount = stateRecording.size();
    scrubberFrame = 0;
    playing = true;
    playSpeed = 1;
}

void SimulationController::SimulateAllReplayRounds()
{
    replayRoundRecordings.clear();
    replayRoundCount = 0;
    replayCurrentRound = -1;
    stateRecording.clear();
    guyPool.reset();
    if (pSim) { delete pSim; pSim = nullptr; }

    int roundCount = (int)replayInfo["RoundInfo"].size();

    ReplayDecoder decoder;
    decoder.inputData = replayInputData;

    int carryGauges[2] = {0, 0};

    bool batchMode = (gameMode == Batch);

    for (int round = 0; round < roundCount; round++) {
        nlohmann::json &roundInfo = replayInfo["RoundInfo"][round];

        if (roundInfo["RandomSeed"] == 0) {
            break;
        }

        pSim = new Simulation;
        const int *startGauges = (replayIsOldFormat && round > 0) ? carryGauges : nullptr;
        pSim->SetupReplayRound(replayInfo, round, replayVersion, decoder, startGauges);
        for (auto &guy : pSim->simGuys) {
            guy->setRecordFrameTriggers(true, true);
            guy->setLogTransitions(viewerLogTransitions);
            guy->setLogTriggers(viewerLogTriggers);
            guy->setLogUnknowns(viewerLogUnknowns);
            guy->setLogHits(viewerLogHits);
            guy->setLogBranches(viewerLogBranches);
            guy->setLogResources(viewerLogResources);
        }

        int prevHealth[2] = {0, 0};
        while (!decoder.finished && pSim->replayingReplay) {
            prevHealth[0] = pSim->simGuys[0]->getHealth();
            prevHealth[1] = pSim->simGuys[1]->getHealth();
            pSim->RunFrame();
            RecordFrame();
            pSim->AdvanceFrame();
        }

        if (batchMode) {
            fprintf(stderr, "R;round %d finished at frame %d\n", round + 1, pSim->frameCounter);
        }

        int winPlayer = roundInfo["WinPlayerType"];

        ReplayRoundResult result;
        std::string errors;

        if (winPlayer == 0 || winPlayer == 1) {
            int losePlayer = winPlayer ^ 1;
            int loserHealth = pSim->simGuys[losePlayer]->getHealth();
            if (loserHealth != 0 || prevHealth[losePlayer] == 0) {
                if (batchMode) {
                    fprintf(stderr, "E;round %d loser (P%d) health %d expected zero prev health %d expected nonzero\n", round + 1, losePlayer + 1, loserHealth, prevHealth[losePlayer]);
                }
                errors += "loser P" + std::to_string(losePlayer + 1) + " health " + std::to_string(loserHealth) + "; ";
                result.hasErrors = true;
            }
        } else {
            for (int p = 0; p < 2; p++) {
                int h = pSim->simGuys[p]->getHealth();
                if (h != 0 || prevHealth[p] == 0) {
                    if (batchMode) {
                        fprintf(stderr, "E;round %d draw P%d health %d expected zero prev health %d expected nonzero\n", round + 1, p + 1, h, prevHealth[p]);
                    }
                    errors += "draw P" + std::to_string(p + 1) + " health " + std::to_string(h) + "; ";
                    result.hasErrors = true;
                }
            }
        }

        bool checkGauge = !replayIsOldFormat || round == roundCount - 1;
        std::array<int, 2> endGauges = {0, 0};
        for (int p = 0; p < 2; p++) {
            int simGauge = pSim->simGuys[p]->getGauge();
            carryGauges[p] = simGauge;
            endGauges[p] = simGauge;
            if (checkGauge) {
                int expectedGauge = roundInfo["SAGaugeStart"][p];
                if (expectedGauge != simGauge) {
                    if (batchMode) {
                        fprintf(stderr, "E;round %d P%d end gauge %d expected %d\n", round + 1, p + 1, simGauge, expectedGauge);
                    }
                    errors += "P" + std::to_string(p + 1) + " gauge " + std::to_string(simGauge) + " expected " + std::to_string(expectedGauge) + "; ";
                    result.hasErrors = true;
                }
            }
        }

        if (result.hasErrors) {
            result.summary = "Round " + std::to_string(round + 1) + ": " + errors;
        } else {
            result.summary = "Round " + std::to_string(round + 1) + ": OK";
        }

        ReplayRoundRecording recording;
        recording.frames = std::move(stateRecording);
        recording.result = std::move(result);
        recording.endGauges = endGauges;
        replayRoundRecordings.push_back(std::move(recording));
        replayRoundCount++;

        stateRecording.clear();
        delete pSim;
        pSim = nullptr;

        decoder.inputState[0] = 0;
        decoder.inputState[1] = 0;
        decoder.prevInputState[0] = 0;
        decoder.prevInputState[1] = 0;
    }

    if (batchMode) {
        fprintf(stderr, "F;replay finished\n");
    }

    if (!pSim) {
        pSim = new Simulation;
    }
}

void SimulationController::LoadReplayRound(int round)
{
    if (round == replayCurrentRound) return;
    if (round < 0 || round >= (int)replayRoundRecordings.size()) return;

    // give back any frames currently held by stateRecording so the pool can reuse them
    if (replayCurrentRound >= 0 && replayCurrentRound < (int)replayRoundRecordings.size()) {
        replayRoundRecordings[replayCurrentRound].frames = std::move(stateRecording);
    }
    stateRecording = std::move(replayRoundRecordings[round].frames);
    replayCurrentRound = round;

    simFrameCount = stateRecording.size();
    scrubberFrame = 0;
    prevScrubberFrame = -1;
    playing = true;
    playSpeed = 1;
}

int CharacterUIController::getOptionFlags()
{
    int retFlags = 0;
    if (buffLevel == 1) {
        retFlags |= buffLevel1;
    }
    if (buffLevel == 2) {
        retFlags |= buffLevel2;
    }
    if (buffLevel == 3) {
        retFlags |= buffLevel3;
    }
    if (buffLevel == 4) {
        retFlags |= buffLevel4;
    }
    if (buffLevel == 5) {
        retFlags |= buffLevel5;
    }
    return retFlags;
}

void CharacterUIController::setOptionFlags(int flags) {
    if (flags & buffLevel1) {
        buffLevel = 1;
    }
    if (flags & buffLevel2) {
        buffLevel = 2;
    }
    if (flags & buffLevel3) {
        buffLevel = 3;
    }
    if (flags & buffLevel4) {
        buffLevel = 4;
    }
    if (flags & buffLevel5) {
        buffLevel = 5;
    }
}

int SimulationController::getOptionFlags()
{
    int retFlags = 0;
    if (forceCounter) {
        retFlags |= SimulationControllerOptionFlags::simCounter;
    }
    if (forcePunishCounter) {
        retFlags |= SimulationControllerOptionFlags::simPunishCounter;
    }
    return retFlags;
}

void SimulationController::setOptionFlags(int flags)
{
    forceCounter = (flags & SimulationControllerOptionFlags::simCounter);
    forcePunishCounter = (flags & SimulationControllerOptionFlags::simPunishCounter);
}

#define DL1 '|'
#define DL2 '_'
#define DL3 'l'

void CharacterUIController::Serialize(std::string &outStr)
{
    outStr += std::to_string(character) + DL1;
    outStr += std::to_string(charVersion) + DL3;
    outStr += std::to_string(getOptionFlags()) + DL3;
    outStr += std::to_string(startHealth) + DL3;
    outStr += std::to_string(startFocus) + DL3;
    outStr += std::to_string(startGauge) + DL1;
    outStr += std::to_string(startPosX.data) + DL1;
    for (auto &i:timelineTriggers) {
        outStr += std::to_string(i.first) + DL1 + std::to_string(i.second.actionID()) + DL1 + std::to_string(i.second.styleID()) + DL1;
    }
    outStr += DL1;
    for (auto &i:inputRegions) {
        outStr += std::to_string(i.frame) + DL1 + std::to_string(i.duration) + DL1 + std::to_string(i.input) + DL1;
    }
    outStr += DL1;
}

void SimulationController::Serialize(std::string &outStr)
{
    outStr = "";
    outStr += std::to_string(gameMode) + DL2;
    outStr += std::to_string(charCount) + DL3 + std::to_string(getOptionFlags()) + DL2;
    for (int i = 0; i < charCount; i++) {
        charControllers[i].Serialize(outStr);
        outStr += DL2;
    }
}

void SimulationController::Restore(std::string strSerialized)
{
    int cursor = 0;
    int max = strSerialized.length();
    gameMode = (EGameMode)atoi(&strSerialized.c_str()[cursor]);
    cursor = strSerialized.find(DL2, cursor) + 1; if (!cursor || cursor >= max) return;
    charCount = atoi(&strSerialized.c_str()[cursor]);
    int cursorNextDL2 = strSerialized.find(DL2, cursor) + 1; if (!cursorNextDL2 || cursorNextDL2 >= max) return;
    cursor = strSerialized.find(DL3, cursor) + 1;
    if (cursor && cursor < max && cursor < cursorNextDL2) {
        setOptionFlags(atoi(&strSerialized.c_str()[cursor]));
    }
    cursor = cursorNextDL2; if (!cursor || cursor >= max) return;
    for (int i = 0; i < charCount; i++) {
        charControllers[i].character = atoi(&strSerialized.c_str()[cursor]);
        cursor = strSerialized.find(DL1, cursor) + 1; if (!cursor || cursor >= max) return;
        charControllers[i].charVersion = atoi(&strSerialized.c_str()[cursor]);
        int cursorNextDL1 = strSerialized.find(DL1, cursor) + 1; if (!cursorNextDL1 || cursorNextDL1 >= max) return;
        cursor = strSerialized.find(DL3, cursor) + 1;
        if (cursor && cursor < max && cursor < cursorNextDL1) {
            charControllers[i].setOptionFlags(atoi(&strSerialized.c_str()[cursor]));
        }
        cursor = strSerialized.find(DL3, cursor) + 1;
        if (cursor && cursor < max && cursor < cursorNextDL1) {
            charControllers[i].startHealth = atoi(&strSerialized.c_str()[cursor]);
        }
        cursor = strSerialized.find(DL3, cursor) + 1;
        if (cursor && cursor < max && cursor < cursorNextDL1) {
            charControllers[i].startFocus = atoi(&strSerialized.c_str()[cursor]);
        }
        cursor = strSerialized.find(DL3, cursor) + 1;
        if (cursor && cursor < max && cursor < cursorNextDL1) {
            charControllers[i].startGauge = atoi(&strSerialized.c_str()[cursor]);
        }
        cursor = cursorNextDL1; if (!cursor || cursor >= max) return;
        charControllers[i].startPosX.data = atoi(&strSerialized.c_str()[cursor]);
        charControllers[i].flStartPosX = charControllers[i].startPosX.f();
        cursor = strSerialized.find(DL1, cursor) + 1; if (!cursor || cursor >= max) return;
        charControllers[i].timelineTriggers.clear();
        charControllers[i].inputRegions.clear();
        int a,b,c;
        while (true) {
            if (strSerialized.c_str()[cursor] == DL1) {
                cursor += 1; if (!cursor || cursor >= max) return;
                break;
            }
            a = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (!cursor || cursor >= max) return;
            b = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (!cursor || cursor >= max) return;
            c = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (!cursor || cursor >= max) return;
            charControllers[i].timelineTriggers[a] = ActionRef(b, c);
        }
        while (true) {
            if (strSerialized.c_str()[cursor] == DL1) {
                cursor += 1; if (!cursor || cursor >= max) return;
                break;
            }
            a = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (!cursor || cursor >= max) return;
            b = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (!cursor || cursor >= max) return;
            c = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (!cursor || cursor >= max) return;
            charControllers[i].inputRegions.push_back({a,b,c});
        }
        cursor = strSerialized.find(DL2, cursor) + 1; if (!cursor || cursor >= max) return;
    }
}