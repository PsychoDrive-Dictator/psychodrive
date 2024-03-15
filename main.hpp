#pragma once

#include <string>
#include "guy.hpp"
#include "json.hpp"

enum hitentryflags {
    crouch = 1,
    air = 2,
    special = 4,
    counter = 8,
    block = 16,
    otg = crouch|air,
    punish_counter = special|counter,
};

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
	LP_pressed = 1024,
	MP_pressed = 2048,
	HP_pressed = 4096,
	LK_pressed = 8192,
	MK_pressed = 16384,
	HK_pressed = 32768,
};

struct color {
    float r;
    float g;
    float b;
};

struct Box {
    float x;
    float y;
    float w;
    float h;
};

struct HitBox {
    Box box;
    int hitEntryID;
    int hitID;
};

struct RenderBox {
    Box box;
    color col;
    bool drive = false;
    bool parry = false;
    bool di = false;
};

void log(std::string logLine);

class Guy;
extern std::vector<Guy *> guys;
extern bool resetpos;

extern int globalFrameCount;

extern bool forceCounter;
extern bool forcePunishCounter;
extern int hitStunAdder;

extern uint32_t globalInputBufferLength;

std::string to_string_leading_zeroes(unsigned int number, unsigned int length);
nlohmann::json parse_json_file(const std::string &fileName);

bool doBoxesHit(Box box1, Box box2);
void drawHitBox(Box box, color col, bool isDrive = false, bool isParry = false, bool isDI = false );