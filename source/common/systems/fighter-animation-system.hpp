#pragma once

// ============================================================
// FighterAnimationSystem
//
// Handles ONLY the procedural animation of child entities
// parented to a fighter's Torso:
//   - Left / Right Leg      → sine-wave swing while walking
//   - Left / Right Shoulder → punch arc, wind-up & guard pose
//   - Head                  → character material; hidden in FP mode
//   - Whole body            → glow when hit / blocking / stunned
//   - Referee arm pump      → synced to knockdown countdown timer
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
    // Frame-rate independent smoothing: moves "current" towards "target".
    // (glm::mix(a, b, rate * dt) overshoots when the frame rate drops; this never does.)
    inline float damp(float current, float target, float rate, float deltaTime)
    {
        return glm::mix(current, target, 1.0f - std::exp(-rate * deltaTime));
    }
    inline glm::vec3 damp(const glm::vec3 &current, const glm::vec3 &target, float rate, float deltaTime)
    {
        return glm::mix(current, target, 1.0f - std::exp(-rate * deltaTime));
    }

    class FighterAnimationSystem
    {
    public:
        // ── Arm pose angles (X-axis rotation in radians) ──────────────
        static constexpr float ARM_REST_X     = -0.3f;
        static constexpr float ARM_STANCE_X   = -0.65f; // relaxed boxing stance (hands slightly up)
        static constexpr float ARM_DEFEND_X   = -1.15f; // forearms in front of the face
        static constexpr float ARM_PUNCH_PEAK = -1.45f;
        static constexpr float ARM_WINDUP_X   =  0.25f; // arm pulled back before an AI punch

        // ── Leg swing constants ───────────────────────────────────────
        float legSwingSpeed     = 8.0f;
        float legSwingAmplitude = 0.2f;

        // ── Animate all children of a single fighter Torso ────────────
        // Call once per frame for each entity that has a FighterComponent.
        void animateChildren(
            Entity           *torso,
            FighterComponent *fighter,
            World            *world,
            float             deltaTime,
            bool              isFirstPerson,
            FighterComponent *cachedFighter,
            FighterComponent *cachedAIFighter,
            bool              isReferee)
        {
            glm::vec3 glow = computeGlow(fighter);
            if (auto *mr = torso->getComponent<MeshRendererComponent>())
                mr->emissive = glow;

            for (auto child : world->getEntities())
            {
                if (child->parent != torso)
                    continue;

                const std::string &n = child->name;
                if (auto *mr = child->getComponent<MeshRendererComponent>())
                    mr->emissive = glow;

                // ── Head ──────────────────────────────────────────────
                if (n.find("Head") != std::string::npos)
                {
                    animateHead(child, fighter, isFirstPerson, isReferee);
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
                // ── Shoulders (the arms are their children) ───────────
                else if (n.find("Left_Shoulder") != std::string::npos ||
                         n.find("Right_Shoulder") != std::string::npos)
                {
                    bool isLeft = n.find("Left_Shoulder") != std::string::npos;
                    float targetX = computeShoulderTarget(fighter, isLeft, cachedFighter, cachedAIFighter, isReferee);
                    // Punches snap out fast, everything else eases
                    float rate = ((isLeft ? fighter->leftPunchTimer : fighter->rightPunchTimer) > 0.0f) ? 35.0f : 16.0f;
                    child->localTransform.rotation.x = damp(child->localTransform.rotation.x, targetX, rate, deltaTime);

                    for (auto arm : world->getEntities())
                    {
                        if (arm->parent != child) continue;
                        if (auto *mr = arm->getComponent<MeshRendererComponent>())
                            mr->emissive = glow;
                    }
                }
            }
        }

    private:
        // ── Body glow (per entity, so shared materials are not modified) ──
        glm::vec3 computeGlow(FighterComponent *fighter)
        {
            glm::vec3 glow(0.0f);
            if (fighter->hitFlashTimer > 0.0f)
                glow += glm::vec3(1.6f, 0.35f, 0.2f) * (fighter->hitFlashTimer / 0.22f);
            if (fighter->blockFlashTimer > 0.0f)
                glow += glm::vec3(0.25f, 0.55f, 1.6f) * (fighter->blockFlashTimer / 0.2f);
            if (fighter->stunnedTimer > 0.0f)
            {
                float pulse = 0.5f + 0.5f * std::sin(fighter->stunnedTimer * 22.0f);
                glow += glm::vec3(0.9f, 0.7f, 0.05f) * (0.25f + 0.45f * pulse);
            }
            return glow;
        }

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

            // The head (headgear) uses the character's signature material
            Material *skinMat = AssetLoader<Material>::get(fighter->skinMaterialName);
            if (!skinMat)
                skinMat = AssetLoader<Material>::get("skin");
            if (skinMat)
                mr->material = skinMat;
        }

        // ── Compute target shoulder X rotation ────────────────────────
        float computeShoulderTarget(
            FighterComponent *fighter,
            bool              isLeft,
            FighterComponent *cachedFighter,
            FighterComponent *cachedAIFighter,
            bool              isReferee)
        {
            float targetX = ARM_REST_X;

            if (isReferee)
            {
                if (!isLeft) return targetX;
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
                    targetX = glm::mix(ARM_REST_X, -1.8f, pump);
                }
                return targetX;
            }

            if (fighter->state == FighterState::KNOCKED_DOWN)
                return ARM_REST_X;

            // Guard: in defend mode with no side selected both arms go up (full guard)
            bool guard = false;
            if (fighter->isDefending)
            {
                bool sideSelected = fighter->guardLeft || fighter->guardRight;
                guard = !sideSelected || (isLeft ? fighter->guardLeft : fighter->guardRight);
            }

            float timer = isLeft ? fighter->leftPunchTimer : fighter->rightPunchTimer;

            if (timer > 0.0f)
            {
                // Fast extension up to the impact point, slower retraction
                float t = 1.0f - (timer / glm::max(fighter->punchDuration, 0.01f));
                float ip = FighterComponent::IMPACT_POINT;
                float arc = (t < ip) ? glm::smoothstep(0.0f, ip, t)
                                     : 1.0f - glm::smoothstep(ip, 1.0f, t);
                targetX = glm::mix(ARM_STANCE_X, ARM_PUNCH_PEAK, arc);
            }
            else if (fighter->windupTimer > 0.0f && fighter->windupLeft == isLeft)
            {
                targetX = ARM_WINDUP_X; // telegraph: the arm is cocked back
            }
            else if (guard)
            {
                targetX = ARM_DEFEND_X;
            }
            else
            {
                targetX = ARM_STANCE_X;
            }

            return targetX;
        }
    };

} // namespace our
