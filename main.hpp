#pragma once

#include <string>
#include "json.hpp"

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
};

void log(std::string logLine);

std::string to_string_leading_zeroes(unsigned int number, unsigned int length);
nlohmann::json parse_json_file(const std::string &fileName);

void drawRectsBox( nlohmann::json rectsJson, int rectsPage, int boxID,  int offsetX, int offsetY, float r, float g, float b, bool isDrive = false, bool isParry = false, bool isDI = false );