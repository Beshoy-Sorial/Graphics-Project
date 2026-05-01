#pragma once

// ============================================================
// AISystem
//
// Handles ALL logic for non-player fighters:
//   - Tactical movement (approach, retreat, strafe, circle)
//   - Attack / Defend / Idle decision tree (weighted FSM)
//   - Combo timing
//   - Referee avoidance movement
//
// Separated from PlayerControllerSystem so that adding
// new AI personalities / behaviours only touches this file.
// ============================================================

#include "../components/fighter.hpp"
#include "../ecs/world.hpp"
#include "./combat-system.hpp"
#include "../input/mouse.hpp"

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

        // Shortest angular difference, keeps result in (-pi, pi)
        static float angleDiff(float target, float current)
        {
            float d = target - current;
            while (d >  glm::pi<float>()) d -= 2.0f * glm::pi<float>();
            while (d < -glm::pi<float>()) d += 2.0f * glm::pi<float>();
            return d;
        }

    public:
        void init(CombatSystem *combatSystem) { combat = combatSystem; }

        // ── Update a single non-player fighter ────────────────────────
        // Returns the movement vector the caller should apply (already normalised).
        // 'torso' is the entity whose transform should be rotated.
        // 'isReferee' changes the movement / look-at target.
        glm::vec3 updateFighter(
            Entity           *torso,
            FighterComponent *fighter,
            FighterComponent *playerFighter,
            FighterComponent *cachedAIFighter,
            Entity           *playerTorso,
            Mouse            &mouse,
            float             deltaTime,
            bool              isReferee,
            float             moveSpeed)
        {
            glm::vec3 move(0.0f);

            if (isReferee)
                return updateReferee(torso, fighter, playerFighter, cachedAIFighter, deltaTime);

            // ── Spatial info ──────────────────────────────────────────
            float dx   = playerFighter->basePosition.x - fighter->basePosition.x;
            float dz   = playerFighter->basePosition.z - fighter->basePosition.z;
            float dist = std::sqrt(dx * dx + dz * dz);

            bool playerPunchingNow  = (playerFighter->leftPunchTimer  > 0.0f ||
                                       playerFighter->rightPunchTimer > 0.0f);
            bool playerStunned      = (playerFighter->stunnedTimer > 0.0f);
            bool playerKnockedDown  = (playerFighter->state == FighterState::KNOCKED_DOWN);

            // Is the player's back turned toward the AI?
            float     playerYaw     = playerTorso->localTransform.rotation.y;
            glm::vec3 playerFwd     = glm::normalize(glm::vec3(std::sin(playerYaw), 0.f, std::cos(playerYaw)));
            glm::vec3 toAI          = (glm::length(glm::vec3(-dx, 0.f, -dz)) > 0.001f)
                                      ? glm::normalize(glm::vec3(-dx, 0.f, -dz))
                                      : glm::vec3(1.f, 0.f, 0.f);
            bool playerBackTurned   = (glm::dot(playerFwd, toAI) < -0.3f);
            bool aiHealthLow        = (fighter->currentHealth < fighter->maxHealth * 0.25f);

            // Opportunistic: if player's back is turned and AI is close, reset timer
            if (playerBackTurned && dist <= 1.5f && fighter->aiDecisionTimer > 0.6f)
                fighter->aiDecisionTimer = 0.5f;

            fighter->aiDecisionTimer -= deltaTime;

            // ── Movement (Footsies) ───────────────────────────────────
            if (playerKnockedDown)
            {
                if (dist < 2.5f)
                {
                    move = glm::vec3(-dx, 0.f, -dz);
                    fighter->isDefending = false;
                }
            }
            else if (playerStunned || playerBackTurned)
            {
                if (dist > 1.2f)
                    move = glm::vec3(dx, 0.f, dz);
                fighter->isDefending = false;
            }
            else if (aiHealthLow && dist < 2.5f)
            {
                move = glm::vec3(-dx, 0.f, -dz);
                fighter->isDefending = (playerPunchingNow && rand01() < fighter->aiBlockChance);
            }
            else if (playerPunchingNow && dist < 2.2f)
            {
                move = glm::vec3(-dx, 0.f, -dz);
                fighter->isDefending = (rand01() < fighter->aiBlockChance * 1.5f);
            }
            else if (dist > fighter->aiApproachDistance)
            {
                move = glm::vec3(dx, 0.f, dz);
                fighter->isDefending = false;
            }
            else
            {
                // In range: occasional strafing / circling
                if (fighter->aiDecisionTimer > 0.0f && rand01() > 0.95f)
                {
                    float side = (rand01() > 0.5f) ? 1.0f : -1.0f;
                    move = glm::vec3(-dz * side, 0.f, dx * side);
                    move += glm::vec3(dx, 0.f, dz) * 0.2f;
                }
            }

            // ── Action Decision ───────────────────────────────────────
            if (fighter->aiDecisionTimer <= 0.0f && !playerKnockedDown)
            {
                int choice = 2; // 0=attack, 1=defend, 2=idle

                if (playerPunchingNow)
                {
                    if (dist <= 1.8f && rand01() < fighter->aiBlockChance)
                        choice = 1;
                    else if (dist > 1.8f && rand01() < fighter->aiAttackWeight * 0.5f)
                    {
                        choice = 0;
                        move   = glm::vec3(dx, 0.f, dz);
                    }
                }
                else if (playerStunned)
                {
                    choice = (rand01() < 0.8f) ? 0 : 2;
                }
                else if (playerBackTurned && dist <= 1.8f)
                {
                    choice = 0;
                }
                else
                {
                    float attackW = glm::max(fighter->aiAttackWeight,  0.0f);
                    float defendW = glm::max(fighter->aiDefendWeight,  0.0f);
                    float idleW   = glm::max(fighter->aiIdleWeight,    0.0f);
                    float total   = attackW + defendW + idleW;
                    if (total < 0.001f) { attackW = 0.4f; defendW = 0.3f; idleW = 0.3f; total = 1.0f; }

                    float roll = rand01() * total;
                    if      (roll < attackW)              choice = 0;
                    else if (roll < attackW + defendW)    choice = 1;
                    else                                  choice = 2;
                }

                // ── Execute decision ──────────────────────────────────
                if (choice == 0)
                {
                    fighter->isDefending = false;

                    bool aiPunchedLeft = fighter->nextPunchLeft;
                    if (fighter->nextPunchLeft)
                        fighter->leftPunchTimer  = FighterComponent::PUNCH_DURATION;
                    else
                        fighter->rightPunchTimer = FighterComponent::PUNCH_DURATION;
                    fighter->nextPunchLeft = !fighter->nextPunchLeft;

                    // Directional block: player guards with opposite mouse button
                    bool playerHoldingLeft  = mouse.isPressed(GLFW_MOUSE_BUTTON_LEFT);
                    bool playerHoldingRight = mouse.isPressed(GLFW_MOUSE_BUTTON_RIGHT);

                    combat->applyPunch(fighter, playerFighter,
                                       /*isAIPunch=*/true, aiPunchedLeft,
                                       playerHoldingLeft, playerHoldingRight);

                    // Combo window at high difficulties
                    if (rand01() < fighter->aiAttackWeight * 0.3f)
                        fighter->aiDecisionTimer = 0.6f;
                    else
                        fighter->aiDecisionTimer = nextDecisionDelay(fighter);
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
