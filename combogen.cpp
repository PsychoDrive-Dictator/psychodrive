#ifdef __EMSCRIPTEN__
#include <emscripten/threading.h>
#endif

#include <map>
#include <algorithm>
#include <locale>
#include <iostream>
#include <sstream>

#include "combogen.hpp"
#include "main.hpp"

ComboFinder finder;

void ComboWorker::Start(bool isFirst) {
    idle = false;
    first = isFirst;

    shuffledWorkerPool = finder.workerPool;
    std::shuffle(shuffledWorkerPool.begin(),shuffledWorkerPool.end(), finder.rng);

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

void ComboWorker::GetNextRoute(void) {
    mutexPendingRoutes.lock();
    if (!pendingRoutes.size()) {
        mutexPendingRoutes.unlock();
again:
        for (auto& worker : shuffledWorkerPool) {
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

void ComboWorker::WorkLoop(void) {
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
    if (finder.running) {
        // ?
        return;
    }
    finder.start = std::chrono::steady_clock::now();

    finder.threadCount = std::thread::hardware_concurrency();
    if (finder.threadCount == 0) {
        finder.threadCount = 1;
    }
#ifdef __EMSCRIPTEN__
    finder.threadCount = emscripten_num_logical_cores()-1;
    // this seems to scale upwards of that, but browsers run out of memory way quicker right now
    if (finder.threadCount > 32) {
        finder.threadCount = 32;
    }
#endif

    for (int i = 0; i < finder.threadCount; i++) {
        ComboWorker *pNewWorker = new ComboWorker;
        finder.workerPool.push_back(pNewWorker);
    }

    bool first = true;
    for (auto worker : finder.workerPool) {
        worker->Start(first);
        first = false;
    }

    log("starting on " + std::to_string(finder.threadCount) + " threads");

    finder.running = true;
}

void updateComboFinder(void)
{
    if (!finder.running) {
        return;
    }

    bool allIdle = true;
    for (auto worker : finder.workerPool) {
        if (worker->mutexDoneRoutes.try_lock()) {
            std::set<DoneRoute, DamageSort> newDoneRoutes;
            std::swap(newDoneRoutes, worker->doneRoutes);
            worker->mutexDoneRoutes.unlock();
            // for (auto &route : newDoneRoutes) {
            //     printRoute(route);
            // }
            finder.doneRoutes.merge(newDoneRoutes);
        }
        if (!worker->idle) {
            allIdle = false;
        }
    }
    if (finder.doneRoutes.size() && finder.doneRoutes.rbegin()->damage > finder.maxDamage) {
        printRoute(*finder.doneRoutes.rbegin());
        finder.maxDamage = finder.doneRoutes.rbegin()->damage;
    }
    if (allIdle) {
        for (auto worker : finder.workerPool) {
            worker->kill = true;
        }

        for (auto worker : finder.workerPool) {
            worker->thread.join();
            finder.totalFrames += worker->framesProcessed;
            finder.doneRoutes.merge(worker->doneRoutes);
            delete worker;
        }
        finder.workerPool.clear();

        std::stringstream formattedTotalFrames;
        std::stringstream formattedFPS;

        struct my_numpunct : std::numpunct<char> {
            std::string do_grouping() const {return "\03";}
        };
        std::locale loc (std::cout.getloc(),new my_numpunct);

        formattedTotalFrames.imbue(loc);
        formattedFPS.imbue(loc);

        formattedTotalFrames << finder.totalFrames;

        const auto end = std::chrono::steady_clock::now();
        float seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - finder.start).count() / 1000.0f;
        uint64_t framesPerSeconds = finder.totalFrames / seconds;
        formattedFPS << framesPerSeconds;

        auto logEntry = "processed " + formattedTotalFrames.str() + " frames in " + std::to_string(seconds) + "s (";
        logEntry += formattedFPS.str() + " fps)";
        log(logEntry);

        finder.running = false;
        finder.totalFrames = 0;
        finder.maxDamage = 0;
        finder.doneRoutes.clear();
    }
}
