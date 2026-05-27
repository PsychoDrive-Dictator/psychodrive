#ifdef __EMSCRIPTEN__
#include <emscripten/threading.h>
#endif

#include <map>
#include <algorithm>
#include <climits>
#include <locale>
#include <iostream>
#include <sstream>

#include "comboutils.hpp"
#include "combogen.hpp"
#include "main.hpp"
#include "ui.hpp"

void ComboWorker::Start(bool isFirst) {
    idle = false;
    first = isFirst;
    wantsRenderSnapshot = false;

    shuffledWorkerPool = pFinder->workerPool;
    std::shuffle(shuffledWorkerPool.begin(),shuffledWorkerPool.end(), pFinder->rng);

    pSim = new Simulation;
    if (first) {
        pSim->Clone(&pFinder->startSnapshot);
        pSim->simGuys[0]->setRecordFrameTriggers(true, pFinder->doLateCancels);
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

    pSim->Clone(&currentRoute.pSimSnapshot->sim);
    if (--currentRoute.pSimSnapshot->refcount == 0) {
        delete currentRoute.pSimSnapshot;
    }
    currentRoute.pSimSnapshot = nullptr;
    pSim->frameCounter = currentRoute.simFrameProgress-1;
    justGotNextRoute = true;
}

void ComboWorker::QueueRouteFork(ActionRef frameTrigger) {
    // call this with mutexPendingRoutes locked!
    pendingSnapshot->refcount++;
    pendingRoutes.emplace_front();
    pendingRoutes.front() = currentRoute;
    pendingRoutes.front().pSimSnapshot = pendingSnapshot;
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
            if (pendingSnapshot == nullptr) {
                pendingSnapshot = new SharedSimulationSnapshot;
            }
            pendingSnapshot->sim.Clone(pSim);
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

            if (pFinder->stopOnRecovery && pFinder->startSnapshot.simGuys[0]->getRecoveryTiming() != pSim->simGuys[0]->getRecoveryTiming()) {
                break;
            }

            if (!pSim->simGuys[0]->getHitStop() &&
                currentRoute.simFrameProgress < pSim->frameCounter) {
                currentRoute.simFrameProgress = pSim->frameCounter;

                bool doActualFrameTriggers = true;
                bool hasAnyFrameTriggers = pSim->simGuys[0]->getFrameTriggers().size();

                // todo try to skip all karas for now, but should probably only skip on free movement
                if (!pFinder->doKaras && pSim->simGuys[0]->getCurrentFrame() <= 1 && !pSim->simGuys[0]->canAct() && !pSim->simGuys[0]->getFreeMovement()) {
                    doActualFrameTriggers = false;
                    hasAnyFrameTriggers = false;
                    for (auto &frameTrigger : pSim->simGuys[0]->getFrameTriggers()) {
                        if (frameTrigger.actionID() < 0) {
                            hasAnyFrameTriggers = true;
                            break;
                        }
                    }
                }

                if (hasAnyFrameTriggers || pSim->simGuys[0]->canAct()) {
                    std::scoped_lock lockPendingRoutes(mutexPendingRoutes);
                    for (auto &frameTrigger : pSim->simGuys[0]->getFrameTriggers()) {
                        bool doThisTrigger = true;
                        if (!pFinder->doLights && (pFinder->lightsActionIDs.find(frameTrigger.actionID()) != pFinder->lightsActionIDs.end())) {
                            doThisTrigger = false;
                        }
                        if (pFinder->stopOnRecovery) {
                            // skip any neutral moves past the first one
                            if (currentRoute.timelineTriggers.size() && (pFinder->triggerGroupZeroActionIDs.find(frameTrigger.actionID()) != pFinder->triggerGroupZeroActionIDs.end())) {
                                doThisTrigger = false;
                            }
                            // don't repeat any moves
                            for (auto & trigger : currentRoute.timelineTriggers) {
                                if (trigger.second.actionID() == frameTrigger.actionID()) {
                                    doThisTrigger = false;
                                }
                            }
                        }
                        if (doThisTrigger && (frameTrigger.actionID() < 0 || doActualFrameTriggers)) {
                            QueueRouteFork(frameTrigger);
                        }
                    }
                    if (pSim->simGuys[0]->canAct() && !pFinder->stopOnRecovery) {
                        if (pFinder->doWalk && (currentRoute.walkForward == 0 || currentRoute.walkForward == 2)) {
                            if (pSim->simGuys[0]->getFocus() < 60000 && pSim->simGuys[0]->getFocus()) {
                                QueueRouteFork(ActionRef(-FORWARD, 0));
                            }
                        }
                        // QueueRouteFork(ActionRef(-BACK, 0));
                        QueueRouteFork(ActionRef(-(UP|FORWARD), 0));
                        QueueRouteFork(ActionRef(-(UP|BACK), 0));
                    }
                    if (pSim->simGuys[0]->canAct() && pFinder->stopOnRecovery) {
                        // neutral jump so air normals can hit, for now
                        QueueRouteFork(ActionRef(-(UP), 0));
                    }
                    // do this with lock held lest someone free it under us already
                    if (pendingSnapshot->refcount) {
                        // we jettison, they own it now
                        pendingSnapshot = nullptr;
                    }
                }
                pSim->simGuys[0]->getFrameTriggers().clear();
            }

            if (pSim->simGuys[1]->getComboDamage() > currentRoute.damage) {
                currentRoute.damage = pSim->simGuys[1]->getComboDamage();
                currentRoute.lastFrameDamage = pSim->frameCounter;
            }

            if (pSim->comboProbe.focusGain > currentRoute.focusGain) {
                currentRoute.focusGain = pSim->comboProbe.focusGain;
            }
            if (pSim->comboProbe.focusDmg > currentRoute.focusDmg) {
                currentRoute.focusDmg = pSim->comboProbe.focusDmg;
            }
            if (pSim->comboProbe.gaugeGain > currentRoute.gaugeGain) {
                currentRoute.gaugeGain = pSim->comboProbe.gaugeGain;
            }
            if (pSim->comboProbe.focusSpend > currentRoute.focusSpend) {
                currentRoute.focusSpend = pSim->comboProbe.focusSpend;
            }
            if (pSim->comboProbe.gaugeSpend > currentRoute.gaugeSpend) {
                currentRoute.gaugeSpend = pSim->comboProbe.gaugeSpend;
            }

            if (pSim->frameCounter - pFinder->startSnapshot.frameCounter >= 10000) {
                fprintf(stderr, "aaa %s\n", routeToString(currentRoute, pSim->simGuys[0]).c_str());
                break;
            }

            // reinstate this when we've saved whether opponent could act on search start
            // if (pSim->simGuys[1]->getComboHits() == 0 && pSim->simGuys[1]->canAct()) {
            //     break;
            // }

            // combo count has reset, end the search
            if (pSim->simGuys[1]->getComboHits() < currentRoute.comboHits || pSim->simGuys[1]->getRecoveryTiming() != pFinder->startRecoveryTiming ) {
                break;
            }

            // opponent knocked down, end the search
            if (pSim->simGuys[1]->getIsDown() && !pSim->simGuys[1]->getAirborne()) {
                break;
            }

            if (pSim->simGuys[1]->getCurrentAction() == 353 && pSim->simGuys[1]->getCurrentFrame() > 160) {
                // they're in the stunned script and already on the ground not able to get hit
                break;
            }

            if (pFinder->stopOnRecovery && !currentRoute.timelineTriggers.size()) {
                // we're done firing all the initial triggers and we may die now - no use trying delays
                break;
            }

            // no hit in the game can get them at this point?
            // if (pSim->simGuys[1]->getJuggleCounter() > 124) {
            //     break;
            // }

            //currentRoute.guyFrameProgress = pSim->simGuys[0]->getCurrentFrame();
            currentRoute.comboHits = pSim->simGuys[1]->getComboHits();
        }

        bool addRoute = true;

        int lastFrameDamage = currentRoute.lastFrameDamage;
        if (std::any_of(currentRoute.timelineTriggers.begin(), currentRoute.timelineTriggers.end(), [lastFrameDamage](const auto& item) { return item.first > lastFrameDamage; })) {
            addRoute = false;
        }

        if (addRoute) {
            int framesToFinishRecovery = 0;
            pSim->simGuys[0]->setRecordFrameTriggers(false, false);
            while (pSim->simGuys[1]->getRecoveryTiming() == pFinder->startRecoveryTiming || !pSim->simGuys[0]->canAct()) {
                pSim->RunFrame();
                pSim->AdvanceFrame();
                framesProcessed++;
                framesToFinishRecovery++;
                if (framesToFinishRecovery > 1000) {
                    fprintf(stderr, "bbb %s\n", routeToString(currentRoute, pSim->simGuys[0]).c_str());
                    break;
                }
            }

            DoneRoute doneRoute;
            doneRoute.timelineTriggers = currentRoute.timelineTriggers;
            doneRoute.damage = currentRoute.damage;
            doneRoute.focusGain = currentRoute.focusGain;
            doneRoute.focusDmg = currentRoute.focusDmg;
            doneRoute.gaugeGain = currentRoute.gaugeGain;
            doneRoute.gaugeGain = currentRoute.gaugeGain;
            doneRoute.focusSpend = currentRoute.focusSpend;
            doneRoute.gaugeSpend = currentRoute.gaugeSpend;
            doneRoute.sideSwitch = (pFinder->startSnapshot.simGuys[0]->getPosX() > pFinder->startSnapshot.simGuys[1]->getPosX()) != (pSim->simGuys[0]->getPosX() > pSim->simGuys[1]->getPosX());
            doneRoute.advantage = pSim->simGuys[1]->getRecoveryTiming() - pSim->simGuys[0]->getRecoveryTiming();
            // std::string logEntry = std::to_string(doneRoute.damage) + " damage: ";
            // for ( auto &trigger : doneRoute.timelineTriggers) {
            //     logEntry += std::to_string(trigger.first) + " " + guys[0]->getActionName(trigger.second.first) + " ";
            // }
            // log(logEntry);
            // fprintf(stderr, "%s\n", logEntry.c_str());
            mutexDoneRoutes.lock();
            auto newRoute = std::make_unique<DoneRoute>(doneRoute);
            auto it = doneRoutes.find(newRoute);
            if (it == doneRoutes.end()) {
                doneRoutes.insert(std::move(newRoute));
            } else {
                const DoneRoute *ex = it->get();
                int newTriggers = (int)newRoute->timelineTriggers.size();
                int exTriggers = (int)ex->timelineTriggers.size();
                bool notWorse = newTriggers <= exTriggers
                    && newRoute->focusSpend <= ex->focusSpend
                    && newRoute->gaugeSpend <= ex->gaugeSpend
                    && newRoute->advantage >= ex->advantage;
                bool betterSomewhere = newTriggers < exTriggers
                    || newRoute->focusSpend < ex->focusSpend
                    || newRoute->gaugeSpend < ex->gaugeSpend
                    || newRoute->advantage > ex->advantage;
                if (notWorse && betterSomewhere) {
                    doneRoutes.erase(it);
                    doneRoutes.insert(std::move(newRoute));
                }
            }
            mutexDoneRoutes.unlock();
        }

        if (pendingSnapshot) {
            delete pendingSnapshot;
            pendingSnapshot = nullptr;
        }

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

std::string timelineTriggerToString(ActionRef trigger, Guy *pGuy)
{
    std::string actionDesc;
    if (trigger.actionID() < 0) {
        renderInput(actionDesc, -trigger.actionID());
    } else {
        actionDesc = pGuy->getActionName(trigger.actionID());
    }
    return actionDesc;
}

std::string routeToString(const ComboRoute &route, Guy *pGuy)
{
    std::string result;
    for ( auto &trigger : route.timelineTriggers) {
        result += timelineTriggerToString(trigger.second, pGuy) + " ";
    }
    return result;
}

std::string routeToString(const DoneRoute &route, Guy *pGuy)
{
    std::string result = std::to_string(route.focusSpend / 10000) + " ";
    result += std::to_string(route.gaugeSpend / 10000) + " ";
    result += std::to_string(route.damage) + " ";
    if (route.advantage >= 0) {
        result += "+";
    }
    result += std::to_string(route.advantage) + " ";
    result += std::to_string(route.focusGain) + " ";
    result += std::to_string(route.gaugeGain) + " ";
    result += std::to_string(route.focusDmg) + " ";
    result += std::to_string(route.sideSwitch) + " ";
    result += std::to_string(route.impossibleInput) + ": ";
    for ( auto &trigger : route.timelineTriggers) {
        result += timelineTriggerToString(trigger.second, pGuy) + " ";
    }
    return result;
}

void printRoute(const DoneRoute &route, Guy *pGuy)
{
    std::string logEntry = routeToString(route, pGuy);
    log(logEntry);
#if !defined(__EMSCRIPTEN__)
    fprintf(stderr, "%s\n", logEntry.c_str());
#endif
}

void ComboFinder::Start(Simulation *pStartSim)
{
    if (running) {
        return;
    }

    pStartSim->gatherEveryone();
    startSnapshot.Clone(pStartSim);

    initChargeChecker(startSnapshot.simGuys[0]->getCharData());

    startRecoveryTiming = startSnapshot.simGuys[1]->getRecoveryTiming();

    lightsActionIDs.clear();
    if (!doLights) {
        for (auto& [key, action] : startSnapshot.simGuys[0]->getCharData()->actionsByID) {
            if (!action) continue;
            if (action->name.find("5LP") != std::string::npos ||
                action->name.find("5LK") != std::string::npos ||
                action->name.find("2LP") != std::string::npos ||
                action->name.find("2LK") != std::string::npos)
            {
                lightsActionIDs.insert(key.actionID());
            }
        }
    }
    triggerGroupZeroActionIDs.clear();
    if (stopOnRecovery) {
        for (auto & entry : startSnapshot.simGuys[0]->getCharData()->triggerGroupByID[0]->entries) {
            triggerGroupZeroActionIDs.insert(entry.actionID);
        }
    }

    threadCount = std::thread::hardware_concurrency();
#ifdef __EMSCRIPTEN__
    threadCount = emscripten_num_logical_cores();
#endif
    if (threadCount == 0) {
        threadCount = 1;
    }
    for (int i = 0; i < threadCount; i++) {
        ComboWorker *pNewWorker = new ComboWorker;
        pNewWorker->pFinder = this;
        workerPool.push_back(pNewWorker);
    }

    bool first = true;
    for (auto worker : workerPool) {
        worker->Start(first);
        first = false;
    }

    start = std::chrono::steady_clock::now();
    lastFPSUpdate = start;
    lastFrameCount = 0;
    currentFPS = 0;
    totalFrames = 0;
    maxDamage = 0;
    doneRoutesByFocusGain.clear();
    doneRoutesByGaugeGain.clear();
    doneRoutesByFocusDmg.clear();
    doneRoutes.clear();
    filteredByDamage.clear();
    filteredByFocusGain.clear();
    filteredByGaugeGain.clear();
    filteredByFocusDmg.clear();
    sawImpossible = false;
    minAdvantage = 0;
    maxAdvantage = 0;
    filterImpossibleOnly = false;
    recentRoutes.clear();
    newBestPending = false;
    stoppedPending = false;
    log("starting on " + std::to_string(threadCount) + " threads");

    running = true;
}

void ComboFinder::Stop(void)
{
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

    finalFPS = framesPerSeconds;
    running = false;
}

bool ComboFinder::filterIsActive(void) const
{
    return filterFocusBars < 6
        || filterGaugeBars < 3
        || filterSideSwitchOnly
        || filterImpossibleOnly
        || doFilterAdvantage;
}

static bool passesFilter(const ComboFinder &f, const DoneRoute *r)
{
    int focusMax = f.filterFocusBars >= 6 ? INT_MAX : f.filterFocusBars * 10000;
    int gaugeMax = f.filterGaugeBars >= 3 ? INT_MAX : f.filterGaugeBars * 10000;
    if (r->focusSpend > focusMax) return false;
    if (r->gaugeSpend > gaugeMax) return false;
    if (f.filterSideSwitchOnly && !r->sideSwitch) return false;
    if (f.filterImpossibleOnly && !r->impossibleInput) return false;
    if (f.doFilterAdvantage && f.filterAdvantageExact && r->advantage != f.filterAdvantage) return false;
    if (f.doFilterAdvantage && !f.filterAdvantageExact && r->advantage < f.filterAdvantage) return false;
    return true;
}

void ComboFinder::Update(void)
{
    if (filterDirty) {
        filteredByDamage.clear();
        filteredByFocusGain.clear();
        filteredByGaugeGain.clear();
        filteredByFocusDmg.clear();

        if (filterIsActive()) {
            for (auto &up : doneRoutes) {
                DoneRoute *r = up.get();
                if (passesFilter(*this, r)) {
                    filteredByDamage.insert(r);
                    filteredByFocusGain.insert(r);
                    filteredByGaugeGain.insert(r);
                    filteredByFocusDmg.insert(r);
                }
            }
        }
        filterDirty = false;
    }

    if (!running) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    float timeSinceLastFPSUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSUpdate).count() / 1000.0f;

    if (timeSinceLastFPSUpdate >= 1.0f) {
        uint64_t currentTotalFrames = 0;
        for (auto worker : workerPool) {
            currentTotalFrames += worker->framesProcessed;
        }
        uint64_t framesDelta = currentTotalFrames - lastFrameCount;
        currentFPS = framesDelta / timeSinceLastFPSUpdate;
        lastFrameCount = currentTotalFrames;
        lastFPSUpdate = now;
    }

    bool allIdle = true;
    for (auto worker : workerPool) {
        if (worker->mutexDoneRoutes.try_lock()) {
            std::set<std::unique_ptr<DoneRoute>, DamageSort> newDoneRoutes;
            std::swap(newDoneRoutes, worker->doneRoutes);
            worker->mutexDoneRoutes.unlock();

            if (newDoneRoutes.size() > 0) {
                int skip = std::max(0, (int)newDoneRoutes.size() - maxRecentRoutes);
                recentRoutes.clear();
                auto it = newDoneRoutes.begin();
                std::advance(it, skip);
                for (; it != newDoneRoutes.end(); ++it) {
                    recentRoutes.push_back(**it);
                }
            }

            for (auto &route : newDoneRoutes) {
                if (route->damage > maxDamage) {
                    printRoute(*route, startSnapshot.simGuys[0]);
                    maxDamage = route->damage;
                    newBestPending = true;
                }
            }
            bool isFiltering = filterIsActive();

            for (auto it = newDoneRoutes.begin(); it != newDoneRoutes.end(); ) {
                auto newRoute = std::move(newDoneRoutes.extract(it++).value());
                auto existing = doneRoutes.find(newRoute);
                bool doInsert = false;
                if (existing == doneRoutes.end()) {
                    doInsert = true;
                } else {
                    const DoneRoute *ex = existing->get();
                    int newTriggers = (int)newRoute->timelineTriggers.size();
                    int exTriggers = (int)ex->timelineTriggers.size();
                    bool notWorse = newTriggers <= exTriggers
                        && newRoute->focusSpend <= ex->focusSpend
                        && newRoute->gaugeSpend <= ex->gaugeSpend
                        && newRoute->advantage >= ex->advantage;
                    bool betterSomewhere = newTriggers < exTriggers
                        || newRoute->focusSpend < ex->focusSpend
                        || newRoute->gaugeSpend < ex->gaugeSpend
                        || newRoute->advantage > ex->advantage;
                    if (notWorse && betterSomewhere) {
                        DoneRoute *oldPtr = existing->get();
                        doneRoutesByFocusGain.erase(oldPtr);
                        doneRoutesByGaugeGain.erase(oldPtr);
                        doneRoutesByFocusDmg.erase(oldPtr);
                        if (isFiltering) {
                            filteredByDamage.erase(oldPtr);
                            filteredByFocusGain.erase(oldPtr);
                            filteredByGaugeGain.erase(oldPtr);
                            filteredByFocusDmg.erase(oldPtr);
                        }
                        doneRoutes.erase(existing);
                        doInsert = true;
                    }
                }
                if (doInsert) {
                    newRoute->impossibleInput = !checkChargeInputs(newRoute->timelineTriggers);
                    if (newRoute->impossibleInput && !sawImpossible) {
                        sawImpossible = true;
                    }
                    if (newRoute->advantage > maxAdvantage) {
                        maxAdvantage = newRoute->advantage;
                    }
                    if (newRoute->advantage < minAdvantage) {
                        minAdvantage = newRoute->advantage;
                    }
                    auto [pos, inserted] = doneRoutes.insert(std::move(newRoute));
                    DoneRoute *newPtr = pos->get();
                    doneRoutesByFocusGain.insert(newPtr);
                    doneRoutesByGaugeGain.insert(newPtr);
                    doneRoutesByFocusDmg.insert(newPtr);
                    if (isFiltering && passesFilter(*this, newPtr)) {
                        filteredByDamage.insert(newPtr);
                        filteredByFocusGain.insert(newPtr);
                        filteredByGaugeGain.insert(newPtr);
                        filteredByFocusDmg.insert(newPtr);
                    }
                }
            }
        }
        if (!worker->idle) {
            allIdle = false;
        }
    }
    if (allIdle) {
        Stop();
        stoppedPending = true;
    }
}

void ComboFinder::Render(void)
{
    if (!running) {
        return;
    }

    for (auto worker : workerPool) {
        std::scoped_lock lockWorkerRenderSnapshot(worker->mutexRenderSnapshot);
        worker->simRenderSnapshot.Render(1.0 / (workerPool.size() / 3.0) , false);
        worker->wantsRenderSnapshot = true;
    }
}