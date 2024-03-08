#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include "json.hpp"
#include <string>
#include <fstream>
#include <ios>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <bitset>

enum Input
{
	NEUTRAL = 0,
	UP = 1,
	DOWN = 2,
	BACK = 4, 
	FORWARD = 8,
	LP = 16,
	MP = 32,
	HP = 64,
	LK = 128,
	MK = 256,
	HK = 512,
};

std::string readFile(const std::string &fileName)
{
    std::ifstream ifs(fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

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
    return nlohmann::json::parse(fileText);
}

void drawBox( float x, float y, float w, float h, float r, float g, float b)
{
    glColor4f(r,g,b, 0.2f);

    glBegin(GL_QUADS);
    
    glVertex2f(x, y);
    glVertex2i(x+w, y);
    glVertex2i(x+w, y+h);
    glVertex2i(x, y+h);
    
    glEnd();

    glColor4f(r,g,b, 1.0f);

    glBegin(GL_LINE_LOOP);

    glVertex2f(x, y);
    glVertex2i(x+w, y);
    glVertex2i(x+w, y+h);
    glVertex2i(x, y+h);

    glEnd();
}
 
std::string to_string_leading_zeroes(unsigned int number, unsigned int length) {
    
     std::string num_str = std::to_string(number);
    
    if(num_str.length() >= length) return num_str;
    
     std::string leading_zeros(length - num_str.length(), '0');
    
    return leading_zeros + num_str;
}



static inline void drawRectsBox( nlohmann::json rectsJson, int rectsPage, int boxID,  int offsetX, int offsetY, float r, float g, float b, bool isDrive = false, bool isParry = false, bool isDI = false )
{
    std::string pageIDString = to_string_leading_zeroes(rectsPage, 2);
    std::string boxIDString = to_string_leading_zeroes(boxID, 3);
    if (!rectsJson.contains(pageIDString) || !rectsJson[pageIDString].contains(boxIDString)) {
        return;
    }
    auto rectJson = rectsJson[pageIDString][boxIDString];
    int xOrig = rectJson["OffsetX"];
    int yOrig = rectJson["OffsetY"];
    int xRadius = rectJson["SizeX"];
    int yRadius = rectJson["SizeY"];
    if (isDrive || isParry || isDI ) {
        int xRadiusDrive = xRadius + 10;
        int yRadiusDrive = yRadius + 10;
        float colorR = 0.0;
        float colorG = 0.6;
        float colorB = 0.1;
        if (isParry) {
            colorR = 0.3;
            colorG = 0.7;
            colorB = 0.9;
        }
        if (isDI) {
            colorR = 0.9;
            colorG = 0.0;
            colorB = 0.0;
        }
        drawBox( xOrig - xRadiusDrive + offsetX, yOrig - yRadiusDrive + offsetY, xRadiusDrive * 2, yRadiusDrive * 2,
                colorR,colorG,colorB);
    }
    drawBox( xOrig - xRadius + offsetX, yOrig - yRadius + offsetY, xRadius * 2, yRadius * 2,r,g,b );
}

static inline void parseRootOffset( nlohmann::json& keyJson, int&offsetX, int& offsetY)
{
    if ( keyJson.contains("RootOffset") ) {
        offsetX = keyJson["RootOffset"]["X"];
        offsetY = keyJson["RootOffset"]["Y"];
    }
}

bool matchInput( int input, uint32_t okKeyFlags, uint32_t okCondFlags, uint32_t dcExcFlags = 0 )
{

    if (dcExcFlags != 0 ) {
        if ((dcExcFlags & input) != dcExcFlags) {
            return false;
        }
    }

    if (okCondFlags & 2) {
        if ((input & 0xF) == (okKeyFlags & 0xF)) {
            return true; // match exact direction
        }
        return false;
    }

    uint32_t match = okKeyFlags & input;

    if (okCondFlags & 0x80) {
        if (match == okKeyFlags) {
            return true; // match all?
        }
    } else if (okCondFlags & 0x40) {
        if (std::bitset<32>(match).count() >= 2) {
            return true; // match 2?
        }
    } else {
        if (match != 0) {
            return true; // match any? :/
        }
    }

    return false;
}

static inline void doSteerKeyOperation(float &value, float keyValue, int operationType)
{
    switch (operationType) {
        case 1: // set
        value = keyValue; 
        break;
        case 2: // add ?
        value += keyValue;
        break;
        default:
        abort();
    }
}

// Main code
int main(int, char**)
{

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER ) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Psycho Drive", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    auto movesDictJson = parse_json_file("honda_moves.json");
    auto rectsJson = parse_json_file("honda_rects.json");
    auto namesJson = parse_json_file("honda_names.json");
    auto triggerGroupsJson = parse_json_file("honda_trigger_groups.json");
    auto triggersJson = parse_json_file("honda_triggers.json");
    auto commandsJson = parse_json_file("honda_commands.json");

    float posX = 50.0f;
    float posY = 0.0f;
    float posOffsetX = 0.0f;
    float posOffsetY = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float accelX = 0.0f;
    float accelY = 0.0f;

    int currentAction = 1;
    int nextAction = -1;
    int currentFrame = 0;
    int actionFrameDuration = 0;
    int marginFrame = 0;
    int currentInput = 0;
    std::deque<int> inputBuffer;

    int deferredActionFrame = -1;
    int deferredAction = 0;

    bool isDrive = false;
    bool wasDrive = false;

    float charColorR = 0.8;
    float charColorG = 0.6;
    float charColorB = 0.2;

    std::string actionName;

    // Main loop
    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
            if (event.type == SDL_KEYDOWN)
            {
                switch (event.key.keysym.sym)
                {
                    case SDLK_a:
                        currentInput |= BACK;
                        break;
                    case SDLK_s:
                        currentInput |= DOWN;
                        break;
                    case SDLK_d:
                        currentInput |= FORWARD;
                        break;
                    case SDLK_SPACE:
                        currentInput |= UP;
                        break;
                    case SDLK_u:
                        currentInput |= LP;
                        break;
                    case SDLK_i:
                        currentInput |= MP;
                        break;
                    case SDLK_o:
                        currentInput |= HP;
                        break;
                    case SDLK_j:
                        currentInput |= LK;
                        break;
                    case SDLK_k:
                        currentInput |= MK;
                        break;
                    case SDLK_l:
                        currentInput |= HK;
                        break;
                    case SDLK_ESCAPE:
                        done = true;
                        break;
                }
            }
            if (event.type == SDL_KEYUP)
            {
                switch (event.key.keysym.sym)
                {
                    case SDLK_a:
                        currentInput &= ~BACK;
                        break;
                    case SDLK_s:
                        currentInput &= ~DOWN;
                        break;
                    case SDLK_d:
                        currentInput &= ~FORWARD;
                        break;
                    case SDLK_SPACE:
                        currentInput &= ~UP;
                        break;
                    case SDLK_u:
                        currentInput &= ~LP;
                        break;
                    case SDLK_i:
                        currentInput &= ~MP;
                        break;
                    case SDLK_o:
                        currentInput &= ~HP;
                        break;
                    case SDLK_j:
                        currentInput &= ~LK;
                        break;
                    case SDLK_k:
                        currentInput &= ~MK;
                        break;
                    case SDLK_l:
                        currentInput &= ~HK;
                        break;
                }
            }
        }

        inputBuffer.push_front(currentInput);
        // how much is too much?
        if (inputBuffer.size() > 20) {
            inputBuffer.pop_back();
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("Psycho Drive");

            ImGui::SliderInt("action", &currentAction, 1, 1500);
            ImGui::SliderInt("frame", &currentFrame, 0, 100);
            ImGui::Text(actionName.c_str());
            ImGui::Text("currentInput %d", currentInput);
            ImGui::Text("pos %f %f", posX, posY);
            ImGui::Text("posOffset %f %f", posOffsetX, posOffsetY);
            ImGui::Text("vel %f %f", velocityX, velocityY);
            ImGui::Text("accel %f %f", accelX, accelY);

            if (ImGui::Button("reset"))
                currentFrame = 0;

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        ImGui::Render();

        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0f, (int)io.DisplaySize.x, (int)io.DisplaySize.y, 0.0f, 0.0f, 1.0f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glTranslatef(0.0f,io.DisplaySize.y,0.0f);
        glScalef(1.0f, -1.0f, 1.0f);


        auto actionIDString = to_string_leading_zeroes(currentAction, 4);
        bool validAction = namesJson.contains(actionIDString);
        actionName = validAction ? namesJson[actionIDString] : "invalid";

        if (movesDictJson.contains(actionName))
        {
            marginFrame = movesDictJson[actionName]["fab"]["ActionFrame"]["MarginFrame"];
            //standing has 0 marginframe, pre-jump has -1, crouch -1..
            actionFrameDuration = movesDictJson[actionName]["fab"]["Frame"];

            if (movesDictJson[actionName].contains("PlaceKey"))
            {
                for (auto& [placeKeyID, placeKey] : movesDictJson[actionName]["PlaceKey"].items())
                {
                    if ( !placeKey.contains("_StartFrame") || placeKey["_StartFrame"] > currentFrame || placeKey["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    for (auto& [frame, offset] : placeKey["PosList"].items()) {
                        int keyStartFrame = placeKey["_StartFrame"];
                        if (atoi(frame.c_str()) == currentFrame - keyStartFrame) {
                            if (placeKey["Axis"] == 0) {
                                posOffsetX = offset.get<int>();
                            } else if (placeKey["Axis"] == 1) {
                                posOffsetY = offset.get<int>();    
                            }
                        }
                    }
                }
            }

            velocityX += accelX;
            velocityY += accelY;

            float oldPosY = posY;

            posX += velocityX;
            posY += velocityY;

            if ( oldPosY > 0.0f && posY <= 0.0f ) // iffy but we prolly move to fixed point anyway at some point
            {
                posY = 0.0f;
                nextAction = 39; // land - need other landing if did air attack?
            }
            glTranslatef(posX + posOffsetX, posY + posOffsetY, 0.0f);

            if (movesDictJson[actionName].contains("SteerKey"))
            {
                for (auto& [steerKeyID, steerKey] : movesDictJson[actionName]["SteerKey"].items())
                {
                    if ( !steerKey.contains("_StartFrame") || steerKey["_StartFrame"] > currentFrame || steerKey["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    int operationType = steerKey["OperationType"];
                    int valueType = steerKey["ValueType"];
                    float fixValue = steerKey["FixValue"];

                    switch (valueType) {
                        case 0: doSteerKeyOperation(velocityX, fixValue,operationType); break;
                        case 1: doSteerKeyOperation(velocityY, fixValue,operationType); break;
                        case 3: doSteerKeyOperation(accelX, fixValue,operationType); break;
                        case 4: doSteerKeyOperation(accelY, fixValue,operationType); break;
                    }
                }
            }

            if (wasDrive && movesDictJson[actionName].contains("DriveSteerKey"))
            {
                for (auto& [steerKeyID, steerKey] : movesDictJson[actionName]["DriveSteerKey"].items())
                {
                    if ( !steerKey.contains("_StartFrame") || steerKey["_StartFrame"] > currentFrame || steerKey["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    int operationType = steerKey["OperationType"];
                    int valueType = steerKey["ValueType"];
                    float fixValue = steerKey["FixValue"];

                    switch (valueType) {
                        case 0: doSteerKeyOperation(velocityX, fixValue,operationType); break;
                        case 1: doSteerKeyOperation(velocityY, fixValue,operationType); break;
                        case 3: doSteerKeyOperation(accelX, fixValue,operationType); break;
                        case 4: doSteerKeyOperation(accelY, fixValue,operationType); break;
                    }
                }
            }

            if (movesDictJson[actionName].contains("PushCollisionKey"))
            {
                for (auto& [pushBoxID, pushBox] : movesDictJson[actionName]["PushCollisionKey"].items())
                {
                    if ( !pushBox.contains("_StartFrame") || pushBox["_StartFrame"] > currentFrame || pushBox["_EndFrame"] <= currentFrame ) {
                        continue;
                    }
                    int rootOffsetX = 0;
                    int rootOffsetY = 0;
                    parseRootOffset( pushBox, rootOffsetX, rootOffsetY );

                    drawRectsBox( rectsJson, 5, pushBox["BoxNo"],rootOffsetX, rootOffsetY, 1.0,1.0,1.0);
                }
            }
            if (movesDictJson[actionName].contains("DamageCollisionKey"))
            {
                for (auto& [hurtBoxID, hurtBox] : movesDictJson[actionName]["DamageCollisionKey"].items())
                {
                    if ( !hurtBox.contains("_StartFrame") || hurtBox["_StartFrame"] > currentFrame || hurtBox["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    int rootOffsetX = 0;
                    int rootOffsetY = 0;
                    parseRootOffset( hurtBox, rootOffsetX, rootOffsetY );

                    bool drive = isDrive || wasDrive;
                    bool parry = currentAction >= 480 && currentAction <= 489;
                    bool di = currentAction >= 850 && currentAction <= 859;

                    for (auto& [boxNumber, boxID] : hurtBox["HeadList"].items()) {
                        drawRectsBox( rectsJson, 8, boxID,rootOffsetX, rootOffsetY,charColorR,charColorG,charColorB,drive,parry,di);
                    }
                    for (auto& [boxNumber, boxID] : hurtBox["BodyList"].items()) {
                        drawRectsBox( rectsJson, 8, boxID,rootOffsetX, rootOffsetY,charColorR,charColorG,charColorB,drive,parry,di);
                    }
                    for (auto& [boxNumber, boxID] : hurtBox["LegList"].items()) {
                        drawRectsBox( rectsJson, 8, boxID,rootOffsetX, rootOffsetY,charColorR,charColorG,charColorB,drive,parry,di);
                    }
                    for (auto& [boxNumber, boxID] : hurtBox["ThrowList"].items()) {
                        drawRectsBox( rectsJson, 7, boxID,rootOffsetX, rootOffsetY,0.95,0.95,0.85 );
                    }
                }
            }

            if (movesDictJson[actionName].contains("AttackCollisionKey"))
            {
                for (auto& [hitBoxID, hitBox] : movesDictJson[actionName]["AttackCollisionKey"].items())
                {
                    if ( !hitBox.contains("_StartFrame") || hitBox["_StartFrame"] > currentFrame || hitBox["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    int rootOffsetX = 0;
                    int rootOffsetY = 0;
                    parseRootOffset( hitBox, rootOffsetX, rootOffsetY );

                    for (auto& [boxNumber, boxID] : hitBox["BoxList"].items()) {
                        if (hitBox["CollisionType"] == 3) {
                            drawRectsBox( rectsJson, 3, boxID,rootOffsetX, rootOffsetY,0.5,0.5,0.5 );
                        } else if (hitBox["CollisionType"] == 0) {
                            drawRectsBox( rectsJson, 0, boxID,rootOffsetX, rootOffsetY,1.0,0.0,0.0, isDrive || wasDrive );
                        }
                    }                    
                }
            }

            if (movesDictJson[actionName].contains("BranchKey"))
            {
                for (auto& [keyID, key] : movesDictJson[actionName]["BranchKey"].items())
                {
                    if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    if (key["Type"] == 0) //always?
                    {
                        nextAction = key["Action"];
                        // do those also override if higher branchID?
                    }
                }
            }

            // should this fall through and let triggers also happen? prolly

            if (movesDictJson[actionName].contains("TriggerKey"))
            {
                for (auto& [keyID, key] : movesDictJson[actionName]["TriggerKey"].items())
                {
                    if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    bool defer = !key["_NotDefer"];
                    int triggerEndFrame = key["_EndFrame"];

                    auto triggerGroupString = to_string_leading_zeroes(key["TriggerGroup"], 3);
                    for (auto& [keyID, key] : triggerGroupsJson[triggerGroupString].items())
                    {
                        int triggerID = atoi(keyID.c_str());
                        std::string actionString = key;
                        int actionID = atoi(actionString.substr(0, actionString.find(" ")).c_str());

                        auto triggerIDString = std::to_string(triggerID);
                        auto actionIDString = to_string_leading_zeroes(actionID, 4);

                        auto norm = triggersJson[actionIDString][triggerIDString]["norm"];
                        int commandNo = norm["command_no"];
                        uint32_t okKeyFlags = norm["ok_key_flags"];
                        uint32_t okCondFlags = norm["ok_key_cond_flags"];
                        uint32_t dcExcFlags = norm["dc_exc_flags"];
                        // condflags..
                        // 10100000000100000: M oicho, but also eg. 22P - any one of three button mask?
                        // 10100000001100000: EX, so any two out of three button mask?
                        // 00100000000100000: heavy punch with one button mask
                        // 00100000001100000: normal throw, two out of two mask t
                        // 00100000010100000: taunt, 6 out of 6 in mask
                        if (okKeyFlags && matchInput(currentInput, okKeyFlags, okCondFlags, dcExcFlags))
                        {
                            // todo need to obey deferral
                            if ( commandNo == -1 ) {
                                if (defer) {
                                    deferredAction = actionID;
                                    deferredActionFrame = triggerEndFrame;
                                } else {
                                    nextAction = actionID;
                                }
                            } else {
                                std::string commandNoString = to_string_leading_zeroes(commandNo, 2);
                                int inputID = commandsJson[commandNoString]["0"]["input_num"].get<int>() - 1;
                                auto commandInputs = commandsJson[commandNoString]["0"]["inputs"];

                                uint32_t inputBufferCursor = 0;

                                while (inputID >= 0 )
                                {
                                    auto input = commandInputs[to_string_leading_zeroes(inputID, 2)]["normal"];
                                    // does 0x40000000 mean neutral?
                                    uint32_t inputOkKeyFlags = input["ok_key_flags"];
                                    uint32_t inputOkCondFlags = input["ok_key_cond_check_flags"];
                                    // condflags..
                                    // 10100000000100000: M oicho, but also eg. 22P - any one of three button mask?
                                    // 10100000001100000: EX, so any two out of three button mask?
                                    // 00100000000100000: heavy punch with one button mask
                                    // 00100000001100000: normal throw, two out of two mask t
                                    // 00100000010100000: taunt, 6 out of 6 in mask
                                    while (inputBufferCursor < inputBuffer.size())
                                    {
                                        if (matchInput(inputBuffer[inputBufferCursor++], inputOkKeyFlags, inputOkCondFlags)) {
                                            inputID--;
                                            break;
                                        }
                                    }

                                    if (inputBufferCursor == inputBuffer.size()) {
                                        break;
                                    }
                                }

                                if (inputID < 0) {
                                    if (defer) {
                                        deferredAction = actionID;
                                        deferredActionFrame = triggerEndFrame;
                                    } else {
                                        nextAction = actionID;
                                    }
                                }
                            }
                            // specifically don't break here, i think another trigger can have higher priority
                            // walking in reverse and breaking would be smarter :-)
                        }
                    }
                }
            }

            if ( deferredActionFrame == currentFrame ) {
                nextAction = deferredAction;

                deferredActionFrame = -1;
                deferredAction = 0;
            }
    
            currentFrame++;

            // evaluate branches after the frame bump, branch frames are meant to be elided afaict
            if (movesDictJson[actionName].contains("BranchKey"))
            {
                for (auto& [keyID, key] : movesDictJson[actionName]["BranchKey"].items())
                {
                    if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    if (key["Type"] == 0) //always?
                    {
                        nextAction = key["Action"];
                        // do those also override if higher branchID?
                    }
                }
            }
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        if (currentFrame >= actionFrameDuration && nextAction == -1)
        {
            if ( currentAction == 33 || currentAction == 34 || currentAction == 35 ) {
                // If done with pre-jump, transition to jump
                nextAction = currentAction + 3;
            }
            else {
                nextAction = 1;
            }
        }

        bool canMove = false;
        int actionCheckCanMove = currentAction;
        if (nextAction != -1 ) {
            actionCheckCanMove = nextAction;
        }
        bool movingForward = actionCheckCanMove == 9 || actionCheckCanMove == 10 || actionCheckCanMove == 11;
        bool movingBackward = actionCheckCanMove == 13 || actionCheckCanMove == 14 || actionCheckCanMove == 15;
        if (actionCheckCanMove == 1 || actionCheckCanMove == 2 || actionCheckCanMove == 4 || //stands, crouch
            movingForward || movingBackward) {
            canMove = true;
        }

        if ( marginFrame != -1 && currentFrame >= marginFrame ) {
            canMove = true;
        }

        // Process movement if any
        if ( canMove )
        {
            if ( currentInput & 1 ) {
                if ( currentInput & 4 ) {
                    nextAction = 35; // BAS_JUMP_B_START
                } else if ( currentInput & 8 ) {
                    nextAction = 34; // BAS_JUMP_F_START
                } else {
                    nextAction = 33; // BAS_JUMP_N_START
                }
            } else if ( currentInput & 2 ) {
                nextAction = 4; // BAS_CRH_Loop
            } else {
                if ((currentInput & (32+256)) == 32+256) {
                    nextAction = 480; // DPA_STD_START
                } else if ( currentInput & 4 && !movingBackward ) {
                    nextAction = 13; // BAS_BACKWARD_START
                } else if ( currentInput & 8 && !movingForward) {
                    nextAction = 9; // BAS_FORWARD_START
                }
            }

            if ( currentInput == 0 ) { // only do that if we're not post-margin for correctness
                nextAction = 1;
            }
        }

        // Transition
        if ( nextAction != -1 )
        {
            currentAction = nextAction;
            currentFrame = 0;

            // commit current place offset
            posX += posOffsetX;
            posOffsetX = 0.0f;
            posY += posOffsetY;
            posOffsetY = 0.0f;

            nextAction = -1;

            // if grounded, reset velocities on transition
            if ( posY == 0.0 && !isDrive) {
                velocityX = 0;
                velocityY = 0;
                accelX = 0;
                accelY = 0;
            }
            if (currentAction == 500 || currentAction == 501) {
                isDrive = true;
            } else if (isDrive == true) {
                isDrive = false;
                wasDrive = true;
            } else {
                wasDrive = false;
            }
        }
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
