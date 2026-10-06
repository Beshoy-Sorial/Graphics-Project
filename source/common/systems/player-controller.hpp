#pragma once

// ============================================================
// PlayerControllerSystem  (Orchestrator)
//
// Responsibilities:
//   1. Locate player / AI / camera entities each frame.
//   2. Camera: broadcast-style follow camera (third person) or first person (V key).
//   3. Read player input (camera-relative WASD movement, punches, guard).
//   4. Delegate punch resolution to CombatSystem (at the moment of impact).
//   5. Delegate AI / Referee decisions to AISystem.
//   6. Run the "knockdown fall & recovery" state machine for ALL fighters.
//   7. Apply movement, knockback, ring-bounds clamping, and collision pushback.
//   8. Delegate child animation to FighterAnimationSystem.
//   9. Draw the in-game HUD (health bars, stance, hit pop-ups, count, KO screen).
//
// Sub-systems:
//   - CombatSystem          (combat-system.hpp)
//   - AISystem              (ai-system.hpp)
//   - FighterAnimationSystem(fighter-animation-system.hpp)
// ============================================================

#include "../application.hpp"
#include "../asset-loader.hpp"
#include "../components/camera.hpp"
#include "../components/fighter.hpp"
#include "../components/free-camera-controller.hpp"
#include "../components/mesh-renderer.hpp"
#include "../ecs/world.hpp"
#include "../material/material.hpp"
#include "../miniaudio.h"
#include "../tournament-manager.hpp"
#include "./combat-system.hpp"
#include "./ai-system.hpp"
#include "./fighter-animation-system.hpp"
#include "./forward-renderer.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <imgui.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace our
{
    extern int g_WeatherMode;

    class PlayerControllerSystem
    {
        Application *app         = nullptr;
        ma_engine   *audioEngine = nullptr;
        bool         soundsLoaded = false;

        // Sounds (initialised once in setAudioEngine)
        ma_sound countSound;
        ma_sound punchSound;
        ma_sound stunSound;

        // Cached pointers (valid only within a match; cleared by resetCache())
        FighterComponent *cachedFighter   = nullptr;
        FighterComponent *cachedAIFighter = nullptr;
        Entity           *playerTorso     = nullptr;
        Entity           *aiTorso         = nullptr;

        // Sub-systems
        CombatSystem           combatSys;
        AISystem               aiSys;
        FighterAnimationSystem animSys;

        // â”€â”€ Camera â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        Entity   *cameraEntity  = nullptr;
        bool      isFirstPerson = false;
        bool      cameraInitialized = false;
        glm::vec3 cameraPosition = {0.0f, 2.5f, 6.0f};
        glm::vec3 cameraTarget   = {0.0f, 1.2f, 0.0f};
        float     shake = 0.0f;   // screen-shake strength (decays)
        float     time  = 0.0f;

        // First person: the camera is a child of the torso, slightly in front of the face.
        // Fighters look along their local +Z, a camera looks along its local -Z, so it is turned by 180Â°.
        static constexpr glm::vec3 FP_OFFSET = {0.0f, 1.25f, 0.55f};
        static constexpr float     FP_PITCH  = -0.12f;
        static constexpr float     FP_FOV    = 75.0f;
        static constexpr float     TP_FOV    = 50.0f;

        // â”€â”€ Movement constants â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        float moveSpeed           = 2.0f;
        float rotationSensitivity = 0.005f;
        bool  mouseLocked         = false;
        static constexpr float RING_LIMIT = 2.4f;

        // â”€â”€ Crowd / HUD feedback â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        float excitement  = 0.0f;
        float resultTimer = 0.0f;

        struct Popup
        {
            std::string text;
            ImU32       color;
            glm::vec3   worldPos;
            float       age;
            float       size;
        };
        std::vector<Popup> popups;

        static float rand01() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }

        static float dampAngle(float current, float target, float rate, float deltaTime)
        {
            float diff = target - current;
            while (diff >  glm::pi<float>()) diff -= glm::two_pi<float>();
            while (diff < -glm::pi<float>()) diff += glm::two_pi<float>();
            return current + diff * (1.0f - std::exp(-rate * deltaTime));
        }

        static float yawTowards(const glm::vec3 &from, const glm::vec3 &to)
        {
            return std::atan2(to.x - from.x, to.z - from.z);
        }

    public:
        // â”€â”€ Life-cycle â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        void enter(Application *a) { app = a; }

        void resetCache()
        {
            cachedFighter     = nullptr;
            cachedAIFighter   = nullptr;
            playerTorso       = nullptr;
            aiTorso           = nullptr;
            cameraEntity      = nullptr;
            cameraInitialized = false;
            resultTimer       = 0.0f;
            mouseLocked       = false;
            excitement        = 0.0f;
            shake             = 0.0f;
            popups.clear();
        }

        void setAudioEngine(ma_engine *engine)
        {
            audioEngine = engine;
            if (!audioEngine) return;

            ma_sound_init_from_file(audioEngine, "assets/audio/conut_down.mp3", 0, NULL, NULL, &countSound);
            ma_sound_init_from_file(audioEngine, "assets/audio/bunch.mp3",      0, NULL, NULL, &punchSound);
            ma_sound_init_from_file(audioEngine, "assets/audio/stun.mp3",       0, NULL, NULL, &stunSound);
            soundsLoaded = true;

            combatSys.init(&punchSound, &stunSound);
            aiSys.init(&combatSys);
        }

        // Releases the sounds (must be called before the audio engine is uninitialized)
        void releaseAudio()
        {
            if (!soundsLoaded) return;
            ma_sound_uninit(&countSound);
            ma_sound_uninit(&punchSound);
            ma_sound_uninit(&stunSound);
            soundsLoaded = false;
            combatSys.init(nullptr, nullptr);
            audioEngine = nullptr;
        }

        float getExcitement() const { return excitement; }

        // â”€â”€ Per-frame update â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        void update(World *world, float deltaTime)
        {
            time += deltaTime;

            // â”€â”€ 1. Find essential entities â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            FighterComponent *playerFighter = nullptr;
            FighterComponent *aiFighter     = nullptr;
            playerTorso = nullptr;
            aiTorso     = nullptr;

            for (auto entity : world->getEntities())
            {
                if (auto *f = entity->getComponent<FighterComponent>())
                {
                    if (f->isPlayer) {
                        playerFighter = f;
                        playerTorso   = entity;
                    } else if (entity->name.find("Referee") == std::string::npos) {
                        aiFighter = f;
                        aiTorso   = entity;
                    }
                }
                if (entity->getComponent<CameraComponent>())
                    cameraEntity = entity;
            }
            if (!playerFighter || !playerTorso) return;

            cachedFighter   = playerFighter;
            cachedAIFighter = aiFighter;

            auto &mouse = app->getMouse();
            auto &kb    = app->getKeyboard();

            // â”€â”€ 2. Mouse lock â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            if (!mouseLocked) {
                mouse.lockMouse(app->getWindow());
                mouseLocked = true;
            }

            // â”€â”€ 3. View / weather toggles â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            if (kb.justPressed(GLFW_KEY_V))
            {
                isFirstPerson = !isFirstPerson;
                cameraInitialized = false;
            }
            if (kb.justPressed(GLFW_KEY_F1))
                g_WeatherMode = (g_WeatherMode + 1) % 3;

            // First-person mouse look
            if (isFirstPerson && playerFighter->canAct())
            {
                glm::vec2 delta = mouse.getMouseDelta();
                playerTorso->localTransform.rotation.y -= delta.x * rotationSensitivity;
            }

            // â”€â”€ 4. Player combat mode toggle (T key) + guard sides â”€â”€â”€â”€
            if (kb.justPressed(GLFW_KEY_T))
            {
                playerFighter->isDefending = !playerFighter->isDefending;
            }
            playerFighter->guardLeft  = playerFighter->isDefending && mouse.isPressed(GLFW_MOUSE_BUTTON_LEFT);
            playerFighter->guardRight = playerFighter->isDefending && mouse.isPressed(GLFW_MOUSE_BUTTON_RIGHT);
            if (playerFighter->guardLeft && playerFighter->guardRight) // both buttons = full guard
                playerFighter->guardLeft = playerFighter->guardRight = false;

            // â”€â”€ 5. Player punches (they land later, at the impact point) â”€â”€
            if (!playerFighter->isDefending && playerFighter->canAct() && playerFighter->hitReactTimer < 0.15f)
            {
                if (mouse.justPressed(GLFW_MOUSE_BUTTON_LEFT))
                    combatSys.startPunch(playerFighter, true);
                if (mouse.justPressed(GLFW_MOUSE_BUTTON_RIGHT))
                    combatSys.startPunch(playerFighter, false);
            }

            // â”€â”€ 6. Update ALL fighters â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            for (auto torso : world->getEntities())
            {
                auto *fighter = torso->getComponent<FighterComponent>();
                if (!fighter) continue;

                // Lazy-initialise stable base position
                if (!fighter->hasInitializedBasePos)
                {
                    fighter->basePosition          = torso->localTransform.position;
                    fighter->hasInitializedBasePos = true;
                }

                bool isReferee = (torso->name.find("Referee") != std::string::npos);

                // Tick timers
                fighter->stunnedTimer    = glm::max(fighter->stunnedTimer    - deltaTime, 0.0f);
                fighter->hitFlashTimer   = glm::max(fighter->hitFlashTimer   - deltaTime, 0.0f);
                fighter->blockFlashTimer = glm::max(fighter->blockFlashTimer - deltaTime, 0.0f);
                fighter->hitReactTimer   = glm::max(fighter->hitReactTimer   - deltaTime, 0.0f);
                fighter->idleTimer      += deltaTime;
                if (fighter->healthTrail > fighter->currentHealth)
                    fighter->healthTrail = damp(fighter->healthTrail, fighter->currentHealth,
                                                fighter->hitFlashTimer > 0.0f ? 0.5f : 3.0f, deltaTime);
                else
                    fighter->healthTrail = fighter->currentHealth;
                if (fighter->stunnedTimer > 0.0f)
                    CombatSystem::interrupt(fighter);

                // â”€â”€ Knockdown detection â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
                if (fighter->currentHealth <= 0.0f &&
                    fighter->state != FighterState::KNOCKED_DOWN)
                {
                    fighter->state          = FighterState::KNOCKED_DOWN;
                    fighter->stateTimer     = 0.0f;
                    fighter->recoveryClicks = 0;
                    fighter->isDefending    = false;
                    fighter->knockdownCount++;
                    CombatSystem::interrupt(fighter);
                    // Third knockdown: technical knockout, no getting up
                    if (fighter->knockdownCount >= FighterComponent::MAX_KNOCKDOWNS)
                        fighter->stateTimer = FighterComponent::KO_COUNT - 1.5f;
                    excitement = 1.0f;
                    shake = glm::max(shake, 1.2f);
                    addPopup(fighter->knockdownCount >= FighterComponent::MAX_KNOCKDOWNS ? "T.K.O.!" : "KNOCKDOWN!",
                             IM_COL32(255, 210, 60, 255), fighter->basePosition, 46.0f);
                }

                // â”€â”€ Knockdown state â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
                if (fighter->state == FighterState::KNOCKED_DOWN)
                {
                    tickKnockdown(torso, fighter, kb, deltaTime);
                    animSys.animateChildren(torso, fighter, world, deltaTime,
                                            isFirstPerson, cachedFighter, cachedAIFighter, isReferee);
                    continue; // No movement while on the floor
                }

                // Stop count sound if no longer knocked down
                stopCountSound(fighter);

                // Upright (and snap back a little when hit)
                float lean = (fighter->hitReactTimer > 0.0f) ? 0.3f * (fighter->hitReactTimer / 0.3f) : 0.0f;
                torso->localTransform.rotation.x = damp(torso->localTransform.rotation.x, lean, 14.0f, deltaTime);

                // â”€â”€ Determine move vector â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
                glm::vec3 move(0.0f);
                if (fighter->isPlayer)
                {
                    if (fighter->canAct()) move = computePlayerMove(kb, torso);
                }
                else if (isReferee)
                {
                    move = aiSys.updateReferee(torso, fighter, playerFighter, aiFighter, deltaTime);
                }
                else if (fighter->canAct())
                {
                    move = aiSys.updateFighter(torso, fighter, playerFighter, playerTorso, deltaTime);
                }

                // â”€â”€ Apply movement + facing â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
                applyMovement(torso, fighter, move, deltaTime, isReferee, playerFighter, aiFighter);

                // â”€â”€ Knockback (decays quickly) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
                fighter->basePosition += fighter->knockback * deltaTime;
                fighter->knockback = damp(fighter->knockback, glm::vec3(0.0f), 7.0f, deltaTime);
                clampToRing(fighter);

                // â”€â”€ Punch timers + impact â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
                fighter->leftPunchTimer  = glm::max(fighter->leftPunchTimer  - deltaTime, 0.0f);
                fighter->rightPunchTimer = glm::max(fighter->rightPunchTimer - deltaTime, 0.0f);
                if (!isReferee)
                {
                    Entity           *targetTorso   = fighter->isPlayer ? aiTorso : playerTorso;
                    FighterComponent *targetFighter = fighter->isPlayer ? aiFighter : playerFighter;
                    float impactTime = fighter->punchDuration * (1.0f - FighterComponent::IMPACT_POINT);
                    if (fighter->leftImpactPending && fighter->leftPunchTimer <= impactTime)
                    {
                        fighter->leftImpactPending = false;
                        onPunchResult(combatSys.resolvePunch(torso, fighter, targetTorso, targetFighter, true),
                                      fighter, targetFighter);
                    }
                    if (fighter->rightImpactPending && fighter->rightPunchTimer <= impactTime)
                    {
                        fighter->rightImpactPending = false;
                        onPunchResult(combatSys.resolvePunch(torso, fighter, targetTorso, targetFighter, false),
                                      fighter, targetFighter);
                    }
                }

                // â”€â”€ Collision pushback â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
                applyCollisionPushback(torso, fighter, world);

                // â”€â”€ Animate children â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
                animSys.animateChildren(torso, fighter, world, deltaTime,
                                        isFirstPerson, cachedFighter, cachedAIFighter, isReferee);
            }

            // â”€â”€ 7. Camera (after everything moved) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            updateCamera(deltaTime);

            // â”€â”€ 8. Feedback decay â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            excitement = glm::max(excitement - 0.3f * deltaTime, 0.0f);
            shake *= std::exp(-7.0f * deltaTime);
            for (auto &p : popups) p.age += deltaTime;
            popups.erase(std::remove_if(popups.begin(), popups.end(),
                                        [](const Popup &p) { return p.age > 1.1f; }),
                         popups.end());
        }

        // â”€â”€ HUD (called from PlayState::onImmediateGui) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        // 'renderer' is optional; if non-null, grayscale is toggled on knockdowns.
        void drawHUD(float deltaTime, ForwardRenderer *renderer = nullptr)
        {
            if (!cachedFighter) return;

            ImGuiIO &io = ImGui::GetIO();
            ImDrawList *dl = ImGui::GetForegroundDrawList();
            ImFont *font = io.Fonts->Fonts.Size > 1 ? io.Fonts->Fonts[1] : ImGui::GetFont();
            const float W = io.DisplaySize.x, H = io.DisplaySize.y;
            auto &tm = our::TournamentManager::getInstance();

            // â”€â”€ Health bars â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            const float margin = 28.0f, barH = 20.0f, barY = 46.0f;
            const float barW = std::min(W * 0.36f, 470.0f);
            drawHealthBar(dl, font, ImVec2(margin, barY), barW, barH, cachedFighter, false, IM_COL32(230, 70, 60, 255));
            if (cachedAIFighter)
                drawHealthBar(dl, font, ImVec2(W - margin - barW, barY), barW, barH, cachedAIFighter, true, IM_COL32(70, 140, 240, 255));

            // Round / difficulty in the middle
            const char *roundName = tm.currentRound == 1 ? "QUARTERFINAL" : (tm.currentRound == 2 ? "SEMIFINAL" : "FINAL");
            textCentered(dl, font, 22.0f, ImVec2(W * 0.5f, 22.0f), IM_COL32(255, 215, 90, 255), roundName);
            textCentered(dl, font, 15.0f, ImVec2(W * 0.5f, 46.0f), IM_COL32(220, 220, 220, 200), tm.difficultyName());

            // â”€â”€ Stance / controls (bottom-left) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            if (cachedFighter->state != FighterState::KNOCKED_DOWN)
            {
                const char *stance;
                ImU32 stanceColor;
                if (!cachedFighter->isDefending) { stance = "ATTACK"; stanceColor = IM_COL32(240, 90, 60, 255); }
                else if (cachedFighter->guardLeft)  { stance = "GUARD  LEFT";  stanceColor = IM_COL32(90, 180, 255, 255); }
                else if (cachedFighter->guardRight) { stance = "GUARD  RIGHT"; stanceColor = IM_COL32(90, 180, 255, 255); }
                else { stance = "FULL GUARD"; stanceColor = IM_COL32(90, 180, 255, 255); }

                ImVec2 chipPos(margin, H - 78.0f);
                ImVec2 size = font->CalcTextSizeA(26.0f, FLT_MAX, 0.0f, stance);
                dl->AddRectFilled(ImVec2(chipPos.x - 10, chipPos.y - 6), ImVec2(chipPos.x + size.x + 10, chipPos.y + size.y + 6),
                                  IM_COL32(0, 0, 0, 150), 6.0f);
                dl->AddRectFilled(ImVec2(chipPos.x - 10, chipPos.y - 6), ImVec2(chipPos.x - 5, chipPos.y + size.y + 6), stanceColor, 2.0f);
                dl->AddText(font, 26.0f, chipPos, stanceColor, stance);
                if (cachedFighter->stunnedTimer > 0.0f)
                    dl->AddText(font, 22.0f, ImVec2(chipPos.x + size.x + 24, chipPos.y + 2), IM_COL32(255, 220, 40, 255), "STUNNED!");

                const char *hint = cachedFighter->isDefending
                    ? "Hold LMB / RMB: guard a side (parry)   No button: full guard   [T] attack   [V] camera   [F1] weather"
                    : "LMB / RMB: left / right punch   [T] guard   WASD: move   [V] camera   [F1] weather   [ESC] menu";
                dl->AddText(font, 15.0f, ImVec2(margin + 1, H - 31.0f), IM_COL32(0, 0, 0, 160), hint);
                dl->AddText(font, 15.0f, ImVec2(margin, H - 32.0f), IM_COL32(230, 230, 230, 210), hint);
            }

            // â”€â”€ Floating hit pop-ups â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            drawPopups(dl, font);

            // â”€â”€ Knockdown count â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            bool isPlayerDown = (cachedFighter->state == FighterState::KNOCKED_DOWN);
            bool isAIDown     = (cachedAIFighter && cachedAIFighter->state == FighterState::KNOCKED_DOWN);
            const float KO_TIME = FighterComponent::KO_COUNT + 1.0f;
            bool playerKO = (isPlayerDown && cachedFighter->stateTimer >= KO_TIME);
            bool aiKO     = (isAIDown && cachedAIFighter->stateTimer >= KO_TIME);

            if ((isPlayerDown || isAIDown) && !playerKO && !aiKO)
            {
                FighterComponent *down = isPlayerDown ? cachedFighter : cachedAIFighter;
                int count = glm::clamp(1 + (int)down->stateTimer, 1, 10);
                char countText[8];
                std::snprintf(countText, sizeof(countText), "%d", count);
                float pulse = 1.0f - (down->stateTimer - std::floor(down->stateTimer));
                textCentered(dl, font, 96.0f + 30.0f * pulse, ImVec2(W * 0.5f, H * 0.30f), IM_COL32(255, 70, 50, 245), countText, true);

                if (isPlayerDown && down->knockdownCount < FighterComponent::MAX_KNOCKDOWNS)
                {
                    int required = 25 + (down->knockdownCount - 1) * 15;
                    float frac = glm::clamp((float)down->recoveryClicks / required, 0.0f, 1.0f);
                    textCentered(dl, font, 30.0f, ImVec2(W * 0.5f, H * 0.45f), IM_COL32(255, 220, 60, 255), "GET UP!  MASH  [X]", true);
                    ImVec2 p0(W * 0.5f - 180.0f, H * 0.45f + 34.0f), p1(W * 0.5f + 180.0f, H * 0.45f + 54.0f);
                    dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 170), 5.0f);
                    dl->AddRectFilled(ImVec2(p0.x + 3, p0.y + 3), ImVec2(p0.x + 3 + (p1.x - p0.x - 6) * frac, p1.y - 3),
                                      IM_COL32(255, 200, 50, 255), 4.0f);
                }
                else if (isAIDown)
                {
                    textCentered(dl, font, 30.0f, ImVec2(W * 0.5f, H * 0.45f), IM_COL32(120, 255, 140, 255),
                                 down->knockdownCount >= FighterComponent::MAX_KNOCKDOWNS ? "THE OPPONENT IS FINISHED!" : "OPPONENT DOWN!", true);
                }
            }

            // Grayscale effect while someone is on the floor
            if (renderer)
                renderer->setGrayscale(isPlayerDown ? 1.0f : (isAIDown ? 0.6f : 0.0f));

            // â”€â”€ KO screen â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            if (playerKO || aiKO)
            {
                resultTimer += deltaTime;
                float fade = glm::clamp(resultTimer * 2.0f, 0.0f, 1.0f);
                dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(0, 0, 0, (int)(140 * fade)));
                textCentered(dl, font, 120.0f, ImVec2(W * 0.5f, H * 0.32f), IM_COL32(255, 210, 60, (int)(255 * fade)), "K.O.", true);
                textCentered(dl, font, 44.0f, ImVec2(W * 0.5f, H * 0.47f),
                             aiKO ? IM_COL32(120, 255, 140, (int)(255 * fade)) : IM_COL32(255, 90, 80, (int)(255 * fade)),
                             aiKO ? "YOU WIN!" : "YOU LOSE", true);

                int thrown = cachedFighter->punchesThrown, landed = cachedFighter->punchesLanded;
                char stats[128];
                std::snprintf(stats, sizeof(stats), "Punches landed: %d / %d   (%d%% accuracy)",
                              landed, thrown, thrown > 0 ? (100 * landed) / thrown : 0);
                textCentered(dl, font, 22.0f, ImVec2(W * 0.5f, H * 0.57f), IM_COL32(235, 235, 235, (int)(255 * fade)), stats, true);

                if (resultTimer > 3.5f)
                {
                    resultTimer = 0.0f;
                    if (aiKO) { tm.currentRound++; tm.currentOpponentIndex = 0; app->changeState("bracket"); }
                    else      { tm.reset(); app->changeState("menu"); }
                }
            }
        }

    private:
        // â”€â”€ Punch outcome â†’ feedback (pop-ups, shake, crowd) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        void onPunchResult(const PunchResult &result, FighterComponent *attacker, FighterComponent *defender)
        {
            if (!defender) return;
            glm::vec3 where = defender->basePosition;
            char text[32];
            switch (result.outcome)
            {
            case PunchOutcome::HIT:
                std::snprintf(text, sizeof(text), result.counter ? "COUNTER! %d" : "%d", (int)std::round(result.damage));
                addPopup(text, result.counter ? IM_COL32(255, 120, 40, 255) : IM_COL32(255, 255, 255, 255),
                         where, result.counter ? 34.0f : 28.0f);
                excitement = glm::min(excitement + (result.counter ? 0.45f : 0.22f), 1.0f);
                shake = glm::max(shake, defender->isPlayer ? (result.counter ? 1.0f : 0.7f) : 0.3f);
                break;
            case PunchOutcome::BLOCKED:
                addPopup("BLOCK", IM_COL32(110, 190, 255, 255), where, 24.0f);
                break;
            case PunchOutcome::PARRIED:
                addPopup("PARRY!", IM_COL32(255, 220, 60, 255), where, 32.0f);
                excitement = glm::min(excitement + 0.15f, 1.0f);
                shake = glm::max(shake, 0.25f);
                break;
            default:
                break;
            }
        }

        void addPopup(const std::string &text, ImU32 color, const glm::vec3 &position, float size)
        {
            glm::vec3 jitter((rand01() - 0.5f) * 0.4f, 0.0f, (rand01() - 0.5f) * 0.4f);
            popups.push_back({text, color, position + jitter + glm::vec3(0.0f, 2.25f, 0.0f), 0.0f, size});
        }

        // â”€â”€ Camera â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        void updateCamera(float deltaTime)
        {
            if (!cameraEntity || !playerTorso || !cachedFighter) return;
            auto *cam = cameraEntity->getComponent<CameraComponent>();
            // The free-fly camera controller would fight with this camera
            if (cameraEntity->getComponent<FreeCameraControllerComponent>())
                cameraEntity->deleteComponent<FreeCameraControllerComponent>();

            glm::vec3 shakeOffset = shake * 0.06f * glm::vec3(std::sin(time * 47.0f), std::sin(time * 53.0f + 1.0f),
                                                               std::sin(time * 41.0f + 2.0f));

            if (isFirstPerson)
            {
                cameraEntity->parent = playerTorso;
                cameraEntity->localTransform.position = FP_OFFSET + shakeOffset * 2.0f;
                cameraEntity->localTransform.rotation = glm::vec3(FP_PITCH, glm::pi<float>(), 0.0f);
                if (cam) cam->fovY = glm::radians(FP_FOV);
                return;
            }

            // â”€â”€ Broadcast camera: behind the player's shoulder, framing both fighters â”€â”€
            cameraEntity->parent = nullptr;
            if (cam) cam->fovY = glm::radians(TP_FOV);

            glm::vec3 p = cachedFighter->basePosition;
            glm::vec3 a = cachedAIFighter ? cachedAIFighter->basePosition : p + CombatSystem::forwardOf(playerTorso) * 2.0f;
            p.y = a.y = 0.0f;
            glm::vec3 axis = a - p;
            float separation = glm::length(axis);
            axis = separation > 0.001f ? axis / separation : glm::vec3(0, 0, -1);
            glm::vec3 right(-axis.z, 0.0f, axis.x);

            float back   = 2.4f + separation * 0.5f;
            float height = 2.0f + separation * 0.2f;
            glm::vec3 desired = p - axis * back + right * 2.3f + glm::vec3(0.0f, height, 0.0f);
            // Stay inside the ropes: when the player is backed into the ropes the camera would end up
            // outside the ring looking through them, so clamp it and rise to look down over the fight.
            const float CAMERA_LIMIT = 2.75f;
            float overshoot = glm::max(std::abs(desired.x), std::abs(desired.z)) - CAMERA_LIMIT;
            desired.x = glm::clamp(desired.x, -CAMERA_LIMIT, CAMERA_LIMIT);
            desired.z = glm::clamp(desired.z, -CAMERA_LIMIT, CAMERA_LIMIT);
            if (overshoot > 0.0f) desired.y += glm::min(overshoot * 0.6f, 1.2f);
            glm::vec3 target = (p + a) * 0.5f + glm::vec3(0.0f, 1.15f, 0.0f);
            // While the player is down, look at them from a bit higher
            if (cachedFighter->state == FighterState::KNOCKED_DOWN)
            {
                desired.y += 0.8f;
                target = p + glm::vec3(0.0f, 0.6f, 0.0f);
            }

            if (!cameraInitialized)
            {
                cameraPosition = desired;
                cameraTarget = target;
                cameraInitialized = true;
            }
            cameraPosition = damp(cameraPosition, desired, 3.5f, deltaTime);
            cameraTarget   = damp(cameraTarget, target, 6.0f, deltaTime);

            glm::vec3 eye = cameraPosition + shakeOffset;
            glm::vec3 dir = glm::normalize(cameraTarget - eye);
            cameraEntity->localTransform.position = eye;
            // A camera looks along -Z: yaw/pitch that rotate -Z onto "dir"
            cameraEntity->localTransform.rotation = glm::vec3(std::asin(glm::clamp(dir.y, -1.0f, 1.0f)),
                                                              std::atan2(-dir.x, -dir.z), 0.0f);
        }

        // â”€â”€ Knockdown fall + recovery state machine â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        void tickKnockdown(Entity *torso, FighterComponent *fighter,
                           Keyboard &kb, float deltaTime)
        {
            fighter->stateTimer += deltaTime;
            bool tko = fighter->knockdownCount >= FighterComponent::MAX_KNOCKDOWNS;
            const float KO_TIME = FighterComponent::KO_COUNT + 1.0f;

            // Fall animation
            float targetRotX = glm::half_pi<float>();
            torso->localTransform.rotation.x = damp(torso->localTransform.rotation.x, targetRotX, 4.0f, deltaTime);
            torso->localTransform.position.y = damp(torso->localTransform.position.y, 0.4f, 3.0f, deltaTime);
            torso->localTransform.position.x = fighter->basePosition.x;
            torso->localTransform.position.z = fighter->basePosition.z;

            if (fighter->stateTimer < KO_TIME)
            {
                if (tko)
                {
                    // no getting up after the third knockdown
                }
                else if (fighter->isPlayer)
                {
                    if (kb.justPressed(GLFW_KEY_X))
                        fighter->recoveryClicks++;

                    int required = 25 + (fighter->knockdownCount - 1) * 15;
                    if (fighter->recoveryClicks >= required)
                    {
                        fighter->state         = FighterState::IDLE;
                        fighter->currentHealth = fighter->maxHealth * 0.5f;
                        fighter->stateTimer    = 0.0f;
                        stopCountSound(fighter);
                    }
                }
                else
                {
                    aiSys.tickRecovery(fighter, deltaTime, soundsLoaded ? &countSound : nullptr);
                }
            }
            else
            {
                fighter->stateTimer = KO_TIME; // clamp
                stopCountSound(fighter);
            }

            // Start countdown sound once
            if (!fighter->soundPlaying && fighter->state == FighterState::KNOCKED_DOWN &&
                fighter->stateTimer < 0.5f && soundsLoaded)
            {
                ma_sound_seek_to_pcm_frame(&countSound, 0);
                ma_sound_start(&countSound);
                fighter->soundPlaying = true;
            }
        }

        void stopCountSound(FighterComponent *fighter)
        {
            if (fighter->soundPlaying)
            {
                if (soundsLoaded) ma_sound_stop(&countSound);
                fighter->soundPlaying = false;
            }
        }

        // â”€â”€ Player WASD movement â†’ move vector â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        glm::vec3 computePlayerMove(Keyboard &kb, Entity *torso)
        {
            glm::vec3 forward, right;
            if (isFirstPerson)
            {
                // Relative to where the fighter looks
                forward = CombatSystem::forwardOf(torso);
                right   = glm::vec3(-forward.z, 0.0f, forward.x);
            }
            else
            {
                // Relative to the camera (W = into the screen, D = to the right)
                float yaw = cameraEntity ? cameraEntity->localTransform.rotation.y : 0.0f;
                forward = glm::vec3(-std::sin(yaw), 0.0f, -std::cos(yaw));
                right   = glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw));
            }
            glm::vec3 move(0.0f);
            if (kb.isPressed(GLFW_KEY_W)) move += forward;
            if (kb.isPressed(GLFW_KEY_S)) move -= forward;
            if (kb.isPressed(GLFW_KEY_A)) move -= right;
            if (kb.isPressed(GLFW_KEY_D)) move += right;
            return move;
        }

        static void clampToRing(FighterComponent *fighter)
        {
            fighter->basePosition.x = glm::clamp(fighter->basePosition.x, -RING_LIMIT, RING_LIMIT);
            fighter->basePosition.z = glm::clamp(fighter->basePosition.z, -RING_LIMIT, RING_LIMIT);
        }

        // â”€â”€ Apply movement + facing + body motion â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        void applyMovement(Entity *torso, FighterComponent *fighter,
                           glm::vec3 move, float deltaTime,
                           bool isReferee,
                           FighterComponent *playerFighter,
                           FighterComponent *aiFighter)
        {
            bool isMoving = (glm::length(move) > 0.001f);

            if (isMoving)
            {
                move = glm::normalize(move);
                float spd = moveSpeed * fighter->speedMultiplier * (fighter->isPlayer ? 1.0f : 0.8f);
                if (fighter->isPunching())         spd *= 0.55f; // planted while punching
                if (fighter->isDefending)          spd *= 0.6f;
                if (fighter->hitReactTimer > 0.0f) spd *= 0.3f;
                fighter->basePosition += move * spd * deltaTime;
                fighter->walkTimer    += deltaTime;
                clampToRing(fighter);
            }
            else
            {
                fighter->walkTimer = 0.0f;
            }

            // â”€â”€ Facing â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            bool controlsOwnYaw = fighter->isPlayer && isFirstPerson;
            if (!controlsOwnYaw)
            {
                float targetYaw = torso->localTransform.rotation.y;
                if (isReferee)
                {
                    glm::vec3 center = playerFighter->basePosition;
                    if (aiFighter) center = (center + aiFighter->basePosition) * 0.5f;
                    targetYaw = yawTowards(fighter->basePosition, center);
                }
                else if (fighter->isPlayer && aiFighter)
                {
                    targetYaw = yawTowards(fighter->basePosition, aiFighter->basePosition); // lock-on
                }
                else if (!fighter->isPlayer && playerFighter)
                {
                    targetYaw = yawTowards(fighter->basePosition, playerFighter->basePosition);
                }
                else if (isMoving)
                {
                    targetYaw = std::atan2(move.x, move.z);
                }
                torso->localTransform.rotation.y = dampAngle(torso->localTransform.rotation.y, targetYaw, 12.0f, deltaTime);
            }

            // â”€â”€ Body motion: walking wobble + boxer's bounce â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            float wobble = isMoving ? 0.08f * std::sin(fighter->walkTimer * 8.0f) : 0.0f;
            torso->localTransform.rotation.z = damp(torso->localTransform.rotation.z, wobble, 10.0f, deltaTime);
            float bounce = isReferee ? 0.0f : 0.025f * std::sin(fighter->idleTimer * 5.5f);
            float pivotY = -0.9f * 0.351f;
            float rz = torso->localTransform.rotation.z;
            torso->localTransform.position = fighter->basePosition +
                glm::vec3(pivotY * std::sin(rz), pivotY * (1.0f - std::cos(rz)) + bounce, 0.0f);
        }

        // â”€â”€ Circle-vs-circle collision pushback â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        void applyCollisionPushback(Entity *torso, FighterComponent *fighter, World *world)
        {
            const float radius = 1.0f;
            for (auto other : world->getEntities())
            {
                if (other == torso) continue;
                auto *otherF = other->getComponent<FighterComponent>();
                if (!otherF) continue;

                float dx   = fighter->basePosition.x - otherF->basePosition.x;
                float dz   = fighter->basePosition.z - otherF->basePosition.z;
                float dist = std::sqrt(dx * dx + dz * dz);

                if (dist < radius && dist > 0.001f)
                {
                    float overlap  = radius - dist;
                    glm::vec3 norm = glm::vec3(dx, 0.f, dz) / dist;
                    fighter->basePosition += norm * overlap;
                    clampToRing(fighter);
                    torso->localTransform.position.x = fighter->basePosition.x;
                    torso->localTransform.position.z = fighter->basePosition.z;
                }
            }
        }

        // â”€â”€ HUD helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        static void textCentered(ImDrawList *dl, ImFont *font, float size, ImVec2 center, ImU32 color,
                                 const char *text, bool shadow = false)
        {
            ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
            ImVec2 pos(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f);
            if (shadow)
            {
                int alpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
                dl->AddText(font, size, ImVec2(pos.x + 2, pos.y + 3), IM_COL32(0, 0, 0, alpha * 3 / 4), text);
            }
            dl->AddText(font, size, pos, color, text);
        }

        void drawHealthBar(ImDrawList *dl, ImFont *font, ImVec2 pos, float width, float height,
                           FighterComponent *f, bool mirrored, ImU32 accent)
        {
            float frac  = glm::clamp(f->currentHealth / f->maxHealth, 0.0f, 1.0f);
            float trail = glm::clamp(f->healthTrail / f->maxHealth, 0.0f, 1.0f);

            // Frame
            dl->AddRectFilled(ImVec2(pos.x - 3, pos.y - 3), ImVec2(pos.x + width + 3, pos.y + height + 3), IM_COL32(0, 0, 0, 170), 5.0f);
            dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(45, 18, 18, 220), 4.0f);

            auto span = [&](float t) {
                return mirrored ? std::make_pair(pos.x + width * (1.0f - t), pos.x + width)
                                : std::make_pair(pos.x, pos.x + width * t);
            };
            // Recent damage (lags behind)
            auto tr = span(trail);
            dl->AddRectFilled(ImVec2(tr.first, pos.y), ImVec2(tr.second, pos.y + height), IM_COL32(255, 240, 200, 230), 4.0f);
            // Current health: green -> yellow -> red
            ImU32 top, bottom;
            if (frac > 0.5f)       { top = IM_COL32(120, 235, 90, 255);  bottom = IM_COL32(40, 160, 50, 255); }
            else if (frac > 0.25f) { top = IM_COL32(255, 215, 70, 255);  bottom = IM_COL32(205, 140, 20, 255); }
            else                   { top = IM_COL32(255, 90, 70, 255);   bottom = IM_COL32(170, 30, 25, 255); }
            auto hs = span(frac);
            if (hs.second > hs.first)
                dl->AddRectFilledMultiColor(ImVec2(hs.first, pos.y), ImVec2(hs.second, pos.y + height), top, top, bottom, bottom);
            dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(255, 255, 255, 90), 4.0f);

            // Name + knockdown markers
            const char *name = f->characterName.c_str();
            ImVec2 ns = font->CalcTextSizeA(22.0f, FLT_MAX, 0.0f, name);
            float nameX = mirrored ? pos.x + width - ns.x : pos.x;
            dl->AddText(font, 22.0f, ImVec2(nameX + 1, pos.y - 28), IM_COL32(0, 0, 0, 180), name);
            dl->AddText(font, 22.0f, ImVec2(nameX, pos.y - 29), IM_COL32(255, 255, 255, 255), name);
            float accentX = mirrored ? nameX - 14.0f : nameX + ns.x + 6.0f;
            dl->AddRectFilled(ImVec2(accentX, pos.y - 21), ImVec2(accentX + 8, pos.y - 13), accent, 2.0f);
            for (int i = 0; i < f->knockdownCount && i < FighterComponent::MAX_KNOCKDOWNS; i++)
            {
                float cx = mirrored ? pos.x + width - 8.0f - i * 16.0f : pos.x + 8.0f + i * 16.0f;
                dl->AddCircleFilled(ImVec2(cx, pos.y + height + 12.0f), 5.0f, IM_COL32(255, 80, 60, 255));
            }
        }

        void drawPopups(ImDrawList *dl, ImFont *font)
        {
            if (!cameraEntity || popups.empty()) return;
            auto *cam = cameraEntity->getComponent<CameraComponent>();
            if (!cam) return;
            ImGuiIO &io = ImGui::GetIO();
            glm::ivec2 size((int)io.DisplaySize.x, (int)io.DisplaySize.y);
            glm::mat4 VP = cam->getProjectionMatrix(size) * cam->getViewMatrix();
            for (const auto &p : popups)
            {
                glm::vec4 clip = VP * glm::vec4(p.worldPos + glm::vec3(0.0f, p.age * 0.6f, 0.0f), 1.0f);
                if (clip.w <= 0.01f) continue;
                glm::vec2 ndc = glm::vec2(clip) / clip.w;
                ImVec2 screen((ndc.x * 0.5f + 0.5f) * io.DisplaySize.x, (0.5f - ndc.y * 0.5f) * io.DisplaySize.y);
                float alpha = glm::clamp(1.0f - (p.age - 0.6f) / 0.5f, 0.0f, 1.0f);
                float pop = 1.0f + 0.35f * glm::clamp(1.0f - p.age * 6.0f, 0.0f, 1.0f); // quick scale "pop"
                ImU32 col = (p.color & ~IM_COL32_A_MASK) | ((ImU32)(alpha * 255) << IM_COL32_A_SHIFT);
                textCentered(dl, font, p.size * pop, screen, col, p.text.c_str(), true);
            }
        }
    };

} // namespace our
