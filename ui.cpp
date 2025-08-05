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

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
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
    
    simController.RenderUI();

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

    ImGui::StyleColorsClassic();

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

    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);

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
}

void CharacterUIController::renderActionSetup(int frameIndex)
{
    if (!simController.pSim) {
        return;
    }

    Guy *pGuy = simController.pSim->getRecordedGuy(frameIndex, getSimCharSlot());

    ImGui::Text("Navigation:");
    if (ImGui::Button("<")) {
        int searchFrame = frameIndex;
        bool foundNoWindow = false;
        while (searchFrame >= 0) {
            Guy *pFrameGuy = simController.pSim->getRecordedGuy(searchFrame, getSimCharSlot());
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
            Guy *pFrameGuy = simController.pSim->getRecordedGuy(searchFrame, getSimCharSlot());
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
        std::string strLabel = "Delete " + std::string(pGuy->FindMove(trigger.first, trigger.second));
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
            vecTriggerDropDownLabels.push_back(pGuy->FindMove(trigger.first, trigger.second));
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

    Guy *pGuy = simController.pSim->getRecordedGuy(simController.scrubberFrame, getSimCharSlot());

    if (pGuy && pGuy->getOpponent()) {
        int opponentID = pGuy->getOpponent()->getUniqueID();
        Guy *pOpponent = simController.pSim->getRecordedGuy(simController.scrubberFrame, opponentID);

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
    int frameCount = simController.pSim->stateRecording.size();

    float cursorX = ImGui::GetCursorPosX() - (frameIndex - kFrameOffset) * (kHorizSpacing + kFrameButtonWidth) + simController.frameMeterMouseDragAmount * simController.kFrameMeterDragRatio;
    float cursorY = ImGui::GetCursorPosY();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45,0.15,0.1,1.0));
    ImGui::BeginDisabled();
    for (int i = 0; i < frameCount; i++) {
        Guy *pGuy = simController.pSim->getRecordedGuy(i, getSimCharSlot());
        Guy *pGuyPrevFrame = simController.pSim->getRecordedGuy(i-1, getSimCharSlot());

        if (!pGuyPrevFrame || (pGuy->getFrameTriggers().size() && (pGuyPrevFrame->getFrameTriggers() != pGuy->getFrameTriggers()))) {
            // cancel window, figure out how far it goes
            int j = i;
            pGuyPrevFrame = pGuy;
            while (true) {
                j++;
                Guy *pGuyJ = simController.pSim->getRecordedGuy(j, getSimCharSlot());
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
    int frameCount = simController.pSim->stateRecording.size();

    if (!rightSide) renderFrameMeterCancelWindows(frameIndex);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - (frameIndex - kFrameOffset) * (kHorizSpacing + kFrameButtonWidth) + simController.frameMeterMouseDragAmount * simController.kFrameMeterDragRatio);
    float tronglePosY = ImGui::GetCursorPosY();

    bool foundAdvantageWindow = false;
    int endAdvantageWindowFrame = -1;
    int advantageFrames = 0;
    if (simController.charCount == 2) {
        // walk from the end and find advantage
        int i = frameCount - 1;
        while (i > 0) {
            Guy *pGuy = simController.pSim->getRecordedGuy(i, getSimCharSlot());
            Guy *pOtherGuy = simController.pSim->getRecordedGuy(i, !getSimCharSlot());
            if (pGuy->getFrameMeterColorIndex() == 1 && pOtherGuy->getFrameMeterColorIndex() != 1) {
                foundAdvantageWindow = true;
                endAdvantageWindowFrame = i;
                while (i > 0) {
                    Guy *pGuy = simController.pSim->getRecordedGuy(i, getSimCharSlot());
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
        Guy *pGuy = simController.pSim->getRecordedGuy(i, getSimCharSlot());
        Guy *pGuyNextFrame = simController.pSim->getRecordedGuy(i+1, getSimCharSlot());
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

void SimulationController::Reset(void)
{
    charControllers.clear();
    charControllers.emplace_back();
    charControllers.emplace_back();

    charControllers[0].character = 22;
    charControllers[0].charVersion = charVersionCount - 1;
    charControllers[0].charColor = { 0.78, 0.5, 0.15 };
    charControllers[1].character = 2;
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

bool SimulationController::NewSim(void)
{
#ifdef __EMSCRIPTEN__
    if (!triedRestoreFromURL) {
        std::string strCombo = (char*)EM_ASM_PTR({
            let params = new URLSearchParams(document.location.search);
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

    recordedGuysPoolIndex = 0;

    maxComboCount = 0;
    maxComboDamage = 0;

    if (hasRestored) {
        playing = true;
        hasRestored = false;
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0,1.0,1.0,0.05));

    if (gameMode != Training) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin("PsychoDrive Left Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus );
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
        }
        if (maxComboCount > 0) {
            std::string strComboInfo = "Max damage: " + std::to_string(maxComboDamage);
            ImGui::Text("%s", strComboInfo.c_str());
        }
        ImGui::End();
    }

    // Top right panel
    int modeSelectorSize = 180;
    ImGui::SetNextWindowPos(ImVec2(renderSizeX - 190.0, 0));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("PsychoDrive Top Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse );
    const char* modes[] = { "Developer", "Move Viewer", "Combo Maker" };
    if (modalDropDown("##gamemode", (int*)&gameMode, modes, IM_ARRAYSIZE(modes), modeSelectorSize)) {
        simInputsChanged = true;
        simController.Reset();

        translateY = gameMode == Training ? 150.0 : 100.0;
    }
    int simFrameCount = pSim ? pSim->stateRecording.size() : 0;

    if (gameMode != Training) {
        std::vector<std::string> vecViewLabels;
        vecViewLabels.push_back("P1 Setup");
        vecViewLabels.push_back("P1 Actions");
        if (gameMode == ComboMaker) {
            vecViewLabels.push_back("P2 Setup");
            vecViewLabels.push_back("P2 Actions");
        }
        modalDropDown("##viewselect", (int*)&viewSelect, vecViewLabels, modeSelectorSize);
#ifdef __EMSCRIPTEN__
        if (simFrameCount > 1) {
            if (ImGui::Button("Share Combo", ImVec2(modeSelectorSize,0))) {
                std::string strSerialized;
                Serialize(strSerialized);
                //Restore(strSerialized);
                EM_ASM({
                    var serialized = UTF8ToString($0);
                    navigator.clipboard.writeText(window.location.protocol + "//" + window.location.host + window.location.pathname + '?combo=' + serialized);
                    //window.location.href = '?combo=' + serialized;
                }, strSerialized.c_str());
            }
        }
#endif
    }
    ImGui::End();

    if (gameMode == Training) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        return;
    }

    for (int i = 0; i < charCount; i++) {
        charControllers[i].RenderUI();
    }

    if (simFrameCount) {
        ImGui::SetNextWindowPos(ImVec2(0, renderSizeY - 150));
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
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
}

void SimulationController::AdvanceUntilComplete(void)
{
    int frameCount = 0;
    bool bLastFrame = false;
    while (true) {
        for (int i = 0; i < charCount; i++) {
            // put forced triggers in place before RunFrame(), because it contains the hitstop end AdvanceFrame()
            Guy *pGuy = pSim->simGuys[charControllers[i].getSimCharSlot()];
            auto &forcedTrigger = pGuy->getForcedTrigger();
            if (charControllers[i].timelineTriggers.find(frameCount) != charControllers[i].timelineTriggers.end()) {
                forcedTrigger = charControllers[i].timelineTriggers[frameCount];
            }
        }
        pSim->RunFrame();

        for (int i = 0; i < charCount; i++) {
            Guy *pGuy = pSim->simGuys[charControllers[i].getSimCharSlot()];

            if (pGuy->getComboHits() > maxComboCount) {
                maxComboCount = pGuy->getComboHits();
            }
            if (pGuy->getComboDamage() > maxComboDamage) {
                maxComboDamage = pGuy->getComboDamage();
            }

            int input = charControllers[i].getInput(frameCount);
            int prevInput = pGuy->getCurrentInput();
            pGuy->Input(addPressBits(input, prevInput));
        }
        pSim->AdvanceFrame();
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
        if (frameCount > 2000) {
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

    simFrameCount = pSim->stateRecording.size();

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
            Guy *pGuy = pSim->getRecordedGuy(searchFrame, charControllers[controllerSearchForNextTrigger].getSimCharSlot());
            if (pGuy->getFrameTriggers().size()) {
                playUntilFrame = searchFrame;
                break;
            }
            searchFrame++;
        }
        charControllers[controllerSearchForNextTrigger].triggerAdded = false;
    }
}

#define DL1 '|'
#define DL2 '_'

void CharacterUIController::Serialize(std::string &outStr)
{
    outStr += std::to_string(character) + DL1 + std::to_string(charVersion) + DL1 + std::to_string(startPosX.data) + DL1;
    for (auto &i:timelineTriggers) {
        outStr += std::to_string(i.first) + DL1 + std::to_string(i.second.first) + DL1 + std::to_string(i.second.second) + DL1;
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
    outStr += std::to_string(gameMode) + DL2 + std::to_string(charCount) + DL2;
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
    cursor = strSerialized.find(DL2, cursor) + 1; if (cursor >= max) return;
    charCount = atoi(&strSerialized.c_str()[cursor]);
    cursor = strSerialized.find(DL2, cursor) + 1; if (cursor >= max) return;
    for (int i = 0; i < charCount; i++) {
        charControllers[i].character = atoi(&strSerialized.c_str()[cursor]);
        cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
        charControllers[i].charVersion = atoi(&strSerialized.c_str()[cursor]);
        cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
        charControllers[i].startPosX.data = atoi(&strSerialized.c_str()[cursor]);
        charControllers[i].flStartPosX = charControllers[i].startPosX.f();
        cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
        charControllers[i].timelineTriggers.clear();
        charControllers[i].inputRegions.clear();
        int a,b,c;
        while (true) {
            if (strSerialized.c_str()[cursor] == DL1) {
                cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
                break;
            }
            a = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
            b = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
            c = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
            charControllers[i].timelineTriggers[a] = std::make_pair(b,c);
        }
        while (true) {
            if (strSerialized.c_str()[cursor] == DL1) {
                cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
                break;
            }
            a = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
            b = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
            c = atoi(&strSerialized.c_str()[cursor]);
            cursor = strSerialized.find(DL1, cursor) + 1; if (cursor >= max) return;
            charControllers[i].inputRegions.push_back({a,b,c});
        }
    }
}