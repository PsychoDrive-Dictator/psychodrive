#ifdef __EMSCRIPTEN__
#include <emscripten.h>
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

bool webWidgets = false;

ImFont *font = nullptr;

Guy *pGuyToDelete = nullptr;

int mobileDropDownOption = -1;
ImGuiID curDropDownID = -1;

 SimulationController simController;

CharacterUIController leftCharController;
CharacterUIController rightCharController;
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

void drawGuyStatusWindow(const char *windowName, Guy *pGuy)
{
    ImGui::Begin(windowName);
    ImGui::PushFont(font);
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
        ImGui::Text("limit category %i hit count %i warudo %i", pGuy->getLimitShotCategory(), pGuy->getProjHitCount(), pGuy->getWarudo() );
    } else {
        ImGui::Text("health %i unique %s debuff %i style %i install %i timer %i", pGuy->getHealth(), pGuy->getUniqueParam().c_str(), pGuy->getDebuffTimer(), pGuy->getStyle(), pGuy->getInstallFrames(), pGuy->getUniqueTimer());
        ImGui::Text("COMBO HITS %i damage %i hitstun %i juggle %i warudo %i", pGuy->getComboHits(), pGuy->getComboDamage(), pGuy->getHitStun(), pGuy->getJuggleCounter(), pGuy->getWarudo());
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
    ImGui::PopFont();
    ImGui::End();

    int minionID = 1;
    for (auto minion : pGuy->getMinions() ) {
        std::string minionWinName = std::string(windowName) + "'s Minion " + std::to_string(minionID++);
        drawGuyStatusWindow(minionWinName.c_str(), minion);
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

void renderAdvancedUI(float frameRate, std::deque<std::string> *pLogQueue, int sizeX, int sizeY)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("PsychoDrive", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground );
    ImGui::PushFont(font);
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
    ImGui::PopFont();
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

void CharacterUIController::RenderUI(void)
{
    ImGui::SetNextWindowPos(ImVec2(rightSide ? 800 : 0, 0));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    const char *pWindowName = rightSide ? "PsychoDrive Right Char Easy Panel" : "PsychoDrive Left Char Easy Panel";

    ImGui::Begin(pWindowName, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground );
    if (modalDropDown("##char", &character, charNiceNames, 100)) {
        simInputsChanged = true;
        changed = true;
        timelineTriggers.clear();
    }
    ImGui::SameLine();
    if (modalDropDown("##charversion", &charVersion, charVersions, charVersionCount, 200)) {
        simInputsChanged = true;
        changed = true;
        timelineTriggers.clear();
    }
    if (simController.scrubberFrame < (int)simController.pSim->stateRecording.size()) {
        int charID = rightSide ? 1 : 0;
        Guy *pGuy = simController.pSim->stateRecording[simController.scrubberFrame][charID];
        if (pGuy->getFrameTriggers().size()) {
            vecTriggerDropDownLabels.clear();
            vecTriggers.clear();
            vecTriggerDropDownLabels.push_back("Available Triggers");
            for (auto &trigger : pGuy->getFrameTriggers()) {
                vecTriggerDropDownLabels.push_back(pGuy->FindMove(trigger.first, trigger.second));
                vecTriggers.push_back(trigger);
            }
            if (modalDropDown("##moves", &pendingTriggerAdd, vecTriggerDropDownLabels, 300)) {
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
    }

    ImGui::End();
}

void renderUI(float frameRate, std::deque<std::string> *pLogQueue, int sizeX, int sizeY)
{
    if (gameMode == Training) {
        renderAdvancedUI(frameRate, pLogQueue, sizeX, sizeY);
    }
    
    // Mode selector button
    int modeSelectorSize = 200;
    ImGui::SetNextWindowPos(ImVec2((sizeX - modeSelectorSize) * 0.5f - ImGui::GetStyle().WindowPadding.x, 0));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("PsychoDrive Top Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground );
    const char* modes[] = { "Training", "Move Viewer" };//, "Move Comparison", "Combo Maker" };
    if (modalDropDown("##gamemode", (int*)&gameMode, modes, IM_ARRAYSIZE(modes), modeSelectorSize)) {
        simInputsChanged = true;
    }
    ImGui::End();

    if (gameMode == MoveViewer || gameMode == ComboMaker) {
        leftCharController.RenderUI();

        ImGui::SetNextWindowPos(ImVec2(10, sizeY - 50));
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin("PsychoDrive Bottom Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground );
        
        int simFrameCount = simController.pSim->stateRecording.size();
        if (simFrameCount) {
            ImGui::SetNextItemWidth(sizeX - 40);
            ImGui::SliderInt("##framedump", &simController.scrubberFrame, 0, simFrameCount - 1);
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

ImGuiIO& initUI(void)
{
    int fontSize = 20;
#ifdef __EMSCRIPTEN__
    webWidgets = true;
    gameMode = MoveViewer;
    fontSize = 28;
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();

    font = io.Fonts->AddFontFromMemoryCompressedTTF(
        Droid_Sans_compressed_data, Droid_Sans_compressed_size, fontSize
    );

    ImGui::GetStyle().TabRounding = 4.0f;
    ImGui::GetStyle().FrameRounding = 4.0f;
    ImGui::GetStyle().GrabRounding = 4.0f;
    ImGui::GetStyle().WindowRounding = 4.0f;
    ImGui::GetStyle().PopupRounding = 4.0f;

    ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(0.9f, 0.9f, 0.9f, 1.00f);

    rightCharController.rightSide = true;
    leftCharController.character = 1;
    rightCharController.character = 2;
    leftCharController.charVersion = charVersionCount - 1;
    rightCharController.charVersion = charVersionCount - 1;

    simController.NewSim();

    return io;
}

void destroyUI(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void SimulationController::NewSim(void)
{
    if (pSim != nullptr) {
        delete pSim;
        pSim = nullptr;
    }

    pSim = new Simulation;

    if (pSim == nullptr) {
        abort();
    }

    pSim->recordingState = true;
}