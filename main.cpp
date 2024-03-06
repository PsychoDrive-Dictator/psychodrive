// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

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
#include <chrono>
#include <thread>

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

void drawBox( float x, float y, float w, float h, float r, float g, float b)
{
    glColor4f(r,g,b, 0.3f);

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

static inline void drawRectsBox( nlohmann::json rectsJson, int rectsPage, int boxID,  int offsetX, int offsetY, float r, float g, float b )
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
    drawBox( xOrig - xRadius + offsetX, yOrig - yRadius + offsetY, xRadius * 2, yRadius * 2,r,g,b );
}

static inline void parseRootOffset( nlohmann::json& keyJson, int&offsetX, int& offsetY)
{
    if ( keyJson.contains("RootOffset") ) {
        offsetX = keyJson["RootOffset"]["X"];
        offsetY = keyJson["RootOffset"]["Y"];
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
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
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

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    std::string movesDictText = readFile("honda_dict.json");
    auto movesDictJson = nlohmann::json::parse(movesDictText);

    std::string rectsText = readFile("honda_rects.json");
    auto rectsJson = nlohmann::json::parse(rectsText);

    std::string namesText = readFile("honda_names.json");
    auto namesJson = nlohmann::json::parse(namesText);

    std::string triggerGroupsText = readFile("honda_trigger_groups.json");
    auto triggerGroupsJson = nlohmann::json::parse(triggerGroupsText);

    std::string triggersText = readFile("honda_triggers.json");
    auto triggersJson = nlohmann::json::parse(triggersText);

    float posX = 50.0f;
    float posY = 0.0f;
    float posOffsetX = 0.0f;
    float posOffsetY = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;

    int currentAction = 1;
    int currentFrame = 0;
    int actionFrameDuration = 0;
    int currentInput = 0;
    

    std::string actionName;

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
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

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        // if (show_demo_window)
        //     ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            ImGui::Begin(movesDictJson["0010_DMG_HL_ST"]["DamageCollisionKey"]["1"]["ATTR"].dump().c_str());

            ImGui::Text(rectsJson["08"]["001"]["OffsetX"].dump().c_str());               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderInt("action", &currentAction, 1, 1500);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::SliderInt("frame", &currentFrame, 0, 100);
            ImGui::Text(actionName.c_str());
            ImGui::Text("currentInput %d", currentInput);

            if (ImGui::Button("reset"))
                currentFrame = 0;

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        // if (show_another_window)
        // {
        //     ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        //     ImGui::Text("Hello from another window!");
        //     if (ImGui::Button("Close Me"))
        //         show_another_window = false;
        //     ImGui::End();
        // }

        // Rendering
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

            posX += velocityX;
            posY += velocityY;
            glTranslatef(posX + posOffsetX, posY + posOffsetY, 0.0f);

            if (movesDictJson[actionName].contains("SteerKey"))
            {
                for (auto& [steerKeyID, steerKey] : movesDictJson[actionName]["SteerKey"].items())
                {
                    if ( !steerKey.contains("_StartFrame") || steerKey["_StartFrame"] > currentFrame || steerKey["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    if (steerKey["OperationType"] == 1) {
                        float fixValue = steerKey["FixValue"];

                        if ( steerKey["ValueType"] == 0 ) {
                            velocityX = fixValue;
                        } else if (steerKey["ValueType"] == 1) {
                            velocityY = fixValue;
                        }
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

                    drawRectsBox( rectsJson, 5, pushBox["BoxNo"],rootOffsetX, rootOffsetY, 1.0,1.0,1.0 );
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

                    for (auto& [boxNumber, boxID] : hurtBox["HeadList"].items()) {
                        drawRectsBox( rectsJson, 8, boxID,rootOffsetX, rootOffsetY,1.0,0.0,1.0 );
                    }
                    for (auto& [boxNumber, boxID] : hurtBox["BodyList"].items()) {
                        drawRectsBox( rectsJson, 8, boxID,rootOffsetX, rootOffsetY,1.0,0.0,1.0 );
                    }
                    for (auto& [boxNumber, boxID] : hurtBox["LegList"].items()) {
                        drawRectsBox( rectsJson, 8, boxID,rootOffsetX, rootOffsetY,1.0,0.0,1.0 );
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
                            drawRectsBox( rectsJson, 0, boxID,rootOffsetX, rootOffsetY,1.0,0.0,0.0 );
                        }
                    }                    
                }
            }

            bool branched = false;

            if (movesDictJson[actionName].contains("BranchKey"))
            {
                for (auto& [keyID, key] : movesDictJson[actionName]["BranchKey"].items())
                {
                    if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    if (key["Type"] == 0) //always?
                    {
                        currentAction = key["Action"];
                        currentFrame = 0;
                        branched = true;
                        break;
                    }
                }
            }

            if (!branched && movesDictJson[actionName].contains("TriggerKey"))
            {
                for (auto& [keyID, key] : movesDictJson[actionName]["TriggerKey"].items())
                {
                    if ( !key.contains("_StartFrame") || key["_StartFrame"] > currentFrame || key["_EndFrame"] <= currentFrame ) {
                        continue;
                    }

                    auto triggerGroupString = to_string_leading_zeroes(key["TriggerGroup"], 3);
                    for (auto& [keyID, key] : triggerGroupsJson[triggerGroupString].items())
                    {
                        int triggerID = atoi(keyID.c_str());
                        std::string actionString = key;
                        int actionID = atoi(actionString.substr(0, actionString.find(" ")).c_str());

                        auto triggerIDString = std::to_string(triggerID);
                        auto actionIDString = to_string_leading_zeroes(actionID, 4);

                        auto norm = triggersJson[actionIDString][triggerIDString]["norm"];
                        if (norm["command_index"] == -1 && norm["ok_key_flags"] != 0 && norm["ok_key_flags"] == currentInput )
                        {
                            currentAction = actionID;
                            currentFrame = 0;
                        }
                    }
                }
            }
        }

        //drawBox( rectJson["OffsetX"], rectJson["OffsetY"], rectJson["SizeX"], rectJson["SizeY"] );

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        currentFrame++;

        if (currentFrame >= actionFrameDuration)
        {
            currentAction = 1;
            currentFrame = 0;
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
