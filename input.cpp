#include <SDL.h>

#include "input.hpp"
#include "main.hpp"
#include "ui.hpp"
#include "render.hpp"

int getInput(int currentInput)
{
    // clear new press bits
    currentInput &= ~(LP_pressed+MP_pressed+HP_pressed+LK_pressed+MK_pressed+HK_pressed);
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
            done = true;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
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
                case SDLK_y:
                    currentInput |= LP+MP+HP;
                    currentInput |= LP_pressed+MP_pressed+HP_pressed;
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
                case SDLK_h:
                    currentInput |= LK+MK+HK;
                    currentInput |= LK_pressed+MK_pressed+HK_pressed;
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
                case SDLK_q:
                    resetpos = true;
                    break;
                case SDLK_0:
                    oneframe = true;
                    break;
                case SDLK_r:
                    recordingInput = !recordingInput;
                    if (recordingInput == true) {
                        // so we can easily look for dupes
                        timelineToInputBuffer(playBackInputBuffer);
                        recordingStartFrame = globalFrameCount;
                        if (playingBackInput) {
                            playingBackInput = false;
                        }
                    }
                    break;
                case SDLK_t:
                    playingBackInput = !playingBackInput;
                    if (playingBackInput == true) {
                        timelineToInputBuffer(playBackInputBuffer);
                    } else {
                        currentInput = 0;
                    }
                    playBackFrame = 0;
                    break;
                case SDLK_DELETE:
                    deleteInputs = true;
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
                case SDLK_y:
                    currentInput &= ~(LP+MP+HP);
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
                case SDLK_h:
                    currentInput &= ~(LK+MK+HK);
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
        if (!ImGui::GetIO().WantCaptureMouse)
        {
            if(event.type == SDL_MOUSEWHEEL)
            {
                if(event.wheel.y > 0) {
                    zoom += 0.2f;
                }
                if(event.wheel.y < 0) {
                    zoom -= 0.2f;
                }
            }
            static bool mouseLeftPressed = false;
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
            {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouseLeftPressed = event.button.state == SDL_PRESSED;
                }
            }
            if (event.type == SDL_MOUSEMOTION)
            {
                if (mouseLeftPressed) {
                    translateX += event.motion.xrel;
                    translateY += event.motion.yrel;
                }
            }
        }
   }
    return currentInput;
}