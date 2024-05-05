#pragma once

#include <SDL.h>
#include <SDL_opengl.h>


#include "main.hpp"

class Guy;

struct color {
    float r;
    float g;
    float b;
};

struct RenderBox {
    Box box;
    float thickness = 10.0;
    color col;
    bool drive = false;
    bool parry = false;
    bool di = false;
};

struct HitMarker {
    float x;
    float y;
    float radius;
    Guy *pOrigin;
    int type;
    int time;
    int maxtime;
};

extern float zoom;
extern float fov;
extern int translateX;
extern int translateY;

void setRenderState(color clearColor, int sizeX, int sizeY);
void addHitMarker(HitMarker newMarker);
void renderMarkersAndStuff(void);
void drawHitBox(Box box, float thickness, color col, bool isDrive /*= false*/, bool isParry /*= false*/, bool isDI /*= false*/ );
void drawBox(float x, float y, float w, float h, float thickness, float r, float g, float b, float a);
void drawQuad(float x, float y, float w, float h, float r, float g, float b, float a);
void drawLoop(float x, float y, float w, float h, float r, float g, float b, float a);
SDL_Window* initWindowRender(void);
void initRenderUI(void);
void destroyRender(void);