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

#include "guy.hpp"
#include "main.hpp"

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
    if (fileText == "") return nullptr;
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

std::string to_string_leading_zeroes(unsigned int number, unsigned int length)
{
    
     std::string num_str = std::to_string(number);
    
    if(num_str.length() >= length) return num_str;
    
     std::string leading_zeros(length - num_str.length(), '0');
    
    return leading_zeros + num_str;
}
 
void drawRectsBox( nlohmann::json rectsJson, int rectsPage, int boxID,  int offsetX, int offsetY, int dir, color col, bool isDrive /*= false*/, bool isParry /*= false*/, bool isDI /*= false*/ )
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
    xOrig *= dir;
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
    drawBox( xOrig - xRadius + offsetX, yOrig - yRadius + offsetY, xRadius * 2, yRadius * 2,col.r,col.g,col.b );
}


std::deque<std::string> logQueue;

void log(std::string logLine)
{
    logQueue.push_back(logLine);
    if (logQueue.size() > 20) {
        logQueue.pop_front();
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
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    Guy guy("honda", 100.0f, 0.0f, 1, {0.8,0.6,0.2});
    Guy otherGuy("ryu", 400.0f, 0.0f, -1, {1.0,1.0,1.0});
    guy.setOpponent(&otherGuy);
    otherGuy.setOpponent(&guy);
    int currentInput = 0;

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
            if (event.type == SDL_CONTROLLERDEVICEADDED)
            {
                SDL_GameController *controller = SDL_GameControllerOpen(event.cdevice.which);
                log("controller added " + std::to_string(event.cdevice.which) + " " + std::to_string((uint64_t)controller));
            }
            if (event.type == SDL_CONTROLLERAXISMOTION)
            {
                switch (event.caxis.axis)
                {
                    case SDL_CONTROLLER_AXIS_LEFTX:
                        // log("SDL_CONTROLLER_AXIS_LEFTX " + std::to_string(event.caxis.value));
                        if (event.caxis.value < 0 )  {
                            currentInput |= BACK;
                        } else if (event.caxis.value > 0 ) {
                            currentInput |= FORWARD;
                        } else if (event.caxis.value == 0){
                            currentInput &= ~BACK;
                            currentInput &= ~FORWARD;
                        }
                        break;
                    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                        if (event.caxis.value > 0 ) {
                            currentInput |= HK;
                        } else {
                            currentInput &= ~HK;
                        }
                        break;
                }
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN)
            {
                switch (event.cbutton.button)
                {
                // log("controller event " + std::to_string(event.button.button))
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        currentInput |= BACK;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        currentInput |= DOWN;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        currentInput |= FORWARD;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        currentInput |= UP;
                        break;
                    case SDL_CONTROLLER_BUTTON_A:
                        currentInput |= LK;
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                        currentInput |= MK;
                        break;
                    case SDL_CONTROLLER_BUTTON_X:
                        currentInput |= LP;
                        break;
                    case SDL_CONTROLLER_BUTTON_Y:
                        currentInput |= MP;
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        currentInput |= HP;
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
            if (event.type == SDL_CONTROLLERBUTTONUP)
            {
                switch (event.cbutton.button)
                {
                // log("controller event " + std::to_string(event.button.button))
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        currentInput &= ~BACK;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        currentInput &= ~DOWN;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        currentInput &= ~FORWARD;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        currentInput &= ~UP;
                        break;
                    case SDL_CONTROLLER_BUTTON_A:
                        currentInput &= ~LK;
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                        currentInput &= ~MK;
                        break;
                    case SDL_CONTROLLER_BUTTON_X:
                        currentInput &= ~LP;
                        break;
                    case SDL_CONTROLLER_BUTTON_Y:
                        currentInput &= ~MP;
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        currentInput &= ~HP;
                        break;
                }
            }
        }

        guy.Input(currentInput);
        otherGuy.Input(currentInput);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("Psycho Drive");

            ImGui::Text("action %s frame %i", guy.getActionName().c_str(), guy.getCurrentAction());
            ImGui::Text("currentInput %d", currentInput);
            float posX, posY, posOffsetX, posOffsetY, velX, velY, accelX, accelY;
            guy.getPosDebug(posX, posY, posOffsetX, posOffsetY);
            guy.getVel(velX, velY, accelX, accelY);
            ImGui::Text("pos %f %f", posX, posY);
            ImGui::Text("posOffset %f %f", posOffsetX, posOffsetY);
            ImGui::Text("vel %f %f", velX, velY);
            ImGui::Text("accel %f %f", accelX, accelY);
            //ImGui::Text("unique %i", uniqueCharge);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();

            ImGui::Begin("Debug Log");
            for (auto logLine : logQueue) {
                ImGui::Text(logLine.c_str());
            }
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
        glScalef(1.5f, -1.5f, 1.0f);

        guy.PreFrame();
        otherGuy.PreFrame();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        std::this_thread::sleep_for(std::chrono::milliseconds(8));

        guy.Frame();
        otherGuy.Frame();
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
