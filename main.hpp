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

enum hurtBoxFlags {
    full_strike_invul = 1,
    projectile_invul = 2,
    air_strike_invul = 4,
    ground_strike_invul = 8,
    armor = 16,
    atemi = 32,
    head = 64,
    body = 128,
    legs = 256,
};

struct HitBox {
    Box box;
    hitBoxType type;
    int hitEntryID;
    int hitID;
};

struct HurtBox {
    Box box;
    int flags = 0;
    int armorID = 0;
};

static inline float randFloat()
{
    float ret = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    return ret;
}

void log(std::string logLine);

class Guy;

extern const char* charNames[];
extern const int charNameCount;

extern float startPos1;
extern float startPos2;

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
std::string readFile(const std::string &fileName);

bool doBoxesHit(Box box1, Box box2);
