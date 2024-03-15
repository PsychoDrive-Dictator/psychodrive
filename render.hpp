#pragma once

#include <SDL.h>
#include <SDL_opengl.h>

#include "main.hpp"

struct color {
    float r;
    float g;
    float b;
};

struct RenderBox {
    Box box;
    color col;
    bool drive = false;
    bool parry = false;
    bool di = false;
};

void setRenderState(color clearColor, int sizeX, int sizeY);
void drawHitBox(Box box, color col, bool isDrive /*= false*/, bool isParry /*= false*/, bool isDI /*= false*/ );

SDL_Window* initWindowRender(void);
void initRenderUI(void);
void destroyRender(void);