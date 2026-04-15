#include <map>
#include <set>
#include <algorithm>

#include "comboutils.hpp"
#include "chara.hpp"
#include "input.hpp"

std::map<int16_t, std::set<int>> mapActionNeedCharge;
std::map<int16_t, std::set<int>> mapActionBreakCharge;
std::set<Charge *> setChargesThatHaveActions;

void checkInput(CharacterData *pCharData, int actionID, uint32_t key, uint32_t cond, uint32_t ng)
{
    for (auto &charge : pCharData->charges) {
        auto it = mapActionNeedCharge.find(actionID);
        if (it != mapActionNeedCharge.end() && it->second.contains(charge.id)) {
            continue;
        }
        if ((ng & 0xF) & (charge.okKeyFlags & 0xF)) {
            mapActionBreakCharge[actionID].insert(charge.id);
            continue;
        }
        // is it a direction?
        uint32_t directionInput = key & 0xF;
        if (directionInput) {
            if (cond & 2) {
                // exact match, if we're not the same as the charge we're cooked
                if (directionInput != (charge.okKeyFlags & 0xF)) {
                    mapActionBreakCharge[actionID].insert(charge.id);
                }
            } else {
                // sloppy match, as long as it doesn't overlap with the inverse of the charge we're good?
                int opposite = getInverseCardinal(charge.okKeyFlags & 0xF);
                if ((key & 0xF) & opposite) {
                    mapActionBreakCharge[actionID].insert(charge.id);
                }
            }
        }
    }
}

void initChargeChecker(CharacterData *pCharData)
{
    mapActionNeedCharge.clear();
    mapActionBreakCharge.clear();
    setChargesThatHaveActions.clear();

    // for now assume that:
    // if an action has any trigger that requires charge, the action requires charge
    // all actions with the same id across different styles have the same input?
    for (auto &trigger : pCharData->triggers) {
        if (!trigger.pCommandClassic) continue;
        for (auto &variant : trigger.pCommandClassic->variants) {
            for (auto &input : variant.inputs) {
                if (input.type == ChargeRelease && input.pCharge) {
                    mapActionNeedCharge[trigger.actionID].insert(input.pCharge->id);
                    setChargesThatHaveActions.insert(input.pCharge);
                }
            }
        }
    }
    // walk a second time from scratch so we can use mapActionNeedCharge
    for (auto &trigger : pCharData->triggers) {
        checkInput(pCharData, trigger.actionID, trigger.okKeyFlags, trigger.okCondFlags, trigger.ngKeyFlags);
        checkInput(pCharData, trigger.actionID, trigger.dcExcFlags, 0, trigger.ngKeyFlags);
        checkInput(pCharData, trigger.actionID, trigger.dcIncFlags, 2, trigger.ngKeyFlags);
        if (!trigger.pCommandClassic) continue;
        for (auto &variant : trigger.pCommandClassic->variants) {
            for (auto &input : variant.inputs) {
                if (input.type == Rotation) {
                    // breaks all charges?
                    for (auto &charge : pCharData->charges) {
                        mapActionBreakCharge[trigger.actionID].insert(charge.id);
                    }
                } else if (input.type == Normal) {
                    checkInput(pCharData, trigger.actionID, input.okKeyFlags, input.okCondFlags, input.ngKeyFlags);
                }
            }
        }
    }

    for (auto &charge : pCharData->charges) {
        fprintf(stderr, "charge %i:\n", charge.id);
        fprintf(stderr, "actions that need it: ");
        for (auto &it : mapActionNeedCharge) {
            if (it.second.contains(charge.id)) {
                if (pCharData->actionsByID.contains({it.first, 0})) {
                    fprintf(stderr, "%s ", pCharData->actionsByID[{it.first, 0}]->niceNameDyn.c_str());
                } else {
                    fprintf(stderr, "%i ", it.first);
                }
            }
        }
        fprintf(stderr, "\n");
        fprintf(stderr, "actions that break it: ");
        for (auto &it : mapActionBreakCharge) {
            if (it.second.contains(charge.id)) {
                if (pCharData->actionsByID.contains({it.first, 0})) {
                    fprintf(stderr, "%s ", pCharData->actionsByID[{it.first, 0}]->niceNameDyn.c_str());
                } else {
                    fprintf(stderr, "%i ", it.first);
                }
            }
        }
        fprintf(stderr, "\n");
    }
}

bool checkChargeInputs(std::map<int16_t, ActionRef> &combo)
{
    std::vector<int> lastFrameChargeBroken;
    lastFrameChargeBroken.resize(setChargesThatHaveActions.size());

    for (auto &elem : lastFrameChargeBroken) elem = -1;

    for (auto & [frame, action] : combo) {
        int chargeIDInOurVector = 0;
        for (auto &charge : setChargesThatHaveActions) {
            int chargeIDInCharData = charge->id;
            if (action.actionID() < 0) {
                // raw input, either it charges or not
                // todo actually wrong direction harold - either store dir or fix up when recording

            } else {
                auto itBreakCharge = mapActionBreakCharge.find(action.actionID());
                if (itBreakCharge != mapActionBreakCharge.end()) {
                    if (itBreakCharge->second.contains(chargeIDInCharData)) {
                        // todo add any possible buffer here
                        lastFrameChargeBroken[chargeIDInOurVector] = frame;
                    }
                }
                auto itNeedCharge = mapActionNeedCharge.find(action.actionID());
                if (itNeedCharge != mapActionNeedCharge.end()) {
                    if (itNeedCharge->second.contains(chargeIDInCharData)) {
                        // todo inputs for rest of command?
                        if (lastFrameChargeBroken[chargeIDInOurVector] + charge->chargeFrames + charge->keepFrames >= frame) {
                            return false;
                        }
                    }
                }
            }
            chargeIDInOurVector++;
        }
    }

    return true;
}