#pragma once

// ============================================================
// PlayerControllerSystem  (Orchestrator)
//
// Responsibilities after refactor:
//   1. Locate player / AI / camera entities each frame.
//   2. Handle first-person ↔ third-person camera toggle (V key).
//   3. Read player WASD movement and translate it to a move vector.
//   4. Delegate combat to CombatSystem.
//   5. Delegate AI / Referee decisions to AISystem.
//   6. Run the "knockdown fall & recovery" state machine for ALL fighters.
//   7. Apply movement, ring-bounds clamping, and collision pushback.
//   8. Delegate child animation to FighterAnimationSystem.
//   9. Draw the in-game HUD (health bars, mode text, KO overlay).
//
// Sub-systems:
//   - CombatSystem          (combat-system.hpp)
//   - AISystem              (ai-system.hpp)
//   - FighterAnimationSystem(fighter-animation-system.hpp)
// ============================================================

#include "../application.hpp"
#include "../asset-loader.hpp"
#include "../components/fighter.hpp"
#include "../components/mesh-renderer.hpp"
#include "../ecs/world.hpp"
#include "../material/material.hpp"
#include "../miniaudio.h"
#include "./combat-system.hpp"
#include "./ai-system.hpp"
#include "./fighter-animation-system.hpp"
#include "./forward-renderer.hpp"

#include <cmath>
#include <cstdlib>
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

        // Sounds (initialised once in setAudioEngine)
        ma_sound countSound;
        ma_sound punchSound;
        ma_sound stunSound;

        // Cached pointers (valid only within a match; cleared by resetCache())
        FighterComponent *cachedFighter   = nullptr;
        FighterComponent *cachedAIFighter = nullptr;

        // Sub-systems
        CombatSystem           combatSys;
        AISystem               aiSys;
        FighterAnimationSystem animSys;

        // ── Camera ────────────────────────────────────────────────────
        Entity *cameraEntity  = nullptr;
        bool    isFirstPerson = false;

        static constexpr glm::vec3 FP_OFFSET = {0.0f, 1.4f, 0.6f};
        static constexpr glm::vec3 TP_OFFSET = {6.0f, 6.0f, 6.0f};
        static constexpr float     TP_PITCH  = -0.7f;
        static constexpr float     FP_PITCH  = -0.3f;

        // ── Movement constants ────────────────────────────────────────
        float moveSpeed           = 2.0f;
        float rotationSensitivity = 0.005f;
        bool  mouseLocked         = false;

        // ── Result timer (used in drawHUD) ────────────────────────────
        float resultTimer = 0.0f;

    public:
        // ── Life-cycle ────────────────────────────────────────────────
        void enter(Application *a) { app = a; }

        void resetCache()
        {
            cachedFighter   = nullptr;
            cachedAIFighter = nullptr;
            cameraEntity    = nullptr;
            resultTimer     = 0.0f;
            mouseLocked     = false;
        }

        void setAudioEngine(ma_engine *engine)
        {
            audioEngine = engine;
            if (!audioEngine) return;

            ma_sound_init_from_file(audioEngine, "assets/audio/conut_down.mp3", 0, NULL, NULL, &countSound);
            ma_sound_init_from_file(audioEngine, "assets/audio/bunch.mp3",      0, NULL, NULL, &punchSound);
            ma_sound_init_from_file(audioEngine, "assets/audio/stun.mp3",       0, NULL, NULL, &stunSound);

            combatSys.init(&punchSound, &stunSound);
            aiSys.init(&combatSys);
        }

        // ── Per-frame update ──────────────────────────────────────────
        void update(World *world, float deltaTime)
        {
            // ── 1. Find essential entities ────────────────────────────
            FighterComponent *playerFighter = nullptr;
            Entity           *playerTorso   = nullptr;
            FighterComponent *aiFighter      = nullptr;

            for (auto entity : world->getEntities())
            {
                if (auto *f = entity->getComponent<FighterComponent>())
                {
                    if (f->isPlayer) {
                        playerFighter = f;
                        playerTorso   = entity;
                    } else if (entity->name.find("Referee") == std::string::npos) {
                        aiFighter = f;
                    }
                }
                if (entity->getComponent<CameraComponent>())
                    cameraEntity = entity;
            }
            if (!playerFighter || !playerTorso) return;

            cachedFighter   = playerFighter;
            cachedAIFighter = aiFighter;

            // ── 2. Mouse lock ─────────────────────────────────────────
            auto &mouse = app->getMouse();
            if (!mouseLocked) {
                mouse.lockMouse(app->getWindow());
                mouseLocked = true;
            }

            // ── 3. First-person mouse look ────────────────────────────
            if (isFirstPerson &&
                playerFighter->state != FighterState::KNOCKED_DOWN &&
                playerFighter->stunnedTimer <= 0.0f)
            {
                glm::vec2 delta = mouse.getMouseDelta();
                playerTorso->localTransform.rotation.y -= delta.x * rotationSensitivity;
            }

            // ── 4. Camera view toggle (V key) ─────────────────────────
            if (cameraEntity)
            {
                if (isFirstPerson)
                {
                    if (cameraEntity->parent != playerTorso)
                    {
                        cameraEntity->parent = playerTorso;
                        cameraEntity->deleteComponent<FreeCameraControllerComponent>();
                        cameraEntity->localTransform.position  = FP_OFFSET;
                        cameraEntity->localTransform.rotation.x = FP_PITCH;
                        cameraEntity->localTransform.rotation.y = 0.0f;
                    }
                }
                else
                {
                    cameraEntity->parent = nullptr;
                    cameraEntity->localTransform.position = TP_OFFSET;
                    cameraEntity->localTransform.rotation = glm::vec3(TP_PITCH, 0.75f, 0.0f);
                    if (auto *cam = cameraEntity->getComponent<CameraComponent>())
                        cam->fovY = 90.0f * (glm::pi<float>() / 180.0f);
                }

                if (app->getKeyboard().justPressed(GLFW_KEY_V))
                    isFirstPerson = !isFirstPerson;
            }

            auto &kb = app->getKeyboard();

            // ── 5. Player combat mode toggle (T key) ──────────────────
            if (kb.justPressed(GLFW_KEY_T))
            {
                playerFighter->isDefending      = !playerFighter->isDefending;
                playerFighter->leftPunchTimer   = 0.0f;
                playerFighter->rightPunchTimer  = 0.0f;
            }

            // Guard arm flags (only meaningful in defend mode)
            bool pGuardLeft  = playerFighter->isDefending && mouse.isPressed(GLFW_MOUSE_BUTTON_LEFT);
            bool pGuardRight = playerFighter->isDefending && mouse.isPressed(GLFW_MOUSE_BUTTON_RIGHT);
            if (pGuardLeft && pGuardRight) pGuardLeft = false; // right-arm priority

            // ── 6. Player attack inputs → CombatSystem ────────────────
            if (!playerFighter->isDefending &&
                playerFighter->stunnedTimer <= 0.0f &&
                playerFighter->state        != FighterState::KNOCKED_DOWN)
            {
                if (mouse.justPressed(GLFW_MOUSE_BUTTON_LEFT))
                {
                    playerFighter->leftPunchTimer = FighterComponent::PUNCH_DURATION;
                    if (aiFighter)
                        combatSys.applyPunch(playerFighter, aiFighter,
                                             /*isAIPunch=*/false, false, false, false);
                }
                if (mouse.justPressed(GLFW_MOUSE_BUTTON_RIGHT))
                {
                    playerFighter->rightPunchTimer = FighterComponent::PUNCH_DURATION;
                    if (aiFighter)
                        combatSys.applyPunch(playerFighter, aiFighter,
                                             /*isAIPunch=*/false, false, false, false);
                }
            }

            // ── 7. Update ALL fighters ────────────────────────────────
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

                // Tick stun timer
                fighter->stunnedTimer = glm::max(fighter->stunnedTimer - deltaTime, 0.0f);

                // ── Knockdown detection ───────────────────────────────
                if (fighter->currentHealth <= 0.0f &&
                    fighter->state != FighterState::KNOCKED_DOWN)
                {
                    fighter->state         = FighterState::KNOCKED_DOWN;
                    fighter->stateTimer    = 0.0f;
                    fighter->recoveryClicks = 0;
                    fighter->knockdownCount++;
                }

                // ── Knockdown state ───────────────────────────────────
                if (fighter->state == FighterState::KNOCKED_DOWN)
                {
                    tickKnockdown(torso, fighter, kb, deltaTime);
                    animSys.animateChildren(torso, fighter, world, deltaTime,
                                            isFirstPerson, false, false,
                                            cachedFighter, cachedAIFighter, isReferee);
                    continue; // No movement while on the floor
                }

                // Stop count sound if no longer knocked down
                if (fighter->soundPlaying)
                {
                    if (audioEngine) ma_sound_stop(&countSound);
                    fighter->soundPlaying = false;
                }

                // Upright rotation reset
                torso->localTransform.rotation.x =
                    glm::mix(torso->localTransform.rotation.x, 0.0f, 10.0f * deltaTime);

                // ── Determine move vector ─────────────────────────────
                glm::vec3 move(0.0f);

                if (fighter->isPlayer && fighter->stunnedTimer <= 0.0f)
                {
                    move = computePlayerMove(kb, torso, fighter);
                }
                else if (!isReferee && fighter->stunnedTimer <= 0.0f)
                {
                    // AISystem returns a move vector AND mutates fighter decisions
                    move = aiSys.updateFighter(torso, fighter, playerFighter, aiFighter,
                                               playerTorso, mouse, deltaTime, false, moveSpeed);
                }
                else if (isReferee)
                {
                    move = aiSys.updateReferee(torso, fighter, playerFighter, aiFighter, deltaTime);
                }

                // ── Apply movement ────────────────────────────────────
                applyMovement(torso, fighter, move, deltaTime, isReferee, playerFighter, aiFighter);

                // ── Tick punch timers ─────────────────────────────────
                fighter->leftPunchTimer  = glm::max(fighter->leftPunchTimer  - deltaTime, 0.0f);
                fighter->rightPunchTimer = glm::max(fighter->rightPunchTimer - deltaTime, 0.0f);

                // ── Collision pushback ────────────────────────────────
                applyCollisionPushback(torso, fighter, world);

                // ── Animate children ──────────────────────────────────
                animSys.animateChildren(torso, fighter, world, deltaTime,
                                        isFirstPerson, pGuardLeft, pGuardRight,
                                        cachedFighter, cachedAIFighter, isReferee);
            }
        }

        // ── HUD (called from PlayState::onImmediateGui) ───────────────
        // 'renderer' is optional; if non-null, grayscale is toggled on player KO.
        void drawHUD(float deltaTime, ForwardRenderer *renderer = nullptr)
        {
            if (!cachedFighter) return;

            // ── Mode / health panel ───────────────────────────────────
            ImGuiIO &io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.0f, 10.0f),
                                    ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.60f);
            ImGui::SetNextWindowSize(ImVec2(185.0f, 0.0f));
            ImGui::Begin("##modehud", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings);

            if (cachedFighter->state == FighterState::KNOCKED_DOWN)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
                ImGui::SetWindowFontScale(2.0f);
                int cnt = std::max(0, 10 - (int)cachedFighter->stateTimer);
                ImGui::Text("   COUNT: %d", cnt);
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 0.f, 1.f));
                ImGui::Text("   MASH [X] TO GET UP!");
                int required = 25 + (cachedFighter->knockdownCount - 1) * 15;
                ImGui::ProgressBar((float)cachedFighter->recoveryClicks / required, ImVec2(-1.f, 20.f));
                ImGui::Text("   (%d / %d CLICKS)", cachedFighter->recoveryClicks, required);
                ImGui::PopStyleColor();
            }
            else if (cachedFighter->isDefending)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.85f, 1.f, 1.f));
                ImGui::Text("  [DEFEND MODE]");
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::TextUnformatted("  Hold LMB  Guard left arm");
                ImGui::TextUnformatted("  Hold RMB  Guard right arm");
                ImGui::TextUnformatted("  (none)    Full guard");
                ImGui::TextUnformatted("  [T] Switch to Attack");
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.35f, 0.2f, 1.f));
                ImGui::Text("  [ATTACK MODE]");
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::TextUnformatted("  LMB  Left punch");
                ImGui::TextUnformatted("  RMB  Right punch");
                ImGui::TextUnformatted("  [T]  Switch to Defend");
            }

            ImGui::Separator();
            ImGui::Text("Player HP:");
            ImGui::ProgressBar(cachedFighter->currentHealth / cachedFighter->maxHealth, ImVec2(-1.f, 0.f));

            if (cachedAIFighter)
            {
                ImGui::Spacing();
                if (cachedAIFighter->state == FighterState::KNOCKED_DOWN)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
                    int cnt = std::max(0, 10 - (int)cachedAIFighter->stateTimer);
                    ImGui::Text("ENEMY DOWN! COUNT: %d", cnt);
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::Text("Enemy HP:");
                    ImGui::ProgressBar(cachedAIFighter->currentHealth / cachedAIFighter->maxHealth, ImVec2(-1.f, 0.f));
                }
            }
            ImGui::End();

            // ── Weather selector ──────────────────────────────────────
            ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_FirstUseEver);
            ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
            const char *weathers[] = {"Sun", "Rain", "Snow"};
            ImGui::Combo("Weather", &our::g_WeatherMode, weathers, IM_ARRAYSIZE(weathers));
            ImGui::End();

            // ── KO overlay + grayscale ────────────────────────────────
            bool isPlayerDown = (cachedFighter->state == FighterState::KNOCKED_DOWN);
            bool isAIDown     = (cachedAIFighter && cachedAIFighter->state == FighterState::KNOCKED_DOWN);
            
            bool playerKO = (isPlayerDown && cachedFighter->stateTimer >= 11.0f);
            bool aiKO     = (isAIDown && cachedAIFighter->stateTimer >= 11.0f);

            // Toggle grayscale via renderer instantly when someone falls down
            if (renderer)
                renderer->setGrayscale((isPlayerDown || isAIDown) ? 1.0f : 0.0f);

            if (playerKO || aiKO)
            {
                ImGui::SetNextWindowPos(
                    ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::Begin("RESULT", nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
                ImGui::SetWindowFontScale(5.0f);
                if (aiKO) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.f, 1.f));
                    ImGui::Text("KO! YOU WIN!");
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
                    ImGui::Text("KO! YOU LOSE!");
                }
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(1.0f);
                ImGui::Spacing();

                resultTimer += deltaTime;
                if (resultTimer > 3.0f)
                {
                    resultTimer = 0.0f;
                    auto &tm = our::TournamentManager::getInstance();
                    if (aiKO) { tm.currentRound++; tm.currentOpponentIndex = 0; app->changeState("bracket"); }
                    else      { tm.reset(); app->changeState("menu"); }
                }
                ImGui::End();
            }
        }

    private:
        // ── Knockdown fall + recovery state machine ───────────────────
        void tickKnockdown(Entity *torso, FighterComponent *fighter,
                           Keyboard &kb, float deltaTime)
        {
            fighter->stateTimer += deltaTime;

            // Fall animation
            float targetRotX = glm::half_pi<float>();
            torso->localTransform.rotation.x =
                glm::mix(torso->localTransform.rotation.x, targetRotX, 4.0f * deltaTime);
            torso->localTransform.position.y =
                glm::mix(torso->localTransform.position.y, 0.4f, 3.0f * deltaTime);
            torso->localTransform.position.x = fighter->basePosition.x;
            torso->localTransform.position.z = fighter->basePosition.z;

            if (fighter->stateTimer < 11.0f)
            {
                if (fighter->isPlayer)
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
                    aiSys.tickRecovery(fighter, deltaTime, &countSound);
                }
            }
            else
            {
                fighter->stateTimer = 11.0f; // clamp
                stopCountSound(fighter);
            }

            // Start countdown sound once
            if (!fighter->soundPlaying && fighter->stateTimer < 0.5f && audioEngine)
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
                if (audioEngine) ma_sound_stop(&countSound);
                fighter->soundPlaying = false;
            }
        }

        // ── Player WASD movement → move vector ───────────────────────
        glm::vec3 computePlayerMove(Keyboard &kb, Entity *torso, FighterComponent *fighter)
        {
            glm::vec3 move(0.0f);
            if (isFirstPerson)
            {
                float yaw       = torso->localTransform.rotation.y;
                glm::vec3 fwd   = glm::vec3(std::sin(yaw), 0.f, std::cos(yaw));
                glm::vec3 right = glm::vec3(std::cos(yaw), 0.f, -std::sin(yaw));
                if (kb.isPressed(GLFW_KEY_W)) move -= fwd;
                if (kb.isPressed(GLFW_KEY_S)) move += fwd;
                if (kb.isPressed(GLFW_KEY_A)) move -= right;
                if (kb.isPressed(GLFW_KEY_D)) move += right;
            }
            else
            {
                if (kb.isPressed(GLFW_KEY_W)) move.z -= 1.0f;
                if (kb.isPressed(GLFW_KEY_S)) move.z += 1.0f;
                if (kb.isPressed(GLFW_KEY_A)) move.x -= 1.0f;
                if (kb.isPressed(GLFW_KEY_D)) move.x += 1.0f;
            }
            return move;
        }

        // ── Apply movement + rotation + ring bounds ───────────────────
        void applyMovement(Entity *torso, FighterComponent *fighter,
                           glm::vec3 move, float deltaTime,
                           bool isReferee,
                           FighterComponent *playerFighter,
                           FighterComponent *aiFighter)
        {
            bool isMoving = (glm::length(move) > 0.0f);

            if (isMoving)
            {
                move = glm::normalize(move);
                float spd = fighter->isPlayer
                            ? (moveSpeed * fighter->speedMultiplier)
                            : (moveSpeed * 0.7f * fighter->speedMultiplier);
                fighter->basePosition += move * spd * deltaTime;
                fighter->walkTimer    += deltaTime;

                // Ring bounds
                fighter->basePosition.x = glm::clamp(fighter->basePosition.x, -2.4f, 2.4f);
                fighter->basePosition.z = glm::clamp(fighter->basePosition.z, -2.4f, 2.4f);

                // Rotation — face move dir (or fight center for referee)
                if (!fighter->isPlayer || !isFirstPerson)
                {
                    float targetYaw = 0.0f;
                    if (isReferee)
                    {
                        glm::vec3 center = playerFighter->basePosition;
                        if (aiFighter)
                            center = (center + aiFighter->basePosition) * 0.5f;
                        targetYaw = std::atan2(center.x - fighter->basePosition.x,
                                               center.z - fighter->basePosition.z);
                    }
                    else
                    {
                        targetYaw = std::atan2(move.x, move.z);
                    }

                    float diff = targetYaw - torso->localTransform.rotation.y;
                    while (diff >  glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
                    while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
                    torso->localTransform.rotation.y += diff * 12.0f * deltaTime;
                }

                // Body wobble
                float wobble = 0.08f * std::sin(fighter->walkTimer * 8.0f);
                torso->localTransform.rotation.z = wobble;
                float pivotY = -0.9f * 0.351f;
                torso->localTransform.position =
                    fighter->basePosition +
                    glm::vec3(pivotY * std::sin(wobble), pivotY * (1.0f - std::cos(wobble)), 0.0f);
            }
            else
            {
                fighter->walkTimer = 0.0f;
                torso->localTransform.rotation.z =
                    glm::mix(torso->localTransform.rotation.z, 0.0f, 10.0f * deltaTime);

                if (fighter->state != FighterState::KNOCKED_DOWN)
                    torso->localTransform.position = fighter->basePosition;

                // Face correct target when idle
                if (!fighter->isPlayer && fighter->state != FighterState::KNOCKED_DOWN)
                {
                    float targetYaw = 0.0f;
                    if (isReferee && playerFighter)
                    {
                        glm::vec3 center = playerFighter->basePosition;
                        if (aiFighter)
                            center = (center + aiFighter->basePosition) * 0.5f;
                        targetYaw = std::atan2(center.x - fighter->basePosition.x,
                                               center.z - fighter->basePosition.z);
                    }
                    else if (playerFighter)
                    {
                        targetYaw = std::atan2(playerFighter->basePosition.x - fighter->basePosition.x,
                                               playerFighter->basePosition.z - fighter->basePosition.z);
                    }

                    float diff = targetYaw - torso->localTransform.rotation.y;
                    while (diff >  glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
                    while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
                    torso->localTransform.rotation.y += diff * 12.0f * deltaTime;
                }
            }
        }

        // ── Circle-vs-circle collision pushback ───────────────────────
        void applyCollisionPushback(Entity *torso, FighterComponent *fighter, World *world)
        {
            const float radius = 1.0f;
            for (auto other : world->getEntities())
            {
                if (other == torso) continue;
                auto *otherF = other->getComponent<FighterComponent>();
                if (!otherF) continue;

                float dx   = fighter->basePosition.x - other->localTransform.position.x;
                float dz   = fighter->basePosition.z - other->localTransform.position.z;
                float dist = std::sqrt(dx * dx + dz * dz);

                if (dist < radius && dist > 0.001f)
                {
                    float overlap  = radius - dist;
                    glm::vec3 norm = glm::normalize(glm::vec3(dx, 0.f, dz));
                    fighter->basePosition += norm * overlap;
                    torso->localTransform.position.x = fighter->basePosition.x;
                    torso->localTransform.position.z = fighter->basePosition.z;
                }
            }
        }
    };

} // namespace our
