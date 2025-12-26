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
    wantsRenderSnapshot = false;

    shuffledWorkerPool = finder.workerPool;
    std::shuffle(shuffledWorkerPool.begin(),shuffledWorkerPool.end(), finder.rng);

    pSim = new Simulation;
    if (first) {
        pSim->Clone(&defaultSim);
        pSim->simGuys[0]->setRecordFrameTriggers(true, finder.doLateCancels);

        currentRoute.pSimSnapshot = new Simulation;
    }

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

    pSim->Clone(currentRoute.pSimSnapshot);
    pSim->frameCounter = currentRoute.simFrameProgress-1;
    justGotNextRoute = true;
}

void ComboWorker::QueueRouteFork(ActionRef frameTrigger) {
    // call this with mutexPendingRoutes locked!
    pendingRoutes.emplace_front();
    pendingRoutes.front() = currentRoute;
    pendingRoutes.front().pSimSnapshot = new Simulation;
    pendingRoutes.front().pSimSnapshot->Clone(currentRoute.pSimSnapshot);
    pendingRoutes.front().timelineTriggers[pSim->frameCounter] = frameTrigger;
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
            if (!justGotNextRoute) {
                currentRoute.pSimSnapshot->Clone(pSim);
            }
            justGotNextRoute = false;

            int curInput = 0;

            if (currentRoute.walkForward) {
                currentRoute.walkForward++;
            }

            auto &forcedTrigger = pSim->simGuys[0]->getForcedTrigger();
            auto frameTrigger = currentRoute.timelineTriggers.find(pSim->frameCounter+1);
            if (frameTrigger != currentRoute.timelineTriggers.end()) {
                if (frameTrigger->second.actionID() > 0) {
                    forcedTrigger = frameTrigger->second;
                    // rearm forward walk if we did a move
                    currentRoute.walkForward = 0;
                } else {
                    curInput = -frameTrigger->second.actionID();
                    if (curInput == FORWARD) {
                        currentRoute.walkForward = 1;
                    }
                    // it's not actually input, it's direction agnostic stuff
                    // input gets inverted, so pre-invert :/
                    if (pSim->simGuys[0]->getDirection() < 0) {
                        curInput = invertDirection(curInput);
                    }
                }
                //pSim->Log(std::to_string(pSim->frameCounter+1) + " " + pSim->simGuys[0]->getActionName(forcedTrigger.actionID()));
            }

            pSim->simGuys[0]->Input(curInput);

            pSim->RunFrame();

            if (wantsRenderSnapshot) {
                wantsRenderSnapshot = false;
                std::scoped_lock lockRenderSnapshot(mutexRenderSnapshot);
                simRenderSnapshot.Clone(pSim);
            }
            pSim->AdvanceFrame();
            framesProcessed++;

            if (!pSim->simGuys[0]->getHitStop() &&
                currentRoute.simFrameProgress < pSim->frameCounter) {
                currentRoute.simFrameProgress = pSim->frameCounter;

                bool doFrameTriggers = true;

                // todo try to skip all karas for now, but should probably only skip on free movement
                if (!finder.doKaras && pSim->simGuys[0]->getCurrentFrame() <= 1 && !pSim->simGuys[0]->canAct()) {
                    doFrameTriggers = false;
                }

                if ((pSim->simGuys[0]->getFrameTriggers().size() && doFrameTriggers) || pSim->simGuys[0]->canAct()) {
                    std::scoped_lock lockPendingRoutes(mutexPendingRoutes);
                    for (auto &frameTrigger : pSim->simGuys[0]->getFrameTriggers()) {
                        if (finder.doLights || (finder.lightsActionIDs.find(frameTrigger.actionID()) == finder.lightsActionIDs.end())) {
                            QueueRouteFork(frameTrigger);
                        }
                    }
                    if (pSim->simGuys[0]->canAct()) {
                        if (finder.doWalk && (currentRoute.walkForward == 0 || currentRoute.walkForward == 2)) {
                            if (pSim->simGuys[0]->getFocus() < 60000 && pSim->simGuys[0]->getFocus()) {
                                QueueRouteFork(ActionRef(-FORWARD, 0));
                            }
                        }
                        // QueueRouteFork(ActionRef(-BACK, 0));
                        QueueRouteFork(ActionRef(-(UP|FORWARD), 0));
                        QueueRouteFork(ActionRef(-(UP|BACK), 0));
                    }
                }
                pSim->simGuys[0]->getFrameTriggers().clear();
            }

            if (pSim->simGuys[1]->getComboDamage() > currentRoute.damage) {
                currentRoute.damage = pSim->simGuys[1]->getComboDamage();
                currentRoute.lastFrameDamage = pSim->frameCounter;
            }

            if (pSim->frameCounter == 3000) {
                fprintf(stderr, "aaa\n");
                break;
            }

            if ((pSim->simGuys[1]->getComboHits() < currentRoute.comboHits) || (pSim->simGuys[1]->getIsDown() && !pSim->simGuys[1]->getAirborne())) {
                break;
            }

            //currentRoute.guyFrameProgress = pSim->simGuys[0]->getCurrentFrame();
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

        delete currentRoute.pSimSnapshot;
        currentRoute.pSimSnapshot = nullptr;

        GetNextRoute();

        if (kill) {
            delete pSim;
            return;
        }
    }
}

std::string formatWithCommas(uint64_t value)
{
    std::stringstream formatted;
    struct my_numpunct : std::numpunct<char> {
        std::string do_grouping() const {return "\03";}
    };
    std::locale loc(std::cout.getloc(), new my_numpunct);
    formatted.imbue(loc);
    formatted << value;
    return formatted.str();
}

uint64_t calculateAverageFPS(void)
{
    const auto end = std::chrono::steady_clock::now();
    float seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - finder.start).count() / 1000.0f;
    return seconds > 0 ? finder.totalFrames / seconds : 0;
}

std::string routeToString(const DoneRoute &route, Guy *pGuy)
{
    std::string result = std::to_string(route.damage) + ": ";
    for ( auto &trigger : route.timelineTriggers) {
        std::string actionDesc;
        if (trigger.second.actionID() < 0) {
            renderInput(actionDesc, -trigger.second.actionID());
        } else {
            actionDesc = pGuy->getActionName(trigger.second.actionID());
        }
        result += std::to_string(trigger.first) + " " + actionDesc + " ";
    }
    return result;
}

void printRoute(const DoneRoute &route)
{
    std::string logEntry = routeToString(route, guys[0]);
    log(logEntry);
    fprintf(stderr, "%s\n", logEntry.c_str());
}

void findCombos(bool doLights = false, bool doLateCancels = false, bool doWalk = false, bool doKaras = false)
{
    if (finder.running) {
        // ?
        return;
    }

    finder.startSnapshot.Clone(&defaultSim);
    finder.startFrame = defaultSim.frameCounter;

    finder.doLights = doLights;
    finder.doLateCancels = doLateCancels;
    finder.doWalk = doWalk;
    finder.doKaras = doKaras;

    if (!finder.doLights) {
        for (auto& [key, action] : defaultSim.simGuys[0]->getCharData()->actionsByID) {
            if (action->name.find("5LP") != std::string::npos ||
                action->name.find("5LK") != std::string::npos ||
                action->name.find("2LP") != std::string::npos ||
                action->name.find("2LK") != std::string::npos)
            {
                finder.lightsActionIDs.insert(key.actionID());
            }
        }
    }

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

    finder.start = std::chrono::steady_clock::now();
    finder.lastFPSUpdate = finder.start;
    finder.lastFrameCount = 0;
    finder.currentFPS = 0;
    finder.totalFrames = 0;
    finder.maxDamage = 0;
    finder.doneRoutes.clear();
    finder.recentRoutes.clear();
    log("starting on " + std::to_string(finder.threadCount) + " threads");

    if (gameMode == Training && !paused) {
        paused = true;
    }

    finder.running = true;
}

void updateComboFinder(void)
{
    if (!finder.running) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    float timeSinceLastFPSUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - finder.lastFPSUpdate).count() / 1000.0f;

    if (timeSinceLastFPSUpdate >= 1.0f) {
        uint64_t currentTotalFrames = 0;
        for (auto worker : finder.workerPool) {
            currentTotalFrames += worker->framesProcessed;
        }
        uint64_t framesDelta = currentTotalFrames - finder.lastFrameCount;
        finder.currentFPS = framesDelta / timeSinceLastFPSUpdate;
        finder.lastFrameCount = currentTotalFrames;
        finder.lastFPSUpdate = now;
    }

    bool allIdle = true;
    for (auto worker : finder.workerPool) {
        if (worker->mutexDoneRoutes.try_lock()) {
            std::set<DoneRoute, DamageSort> newDoneRoutes;
            std::swap(newDoneRoutes, worker->doneRoutes);

            // Grab last N routes for display
            if (newDoneRoutes.size() > 0) {
                int skip = std::max(0, (int)newDoneRoutes.size() - finder.maxRecentRoutes);
                auto it = newDoneRoutes.begin();
                std::advance(it, skip);
                finder.recentRoutes.assign(it, newDoneRoutes.end());
            }

            worker->mutexDoneRoutes.unlock();
            for (auto &route : newDoneRoutes) {
                if (route.damage > finder.maxDamage) {
                    printRoute(route);
                    finder.maxDamage = route.damage;
                    defaultSim.Clone(&finder.startSnapshot);
                    defaultSim.frameCounter = finder.startFrame;
                    finder.playing = true;
                    paused = false;
                }
            }
            finder.doneRoutes.merge(newDoneRoutes);
        }
        if (!worker->idle) {
            allIdle = false;
        }
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

        finder.finalFPS = framesPerSeconds;
        finder.running = false;

        if (gameMode == Training) {
            paused = false;
            finder.playing = true;
            defaultSim.Clone(&finder.startSnapshot);
            defaultSim.frameCounter = finder.startFrame;
        }
    }
}

void renderComboFinder(void)
{
    if (!finder.running) {
        return;
    }

    int i = finder.workerPool.size();
    for (auto worker : finder.workerPool) {
        std::scoped_lock lockWorkerRenderSnapshot(worker->mutexRenderSnapshot);
        worker->simRenderSnapshot.Render((i--) * -20.0f);
        worker->wantsRenderSnapshot = true;
    }
}