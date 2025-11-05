#pragma once

#include <SDL.h>

#include "main.hpp"

class Guy;

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
    Guy* pOrigin;
    int type;
    int time;
    int maxtime;
    int seed;
    float dirX;
    float dirY;
};

extern bool thickboxes;
extern bool renderPositionAnchors;
extern float zoom;
extern float fov;
extern int translateX;
extern int translateY;

void setRenderState(color clearColor, int sizeX, int sizeY);
void setScreenSpaceRenderState(int sizeX, int sizeY);
void addHitMarker(HitMarker newMarker);
void clearHitMarkers(void);
void renderMarkersAndStuff(void);
void drawHitMarker(float x, float y, float radius, int hitType, int time, int maxTime, float dirX = 1.0f, float dirY = 0.0f, int seed = 0);
void drawHitBox(Box box, float thickness, color col, bool isDrive /*= false*/, bool isParry /*= false*/, bool isDI /*= false*/ );
void drawBox(float x, float y, float w, float h, float thickness, float r, float g, float b, float a, bool noFront = false);
SDL_Window* initWindowRender(void);
void initRenderUI(void);
void destroyRender(void);
