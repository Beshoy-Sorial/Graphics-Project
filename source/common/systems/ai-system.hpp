#pragma once

// ============================================================
// AISystem
//
// Handles ALL logic for non-player fighters:
//   - Tactical movement (approach, retreat, strafe / circle, rope escape)
//   - Attack / Defend / Idle decision tree (weighted FSM)
//   - Telegraphed punches (wind-up) and combos
//   - Reacting to the player's punches (once per punch)
//   - Referee avoidance movement
//
// Separated from PlayerControllerSystem so that adding
// new AI personalities / behaviours only touches this file.
// ============================================================

#include "../components/fighter.hpp"
#include "../ecs/world.hpp"
#include "./combat-system.hpp"

#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace our
{
    class AISystem
    {
        CombatSystem *combat = nullptr;

        // ── Helpers ───────────────────────────────────────────────────
        static float rand01()
        {
            return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        }

        static float nextDecisionDelay(FighterComponent *f)
        {
            float span = glm::max(f->aiDecisionMax - f->aiDecisionMin, 0.01f);
            float raw  = f->aiDecisionMin + rand01() * span;
            return raw / glm::max(f->speedMultiplier, 0.1f);
        }

        // Begin the wind-up (telegraph) of a punch
        void beginWindup(FighterComponent *f, float scale = 1.0f)
        {
            f->isDefending = false;
            f->windupLeft  = f->nextPunchLeft;
            f->nextPunchLeft = !f->nextPunchLeft;
            f->windupTimer = glm::max(f->aiWindupTime * scale, 0.05f);
        }

    public:
        static constexpr float RING_LIMIT = 2.4f;

        void init(CombatSystem *combatSystem) { combat = combatSystem; }

        // ── Update a single non-player fighter ────────────────────────
        // Returns the movement vector the caller should apply (normalised by the caller).
        glm::vec3 updateFighter(
            Entity           *torso,
            FighterComponent *fighter,
            FighterComponent *playerFighter,
            Entity           *playerTorso,
            float             deltaTime)
        {
            glm::vec3 move(0.0f);
            if (!playerFighter || !playerTorso) return move;

            // ── Spatial info ──────────────────────────────────────────
            glm::vec3 toPlayer = playerFighter->basePosition - fighter->basePosition;
            toPlayer.y = 0.0f;
            float dist = glm::length(toPlayer);
            glm::vec3 dirToPlayer = dist > 0.001f ? toPlayer / dist : glm::vec3(0, 0, 1);
            glm::vec3 side(-dirToPlayer.z, 0.0f, dirToPlayer.x); // perpendicular: used for circling

            bool playerPunchingNow = playerFighter->isPunching();
            bool playerStunned     = (playerFighter->stunnedTimer > 0.0f);
            bool playerKnockedDown = (playerFighter->state == FighterState::KNOCKED_DOWN);
            bool inRange           = dist <= FighterComponent::PUNCH_RANGE * 0.95f;

            // Is the player's back turned toward the AI?
            glm::vec3 playerFwd   = CombatSystem::forwardOf(playerTorso);
            bool playerBackTurned = (glm::dot(playerFwd, -dirToPlayer) < -0.3f);
            bool aiHealthLow      = (fighter->currentHealth < fighter->maxHealth * 0.25f);

            // ── 1. Wind-up in progress: stay planted, then throw ──────
            if (fighter->windupTimer > 0.0f)
            {
                fighter->windupTimer -= deltaTime;
                if (fighter->windupTimer <= 0.0f)
                {
                    fighter->windupTimer = 0.0f;
                    combat->startPunch(fighter, fighter->windupLeft);
                    if (fighter->comboRemaining > 0)
                    {
                        fighter->comboRemaining--;
                        fighter->aiDecisionTimer = 0.16f / glm::max(fighter->speedMultiplier, 0.1f);
                    }
                    else
                    {
                        fighter->aiDecisionTimer = nextDecisionDelay(fighter);
                    }
                }
                return move;
            }

            // ── 2. React once to every new player punch ───────────────
            if (playerPunchingNow && !fighter->sawPlayerPunch)
            {
                fighter->sawPlayerPunch = true;
                if (dist < 2.0f && !fighter->isPunching() && rand01() < fighter->aiBlockChance * 1.3f)
                {
                    fighter->isDefending     = true;
                    fighter->comboRemaining  = 0;
                    fighter->aiDecisionTimer = 0.35f + rand01() * 0.3f;
                }
            }
            else if (!playerPunchingNow)
            {
                fighter->sawPlayerPunch = false;
            }

            fighter->aiDecisionTimer -= deltaTime;
            fighter->strafeTimer     -= deltaTime;

            // ── 3. Movement (footwork) ────────────────────────────────
            if (playerKnockedDown)
            {
                // Go to a neutral distance while the referee counts
                if (dist < 2.5f) move = -dirToPlayer;
                fighter->isDefending = false;
            }
            else if (playerStunned || playerBackTurned)
            {
                if (!inRange) move = dirToPlayer; // press the advantage
                fighter->isDefending = false;
            }
            else if (aiHealthLow && dist < 2.2f && !fighter->isPunching())
            {
                move = -dirToPlayer + side * fighter->strafeDir * 0.6f; // back off while circling
            }
            else if (dist > fighter->aiApproachDistance)
            {
                move = dirToPlayer;
                if (dist < 2.2f) move += side * fighter->strafeDir * 0.35f; // approach at an angle
            }
            else if (dist < fighter->aiRetreatDistance)
            {
                move = -dirToPlayer;
            }
            else if (fighter->strafeTimer > 0.0f)
            {
                move = side * fighter->strafeDir; // circle the opponent
            }
            else if (rand01() < 0.6f * deltaTime) // on average every ~1.7 seconds
            {
                fighter->strafeDir   = (rand01() > 0.5f) ? 1.0f : -1.0f;
                fighter->strafeTimer = 0.4f + rand01() * 0.8f;
            }

            // Don't get trapped against the ropes: slide along them instead
            glm::vec3 nextPos = fighter->basePosition + move * 0.4f;
            if (std::abs(nextPos.x) > RING_LIMIT - 0.2f || std::abs(nextPos.z) > RING_LIMIT - 0.2f)
            {
                glm::vec3 toCenter = -fighter->basePosition;
                toCenter.y = 0.0f;
                if (glm::length(toCenter) > 0.001f)
                    move += glm::normalize(toCenter) * 0.8f + side * fighter->strafeDir * 0.5f;
            }

            // ── 4. Action decision ────────────────────────────────────
            if (fighter->aiDecisionTimer <= 0.0f && !playerKnockedDown)
            {
                if (fighter->comboRemaining > 0 && inRange)
                {
                    beginWindup(fighter, 0.45f); // follow-up punches are faster
                    return glm::vec3(0.0f);
                }
                fighter->comboRemaining = 0;

                int choice = 2; // 0=attack, 1=defend, 2=idle
                if (playerStunned || (playerBackTurned && inRange))
                {
                    choice = (rand01() < 0.85f) ? 0 : 2;
                }
                else
                {
                    float attackW = glm::max(fighter->aiAttackWeight, 0.0f);
                    float defendW = glm::max(fighter->aiDefendWeight, 0.0f);
                    float idleW   = glm::max(fighter->aiIdleWeight,   0.0f);
                    float total   = attackW + defendW + idleW;
                    if (total < 0.001f) { attackW = 0.4f; defendW = 0.3f; idleW = 0.3f; total = 1.0f; }

                    float roll = rand01() * total;
                    if      (roll < attackW)           choice = 0;
                    else if (roll < attackW + defendW) choice = 1;
                    else                               choice = 2;
                }

                // ── Execute decision ──────────────────────────────────
                if (choice == 0 && inRange)
                {
                    // Aggressive AIs chain 1-2 extra punches
                    fighter->comboRemaining = (rand01() < fighter->aiAttackWeight * 0.6f)
                                              ? 1 + (rand01() < fighter->aiAttackWeight * 0.5f ? 1 : 0)
                                              : 0;
                    beginWindup(fighter);
                    return glm::vec3(0.0f);
                }
                else if (choice == 0)
                {
                    // Wants to attack but too far: close the distance and decide again soon
                    fighter->isDefending     = false;
                    fighter->aiDecisionTimer = 0.15f;
                }
                else if (choice == 1)
                {
                    fighter->isDefending     = true;
                    fighter->aiDecisionTimer = nextDecisionDelay(fighter);
                }
                else
                {
                    fighter->isDefending     = false;
                    fighter->aiDecisionTimer = nextDecisionDelay(fighter);
                }
            }

            return move;
        }

        // ── Referee: stay out of the way, face fight center ──────────
        glm::vec3 updateReferee(
            Entity           *torso,
            FighterComponent *fighter,
            FighterComponent *playerFighter,
            FighterComponent *cachedAIFighter,
            float             deltaTime)
        {
            glm::vec3 move(0.0f);

            float distToPlayer = glm::distance(fighter->basePosition, playerFighter->basePosition);
            glm::vec3 dangerPos   = playerFighter->basePosition;
            float     minDist     = distToPlayer;

            if (cachedAIFighter)
            {
                float d = glm::distance(fighter->basePosition, cachedAIFighter->basePosition);
                if (d < minDist) { minDist = d; dangerPos = cachedAIFighter->basePosition; }
            }

            if (minDist < 2.0f)
            {
                glm::vec3 esc = fighter->basePosition - dangerPos;
                esc.y = 0.0f;
                if (glm::length(esc) < 0.01f) esc = glm::vec3(1.f, 0.f, 0.f);
                move = glm::normalize(esc)
                     + glm::vec3(-fighter->basePosition.x, 0.f, -fighter->basePosition.z) * 0.3f;
            }

            return move;
        }

        // ── AI frame-rate-independent recovery (while knocked down) ──
        void tickRecovery(FighterComponent *fighter, float deltaTime, ma_sound *countSound)
        {
            float clicksPerSecond = fighter->aiRecoveryChancePerFrame * 60.0f;
            if (rand01() < clicksPerSecond * deltaTime)
                fighter->recoveryClicks++;

            int required = 25 + (fighter->knockdownCount - 1) * 15;
            if (fighter->recoveryClicks >= required)
            {
                fighter->state         = FighterState::IDLE;
                fighter->currentHealth = fighter->maxHealth * 0.5f;
                fighter->stateTimer    = 0.0f;
                if (fighter->soundPlaying && countSound)
                {
                    ma_sound_stop(countSound);
                    fighter->soundPlaying = false;
                }
            }
        }
    };

} // namespace our
