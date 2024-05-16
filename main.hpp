#pragma once

#include <string>
#include <deque>
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

struct Box {
    float x;
    float y;
    float w;
    float h;
};

enum hitBoxType {
    hit = 1,
    grab = 2,
    projectile = 3,
    domain = 4,
    destroy_projectile = 5,
    proximity_guard = 6,
};

struct HitBox {
    Box box;
    hitBoxType type;
    int hitEntryID;
    int hitID;
};

static inline float randFloat()
{
    float ret = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    return ret;
}

void log(std::string logLine);

class Guy;
extern std::vector<Guy *> guys;
extern bool resetpos;

extern bool done;
extern bool paused;
extern bool oneframe;
extern int globalFrameCount;

extern bool recordingInput;
extern std::vector<int> recordedInput;
extern int recordingStartFrame;

extern bool playingBackInput;
extern std::deque<int> playBackInputBuffer;
extern int playBackFrame;

extern bool deleteInputs;

extern bool limitRate;

extern bool forceCounter;
extern bool forcePunishCounter;
extern int hitStunAdder;

extern uint32_t globalInputBufferLength;

std::string to_string_leading_zeroes(unsigned int number, unsigned int length);
nlohmann::json parse_json_file(const std::string &fileName);

bool doBoxesHit(Box box1, Box box2);
