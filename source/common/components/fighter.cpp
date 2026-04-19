#include "fighter.hpp"
#include "../deserialize-utils.hpp"

namespace our {

    void FighterComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;
        isPlayer    = data.value("isPlayer",    isPlayer);
        characterName = data.value("characterName", characterName);
        skinMaterialName = data.value("skinMaterialName", skinMaterialName);
        strengthMultiplier = data.value("strengthMultiplier", strengthMultiplier);
        speedMultiplier = data.value("speedMultiplier", speedMultiplier);
        
        maxHealth   = data.value("maxHealth",   maxHealth);
        currentHealth = maxHealth; // Always start at full health
    }

}
