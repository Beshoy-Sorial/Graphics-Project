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
        float healthTrail   = 100.0f; // HUD: lags behind currentHealth to show the damage just taken

        // ── State machine ──────────────────────────────────────────
        FighterState state      = FighterState::IDLE;
        float        stateTimer = 0.0f; // Time spent in current state

        // ── Knockdown mini-game ────────────────────────────────────
        int knockdownCount  = 0;  // How many times they have fallen
        int recoveryClicks  = 0;  // 'X' key presses since last knockdown
        static constexpr int   MAX_KNOCKDOWNS = 3;     // 3rd knockdown = automatic KO
        static constexpr float KO_COUNT       = 10.0f; // seconds to get up before the KO

        // ── Walking animation ──────────────────────────────────────
        float walkTimer = 0.0f;   // Accumulates time to drive leg sine wave
        float idleTimer = 0.0f;   // Drives the boxer's idle "bounce"

        // ── Root Position Tracking ─────────────────────────────────
        glm::vec3 basePosition = {0.0f, 0.0f, 0.0f};
        bool hasInitializedBasePos = false;

        // ── Combat Mode ────────────────────────────────────────────
        bool isDefending = false;     // true = defend mode (arms guard head)
        // Which side is guarded while defending. Neither = full guard (blocks everything but
        // still takes a little "chip" damage). Guarding the side the punch comes from = parry.
        bool guardLeft  = false;
        bool guardRight = false;

        // ── Punch animation ────────────────────────────────────────
        float leftPunchTimer  = 0.0f; // counts down while left arm is punching
        float rightPunchTimer = 0.0f; // counts down while right arm is punching
        float punchDuration   = 0.32f; // duration of the current punch (depends on speed)
        bool  leftImpactPending  = false; // the punch has not reached the target yet
        bool  rightImpactPending = false;
        bool  nextPunchLeft   = true; // alternates L/R with each click

        static constexpr float PUNCH_DURATION = 0.32f; // seconds for full punch cycle (at speed 1)
        static constexpr float IMPACT_POINT   = 0.42f; // fraction of the punch at which it lands
        static constexpr float PUNCH_RANGE    = 1.55f; // max distance between fighters for a punch to land

        // ── Hit reactions ──────────────────────────────────────────
        float hitFlashTimer   = 0.0f;     // > 0: body flashes (got hit)
        float blockFlashTimer = 0.0f;     // > 0: body flashes blue (blocked a punch)
        float hitReactTimer   = 0.0f;     // > 0: torso snaps back, movement slowed
        glm::vec3 knockback   = {0.0f, 0.0f, 0.0f}; // velocity pushed by the last hit (decays)

        // ── AI Logic ───────────────────────────────────────────────
        float aiDecisionTimer = 0.0f; // Time until the AI makes its next move
        // AI behavior tuning (set per difficulty in Playstate)
        float aiAttackWeight = 0.40f;
        float aiDefendWeight = 0.30f;
        float aiIdleWeight = 0.30f;

        float aiDecisionMin = 0.45f; // seconds
        float aiDecisionMax = 1.00f; // seconds

        float aiApproachDistance = 1.20f;
        float aiRetreatDistance = 0.70f;

        float aiBlockChance = 0.30f;             // 0..1 chance to defend vs incoming punch
        float aiRecoveryChancePerFrame = 0.05f;  // 0..1 chance each frame while down
        float aiWindupTime = 0.30f;              // telegraph before each punch (lower = harder)

        float windupTimer   = 0.0f;  // > 0 while the AI is cocking its arm back before a punch
        bool  windupLeft    = true;
        int   comboRemaining = 0;    // punches left in the current combo
        float strafeTimer   = 0.0f;  // > 0 while circling around the player
        float strafeDir     = 1.0f;
        bool  sawPlayerPunch = false; // edge detection: react once per player punch

        // ── Stun (from landing a punch on a blocking opponent) ─────
        float stunnedTimer = 0.0f;    // While > 0, this fighter cannot act
        static constexpr float STUN_DURATION = 0.8f; // seconds of stun

        // ── Match statistics ───────────────────────────────────────
        int punchesThrown = 0;
        int punchesLanded = 0;

        // ── Sound Management ───────────────────────────────────────
        bool soundPlaying = false;

        bool isPunching() const { return leftPunchTimer > 0.0f || rightPunchTimer > 0.0f; }
        bool canAct() const {
            return state != FighterState::KNOCKED_DOWN && stunnedTimer <= 0.0f;
        }

        // Component type ID
        static std::string getID() { return "Fighter"; }

        void deserialize(const nlohmann::json& data) override;
    };

}
