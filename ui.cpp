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
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
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

void renderComboMeter(bool rightSpot, int comboHits) {
    ImGui::SetNextWindowPos(ImVec2(rightSpot ? renderSizeX - 100.0f : 0, 300.0));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    const char *pComboWindowName = rightSpot ? "Right Combo Meter" : "Left Combo Meter";

    ImGui::Begin(pComboWindowName, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
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
    ImGui::End();
}

void drawGuyStatusWindow(const char *windowName, Guy *pGuy)
{
    ImGui::Begin(windowName);
    color col = pGuy->getColor();
    ImGui::TextColored(ImVec4(col.r, col.g, col.b, 1), "name %s moveset %s", pGuy->getName()->c_str(), pGuy->getCharacter().c_str());
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
    std::vector<const char *> vecGuyNames;
    vecGuyNames.push_back("none");
    std::map<int, Guy *> mapDropDownIDToGuyPtr;
    mapDropDownIDToGuyPtr[0] = nullptr;
    int guyID = 1, newOpponentID = 0;
    for (auto guy : guys) {
        if (guy == pGuy) {
            continue;
        }
        vecGuyNames.push_back( guy->getName()->c_str() );
        mapDropDownIDToGuyPtr[guyID++] = guy;
    }
    for (auto [ i, guy ] : mapDropDownIDToGuyPtr ) {
        if (guy == pGuy->getOpponent()) {
            guyID = i;
            break;
        }
    }
    newOpponentID = guyID;
    modalDropDown("opponent", &newOpponentID, vecGuyNames, 100);
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
    ImGui::Text("action %i frame %i name %s", pGuy->getCurrentAction(), pGuy->getCurrentFrame(), pGuy->getActionName().c_str());
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
    ImGui::Text("pos %.2f %.2f %.2f direction %i posOffset %.2f %.2f", posX.f(), posY.f(), pGuy->getPosX().f(), pGuy->getDirection(), posOffsetX.f(), posOffsetY.f());
    ImGui::Text("vel %f %f %f accel %f %f %f", velX.f(), velY.f(), pGuy->getHitVelX().f(), accelX.f(), accelY.f(), pGuy->getHitAccelX().f());
    if ( !pGuy->getOpponent() ) {
        ImGui::SameLine();
        if ( ImGui::Button("switch direction") ) { pGuy->switchDirection(); }
    }
    std::vector<HitBox> *hitBoxes = pGuy->getHitBoxes();
    Fixed maxXHitBox = 0.0f;
    for (auto hitbox : *hitBoxes) {
        if (hitbox.type != hitBoxType::hit) continue;
        Fixed hitBoxX = hitbox.box.x + hitbox.box.w;
        if (hitBoxX > maxXHitBox) {
            maxXHitBox = hitBoxX;
        }
    }
    ImGui::Text("push %zd hit %zd hit extent %.2f hurt %zd", pGuy->getPushBoxes()->size(), hitBoxes->size(), maxXHitBox.f(), pGuy->getHurtBoxes()->size());
    if (pGuy->getProjectile()) {
        ImGui::Text("limit category %i hit count %i hitstop %i", pGuy->getLimitShotCategory(), pGuy->getProjHitCount(), pGuy->getHitStop() );
        if (pGuy->getWarudo()) {
            ImGui::SameLine();
            ImGui::Text("warudo");
        }
    } else {
        ImGui::Text("health %i unique %s debuff %i style %i install %i timer %i", pGuy->getHealth(), pGuy->getUniqueParam().c_str(), pGuy->getDebuffTimer(), pGuy->getStyle(), pGuy->getInstallFrames(), pGuy->getUniqueTimer());
        ImGui::Text("COMBO HITS %i damage %i hitstun %i juggle %i hitstop %i", pGuy->getComboHits(), pGuy->getComboDamage(), pGuy->getHitStun(), pGuy->getJuggleCounter(), pGuy->getHitStop());
        if (pGuy->getWarudo()) {
            ImGui::SameLine();
            ImGui::Text("warudo");
        }
    }
    if ( ImGui::Button("destroy") ) { pGuyToDelete = pGuy; }
    ImGui::SameLine();
    ImGui::Text("log:");
    ImGui::SameLine();
    ImGui::Checkbox("unknowns", &pGuy->logUnknowns);
    ImGui::SameLine();
    ImGui::Checkbox("hits", &pGuy->logHits);
    ImGui::SameLine();
    ImGui::Checkbox("triggers", &pGuy->logTriggers);
    ImGui::SameLine();
    ImGui::Checkbox("branches", &pGuy->logBranches);
    ImGui::SameLine();
    ImGui::Checkbox("transitions", &pGuy->logTransitions);
    auto logQueue = pGuy->getLogQueue();
    for (int i = logQueue.size() - 1; i >= 0; i--) {
        ImGui::Text("%s", logQueue[i].c_str());
    }
    ImGui::End();

    int minionID = 1;
    for (auto minion : pGuy->getMinions() ) {
        std::string minionWinName = std::string(windowName) + "'s Minion " + std::to_string(minionID++);
        drawGuyStatusWindow(minionWinName.c_str(), minion);
    }

    if (guys.size() >= 2 && (pGuy == guys[0] || pGuy == guys[1]) && pGuy->getOpponent() && pGuy->getOpponent()->getComboHits()) {
        renderComboMeter(pGuy == guys[1], pGuy->getOpponent()->getComboHits());
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
        ImGui::Text("input %i %i", i.first, i.second);
    }
    ImGui::Text("frame %d", globalFrameCount);
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
    ImGui::SliderInt("hitstun adder", &hitStunAdder, -10, 10);
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
}

void renderUI(float frameRate, std::deque<std::string> *pLogQueue, int sizeX, int sizeY)
{
    renderSizeX = sizeX;
    renderSizeY = sizeY;
    if (!toggleRenderUI) {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return;
    }

    if (gameMode == Training) {
        renderAdvancedUI(frameRate, pLogQueue);
    }
    
    // Mode selector button
    int modeSelectorSize = 200;
    ImGui::SetNextWindowPos(ImVec2((renderSizeX - modeSelectorSize) * 0.5f - ImGui::GetStyle().WindowPadding.x, 0));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("PsychoDrive Top Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground );
    const char* modes[] = { "Training", "Move Viewer", "Combo Maker" };
    if (modalDropDown("##gamemode", (int*)&gameMode, modes, IM_ARRAYSIZE(modes), modeSelectorSize)) {
        simInputsChanged = true;
        simController.Reset();

        translateY = gameMode == Training ? 150.0 : 100.0;
    }
    ImGui::End();

    if (gameMode != Training) {
        simController.RenderUI();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

ImGuiIO& initUI(void)
{
#ifdef __EMSCRIPTEN__
    webWidgets = true;
    gameMode = MoveViewer;
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();

    comboFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        Droid_Sans_compressed_data, Droid_Sans_compressed_size, 96.0
    );
    comboFontSubscript = io.Fonts->AddFontFromMemoryCompressedTTF(
        Droid_Sans_compressed_data, Droid_Sans_compressed_size, 64.0
    );
    fontBigger = io.Fonts->AddFontFromMemoryCompressedTTF(
        Droid_Sans_compressed_data, Droid_Sans_compressed_size, 28.0
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

    return io;
}

void destroyUI(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void CharacterUIController::RenderUI(void)
{
    ImGui::SetNextWindowPos(ImVec2(rightSide ? renderSizeX - 265.0f : 0, 0));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    const char *pWindowName = rightSide ? "PsychoDrive Right Char Easy Panel" : "PsychoDrive Left Char Easy Panel";

    ImGui::Begin(pWindowName, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground );
    if (modalDropDown("##char", &character, charNiceNames, 207)) {
        simInputsChanged = true;
        changed = true;
        timelineTriggers.clear();
    }
    ImGui::SameLine();
    if (modalDropDown("##charversion", &charVersion, charVersions, charVersionCount, 35)) {
        simInputsChanged = true;
        changed = true;
        timelineTriggers.clear();
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
        if (newStartPosX != flStartPosX) {
            flStartPosX = newStartPosX;
            startPosX = Fixed(flStartPosX, true);
            simInputsChanged = true;
            changed = true;
        }
    }

    if (!simController.pSim) {
        ImGui::End();
        return;
    }

    Guy *pGuy = simController.pSim->getRecordedGuy(simController.scrubberFrame, getSimCharSlot());

    if (timelineTriggers.find(simController.scrubberFrame) != timelineTriggers.end()) {
        auto &trigger = timelineTriggers[simController.scrubberFrame];
        std::string strLabel = "Delete " + std::string(pGuy->FindMove(trigger.first, trigger.second));
        if (ImGui::Button(strLabel.c_str())) {
            timelineTriggers.erase(timelineTriggers.find(simController.scrubberFrame));
            simInputsChanged = true;
            changed = true;
        }
    } else if (pGuy && pGuy->getFrameTriggers().size()) {
        vecTriggerDropDownLabels.clear();
        vecTriggers.clear();
        vecTriggerDropDownLabels.push_back("Available Triggers");
        for (auto &trigger : pGuy->getFrameTriggers()) {
            vecTriggerDropDownLabels.push_back(pGuy->FindMove(trigger.first, trigger.second));
            vecTriggers.push_back(trigger);
        }
        if (modalDropDown("##moves", &pendingTriggerAdd, vecTriggerDropDownLabels, 250)) {
            if (pendingTriggerAdd != 0) {
                std::erase_if(timelineTriggers, [](const auto& item) {
                    auto const& [key, value] = item;
                    return (key >= simController.scrubberFrame);
                });
                timelineTriggers[simController.scrubberFrame] = vecTriggers[pendingTriggerAdd - 1];

                simInputsChanged = true;
                changed = true;
                pendingTriggerAdd = 0;
            }
        }
    }
    ImGui::End();


    if (pGuy && pGuy->getOpponent()) {
        int opponentID = pGuy->getOpponent()->getUniqueID();
        Guy *pOpponent = simController.pSim->getRecordedGuy(simController.scrubberFrame, opponentID);
        //Guy *pOpponentPrevFrame = simController.pSim->getRecordedGuy(std::max(0, simController.scrubberFrame - 1), opponentID);

        if (pOpponent) {
            //bool newComboHit = pOpponentPrevFrame->getComboHits() != pOpponent->getComboHits();
            int comboHits = pOpponent->getComboHits();

            if (comboHits != 0) {
                renderComboMeter(rightSide, comboHits);
            }
        }
    }
}

static ImVec4 frameMeterColors[] = {
    { 1.0,1.0,1.0,1.0 }, // default blinding white
    { 0.206,0.202,0.184,1.0 }, // can act/move very dark grey
    { 0.02,0.443,0.729,1.0 }, // recovery blue
    { 0.0,0.733,0.573,1.0 }, // startup green
    { 0.78,0.173,0.4,1.0 }, // active red
    { 1.0,0.965,0.224,1.0 }, // hitstun yellow
    { 0.0,0.0,0.0,0.0 }, // hitstop missing
};

void CharacterUIController::renderFrameMeterCancelWindows(int frameIndex)
{
    int frameCount = simController.pSim->stateRecording.size();

    float cursorX = ImGui::GetCursorPosX() - (frameIndex - kFrameOffset) * (kHorizSpacing + kFrameButtonWidth) + simController.frameMeterMouseDragAmount * simController.kFrameMeterDragRatio;
    float cursorY = ImGui::GetCursorPosY();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45,0.15,0.1,1.0));
    ImGui::BeginDisabled();
    for (int i = 0; i < frameCount; i++) {
        Guy *pGuy = simController.pSim->getRecordedGuy(i, getSimCharSlot());
        Guy *pGuyPrevFrame = simController.pSim->getRecordedGuy(i-1, getSimCharSlot());

        if ((!pGuyPrevFrame || !pGuyPrevFrame->getFrameTriggers().size()) && pGuy->getFrameTriggers().size()) {
            // cancel window, figure out how far it goes
            int j = i;
            while (true) {
                j++;
                Guy *pGuyJ = simController.pSim->getRecordedGuy(j, getSimCharSlot());
                if (!pGuyJ || !pGuyJ->getFrameTriggers().size()) {
                    break;
                }
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
    int frameCount = simController.pSim->stateRecording.size();

    if (!rightSide) renderFrameMeterCancelWindows(frameIndex);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - (frameIndex - kFrameOffset) * (kHorizSpacing + kFrameButtonWidth) + simController.frameMeterMouseDragAmount * simController.kFrameMeterDragRatio);
    float tronglePosY = ImGui::GetCursorPosY();

    for (int i = 0; i < frameCount; i++) {
        Guy *pGuy = simController.pSim->getRecordedGuy(i, getSimCharSlot());
        Guy *pGuyNextFrame = simController.pSim->getRecordedGuy(i+1, getSimCharSlot());
        if (i != 0) ImGui::SameLine();

        ImGui::PushID(i);

        int colorIndex = pGuy->getFrameMeterColorIndex();
        ImGui::PushStyleColor(ImGuiCol_Button, frameMeterColors[colorIndex]);
        bool darkText = false;
        if (colorIndex != 1) {
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
        //ImGui::BeginDisabled();
        ImGui::Button(strButtonCaption.c_str(), ImVec2(kFrameButtonWidth,kFrameButtonHeight));
       // ImGui::EndDisabled();
        if (darkText) {
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleColor();

        simController.doFrameMeterDrag();

        ImGui::PopID();
    }

    if (rightSide) renderFrameMeterCancelWindows(frameIndex);

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

void SimulationController::Reset(void)
{
    charControllers.clear();
    charControllers.emplace_back();
    charControllers.emplace_back();

    charControllers[0].character = 1;
    charControllers[0].charVersion = charVersionCount - 1;
    charControllers[0].charColor = { 255.0, 0.0, 255.0 };
    charControllers[1].character = 2;
    charControllers[1].charVersion = charVersionCount - 1;
    charControllers[1].rightSide = true;
    charControllers[1].charColor = { 255.0, 255.0, 0.0 };

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

bool SimulationController::NewSim(void)
{
    charCount = 1;

    if (gameMode == ComboMaker) {
        charCount = 2;
    }

    bool charsLoaded = true;

    for (int i = 0; i < charCount; i++) {
        const char *charName = charNames[charControllers[i].character];
        if (!isCharLoaded(charName)) {
            requestCharDownload(charName);
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

    pSim->recordingState = true;

    for (int i = 0; i < charCount; i++) {
        pSim->CreateGuyFromCharController(charControllers[i]);
    }

    return true;
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

void SimulationController::RenderUI(void)
{
    for (int i = 0; i < charCount; i++) {
        charControllers[i].RenderUI();
    }

    int simFrameCount = pSim ? pSim->stateRecording.size() : 0;
    if (simFrameCount) {
        ImGui::SetNextWindowPos(ImVec2(0, renderSizeY - 150));
        ImGui::SetNextWindowSize(ImVec2(renderSizeX, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0,1.0,1.0,0.05));
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
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();

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
    }
}

void SimulationController::AdvanceUntilComplete(void)
{
    int frameCount = 0;
    while (true) {
        for (int i = 0; i < charCount; i++) {
            auto &forcedTrigger = pSim->simGuys[charControllers[i].getSimCharSlot()]->getForcedTrigger();
            if (charControllers[i].timelineTriggers.find(frameCount) != charControllers[i].timelineTriggers.end()) {
                forcedTrigger = charControllers[i].timelineTriggers[frameCount];
            }
        }
        pSim->AdvanceFrame();
        frameCount++;

        bool bDone = true;

        for (int i = 0; i < charCount; i++) {
            Guy *pGuy = pSim->simGuys[charControllers[i].getSimCharSlot()];
            // if we're not idle, we're not done
            if (pGuy->getCurrentAction() > 2) {
                bDone = false;
            }
            // if we have any minions, we're not done
            if (pGuy->getMinions().size()) {
                bDone = false;
            }

            // if we're supposed to do an action sometime in the future, we're not done
            for (auto &[key, trigger] : charControllers[i].timelineTriggers) {
                // bias a bit to have some buffer for stuff to get delayed?
                if (key > frameCount - 10) {
                    bDone = false;
                }
            }
        }

        // failsafe
        if (frameCount > 1000) {
            bDone = true;
        }

        if (bDone) {
            break;
        }
    }

    simFrameCount = pSim->stateRecording.size();
}