#pragma once

#include <string>
#include <deque>
#include "json.hpp"
#include "fixed.hpp"

enum EGameMode {
    Batch = -1,
    Training = 0,
    MoveViewer,
    ComboMaker
};

enum hitentryflags {
    crouch = 1,
    air = 2,
    special = 4, // todo this is also burnout
    counter = 8,
    block = 16,
    otg = crouch|air,
    punish_counter = special|counter,
};

struct Box {
    Fixed x;
    Fixed y;
    Fixed w;
    Fixed h;
};

enum hitBoxType {
    hit = 1,
    grab = 2,
    projectile = 3,
    domain = 4,
    destroy_projectile = 5,
    proximity_guard = 6,
    direct_damage = 7,
};

enum hitBoxFlags {
    overhead = 1,
    low = 2,
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
    int flags;
};

struct HurtBox {
    Box box;
    int flags = 0;
    int armorID = 0;
};

class Guy;

struct PendingHit {
    Guy *pGuyHitting;
    Guy *pGuyGettingHit;
    HitBox hitBox;
    HurtBox hurtBox;
    nlohmann::json *pHitEntry;
    int hitEntryID;
    int hitEntryFlag;
    bool blocked;
    bool bombBurst;
};

struct color {
    float r;
    float g;
    float b;
};

static inline float randFloat()
{
    float ret = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    return ret;
}

void log(std::string logLine);

class Guy;

extern EGameMode gameMode;

extern std::vector<const char *> charNames;
extern std::vector<const char *> charNiceNames;

const char *getCharNameFromID(int charID);

extern const char* charVersions[];
extern const int charVersionCount;

bool isCharLoaded(std::string charName);
void requestCharDownload(std::string charName);

extern float startPos1;
extern float startPos2;

extern std::vector<Guy *> guys;

extern bool resetpos;

extern bool done;
extern bool paused;
extern bool oneframe;
extern int globalFrameCount;
extern int replayFrameNumber;
extern bool lockCamera;
extern bool toggleRenderUI;

extern bool runComboFinder;
extern bool recordingInput;
extern std::vector<int> recordedInput;
extern int recordingStartFrame;

extern bool playingBackInput;
extern std::deque<int> playBackInputBuffer;
extern int playBackFrame;

extern std::vector<float> hitBoxRangePlotX;
extern std::vector<float> hitBoxRangePlotY;

struct normalRangePlotEntry {
    std::vector<float> hitBoxRangePlotX;
    std::vector<float> hitBoxRangePlotY;
    std::string strName;
    color col;
};

extern std::vector<normalRangePlotEntry> vecPlotEntries;

extern bool deleteInputs;

extern bool limitRate;

extern bool forceCounter;
extern bool forcePunishCounter;
extern int hitStunAdder;

std::string to_string_leading_zeroes(unsigned int number, unsigned int length);
std::string readFile(const std::string &fileName);
nlohmann::json parse_json_file(const std::string &fileName);
nlohmann::json *loadCharFile(const std::string &charName, int version, const std::string &jsonName);
void createGuy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color);

bool doBoxesHit(Box box1, Box box2);
