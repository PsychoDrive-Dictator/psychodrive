#ifdef __EMSCRIPTEN__
#include <emscripten/threading.h>
#endif

#include <map>
#include <set>
#include <algorithm>
#include <deque>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <locale>
#include <iostream>
#include <sstream>

#include "combogen.hpp"
#include "simulation.hpp"
#include "main.hpp"
#include "guy.hpp"

struct ComboRoute {
    std::map<int, std::pair<int, int>> timelineTriggers;
    int comboHits = 0;
    int simFrameProgress = 0;
    int guyFrameProgress = 0;
    int damage = 0;
    int lastFrameDamage = 0;
    Guy guys[2];
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

class ComboWorker;

std::vector<ComboWorker*> workerPool;

class ComboWorker {
public:
    void Start(bool isFirst) {
        idle = false;
        first = isFirst;

        pSim = new Simulation;
        pSim->CreateGuy(*guys[0]->getName(), guys[0]->getVersion(), guys[0]->getPosX(), guys[0]->getPosY(), guys[0]->getDirection(), guys[0]->getColor());
        pSim->CreateGuy(*guys[1]->getName(), guys[1]->getVersion(), guys[1]->getPosX(), guys[1]->getPosY(), guys[1]->getDirection(), guys[1]->getColor());
        *pSim->simGuys[0] = *guys[0];
        *pSim->simGuys[1] = *guys[1];
        // pSim->simGuys[0]->setSim(pSim);
        // pSim->simGuys[1]->setSim(pSim);
        // todo not really facsimiles.. don't leak movelist
        pSim->simGuys[0]->facSimile = true;
        pSim->simGuys[1]->facSimile = true;
        pSim->simGuys[0]->setOpponent(pSim->simGuys[1]);
        pSim->simGuys[1]->setOpponent(pSim->simGuys[0]);
        if (pSim->simGuys[1]->getAttacker() != nullptr) {
            pSim->simGuys[1]->setAttacker(pSim->simGuys[0]);
        }
        pSim->simGuys[0]->setRecordFrameTriggers(true);

        thread = std::thread(&ComboWorker::WorkLoop, this);
    }

    void GetNextRoute(void) {
        mutexPendingRoutes.lock();
        if (!pendingRoutes.size()) {
            mutexPendingRoutes.unlock();
    again:
            for (auto& worker : workerPool) {
                if (worker->mutexPendingRoutes.try_lock()) {
                    if (worker->pendingRoutes.size()) {
                        currentRoute = worker->pendingRoutes.front();
                        worker->pendingRoutes.pop_front();
                        worker->mutexPendingRoutes.unlock();
                        goto stolen;
                    }
                    worker->mutexPendingRoutes.unlock();
                }
            }
            idle = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (kill) {
                return;
            }
            goto again;
        }
        currentRoute = pendingRoutes.front();
        pendingRoutes.pop_front();
        mutexPendingRoutes.unlock();
    stolen:
        idle = false;

        *pSim->simGuys[0] = currentRoute.guys[0];
        *pSim->simGuys[1] = currentRoute.guys[1];
        // patch up pointers from potentially other sim
        pSim->simGuys[0]->setOpponent(pSim->simGuys[1]);
        pSim->simGuys[1]->setOpponent(pSim->simGuys[0]);
        if (pSim->simGuys[1]->getAttacker() != nullptr) {
            pSim->simGuys[1]->setAttacker(pSim->simGuys[0]);
        }
        pSim->frameCounter = currentRoute.simFrameProgress-1;
    }

    void WorkLoop(void) {
        if (!first) {
            GetNextRoute();
        }

        if (kill) {
            delete pSim;
            return;
        }

        while (true) {
            while (true) {
                currentRoute.guys[0] = *pSim->simGuys[0];
                currentRoute.guys[1] = *pSim->simGuys[1];

                auto &forcedTrigger = pSim->simGuys[0]->getForcedTrigger();
                if (currentRoute.timelineTriggers.find(pSim->frameCounter+1) != currentRoute.timelineTriggers.end()) {
                    forcedTrigger = currentRoute.timelineTriggers[pSim->frameCounter+1];
                    //pSim->Log(std::to_string(pSim->frameCounter+1) + " " + pSim->simGuys[0]->getActionName(forcedTrigger.first));
                }

                pSim->RunFrame();
                pSim->AdvanceFrame();
                framesProcessed++;

                if (currentRoute.guyFrameProgress < pSim->simGuys[0]->getCurrentFrame() &&
                    currentRoute.simFrameProgress < pSim->frameCounter) {
                    currentRoute.simFrameProgress = pSim->frameCounter;

                    bool doFrameTriggers = true;

                    // todo try to skip all karas for now, but should probably only skip on free movement
                    if (pSim->simGuys[0]->getCurrentFrame() <= 1 && !pSim->simGuys[0]->canAct()) {
                        doFrameTriggers = false;
                    }

                    if (pSim->simGuys[0]->getFrameTriggers().size() && doFrameTriggers) {
                        std::scoped_lock lockPendingRoutes(mutexPendingRoutes);
                        //for (auto frameTrigger = pSim->simGuys[0]->getFrameTriggers().rbegin(); frameTrigger != pSim->simGuys[0]->getFrameTriggers().rend(); ++frameTrigger) {
                        for (auto &frameTrigger : pSim->simGuys[0]->getFrameTriggers()) {
                            pendingRoutes.push_front(currentRoute);
                            pendingRoutes.front().timelineTriggers[pSim->frameCounter] = frameTrigger;
                        }
                    }
                    pSim->simGuys[0]->getFrameTriggers().clear();
                }

                if (pSim->simGuys[1]->getComboDamage() > currentRoute.damage) {
                    currentRoute.damage = pSim->simGuys[1]->getComboDamage();
                    currentRoute.lastFrameDamage = pSim->frameCounter;
                }

                if (pSim->frameCounter == 2000 || (pSim->simGuys[1]->getComboHits() < currentRoute.comboHits) || (pSim->simGuys[1]->getIsDown() && !pSim->simGuys[1]->getAirborne())) {
                    //pSim->Log("framecount " + std::to_string(pSim->frameCounter));
                    break;
                }

                currentRoute.guyFrameProgress = pSim->simGuys[0]->getCurrentFrame();
                currentRoute.comboHits = pSim->simGuys[1]->getComboHits();
            }

            int lastFrameDamage = currentRoute.lastFrameDamage;
            std::erase_if(currentRoute.timelineTriggers, [lastFrameDamage](const auto& item) {
                return item.first > lastFrameDamage;
            });

            DoneRoute doneRoute;
            doneRoute.timelineTriggers = currentRoute.timelineTriggers;
            doneRoute.damage = currentRoute.damage;
            // std::string logEntry = std::to_string(doneRoute.damage) + " damage: ";
            // for ( auto &trigger : doneRoute.timelineTriggers) {
            //     logEntry += std::to_string(trigger.first) + " " + guys[0]->getActionName(trigger.second.first) + " ";
            // }
            // log(logEntry);
            // fprintf(stderr, "%s\n", logEntry.c_str());
            mutexDoneRoutes.lock();
            doneRoutes.insert(doneRoute);
            mutexDoneRoutes.unlock();

            GetNextRoute();

            if (kill) {
                delete pSim;
                return;
            }
        }
    }
    std::atomic<bool> idle;
    std::atomic<bool> kill;
    uint64_t framesProcessed = 0;
    bool first;
    ComboRoute currentRoute;
    Simulation *pSim = nullptr;
    std::thread thread;
    std::mutex mutexPendingRoutes;
    std::deque<ComboRoute> pendingRoutes;
    std::mutex mutexDoneRoutes;
    std::set<DoneRoute, DamageSort> doneRoutes;
};

void printRoute(const DoneRoute &route)
{
    std::string logEntry = std::to_string(route.damage) + " damage: ";
    for ( auto &trigger : route.timelineTriggers) {
        logEntry += std::to_string(trigger.first) + " " + guys[0]->getActionName(trigger.second.first) + " ";
    }
    log(logEntry);
    fprintf(stderr, "%s\n", logEntry.c_str());
}

void findCombos(void)
{
    const auto start = std::chrono::steady_clock::now();

    int threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) {
        threadCount = 1;
    }
#ifdef __EMSCRIPTEN__
    threadCount = emscripten_num_logical_cores();
#endif
    for (int i = 0; i < threadCount; i++) {
        ComboWorker *pNewWorker = new ComboWorker;
        workerPool.push_back(pNewWorker);
    }

    bool first = true;
    for (auto worker : workerPool) {
        worker->Start(first);
        first = false;
    }


    uint64_t totalFrames = 0;
    int maxDamage = 0;
    std::set<DoneRoute, DamageSort> doneRoutes;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bool allIdle = true;
        for (auto worker : workerPool) {
            if (worker->mutexDoneRoutes.try_lock()) {
                std::set<DoneRoute, DamageSort> newDoneRoutes;
                std::swap(newDoneRoutes, worker->doneRoutes);
                worker->mutexDoneRoutes.unlock();
                // for (auto &route : newDoneRoutes) {
                //     printRoute(route);
                // }
                doneRoutes.merge(newDoneRoutes);
            }
            if (!worker->idle) {
                allIdle = false;
            }
        }
        if (doneRoutes.size() && doneRoutes.rbegin()->damage > maxDamage) {
            printRoute(*doneRoutes.rbegin());
            maxDamage = doneRoutes.rbegin()->damage;
        }
        if (allIdle) {
            break;
        }
    }

    for (auto worker : workerPool) {
        worker->kill = true;
    }

    for (auto worker : workerPool) {
        worker->thread.join();
        totalFrames += worker->framesProcessed;
        doneRoutes.merge(worker->doneRoutes);
        delete worker;
    }
    workerPool.clear();

    // for (auto &route : doneRoutes) {
    //     std::string logEntry = std::to_string(route.damage) + " damage: ";
    //     for ( auto &trigger : route.timelineTriggers) {
    //         logEntry += std::to_string(trigger.first) + " " + guys[0]->getActionName(trigger.second.first) + " ";
    //     }
    //     log(logEntry);
    //     fprintf(stderr, "%s\n", logEntry.c_str());
    // }

    std::stringstream formattedTotalFrames;
    std::stringstream formattedFPS;

    struct my_numpunct : std::numpunct<char> {
        std::string do_grouping() const {return "\03";}
    };
    std::locale loc (std::cout.getloc(),new my_numpunct);

    formattedTotalFrames.imbue(loc);
    formattedFPS.imbue(loc);

    formattedTotalFrames << totalFrames;


    const auto end = std::chrono::steady_clock::now();
    float seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f;
    uint64_t framesPerSeconds = totalFrames / seconds;
    formattedFPS << framesPerSeconds;

    auto logEntry = "processed " + formattedTotalFrames.str() + " frames in " + std::to_string(seconds) + "s (";
    logEntry += formattedFPS.str() + " fps)";
    log(logEntry);
    fprintf(stderr, "%s\n", logEntry.c_str());
}