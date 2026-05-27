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

struct SharedSimulationSnapshot {
    std::atomic<int> refcount = 0;
    Simulation sim;
};

struct ComboRoute {
    std::map<int16_t, ActionRef> timelineTriggers;
    int comboHits = 0;
    int simFrameProgress = 0;
    int guyFrameProgress = 0;
    int damage = 0;
    int focusGain = 0;
    int gaugeGain = 0;
    int focusDmg = 0;
    int focusSpend = 0;
    int gaugeSpend = 0;
    int lastFrameDamage = 0;
    int walkForward = 0;
    SharedSimulationSnapshot *pSimSnapshot = nullptr;
};

struct DoneRoute {
    std::map<int16_t, ActionRef> timelineTriggers;
    int damage = 0;
    int focusGain = 0;
    int gaugeGain = 0;
    int focusDmg = 0;
    int focusSpend = 0;
    int gaugeSpend = 0;
    int advantage = 0;
    bool sideSwitch = false;
    bool impossibleInput = false;
};

struct DamageSort {
    // this is also our index key for dedupe
    bool operator()(const std::unique_ptr<DoneRoute>& lhs, const std::unique_ptr<DoneRoute>& rhs) const {
        if (lhs->damage != rhs->damage) {
            return lhs->damage < rhs->damage;
        } else if (lhs->focusGain != rhs->focusGain) {
            return lhs->focusGain < rhs->focusGain;
        } else if (lhs->gaugeGain != rhs->gaugeGain) {
            return lhs->gaugeGain < rhs->gaugeGain;
        } else if (lhs->focusDmg != rhs->focusDmg) {
            return lhs->focusDmg < rhs->focusDmg;
        } else if (lhs->sideSwitch != rhs->sideSwitch) {
            return lhs->sideSwitch < rhs->sideSwitch;
        } else if (lhs->impossibleInput != rhs->impossibleInput) {
            return lhs->impossibleInput < rhs->impossibleInput;
        }
        return false;
    }
};

struct DoneRoutePtrDamageSort {
    bool operator()(const DoneRoute *lhs, const DoneRoute *rhs) const {
        if (lhs->damage != rhs->damage) {
            return lhs->damage < rhs->damage;
        } else if (lhs->focusGain != rhs->focusGain) {
            return lhs->focusGain < rhs->focusGain;
        } else if (lhs->gaugeGain != rhs->gaugeGain) {
            return lhs->gaugeGain < rhs->gaugeGain;
        } else if (lhs->focusDmg != rhs->focusDmg) {
            return lhs->focusDmg < rhs->focusDmg;
        } else if (lhs->sideSwitch != rhs->sideSwitch) {
            return lhs->sideSwitch < rhs->sideSwitch;
        } else if (lhs->impossibleInput != rhs->impossibleInput) {
            return lhs->impossibleInput < rhs->impossibleInput;
        }
        return false;
    }
};

struct FocusGainSort {
    bool operator()(DoneRoute* lhs, DoneRoute* rhs) const {
        if (lhs->focusGain != rhs->focusGain) {
            return lhs->focusGain < rhs->focusGain;
        } else if (lhs->damage != rhs->damage) {
            return lhs->damage < rhs->damage;
        } else if (lhs->gaugeGain != rhs->gaugeGain) {
            return lhs->gaugeGain < rhs->gaugeGain;
        } else if (lhs->focusDmg != rhs->focusDmg) {
            return lhs->focusDmg < rhs->focusDmg;
        } else if (lhs->sideSwitch != rhs->sideSwitch) {
            return lhs->sideSwitch < rhs->sideSwitch;
        } else if (lhs->impossibleInput != rhs->impossibleInput) {
            return lhs->impossibleInput < rhs->impossibleInput;
        }
        return false;
    }
};

struct GaugeGainSort {
    bool operator()(DoneRoute* lhs, DoneRoute* rhs) const {
        if (lhs->gaugeGain != rhs->gaugeGain) {
            return lhs->gaugeGain < rhs->gaugeGain;
        } else if (lhs->damage != rhs->damage) {
            return lhs->damage < rhs->damage;
        } else if (lhs->focusGain != rhs->focusGain) {
            return lhs->focusGain < rhs->focusGain;
        } else if (lhs->focusDmg != rhs->focusDmg) {
            return lhs->focusDmg < rhs->focusDmg;
        } else if (lhs->sideSwitch != rhs->sideSwitch) {
            return lhs->sideSwitch < rhs->sideSwitch;
        } else if (lhs->impossibleInput != rhs->impossibleInput) {
            return lhs->impossibleInput < rhs->impossibleInput;
        }
        return false;
    }
};

struct FocusDmgSort {
    bool operator()(DoneRoute* lhs, DoneRoute* rhs) const {
        if (lhs->focusDmg != rhs->focusDmg) {
            return lhs->focusDmg < rhs->focusDmg;
        } else if (lhs->damage != rhs->damage) {
            return lhs->damage < rhs->damage;
        } else if (lhs->focusGain != rhs->focusGain) {
            return lhs->focusGain < rhs->focusGain;
        } else if (lhs->gaugeGain != rhs->gaugeGain) {
            return lhs->gaugeGain < rhs->gaugeGain;
        } else if (lhs->sideSwitch != rhs->sideSwitch) {
            return lhs->sideSwitch < rhs->sideSwitch;
        } else if (lhs->impossibleInput != rhs->impossibleInput) {
            return lhs->impossibleInput < rhs->impossibleInput;
        }
        return false;
    }
};

class ComboFinder;

class ComboWorker {
public:
    void Start(bool isFirst);
    void GetNextRoute(void);
    void QueueRouteFork(ActionRef frameTrigger);
    void WorkLoop(void);

    ComboFinder *pFinder = nullptr;
    std::atomic<bool> idle;
    std::atomic<bool> kill;
    std::atomic<uint64_t> framesProcessed = 0;
    bool first;
    std::vector<ComboWorker*> shuffledWorkerPool;
    ComboRoute currentRoute;
    Simulation *pSim = nullptr;
    SharedSimulationSnapshot *pendingSnapshot = nullptr;
    std::thread thread;
    std::mutex mutexPendingRoutes;
    std::deque<ComboRoute> pendingRoutes;
    std::mutex mutexDoneRoutes;
    std::set<std::unique_ptr<DoneRoute>, DamageSort> doneRoutes;
    bool justGotNextRoute = false;
    std::atomic<bool> wantsRenderSnapshot;
    std::mutex mutexRenderSnapshot;
    Simulation simRenderSnapshot;
};

class ComboFinder {
public:
    void Start(Simulation *pStartSim);
    void Update(void);
    void Render(void);
    void Stop(void);
    bool filterIsActive(void) const;

    std::vector<ComboWorker*> workerPool;
    int threadCount = 0;
    bool running = false;
    std::chrono::time_point<std::chrono::steady_clock> start;
    bool doLights = false;
    bool doLateCancels = false;
    bool doWalk = false;
    bool doKaras = false;
    bool stopOnRecovery = false;
    std::set<int> lightsActionIDs;
    std::set<int> triggerGroupZeroActionIDs;

    uint64_t totalFrames = 0;
    int maxDamage = 0;
    std::set<std::unique_ptr<DoneRoute>, DamageSort> doneRoutes;
    std::set<DoneRoute *, FocusGainSort> doneRoutesByFocusGain;
    std::set<DoneRoute *, GaugeGainSort> doneRoutesByGaugeGain;
    std::set<DoneRoute *, FocusDmgSort> doneRoutesByFocusDmg;

    int filterFocusBars = 6;
    int filterGaugeBars = 3;
    bool filterSideSwitchOnly = false;
    bool filterImpossibleOnly = false;
    int filterAdvantage = 0;
    bool doFilterAdvantage = false;
    bool filterAdvantageExact = false;

    int minAdvantage = 0;
    int maxAdvantage = 0;
    bool sawImpossible = false;
    bool filterDirty = false;
    std::set<DoneRoute *, DoneRoutePtrDamageSort> filteredByDamage;
    std::set<DoneRoute *, FocusGainSort> filteredByFocusGain;
    std::set<DoneRoute *, GaugeGainSort> filteredByGaugeGain;
    std::set<DoneRoute *, FocusDmgSort> filteredByFocusDmg;

    std::default_random_engine rng;

    std::chrono::time_point<std::chrono::steady_clock> lastFPSUpdate;
    uint64_t lastFrameCount = 0;
    uint64_t currentFPS = 0;
    uint64_t finalFPS = 0;

    std::deque<DoneRoute> recentRoutes;
    static constexpr int maxRecentRoutes = 10;

    Simulation startSnapshot;
    int startRecoveryTiming;

    bool playing = false;
    int playingRoute = 0;

    bool newBestPending = false;
    bool stoppedPending = false;
};

std::string timelineTriggerToString(ActionRef trigger, Guy *pGuy);
std::string routeToString(const ComboRoute &route, Guy *pGuy);
std::string routeToString(const DoneRoute &route, Guy *pGuy);
std::string formatWithCommas(uint64_t value);
