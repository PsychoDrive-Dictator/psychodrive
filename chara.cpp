#include "chara.hpp"

CharacterData *loadCharacter(nlohmann::json *pTriggerGroupsJson, nlohmann::json *pTriggersJson, nlohmann::json *pCommandsJson, nlohmann::json *pChargeJson)
{
    CharacterData *pRet = new CharacterData;
    for (auto& [keyID, key] : pChargeJson->items()) {
    }

    return pRet;
}

