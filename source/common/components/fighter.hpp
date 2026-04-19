#pragma once

#include "../ecs/component.hpp"
#include <string>
#include <glm/glm.hpp>

namespace our {

    // All the states a fighter can be in at any given moment
    enum class FighterState {
        IDLE,
        ATTACKING_L,   // Throwing left punch
        ATTACKING_R,   // Throwing right punch
        DEFENDING,     // Holding block
        STUNNED,       // Frozen after a blocked punch
        KNOCKED_DOWN   // Health reached 0
    };

    // FighterComponent stores all combat data for a boxer entity.
    // It is attached to the Torso entity (the hierarchy root).
    // The system that reads it is PlayerControllerSystem (for the player)
    // and will later be read by AISystem (for the opponent).
    class FighterComponent : public Component {
    public:
        // ── Identity ───────────────────────────────────────────────
        bool isPlayer = false;   // true = player-controlled, false = AI
        std::string characterName = "Boxer";
        std::string skinMaterialName = ""; // Name of material for skin swapping

        // ── Stats ──────────────────────────────────────────────────
        float strengthMultiplier = 1.0f;
        float speedMultiplier    = 1.0f;

        // ── Health ─────────────────────────────────────────────────
        float maxHealth     = 100.0f;
        float currentHealth = 100.0f;

        // ── State machine ──────────────────────────────────────────
        FighterState state      = FighterState::IDLE;
        float        stateTimer = 0.0f; // Time spent in current state

        // ── Knockdown mini-game ────────────────────────────────────
        int knockdownCount  = 0;  // How many times they have fallen
        int recoveryClicks  = 0;  // 'X' key presses since last knockdown

        // ── Walking animation ──────────────────────────────────────
        float walkTimer = 0.0f;   // Accumulates time to drive leg sine wave
        
        // ── Root Position Tracking ─────────────────────────────────
        glm::vec3 basePosition = {0.0f, 0.0f, 0.0f};
        bool hasInitializedBasePos = false;

        // ── Combat Mode ────────────────────────────────────────────
        bool isDefending = false;     // true = defend mode (arms guard head)

        // ── Punch animation ────────────────────────────────────────
        float leftPunchTimer  = 0.0f; // counts down while left arm is punching
        float rightPunchTimer = 0.0f; // counts down while right arm is punching
        bool  nextPunchLeft   = true; // alternates L/R with each click

        static constexpr float PUNCH_DURATION = 0.25f; // seconds for full punch cycle

        // ── AI Logic ───────────────────────────────────────────────
        float aiDecisionTimer = 0.0f; // Time until the AI makes its next move

        // ── Stun (from landing a punch on a blocking opponent) ─────
        float stunnedTimer = 0.0f;    // While > 0, this fighter cannot act
        static constexpr float STUN_DURATION = 0.8f; // seconds of stun

        // ── Sound Management ───────────────────────────────────────
        bool soundPlaying = false;

        // Component type ID
        static std::string getID() { return "Fighter"; }

        void deserialize(const nlohmann::json& data) override;
    };

}
