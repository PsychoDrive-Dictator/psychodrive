#pragma once

#include <cassert>
#include <cstring>
#include <string>
#include <deque>
#include "json.hpp"
#include "fixed.hpp"

template<typename T, std::size_t N, bool AssertOnOverflow = false>
class FixedBuffer {
private:
    T buffer[N];
    std::size_t count = 0;

public:
    inline void push_front(const T& value) {
        if constexpr (AssertOnOverflow) {
            assert(count < N);
        }
        std::memmove(&buffer[1], &buffer[0], (N - 1) * sizeof(T));
        buffer[0] = value;
        if (count < N) {
            count++;
        }
    }

    inline void push_back(const T& value) {
        if constexpr (AssertOnOverflow) {
            assert(count < N);
        }
        buffer[count] = value;
        if (count < N) {
            count++;
        }
    }

    inline void clear() {
        count = 0;
    }

    inline T& operator[](std::size_t index) {
        return buffer[index];
    }

    inline const T& operator[](std::size_t index) const {
        return buffer[index];
    }

    inline FixedBuffer& operator=(const FixedBuffer& other) {
        if (this != &other) {
            count = other.count;
            if (count > 0) {
                std::memcpy(buffer, other.buffer, count * sizeof(T));
            }
        }
        return *this;
    }

    inline std::size_t size() const {
        return count;
    }

    inline bool operator!=(const FixedBuffer& other) const {
        if (count != other.count) return true;
        return std::memcmp(buffer, other.buffer, count * sizeof(T)) != 0;
    }
};

template<typename T>
class ObjectPool {
private:
    std::vector<T*> allocated;
    std::vector<T*> available;
    std::vector<T*> blocks;
    std::size_t growSize;

public:
    ObjectPool(std::size_t initialSize) : growSize(initialSize) {
        grow();
    }

    ~ObjectPool() {
        for (auto block : blocks) {
            delete[] block;
        }
    }

    T* allocate() {
        if (available.empty()) {
            grow();
        }
        T* obj = available.back();
        available.pop_back();
        allocated.push_back(obj);
        return obj;
    }

    void release(T* obj) {
        auto it = std::find(allocated.begin(), allocated.end(), obj);
        if (it != allocated.end()) {
            allocated.erase(it);
            available.push_back(obj);
        }
    }

    void reset() {
        available.insert(available.end(), allocated.begin(), allocated.end());
        allocated.clear();
    }

private:
    void grow() {
        T* block = new T[growSize];
        blocks.push_back(block);
        for (std::size_t i = 0; i < growSize; i++) {
            available.push_back(&block[i]);
        }
    }
};

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
    unique = 8,
};

enum hitBoxFlags {
    overhead = 1,
    low = 2,
    avoids_standing = 4,
    avoids_crouching = 8,
    avoids_airborne = 16,
    only_hits_behind = 32,
    only_hits_front = 64,
    only_hits_in_combo = 128,
    multi_counter = 256,
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
    int hitID;
    int flags;
    struct HitData *pHitData = nullptr;
};

struct HurtBox {
    Box box;
    int flags = 0;
    struct AtemiData *pAtemiData = nullptr;
};

class Guy;
struct HitEntry;
struct AtemiData;
struct HitData;

struct PendingHit {
    Guy *pGuyHitting;
    Guy *pGuyGettingHit;
    HitBox hitBox;
    HurtBox hurtBox;
    HitEntry *pHitEntry;
    int hitEntryFlag;
    int hitDataID;
    bool blocked;
    bool parried;
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
int getCharIDFromName(const char *charName);

extern const char* charVersions[];
extern const int charVersionCount;

bool isCharLoaded(std::string charName);
void requestCharDownload(std::string charName);

extern float startPos1;
extern float startPos2;

class Simulation;
extern Simulation defaultSim;
extern std::vector<Guy *> &guys;

extern bool resetpos;

extern bool done;
extern bool paused;
extern bool oneframe;
extern int runUntilFrame;
extern bool lockCamera;
extern bool toggleRenderUI;

extern bool saveState;
extern bool restoreState;

extern bool comboFinderDoLights;
extern bool comboFinderDoLateCancels;
extern bool comboFinderDoWalk;
extern bool comboFinderDoKaras;
extern bool showComboFinder;
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

std::string to_string_leading_zeroes(unsigned int number, unsigned int length);
std::string readFile(const std::string &fileName);
nlohmann::json parse_json_file(const std::string &fileName);
nlohmann::json *loadCharFile(const std::string &charName, int version, const std::string &jsonName);
void createGuy(std::string charName, int charVersion, Fixed x, Fixed y, int startDir, color color);
