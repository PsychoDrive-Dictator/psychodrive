#include <map>
#include <set>
#include <algorithm>
#include <deque>

#include "combogen.hpp"
#include "simulation.hpp"
#include "main.hpp"
#include "guy.hpp"

struct ComboRoute {
    std::map<int, std::pair<int, int>> timelineTriggers;
    bool startedCombo = false;
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

std::deque<ComboRoute> pendingRoutes;
std::set<DoneRoute, DamageSort> doneRoutes;

void findCombos(void)
{
    const auto start = std::chrono::steady_clock::now();

    ComboRoute currentRoute;

    Simulation *pSim = new Simulation;
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

            if (currentRoute.guyFrameProgress < pSim->simGuys[0]->getCurrentFrame() &&
                currentRoute.simFrameProgress < pSim->frameCounter) {
                currentRoute.simFrameProgress = pSim->frameCounter;
                for (auto &frameTrigger : pSim->simGuys[0]->getFrameTriggers()) {
                    pendingRoutes.push_front(currentRoute);
                    pendingRoutes.front().timelineTriggers[pSim->frameCounter] = frameTrigger;
                }
            }

            if (pSim->simGuys[1]->getComboDamage() > currentRoute.damage) {
                currentRoute.damage = pSim->simGuys[1]->getComboDamage();
                currentRoute.lastFrameDamage = pSim->frameCounter;
            }

            if (pSim->simGuys[1]->getComboHits()) {
                currentRoute.startedCombo = true;
            }

            if (pSim->frameCounter == 2000 || (!pSim->simGuys[1]->getComboHits() && currentRoute.startedCombo) || pSim->simGuys[1]->getIsDown() || pSim->simGuys[0]->canAct()) {
                //pSim->Log("framecount " + std::to_string(pSim->frameCounter));
                break;
            }

            currentRoute.guyFrameProgress = pSim->simGuys[0]->getCurrentFrame();
        }

        std::erase_if(currentRoute.timelineTriggers, [=](const auto& item) {
            return item.first > currentRoute.lastFrameDamage;
        });

        DoneRoute doneRoute;
        doneRoute.timelineTriggers = currentRoute.timelineTriggers;
        doneRoute.damage = currentRoute.damage;
        std::string logEntry = std::to_string(doneRoute.damage) + " damage: ";
        for ( auto &trigger : doneRoute.timelineTriggers) {
            logEntry += std::to_string(trigger.first) + " " + guys[0]->getActionName(trigger.second.first) + " ";
        }
        log(logEntry);
        fprintf(stderr, "%s\n", logEntry.c_str());
        doneRoutes.insert(doneRoute);

        if (!pendingRoutes.size()) {
            break;
        }

        currentRoute = pendingRoutes.front();
        pendingRoutes.pop_front();

        *pSim->simGuys[0] = currentRoute.guys[0];
        *pSim->simGuys[1] = currentRoute.guys[1];
        pSim->frameCounter = currentRoute.simFrameProgress-1;
    }

    delete pSim;

    for (auto &route : doneRoutes) {
        std::string logEntry = std::to_string(route.damage) + " damage: ";
        for ( auto &trigger : route.timelineTriggers) {
            logEntry += std::to_string(trigger.first) + " " + guys[0]->getActionName(trigger.second.first) + " ";
        }
        log(logEntry);
        fprintf(stderr, "%s\n", logEntry.c_str());
    }

    const auto end = std::chrono::steady_clock::now();
    auto logEntry = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f) + "s elapsed";
    log(logEntry);
    fprintf(stderr, "%s\n", logEntry.c_str());

    doneRoutes.clear();
}