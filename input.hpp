#pragma once

#include <map>

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

static const int keyboardID = 0;
static const int recordingID = 1;
static const int firstJoystickID = 10;
extern std::map<int, int> currentInputMap;

void updateInputs(void);