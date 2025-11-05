#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <set>
#include <deque>
#include <chrono>
#include <random>

#include "guy.hpp"
#include "simulation.hpp"

struct ComboRoute {
    std::map<int, std::pair<int, int>> timelineTriggers;
    int comboHits = 0;
    int simFrameProgress = 0;
    int guyFrameProgress = 0;
    int damage = 0;
    int lastFrameDamage = 0;
    Simulation *pSimSnapshot = nullptr;
};

struct DoneRoute {
    std::map<int, std::pair<int, int>> timelineTriggers;
    int damage = 0;
};

struct DamageSort {
    bool operator()(DoneRoute const& lhs, DoneRoute const& rhs) const {
        if (lhs.damage == rhs.damage) {
            return lhs.timelineTriggers < rhs.timelineTriggers;
        }
        return lhs.damage < rhs.damage;
    }
};

class ComboWorker {
public:
    void Start(bool isFirst);
    void GetNextRoute(void);
    void QueueRouteFork(std::pair<int,int> frameTrigger);
    void WorkLoop(void);

    std::atomic<bool> idle;
    std::atomic<bool> kill;
    uint64_t framesProcessed = 0;
    bool first;
    std::vector<ComboWorker*> shuffledWorkerPool;
    ComboRoute currentRoute;
    Simulation *pSim = nullptr;
    std::thread thread;
    std::mutex mutexPendingRoutes;
    std::deque<ComboRoute> pendingRoutes;
    std::mutex mutexDoneRoutes;
    std::set<DoneRoute, DamageSort> doneRoutes;
};

class ComboFinder {
public:
    std::vector<ComboWorker*> workerPool;
    int threadCount = 0;
    bool running = false;
    std::chrono::time_point<std::chrono::steady_clock> start;

    uint64_t totalFrames = 0;
    int maxDamage = 0;
    std::set<DoneRoute, DamageSort> doneRoutes;

    std::default_random_engine rng;
};

extern ComboFinder finder;

void findCombos(void);
void updateComboFinder(void);