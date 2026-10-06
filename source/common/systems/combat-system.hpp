#pragma once

// ============================================================
// CombatSystem
//
// Handles ONLY punch timing, hit detection and damage/stun application.
// Used by both PlayerControllerSystem (for player punches)
// and AISystem (for AI punches). Keeps damage math in one place.
//
// A punch has two moments:
//   1. startPunch()   - the arm starts moving (animation begins)
//   2. resolvePunch() - the fist reaches the target (IMPACT_POINT of the
//                       animation). Range, facing and guard are checked
//                       HERE, so the defender has a short window to react.
// ============================================================

#include "../components/fighter.hpp"
#include "../ecs/entity.hpp"
#include "../miniaudio.h"

#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace our
{
    enum class PunchOutcome { MISS, HIT, BLOCKED, PARRIED };

    struct PunchResult
    {
        PunchOutcome outcome = PunchOutcome::MISS;
        float damage   = 0.0f;
        bool  counter  = false;  // landed while the defender was punching
        bool  knockdown = false; // this punch knocked the defender down
    };

    class CombatSystem
    {
        ma_sound *punchSnd = nullptr;
        ma_sound *stunSnd  = nullptr;

        static float rand01() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }

        void play(ma_sound *sound, float volume)
        {
            if (!sound) return;
            ma_sound_set_volume(sound, volume);
            ma_sound_seek_to_pcm_frame(sound, 0);
            ma_sound_start(sound);
        }

    public:
        static constexpr float BASE_DAMAGE = 8.0f;

        // ── Initialization ────────────────────────────────────────────
        void init(ma_sound *punch, ma_sound *stun)
        {
            punchSnd = punch;
            stunSnd  = stun;
        }

        // Forward direction of a fighter in world space (fighters look along their local +Z)
        static glm::vec3 forwardOf(const Entity *torso)
        {
            float yaw = torso->localTransform.rotation.y;
            return glm::vec3(std::sin(yaw), 0.0f, std::cos(yaw));
        }

        // ── Start a punch (animation). Returns false if that arm is still busy ──
        bool startPunch(FighterComponent *f, bool left)
        {
            if (!f->canAct()) return false;
            float &timer = left ? f->leftPunchTimer : f->rightPunchTimer;
            if (timer > 0.0f) return false; // arm still moving: no button-mashing infinite damage
            f->punchDuration = FighterComponent::PUNCH_DURATION / glm::max(f->speedMultiplier, 0.1f);
            timer = f->punchDuration;
            (left ? f->leftImpactPending : f->rightImpactPending) = true;
            f->punchesThrown++;
            return true;
        }

        // Cancel any punch in progress (used when a fighter gets hit or stunned)
        static void interrupt(FighterComponent *f)
        {
            f->leftImpactPending = f->rightImpactPending = false;
            f->windupTimer = 0.0f;
            f->comboRemaining = 0;
        }

        // ── The fist reaches the target: decide what happens ──────────
        PunchResult resolvePunch(Entity *attackerTorso, FighterComponent *attacker,
                                 Entity *defenderTorso, FighterComponent *defender, bool leftHand)
        {
            PunchResult result;
            if (!attacker || !defender || !attackerTorso || !defenderTorso) return result;
            if (defender->state == FighterState::KNOCKED_DOWN) return result;

            glm::vec3 toDefender = defender->basePosition - attacker->basePosition;
            toDefender.y = 0.0f;
            float dist = glm::length(toDefender);
            if (dist > FighterComponent::PUNCH_RANGE) return result; // whiff
            glm::vec3 dir = dist > 0.001f ? toDefender / dist : forwardOf(attackerTorso);

            // The attacker must roughly face the defender (~65 degree cone)
            if (glm::dot(forwardOf(attackerTorso), dir) < 0.42f) return result;

            float damage = BASE_DAMAGE * attacker->strengthMultiplier * (0.85f + 0.3f * rand01());
            // Counter hit: catching someone in the middle of their own punch hurts more
            if (defender->isPunching() || defender->windupTimer > 0.0f) { damage *= 1.5f; result.counter = true; }
            // Hitting someone who is turned away
            if (glm::dot(forwardOf(defenderTorso), -dir) < -0.2f) damage *= 1.3f;

            // ── Guard ─────────────────────────────────────────────────
            if (defender->isDefending && !result.counter)
            {
                bool parry = false;
                bool openSide = false;
                if (defender->isPlayer)
                {
                    // Directional guard: the attacker's LEFT hand arrives on the defender's RIGHT side
                    bool sideHeld = defender->guardLeft || defender->guardRight;
                    if (sideHeld)
                    {
                        bool correct = leftHand ? defender->guardRight : defender->guardLeft;
                        parry = correct;
                        openSide = !correct;
                    }
                }
                else
                {
                    parry = rand01() < defender->aiBlockChance;
                }

                if (parry)
                {
                    // Perfect block: no damage, the attacker is thrown off balance
                    attacker->stunnedTimer = FighterComponent::STUN_DURATION;
                    interrupt(attacker);
                    defender->blockFlashTimer = 0.2f;
                    play(stunSnd, 1.0f);
                    result.outcome = PunchOutcome::PARRIED;
                    return result;
                }
                if (!openSide)
                {
                    // Full guard: only a little chip damage gets through
                    result.damage = damage * 0.2f;
                    defender->currentHealth = glm::max(defender->currentHealth - result.damage, 0.0f);
                    defender->blockFlashTimer = 0.15f;
                    defender->knockback += dir * 0.9f;
                    play(punchSnd, 0.45f);
                    result.outcome = PunchOutcome::BLOCKED;
                    result.knockdown = defender->currentHealth <= 0.0f;
                    return result;
                }
                // Guarding the wrong side: the punch goes through the open side
            }

            // ── Clean hit ────────────────────────────────────────────
            result.damage = damage;
            defender->currentHealth = glm::max(defender->currentHealth - damage, 0.0f);
            defender->hitFlashTimer = 0.22f;
            defender->hitReactTimer = result.counter ? 0.45f : 0.3f;
            defender->knockback += dir * (result.counter ? 2.6f : 1.8f);
            interrupt(defender); // getting hit cancels your own punch
            attacker->punchesLanded++;
            play(punchSnd, 1.0f);
            result.outcome = PunchOutcome::HIT;
            result.knockdown = defender->currentHealth <= 0.0f;
            return result;
        }
    };

} // namespace our
