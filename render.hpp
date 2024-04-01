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

extern float zoom;
extern int translateX;
extern int translateY;

void setRenderState(color clearColor, int sizeX, int sizeY);
void drawHitBox(Box box, color col, bool isDrive /*= false*/, bool isParry /*= false*/, bool isDI /*= false*/ );
void drawBox(float x, float y, float w, float h, float r, float g, float b);
void drawQuad(float x, float y, float w, float h, float r, float g, float b, float a);
void drawLoop(float x, float y, float w, float h, float r, float g, float b, float a);
SDL_Window* initWindowRender(void);
void initRenderUI(void);
void destroyRender(void);