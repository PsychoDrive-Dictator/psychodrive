#include <SDL.h>

#include "input.hpp"
#include "main.hpp"
#include "ui.hpp"
#include "render.hpp"

std::map<int, int> currentInputMap;

void updateInputs(void)
{
    // clear new press bits
    for (auto i : currentInputMap) {
        currentInputMap[i.first] &= ~(LP_pressed+MP_pressed+HP_pressed+LK_pressed+MK_pressed+HK_pressed);
    }
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
                    currentInputMap[keyboardID] |= BACK;
                    break;
                case SDLK_s:
                    currentInputMap[keyboardID] |= DOWN;
                    break;
                case SDLK_d:
                    currentInputMap[keyboardID] |= FORWARD;
                    break;
                case SDLK_SPACE:
                    currentInputMap[keyboardID] |= UP;
                    break;
                case SDLK_y:
                    currentInputMap[keyboardID] |= LP+MP+HP;
                    currentInputMap[keyboardID] |= LP_pressed+MP_pressed+HP_pressed;
                    break;
                case SDLK_u:
                    currentInputMap[keyboardID] |= LP;
                    currentInputMap[keyboardID] |= LP_pressed;
                    break;
                case SDLK_i:
                    currentInputMap[keyboardID] |= MP;
                    currentInputMap[keyboardID] |= MP_pressed;
                    break;
                case SDLK_o:
                    currentInputMap[keyboardID] |= HP;
                    currentInputMap[keyboardID] |= HP_pressed;
                    break;
                case SDLK_h:
                    currentInputMap[keyboardID] |= LK+MK+HK;
                    currentInputMap[keyboardID] |= LK_pressed+MK_pressed+HK_pressed;
                    break;
                case SDLK_j:
                    currentInputMap[keyboardID] |= LK;
                    currentInputMap[keyboardID] |= LK_pressed;
                    break;
                case SDLK_k:
                    currentInputMap[keyboardID] |= MK;
                    currentInputMap[keyboardID] |= MK_pressed;
                    break;
                case SDLK_l:
                    currentInputMap[keyboardID] |= HK;
                    currentInputMap[keyboardID] |= HK_pressed;
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
                        playBackFrame = 0;
                    } else {
                        currentInputMap[recordingID] = 0;
                    }
                    paused = false;
                    break;
                case SDLK_F8:
                    limitRate = !limitRate;
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
            int inputID = firstJoystickID + event.caxis.which;
            switch (event.caxis.axis)
            {
                case SDL_CONTROLLER_AXIS_LEFTX:
                    if (event.caxis.value < -deadzone )  {
                        currentInputMap[inputID] |= BACK;
                        currentInputMap[inputID] &= ~FORWARD;
                    } else if (event.caxis.value > deadzone ) {
                        currentInputMap[inputID] |= FORWARD;
                        currentInputMap[inputID] &= ~BACK;
                    } else {
                        currentInputMap[inputID] &= ~(FORWARD+BACK);
                    }
                    break;
                case SDL_CONTROLLER_AXIS_LEFTY:
                    if (event.caxis.value < -deadzone )  {
                        currentInputMap[inputID] |= UP;
                        currentInputMap[inputID] &= ~DOWN;
                    } else if (event.caxis.value > deadzone ) {
                        currentInputMap[inputID] |= DOWN;
                        currentInputMap[inputID] &= ~UP;
                    } else {
                        currentInputMap[inputID] &= ~(DOWN+UP);
                    }
                    break;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                    if (event.caxis.value > 0 ) {
                        currentInputMap[inputID] |= HK;
                        currentInputMap[inputID] |= HK_pressed;
                    } else {
                        currentInputMap[inputID] &= ~HK;
                    }
                    break;
            }
        }
        if (event.type == SDL_CONTROLLERBUTTONDOWN)
        {
            int inputID = firstJoystickID + event.cbutton.which;
            switch (event.cbutton.button)
            {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    currentInputMap[inputID] |= BACK;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    currentInputMap[inputID] |= DOWN;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    currentInputMap[inputID] |= FORWARD;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    currentInputMap[inputID] |= UP;
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                    currentInputMap[inputID] |= LK;
                    currentInputMap[inputID] |= LK_pressed;
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    currentInputMap[inputID] |= MK;
                    currentInputMap[inputID] |= MK_pressed;
                    break;
                case SDL_CONTROLLER_BUTTON_X:
                    currentInputMap[inputID] |= LP;
                    currentInputMap[inputID] |= LP_pressed;
                    break;
                case SDL_CONTROLLER_BUTTON_Y:
                    currentInputMap[inputID] |= MP;
                    currentInputMap[inputID] |= MP_pressed;
                    break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    currentInputMap[inputID] |= HP;
                    currentInputMap[inputID] |= HP_pressed;
                    break;
            }
        }
        if (event.type == SDL_KEYUP)
        {
            switch (event.key.keysym.sym)
            {
                case SDLK_a:
                    currentInputMap[keyboardID] &= ~BACK;
                    break;
                case SDLK_s:
                    currentInputMap[keyboardID] &= ~DOWN;
                    break;
                case SDLK_d:
                    currentInputMap[keyboardID] &= ~FORWARD;
                    break;
                case SDLK_SPACE:
                    currentInputMap[keyboardID] &= ~UP;
                    break;
                case SDLK_y:
                    currentInputMap[keyboardID] &= ~(LP+MP+HP);
                    break;
                case SDLK_u:
                    currentInputMap[keyboardID] &= ~LP;
                    break;
                case SDLK_i:
                    currentInputMap[keyboardID] &= ~MP;
                    break;
                case SDLK_o:
                    currentInputMap[keyboardID] &= ~HP;
                    break;
                case SDLK_h:
                    currentInputMap[keyboardID] &= ~(LK+MK+HK);
                    break;
                case SDLK_j:
                    currentInputMap[keyboardID] &= ~LK;
                    break;
                case SDLK_k:
                    currentInputMap[keyboardID] &= ~MK;
                    break;
                case SDLK_l:
                    currentInputMap[keyboardID] &= ~HK;
                    break;
            }
        }
        if (event.type == SDL_CONTROLLERBUTTONUP)
        {
            int inputID = firstJoystickID + event.cbutton.which;
            switch (event.cbutton.button)
            {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    currentInputMap[inputID] &= ~BACK;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    currentInputMap[inputID] &= ~DOWN;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    currentInputMap[inputID] &= ~FORWARD;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    currentInputMap[inputID] &= ~UP;
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                    currentInputMap[inputID] &= ~LK;
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    currentInputMap[inputID] &= ~MK;
                    break;
                case SDL_CONTROLLER_BUTTON_X:
                    currentInputMap[inputID] &= ~LP;
                    break;
                case SDL_CONTROLLER_BUTTON_Y:
                    currentInputMap[inputID] &= ~MP;
                    break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    currentInputMap[inputID] &= ~HP;
                    break;
            }
        }
        if (!ImGui::GetIO().WantCaptureMouse)
        {
            if(event.type == SDL_MOUSEWHEEL)
            {
                if(event.wheel.y > 0) {
                    zoom += 50.0;
                }
                if(event.wheel.y < 0) {
                    zoom -= 50.0;
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
}