#pragma once

// ============================================================
// CombatSystem
//
// Handles ONLY hit detection and damage/stun application.
// Called by both PlayerControllerSystem (for player punches)
// and AISystem (for AI punches). Keeps damage math in one place.
// ============================================================

#include "../components/fighter.hpp"
#include "../miniaudio.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// Forward-declare input types to avoid a circular include
namespace our { struct Mouse; }

namespace our
{
    class CombatSystem
    {
        ma_sound *punchSnd = nullptr;
        ma_sound *stunSnd  = nullptr;

    public:
        // ── Initialization ────────────────────────────────────────────
        void init(ma_sound *punch, ma_sound *stun)
        {
            punchSnd = punch;
            stunSnd  = stun;
        }

        // ── Play punch sound once (won't restart if already playing) ──
        void playPunch()
        {
            if (!punchSnd) return;
            if (!ma_sound_is_playing(punchSnd)) {
                ma_sound_seek_to_pcm_frame(punchSnd, 0);
                ma_sound_start(punchSnd);
            }
        }

        // ── Play stun sound once ───────────────────────────────────────
        void playStun()
        {
            if (!stunSnd) return;
            if (!ma_sound_is_playing(stunSnd)) {
                ma_sound_seek_to_pcm_frame(stunSnd, 0);
                ma_sound_start(stunSnd);
            }
        }

        // ── Apply a punch from 'attacker' towards 'defender' ──────────
        // Returns true if the punch landed (hit or blocked).
        // 'punchRange': max distance for a punch to connect.
        // 'isAIPunch' / 'aiPunchedLeft' only needed for directional blocking logic.
        bool applyPunch(
            FighterComponent *attacker,
            FighterComponent *defender,
            bool              isAIPunch,
            bool              aiPunchedLeft,
            bool              playerHoldingLeft,   // RMB held (player guards left side)
            bool              playerHoldingRight,  // LMB held (player guards right side)
            float             punchRange = 1.5f)
        {
            if (!attacker || !defender) return false;
            if (defender->state == FighterState::KNOCKED_DOWN) return false;

            float dist = glm::distance(attacker->basePosition, defender->basePosition);
            if (dist > punchRange) return false;

            playPunch();

            if (isAIPunch)
            {
                // Directional block: player must hold the correct mouse button
                // to block from the correct side.
                bool blockedCorrectSide = false;
                if (defender->isDefending)
                {
                    if (aiPunchedLeft && playerHoldingRight)
                        blockedCorrectSide = true;
                    else if (!aiPunchedLeft && playerHoldingLeft)
                        blockedCorrectSide = true;
                }

                if (blockedCorrectSide)
                {
                    // AI punch blocked → AI gets stunned
                    attacker->stunnedTimer = FighterComponent::STUN_DURATION;
                    playStun();
                    return true;
                }
            }
            else
            {
                // Player punch: if AI is defending, stun the player (blocked)
                if (defender->isDefending)
                {
                    attacker->stunnedTimer = FighterComponent::STUN_DURATION;
                    playStun();
                    return true;
                }
            }

            // Hit connects — apply damage
            float damage = 10.0f * attacker->strengthMultiplier;
            defender->currentHealth = glm::max(defender->currentHealth - damage, 0.0f);
            return true;
        }
    };

} // namespace our
