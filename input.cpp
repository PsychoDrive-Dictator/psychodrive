#include <SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

#include "input.hpp"
#include "main.hpp"
#include "ui.hpp"
#include "render.hpp"

std::map<int, int> currentInputMap;

bool touchControls = false;

struct FingerState {
    float x;
    float y;
};

SDL_FingerID directionFinger = -1;
std::map<SDL_FingerID, FingerState> mapFingerPositions;

struct GamepadFlags {
    bool horizAnalogInput = false;
    bool vertAnalogInput = false;
};

std::map<int, GamepadFlags> mapGamepadFlags;

class TouchButton {
public:
    TouchButton(float x, float y, float yOffset, float radius, int inputSlot, int input) :
        x(x), y(y), yOffset(yOffset), radius(radius), inputSlot(inputSlot), input(input) { }

    void CheckInput(std::map<SDL_FingerID, FingerState> &mapState) {
        bool foundTouch = false;
        for (auto & fingerState: mapState) {
            FingerState &state = fingerState.second;
            if (state.x >= x - radius && state.x <= x+radius && state.y >= y+(yOffset-radius)/aspect && state.y <= y+(yOffset+radius)/aspect)
                foundTouch = true;
        }
        if (foundTouch) {
            currentInputMap[inputSlot] |= input;
            if (input > 8 && active == false)
                currentInputMap[inputSlot] |= input << 6;
            active = true;
        } else if (active == true) {
            currentInputMap[inputSlot] &= ~input;
            active = false;
        }
    }

    void Render(int winX, int winY) {
        aspect = (float)winY / (float)winX;
        drawBox( (x - radius) * winX, y * winY + (yOffset - radius) * winY / aspect, radius * 2.0 * winX, radius * 2.0 * winY / aspect, 1.0, 1.0,1.0,1.0, active ? 0.25 : 0.1);
    }

private:
    float x;
    float y;
    float yOffset;
    float radius;
    int inputSlot;
    int input;
    bool active = false;
    float aspect = 1.0;
};

class TouchPad {
public:
    TouchPad(float x, float y, float innerRadius, int inputSlot) :
        x(x), y(y), innerRadius(innerRadius), inputSlot(inputSlot) { }

    void CheckInput(FingerState state) {
#ifdef __EMSCRIPTEN__
        int oldInput = currentInputMap[inputSlot];
#endif
        if (state.x >= 0.0f && state.x >= x + innerRadius) {
            currentInputMap[inputSlot] |= FORWARD;
        } else {
            currentInputMap[inputSlot] &= ~FORWARD;
        }
        if (state.x >= 0.0f && state.x <= x - innerRadius) {
            currentInputMap[inputSlot] |= BACK;
        } else {
            currentInputMap[inputSlot] &= ~BACK;
        }
        if (state.y >= 0.0f && state.y <= y - innerRadius/aspect) {
            currentInputMap[inputSlot] |= UP;
        } else {
            currentInputMap[inputSlot] &= ~UP;
        }
        if (state.y >= 0.0f && state.y >= y + innerRadius/aspect) {
            currentInputMap[inputSlot] |= DOWN;
        } else {
            currentInputMap[inputSlot] &= ~DOWN;
        }

#ifdef __EMSCRIPTEN__
        if (currentInputMap[inputSlot] != oldInput)
            emscripten_vibrate(5);
#endif
    }

    void Render(int winX, int winY) {
        aspect = (float)winY / (float)winX;
        drawBox( (x - innerRadius) * winX, y * winY - innerRadius * winY / aspect, innerRadius * 2.0 * winX, innerRadius * 2.0 * winY / aspect, 1.0, 1.0,1.0,1.0, 0.1);
        float xOffset = 0.0f, yOffset = 0.0f;
        if (currentInputMap[inputSlot] & FORWARD) {
            xOffset = innerRadius / 2.0;
        }
        if (currentInputMap[inputSlot] & BACK) {
            xOffset = -innerRadius / 2.0;
        }
        if (currentInputMap[inputSlot] & UP) {
            yOffset = innerRadius / 2.0;
        }
        if (currentInputMap[inputSlot] & DOWN) {
            yOffset = -innerRadius / 2.0;
        }
        drawBox( (x - innerRadius / 2.0 + xOffset) * winX, y * winY - (innerRadius / 2.0 + yOffset) * winY / aspect, innerRadius * winX, innerRadius * winY / aspect, 1.0, 1.0,1.0,1.0, 0.1);
    }
private:
    float x;
    float y;
    float innerRadius;
    int inputSlot;
    float aspect = 1.0;
};

std::vector<TouchButton> vecTouchButtons;
std::vector<TouchPad> vecTouchPads;

int addPressBits(int curInput, int prevInput)
{
    // put down the _pressed bits on first appearance
    int retInput = curInput;
    int key = 4;
    while (key < 10)
    {
        int keyInput = 1<<key;
        if (curInput & keyInput && !(prevInput & keyInput)) {
            retInput |= 1<<(key+6);
        }
        key++;
    }

    return retInput;
}

void updateInputs(int sizeX, int sizeY)
{
    // clear new press bits
    for (auto i : currentInputMap) {
        currentInputMap[i.first] &= ~(LP_pressed+MP_pressed+HP_pressed+LK_pressed+MK_pressed+HK_pressed);
    }
    SDL_Event event;
    bool directionFingerUp = false;
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
                case SDLK_w:
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
                case SDLK_F2:
                    toggleRenderUI = !toggleRenderUI;
                    break;
                case SDLK_F3:
                    thickboxes = !thickboxes;
                    break;
                case SDLK_F4:
                    renderPositionAnchors = !renderPositionAnchors;
                    break;
                case SDLK_F5:
                    lockCamera = !lockCamera;
                    break;
                case SDLK_F8:
                    limitRate = !limitRate;
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
                        mapGamepadFlags[inputID].horizAnalogInput = true;
                    } else if (event.caxis.value > deadzone ) {
                        currentInputMap[inputID] |= FORWARD;
                        currentInputMap[inputID] &= ~BACK;
                        mapGamepadFlags[inputID].horizAnalogInput = true;
                    } else if (mapGamepadFlags[inputID].horizAnalogInput) {
                        currentInputMap[inputID] &= ~(FORWARD+BACK);
                        mapGamepadFlags[inputID].horizAnalogInput = false;
                    }
                    break;
                case SDL_CONTROLLER_AXIS_LEFTY:
                    if (event.caxis.value < -deadzone )  {
                        currentInputMap[inputID] |= UP;
                        currentInputMap[inputID] &= ~DOWN;
                        mapGamepadFlags[inputID].vertAnalogInput = true;
                    } else if (event.caxis.value > deadzone ) {
                        currentInputMap[inputID] |= DOWN;
                        currentInputMap[inputID] &= ~UP;
                        mapGamepadFlags[inputID].vertAnalogInput = true;
                    } else if (mapGamepadFlags[inputID].vertAnalogInput) {
                        currentInputMap[inputID] &= ~(DOWN+UP);
                        mapGamepadFlags[inputID].vertAnalogInput = false;
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
                case SDLK_w:
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
        if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERMOTION || event.type == SDL_FINGERUP)
        {
            if (!touchControls && gameMode == Training) {
                touchControls = true;
                initTouchControls();
            }

            if (touchControls) {
                if (event.type == SDL_FINGERUP) {
                    event.tfinger.x = -1.0f;
                    event.tfinger.y = -1.0f;

                    if (event.tfinger.fingerId == directionFinger) {
                        directionFingerUp = true;
                    }
                }
                FingerState state = { event.tfinger.x, event.tfinger.y };
                mapFingerPositions[event.tfinger.fingerId] = state;
                //log("stickyfingerz " + std::to_string(event.tfinger.fingerId) + " " + std::to_string(event.tfinger.x) + " " + std::to_string(event.tfinger.y));

                if (event.type == SDL_FINGERDOWN && event.tfinger.y > 0.5 && event.tfinger.x < 0.5) {
                    directionFinger = event.tfinger.fingerId;
                    vecTouchPads.push_back(TouchPad(event.tfinger.x, event.tfinger.y, 25.0 / sizeX, keyboardID));
                }
            }
        }
        static bool mouseLeftPressed = false;
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
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
            {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouseLeftPressed = event.button.state == SDL_PRESSED && event.button.y < sizeY / 2;
                }
            }
            if (event.type == SDL_MOUSEMOTION)
            {
                if (mouseLeftPressed) {
                    translateX += event.motion.xrel;
                    translateY += event.motion.yrel;
                }
            }
        } else {
            mouseLeftPressed = false;
        }
    }

    // resolve finger positions into touch button input
    for (auto & button : vecTouchButtons) {
        button.CheckInput(mapFingerPositions);
    }
    if (directionFinger != -1) {
        for (auto & pad : vecTouchPads) {
            pad.CheckInput(mapFingerPositions[directionFinger]);
        }
    }
    // only remove pad after checking input to remove the direction input
    if (directionFingerUp) {
        directionFinger = -1;
        vecTouchPads.clear();
    }
}

void initTouchControls(void)
{
    vecTouchButtons.push_back(TouchButton(0.6, 0.70, 0.05, 0.06, keyboardID, LP));
    vecTouchButtons.push_back(TouchButton(0.75, 0.70, 0.0, 0.06, keyboardID, MP));
    vecTouchButtons.push_back(TouchButton(0.90, 0.70, 0.0, 0.06, keyboardID, HP));
    vecTouchButtons.push_back(TouchButton(0.6, 0.70, 0.20, 0.06, keyboardID, LK));
    vecTouchButtons.push_back(TouchButton(0.75, 0.70, 0.15, 0.06, keyboardID, MK));
    vecTouchButtons.push_back(TouchButton(0.90, 0.70, 0.15, 0.06, keyboardID, HK));
}

void renderTouchControls(int sizeX, int sizeY)
{
    for (auto & button : vecTouchButtons) {
        button.Render(sizeX, sizeY);
    }
    for (auto & pad : vecTouchPads) {
        pad.Render(sizeX, sizeY);
    }
}