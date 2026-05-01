#pragma once

// ============================================================
// FighterAnimationSystem
//
// Handles ONLY the procedural animation of child entities
// parented to a fighter's Torso:
//   - Left / Right Leg   → sine-wave swing while walking
//   - Left / Right Shoulder → punch arc & guard pose
//   - Left / Right Arm   → children of shoulders; stun color
//   - Head               → stun color swap; hidden in FP mode
//   - Referee arm pump   → synced to knockdown countdown timer
// ============================================================

#include "../components/fighter.hpp"
#include "../components/mesh-renderer.hpp"
#include "../asset-loader.hpp"
#include "../material/material.hpp"
#include "../ecs/world.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace our
{
    class FighterAnimationSystem
    {
    public:
        // ── Arm pose angles (X-axis rotation in radians) ──────────────
        static constexpr float ARM_REST_X   = -0.3f;
        static constexpr float ARM_DEFEND_X = -1.8f;
        static constexpr float ARM_PUNCH_PEAK = -1.4f;

        // ── Leg swing constants ───────────────────────────────────────
        float legSwingSpeed     = 8.0f;
        float legSwingAmplitude = 0.2f;

        // ── Animate all children of a single fighter Torso ────────────
        // Call once per frame for each entity that has a FighterComponent.
        //
        //  torso       – the root entity (has FighterComponent)
        //  fighter     – the FighterComponent on 'torso'
        //  world       – needed to iterate children
        //  deltaTime
        //  isFirstPerson  – hide player head when in FP camera mode
        //  pGuardLeft  – player is holding LMB in defend mode (left arm up)
        //  pGuardRight – player is holding RMB in defend mode (right arm up)
        //  cachedFighter    – player fighter (for referee countdown sync)
        //  cachedAIFighter  – AI fighter (for referee countdown sync)
        //  isReferee   – changes arm pump logic
        void animateChildren(
            Entity           *torso,
            FighterComponent *fighter,
            World            *world,
            float             deltaTime,
            bool              isFirstPerson,
            bool              pGuardLeft,
            bool              pGuardRight,
            FighterComponent *cachedFighter,
            FighterComponent *cachedAIFighter,
            bool              isReferee)
        {
            for (auto child : world->getEntities())
            {
                if (child->parent != torso)
                    continue;

                const std::string &n = child->name;

                // ── Head ──────────────────────────────────────────────
                if (n.find("Head") != std::string::npos)
                {
                    animateHead(child, fighter, isFirstPerson, isReferee);
                }
                // ── Bare Arm (direct child of torso, not shoulder) ────
                else if (n.find("Arm") != std::string::npos)
                {
                    applyStunColor(child, fighter);
                }
                // ── Left Leg ──────────────────────────────────────────
                else if (n.find("Left_Leg") != std::string::npos)
                {
                    child->localTransform.rotation.x =
                        legSwingAmplitude * std::sin(fighter->walkTimer * legSwingSpeed);
                    child->localTransform.rotation.z = -torso->localTransform.rotation.z;
                }
                // ── Right Leg ─────────────────────────────────────────
                else if (n.find("Right_Leg") != std::string::npos)
                {
                    child->localTransform.rotation.x =
                        -legSwingAmplitude * std::sin(fighter->walkTimer * legSwingSpeed);
                    child->localTransform.rotation.z = -torso->localTransform.rotation.z;
                }
                // ── Left Shoulder ─────────────────────────────────────
                else if (n.find("Left_Shoulder") != std::string::npos)
                {
                    float targetX = computeShoulderTarget(
                        fighter, /*isLeft=*/true,
                        pGuardLeft, pGuardRight,
                        cachedFighter, cachedAIFighter,
                        isReferee);

                    child->localTransform.rotation.x =
                        glm::mix(child->localTransform.rotation.x, targetX, 18.0f * deltaTime);

                    // Stun color on arm children of this shoulder
                    applyStunColorToChildren(child, fighter, world);
                }
                // ── Right Shoulder ────────────────────────────────────
                else if (n.find("Right_Shoulder") != std::string::npos)
                {
                    float targetX = computeShoulderTarget(
                        fighter, /*isLeft=*/false,
                        pGuardLeft, pGuardRight,
                        cachedFighter, cachedAIFighter,
                        isReferee);

                    child->localTransform.rotation.x =
                        glm::mix(child->localTransform.rotation.x, targetX, 18.0f * deltaTime);

                    applyStunColorToChildren(child, fighter, world);
                }
            }
        }

    private:
        // ── Head animation ────────────────────────────────────────────
        void animateHead(Entity *head, FighterComponent *fighter,
                         bool isFirstPerson, bool isReferee)
        {
            if (fighter->isPlayer && isFirstPerson)
            {
                head->localTransform.scale = glm::vec3(0.0f); // hidden in FP
            }
            else
            {
                head->localTransform.scale = glm::vec3(isReferee ? 0.152f : 0.191f);
            }

            auto *mr = head->getComponent<MeshRendererComponent>();
            if (!mr) return;

            Material *skinMat   = AssetLoader<Material>::get(fighter->skinMaterialName);
            if (!skinMat)
                skinMat = AssetLoader<Material>::get("skin");
            Material *yellowMat = AssetLoader<Material>::get("skin_yellow");

            if (skinMat && yellowMat)
                mr->material = (fighter->stunnedTimer > 0.0f) ? yellowMat : skinMat;
        }

        // ── Stun color on a direct child ──────────────────────────────
        void applyStunColor(Entity *entity, FighterComponent *fighter)
        {
            auto *mr = entity->getComponent<MeshRendererComponent>();
            if (!mr) return;
            Material *skinMat   = AssetLoader<Material>::get("skin");
            Material *yellowMat = AssetLoader<Material>::get("skin_yellow");
            if (skinMat && yellowMat)
                mr->material = (fighter->stunnedTimer > 0.0f) ? yellowMat : skinMat;
        }

        // ── Stun color on grandchildren (arm children of a shoulder) ──
        void applyStunColorToChildren(Entity *shoulder, FighterComponent *fighter, World *world)
        {
            for (auto arm : world->getEntities())
            {
                if (arm->parent != shoulder) continue;
                applyStunColor(arm, fighter);
            }
        }

        // ── Compute target shoulder X rotation ────────────────────────
        float computeShoulderTarget(
            FighterComponent *fighter,
            bool              isLeft,
            bool              pGuardLeft,
            bool              pGuardRight,
            FighterComponent *cachedFighter,
            FighterComponent *cachedAIFighter,
            bool              isReferee)
        {
            float targetX = ARM_REST_X;

            if (isReferee && isLeft)
            {
                // Referee's left arm pumps up/down during countdown
                FighterComponent *downed = nullptr;
                if (cachedFighter    && cachedFighter->state    == FighterState::KNOCKED_DOWN)
                    downed = cachedFighter;
                else if (cachedAIFighter && cachedAIFighter->state == FighterState::KNOCKED_DOWN)
                    downed = cachedAIFighter;

                if (downed)
                {
                    float fraction = downed->stateTimer - std::floor(downed->stateTimer);
                    float pump     = (fraction < 0.5f) ? (fraction * 2.0f) : ((1.0f - fraction) * 2.0f);
                    targetX = glm::mix(ARM_REST_X, ARM_DEFEND_X, pump);
                }
                return targetX;
            }

            bool guard    = fighter->isPlayer ? (isLeft ? pGuardLeft : pGuardRight)
                                              : fighter->isDefending;
            float &timer  = isLeft ? fighter->leftPunchTimer : fighter->rightPunchTimer;

            if (guard)
            {
                targetX = ARM_DEFEND_X;
            }
            else if (timer > 0.0f)
            {
                float duration = FighterComponent::PUNCH_DURATION / fighter->speedMultiplier;
                float t        = 1.0f - (timer / duration);
                float arc      = (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f);
                targetX        = glm::mix(ARM_REST_X, ARM_PUNCH_PEAK, arc);
            }

            return targetX;
        }
    };

} // namespace our
