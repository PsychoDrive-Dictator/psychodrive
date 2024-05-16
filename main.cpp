#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include "json.hpp"

#include <unistd.h>

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

bool forceCounter = false;
bool forcePunishCounter = false;
int hitStunAdder = 0;
uint32_t globalInputBufferLength = 4; // 4 frames of input buffering

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
    return true;
}
 
void drawHitBox(Box box, color col, bool isDrive /*= false*/, bool isParry /*= false*/, bool isDI /*= false*/ )
{
    if (isDrive || isParry || isDI ) {
        int driveOffset = 5;
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
        drawBox( box.x-driveOffset, box.y-driveOffset, box.w+driveOffset*2, box.h+driveOffset*2,colorR,colorG,colorB);
    }
    drawBox( box.x, box.y, box.w, box.h,col.r,col.g,col.b );
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
    const char *pEnv = getenv("ENABLE_CONTROLLER");
    auto SDL_init_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
    if ( pEnv ) SDL_init_flags |= SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(SDL_init_flags ) != 0)
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

    Guy guy("honda", 100.0f, 0.0f, 1, {0.8,0.6,0.2});
    Guy otherGuy("honda", 450.0f, 0.0f, -1, {1.0,1.0,1.0});
    guy.setOpponent(&otherGuy);
    otherGuy.setOpponent(&guy);

    uint32_t frameStartTime = SDL_GetTicks();
    int currentInput = 0;

    // Main loop
    bool paused = false;
    bool done = false;
    bool oneframe = false;
    while (!done)
    {
        const float desiredFrameTimeMS = 1000.0 / 60.0f;
        uint32_t currentTime = SDL_GetTicks();
        if (currentTime - frameStartTime < desiredFrameTimeMS) {
            const float timeToSleepMS = (desiredFrameTimeMS - (currentTime - frameStartTime));
            usleep(timeToSleepMS * 1000);
            //std::this_thread::sleep_for(std::chrono::milliseconds(int(timeToSleepMS)));
        }
        frameStartTime = SDL_GetTicks();
        // clear new press bits
        currentInput &= ~(LP_pressed+MP_pressed+HP_pressed+LK_pressed+MK_pressed+HK_pressed);

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
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
                        currentInput |= LP_pressed;
                        break;
                    case SDLK_i:
                        currentInput |= MP;
                        currentInput |= MP_pressed;
                        break;
                    case SDLK_o:
                        currentInput |= HP;
                        currentInput |= HP_pressed;
                        break;
                    case SDLK_j:
                        currentInput |= LK;
                        currentInput |= LK_pressed;
                        break;
                    case SDLK_k:
                        currentInput |= MK;
                        currentInput |= MK_pressed;
                        break;
                    case SDLK_l:
                        currentInput |= HK;
                        currentInput |= HK_pressed;
                        break;
                    case SDLK_p:
                        paused = !paused;
                        break;
                    case SDLK_0:
                        oneframe = true;
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
                const static int deadzone = 8192;
                switch (event.caxis.axis)
                {
                    case SDL_CONTROLLER_AXIS_LEFTX:
                        if (event.caxis.value < -deadzone )  {
                            currentInput |= BACK;
                            currentInput &= ~FORWARD;
                        } else if (event.caxis.value > deadzone ) {
                            currentInput |= FORWARD;
                            currentInput &= ~BACK;
                        } else {
                            currentInput &= ~(FORWARD+BACK);
                        }
                        break;
                    case SDL_CONTROLLER_AXIS_LEFTY:
                        if (event.caxis.value < -deadzone )  {
                            currentInput |= UP;
                            currentInput &= ~DOWN;
                        } else if (event.caxis.value > deadzone ) {
                            currentInput |= DOWN;
                            currentInput &= ~UP;
                        } else {
                            currentInput &= ~(DOWN+UP);
                        }
                        break;
                    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                        if (event.caxis.value > 0 ) {
                            currentInput |= HK;
                            currentInput |= HK_pressed;
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
                        currentInput |= LK_pressed;
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                        currentInput |= MK;
                        currentInput |= MK_pressed;
                        break;
                    case SDL_CONTROLLER_BUTTON_X:
                        currentInput |= LP;
                        currentInput |= LP_pressed;
                        break;
                    case SDL_CONTROLLER_BUTTON_Y:
                        currentInput |= MP;
                        currentInput |= MP_pressed;
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        currentInput |= HP;
                        currentInput |= HP_pressed;
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
        otherGuy.Input(0);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("Psycho Drive");

            Guy *pGuy = &guy;
            static bool otherguy = true;
            ImGui::Checkbox("P2", &otherguy);
            if (otherguy) {
                pGuy = &otherGuy;
            }
            ImGui::Text("action %s frame %i", pGuy->getActionName().c_str(), pGuy->getCurrentFrame());
            ImGui::Text("currentInput %d", currentInput);
            float posX, posY, posOffsetX, posOffsetY, velX, velY, accelX, accelY;
            pGuy->getPosDebug(posX, posY, posOffsetX, posOffsetY);
            pGuy->getVel(velX, velY, accelX, accelY);
            ImGui::Text("pos %f %f", posX, posY);
            ImGui::Text("posOffset %f %f", posOffsetX, posOffsetY);
            ImGui::Text("vel %f %f", velX, velY);
            ImGui::Text("accel %f %f", accelX, accelY);
            ImGui::Text("push %li hit %li hurt %li", pGuy->getPushBoxes()->size(), pGuy->getHitBoxes()->size(), pGuy->getHurtBoxes()->size());
            ImGui::Text("COMBO HITS %i damage %i hitstun %i juggle %i", pGuy->getComboHits(), pGuy->getComboDamage(), pGuy->getHitStun(), pGuy->getJuggleCounter());
            ImGui::SliderInt("hitstun adder", &hitStunAdder, -10, 10);
            ImGui::Checkbox("force counter", &forceCounter);
            ImGui::SameLine();
            ImGui::Checkbox("force PC", &forcePunishCounter);
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
        
        ImVec4 clear_color = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

        if (guy.getWarudo() || otherGuy.getWarudo()) {
            clear_color = ImVec4(0.075f, 0.075f, 0.075f, 1.00f);
        }

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

        bool guyInWarudo = false;
        bool otherGuyInWarudo = false;
        if (oneframe || !paused) {
            guyInWarudo = !guy.PreFrame();
            otherGuyInWarudo = !otherGuy.PreFrame();
        }

        guy.Render();
        otherGuy.Render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        if (oneframe || !paused) {
            if ( !guyInWarudo ) {
                guy.Push(&otherGuy);
                guy.CheckHit(&otherGuy);
                guy.Frame();
            }

            if ( !otherGuyInWarudo ) {
                // otherGuy.Push(&guy);
                otherGuy.CheckHit(&guy);
                otherGuy.Frame();
            }
        }

        oneframe = false;
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
