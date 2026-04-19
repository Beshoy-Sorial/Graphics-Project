#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our {

    struct CharacterDef {
        std::string name;
        std::string torsoMaterial;
        glm::vec4 buttonColor;
        float strength;
        float speed;
    };

    class TournamentManager {
    public:
        static TournamentManager& getInstance() {
            static TournamentManager instance;
            return instance;
        }

        int selectedCharacterIndex = 0;
        int currentOpponentIndex = 0;
        int currentRound = 1; // 1: Quarterfinals, 2: Semifinals, 3: Finals

        const std::vector<CharacterDef> characters = {
            {"Red Dragon",   "torso_red",    {0.8f, 0.1f, 0.1f, 1.0f}, 1.2f, 1.0f},
            {"Blue Frost",   "torso_blue",   {0.1f, 0.3f, 0.8f, 1.0f}, 1.0f, 1.2f},
            {"Green Viper",  "torso_green",  {0.1f, 0.7f, 0.1f, 1.0f}, 0.9f, 1.4f},
            {"Gold Lion",    "torso_yellow", {0.8f, 0.7f, 0.1f, 1.0f}, 1.4f, 0.8f},
            {"Purple Night", "torso_purple", {0.5f, 0.1f, 0.7f, 1.0f}, 1.1f, 1.1f},
            {"Cyan Storm",   "torso_cyan",   {0.1f, 0.7f, 0.8f, 1.0f}, 1.0f, 1.3f},
            {"Orange Heat",  "torso_orange", {0.9f, 0.5f, 0.1f, 1.0f}, 1.3f, 0.9f},
            {"Black Shadow", "torso_black",  {0.2f, 0.2f, 0.2f, 1.0f}, 1.2f, 1.2f}
        };

        const CharacterDef& getSelectedCharacter() const {
            return characters[selectedCharacterIndex];
        }

        void reset() {
            currentRound = 1;
            currentOpponentIndex = 0;
            // Note: Keep selectedCharacterIndex so they don't have to re-select if they just want to retry
        }

    private:
        TournamentManager() = default;
    };

}
