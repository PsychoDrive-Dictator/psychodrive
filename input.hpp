#pragma once

#include <cstddef>
#include <map>
#include <string>

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
	CONSUMED = 65536,
};

enum InputID
{
	nullInputID = 0,
	keyboardID = 1,
	recordingID = 2,
	replayLeft = 3,
	replayRight = 4,
	firstJoystickID = 10,
};

#define printInput(printedInput) \
if (input & printedInput) { \
	if (printed) strOut += "+"; \
	strOut += #printedInput; \
	printed = true; \
} \

static inline void renderInput(std::string &strOut, int input)
{
	bool printed = false;
	if (input & UP) {
		strOut += "UP";
		printed = true;
	}
	if (input & DOWN) {
		if (printed) strOut += "+";
		strOut += "DOWN";
		printed = true;
	}
	if (input & BACK) {
		if (printed) strOut += "+";
		strOut += "BACK";
		printed = true;
	}
	if (input & FORWARD) {
		if (printed) strOut += "+";
		strOut += "FORWARD";
		printed = true;
	}
	printInput(LP);
	printInput(LK);
	printInput(MP);
	printInput(MK);
	printInput(HP);
	printInput(HK);
}

static inline int invertDirection(int input)
{
	int newMask = 0;
	if (input & BACK) {
		newMask |= FORWARD;
	}
	if (input & FORWARD) {
		newMask |= BACK;
	}
	input &= ~(FORWARD+BACK);
	input |= newMask;
	return input;
}

static inline int inputAngle(int input)
{
	input = input & 0xF;
	switch (input) {
		case FORWARD+UP: return 45;
		case UP: return 90;
		case BACK+UP: return 135;
		case BACK: return 180;
		case BACK+DOWN: return 225;
		case DOWN: return 270;
		case FORWARD+DOWN: return 315;
		case FORWARD: return 360;
		default: case NEUTRAL: return 0;
	}
	return -1;
}

static inline int angleDiff(int curAngle, int futureAngle)
{
	int res = futureAngle - curAngle;
	if (res > 180) return res - 360;
	if (res < -180) return res + 360;
	return res;
}

extern std::map<int, int> currentInputMap;

int addPressBits(int curInput, int prevInput);
void updateInputs(int sizeX, int sizeY);
void initTouchControls(void);
void renderTouchControls(int sizeX, int sizeY);