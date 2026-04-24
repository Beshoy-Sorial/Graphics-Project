#pragma once

#include "../application.hpp"
#include "../asset-loader.hpp"
#include "../components/fighter.hpp"
#include "../components/mesh-renderer.hpp"
#include "../ecs/world.hpp"
#include "../material/material.hpp"
#include "../miniaudio.h"

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

  // PlayerControllerSystem handles:
  //   1. WASD movement with smooth torso rotation toward movement dir.
  //   2. Sine-wave leg animation while walking.
  //   3. E key toggles ATTACK / DEFEND mode.
  //   4. In ATTACK mode: left mouse click fires alternating L/R punch animation.
  //   5. In DEFEND mode: both arms are raised to guard the head.
  //   6. drawHUD() renders the mode indicator via ImGui (called from
  //   onImmediateGui). Phase 4: The Knockdown Mini-Game
  // - [x] **Fall Animation:** When a fighter's health hits 0, rotate their Torso
  // 90 degrees backward so they fall flat.
  // - [x] **Referee Timer:** Start a 10-second countdown and draw it on the
  // screen using `ImGui`.
  // - [x] **Recovery Mechanic:** If it's the player, require them to mash the 'X'
  // key a certain number of times to stand back up. Increase required clicks for
  // every subsequent fall.
  // - [x] **Match End Condition:** If the timer runs out before standing up,
  // trigger a KO and end the match.
  class PlayerControllerSystem
  {
    Application *app = nullptr;
    ma_engine *audioEngine = nullptr;
    ma_sound countSound; // Sound handle for the long countdown audio
    ma_sound punchSound; // Sound handle for punch
    ma_sound stunSound;  // Sound handle for stun

    // Cached pointer so HUD can read it without searching entities again
    FighterComponent *cachedFighter = nullptr;
    FighterComponent *cachedAIFighter = nullptr;

    // ── Camera Toggle ─────────────────────────────────────────────────────
    Entity *cameraEntity = nullptr;
    bool isFirstPerson = false;
    float resultTimer = 0.0f; // Time since KO result appeared; member so resetCache() can clear it
    static constexpr glm::vec3 FP_OFFSET = {0.0f, 1.4f, 0.6f}; // In front of face
    static constexpr glm::vec3 TP_OFFSET = {6.0f, 6.0f,
                                            6.0f}; // Broadcaster View (Top-Left)
    static constexpr float TP_PITCH = -0.7f;       // Looking down from corner
    static constexpr float FP_PITCH = -0.3f;

    // ── Movement ──────────────────────────────────────────────────────────
    float moveSpeed = 2.0f;
    float rotationSensitivity = 0.005f;
    bool mouseLocked = false;

    // ── Leg swing ─────────────────────────────────────────────────────────
    float legSwingSpeed = 8.0f;
    float legSwingAmplitude = 0.2f; // ~11 degrees

    // ── Arm poses (local X-axis rotation in radians) ──────────────────────
    static constexpr float ARM_REST_X = -0.3f;     // idle: slight forward lean
    static constexpr float ARM_DEFEND_X = -1.8f;   // defend: arms cover head
    static constexpr float ARM_PUNCH_PEAK = -1.4f; // peak of punch extension

  public:
    // Call before world.clear() to prevent drawHUD() from reading dangling pointers
    // on the first frame of a new match (onImmediateGui runs before update each frame).
    void resetCache() {
      cachedFighter = nullptr;
      cachedAIFighter = nullptr;
      cameraEntity = nullptr;
      resultTimer = 0.0f;
      mouseLocked = false;
    }

    void enter(Application *app) { this->app = app; }

    void setAudioEngine(ma_engine *engine)
    {
      this->audioEngine = engine;
      if (audioEngine)
      {
        // Initialize the countdown sound once here to avoid engine corruption
        ma_sound_init_from_file(audioEngine, "assets/audio/conut_down.mp3", 0,
                                NULL, NULL, &countSound);
        ma_sound_init_from_file(audioEngine, "assets/audio/bunch.mp3", 0,
                                NULL, NULL, &punchSound);
        ma_sound_init_from_file(audioEngine, "assets/audio/stun.mp3", 0,
                                NULL, NULL, &stunSound);
      }
    }

    void update(World *world, float deltaTime)
    {
      // ── 1. Find the Player ───────────────────────────────────────────
      FighterComponent *playerFighter = nullptr;
      Entity *playerTorso = nullptr;
      FighterComponent *aiFighter = nullptr;

      for (auto entity : world->getEntities())
      {
        auto *f = entity->getComponent<FighterComponent>();
        if (f)
        {
          if (f->isPlayer)
          {
            playerFighter = f;
            playerTorso = entity;
          }
          else if (entity->name.find("Referee") == std::string::npos)
          {
            aiFighter = f; // Found the AI opponent
          }
        }
        // Also find the main camera
        if (entity->getComponent<CameraComponent>())
        {
          cameraEntity = entity;
        }
      }
      if (!playerFighter || !playerTorso)
        return;
      cachedFighter = playerFighter; // expose for HUD
      cachedAIFighter = aiFighter;

      // ── Mouse Look Logic (ONLY in First-Person) ──
      auto &mouse = app->getMouse();
      if (!mouseLocked)
      {
        mouse.lockMouse(app->getWindow());
        mouseLocked = true;
      }

      if (isFirstPerson && playerTorso &&
          playerFighter->state != FighterState::KNOCKED_DOWN &&
          playerFighter->stunnedTimer <= 0.0f)
      {
        glm::vec2 mouseDelta = mouse.getMouseDelta();
        playerTorso->localTransform.rotation.y -=
            mouseDelta.x * rotationSensitivity;
      }

      // ── Camera View Logic ──
      if (cameraEntity)
      {
        if (isFirstPerson)
        {
          // First-Person: Attach to torso
          if (cameraEntity->parent != playerTorso)
          {
            cameraEntity->parent = playerTorso;
            cameraEntity->deleteComponent<FreeCameraControllerComponent>();
            cameraEntity->localTransform.position = FP_OFFSET;
            cameraEntity->localTransform.rotation.x = FP_PITCH;
            cameraEntity->localTransform.rotation.y = 0;
          }
        }
        else
        {
          // Far-View: Static broadcaster camera at high-left corner
          cameraEntity->parent = nullptr; // Detach
          cameraEntity->localTransform.position = TP_OFFSET;
          cameraEntity->localTransform.rotation = glm::vec3(TP_PITCH, 0.75f, 0.0f); // Face center from corner

          // Ensure FOV is standard
          if (auto *cam = cameraEntity->getComponent<CameraComponent>())
          {
            cam->fovY = 90.0f * (glm::pi<float>() / 180.0f);
          }
        }

        // Toggle with 'V'
        if (app->getKeyboard().justPressed(GLFW_KEY_V))
        {
          isFirstPerson = !isFirstPerson;
        }
      }

      auto &kb = app->getKeyboard();

      // ── T: toggle combat mode ────────────────────────────────────────
      if (kb.justPressed(GLFW_KEY_T))
      {
        playerFighter->isDefending = !playerFighter->isDefending;
        playerFighter->leftPunchTimer = 0.0f;
        playerFighter->rightPunchTimer = 0.0f;
      }

      // ── Guard state for arm animation (arms UP only when holding button) ──
      bool pGuardLeft =
          playerFighter->isDefending && mouse.isPressed(GLFW_MOUSE_BUTTON_LEFT);
      bool pGuardRight =
          playerFighter->isDefending && mouse.isPressed(GLFW_MOUSE_BUTTON_RIGHT);
      if (pGuardLeft && pGuardRight)
        pGuardLeft = false; // Only one arm at a time, priority to Right arm

      // ── Player attack inputs (only in attack mode, not stunned, and not
      // knocked down) ──
      if (!playerFighter->isDefending && playerFighter->stunnedTimer <= 0.0f &&
          playerFighter->state != FighterState::KNOCKED_DOWN)
      {
        if (mouse.justPressed(GLFW_MOUSE_BUTTON_LEFT))
        {
          playerFighter->leftPunchTimer = FighterComponent::PUNCH_DURATION;
          if (audioEngine) {
            if (!ma_sound_is_playing(&punchSound)) {
              ma_sound_seek_to_pcm_frame(&punchSound, 0);
              ma_sound_start(&punchSound);
            }
          }
          if (aiFighter)
          {
            float dist = glm::distance(playerFighter->basePosition,
                                       aiFighter->basePosition);
            if (dist <= 1.5f)
            {
              if (aiFighter->isDefending)
              {
                // Blocked!
                playerFighter->stunnedTimer = FighterComponent::STUN_DURATION;
                if (audioEngine) {
                  if (!ma_sound_is_playing(&stunSound)) {
                    ma_sound_seek_to_pcm_frame(&stunSound, 0);
                    ma_sound_start(&stunSound);
                  }
                }
              }
              else if (aiFighter->state != FighterState::KNOCKED_DOWN)
              {
                float damage = 10.0f * playerFighter->strengthMultiplier;
                aiFighter->currentHealth =
                    glm::max(aiFighter->currentHealth - damage, 0.0f);
              }
            }
          }
        }
        if (mouse.justPressed(GLFW_MOUSE_BUTTON_RIGHT))
        {
          playerFighter->rightPunchTimer = FighterComponent::PUNCH_DURATION;
          if (audioEngine) {
            if (!ma_sound_is_playing(&punchSound)) {
              ma_sound_seek_to_pcm_frame(&punchSound, 0);
              ma_sound_start(&punchSound);
            }
          }
          if (aiFighter)
          {
            float dist = glm::distance(playerFighter->basePosition,
                                       aiFighter->basePosition);
            if (dist <= 1.5f)
            {
              if (aiFighter->isDefending)
              {
                // Blocked!
                playerFighter->stunnedTimer = FighterComponent::STUN_DURATION;
                if (audioEngine) {
                  if (!ma_sound_is_playing(&stunSound)) {
                    ma_sound_seek_to_pcm_frame(&stunSound, 0);
                    ma_sound_start(&stunSound);
                  }
                }
              }
              else if (aiFighter->state != FighterState::KNOCKED_DOWN)
              {
                float damage = 10.0f * playerFighter->strengthMultiplier;
                aiFighter->currentHealth =
                    glm::max(aiFighter->currentHealth - damage, 0.0f);
              }
            }
          }
        }
      }

      // ── 2. Update ALL Fighters (Player, AI, Referee) ──────────────────
      for (auto torso : world->getEntities())
      {
        auto *fighter = torso->getComponent<FighterComponent>();
        if (!fighter)
          continue;

        if (!fighter->hasInitializedBasePos)
        {
          fighter->basePosition = torso->localTransform.position;
          fighter->hasInitializedBasePos = true;
        }

        glm::vec3 move(0.0f);
        bool isReferee = (torso->name.find("Referee") != std::string::npos);

        // Tick down stun and punch timers every frame
        fighter->stunnedTimer = glm::max(fighter->stunnedTimer - deltaTime, 0.0f);

        // Knockdown Check
        if (fighter->currentHealth <= 0.0f &&
            fighter->state != FighterState::KNOCKED_DOWN)
        {
          fighter->state = FighterState::KNOCKED_DOWN;
          fighter->stateTimer = 0.0f;
          fighter->recoveryClicks = 0;
          fighter->knockdownCount++;
        }

        // If stunned or knocked down, skip input/decision/movement logic
        if (fighter->state == FighterState::KNOCKED_DOWN)
        {
          fighter->stateTimer += deltaTime;

          // Fall animation: Rotate 90 degrees forward (around X axis)
          float targetRotX = glm::half_pi<float>(); // 90 degrees forward
          torso->localTransform.rotation.x = glm::mix(
              torso->localTransform.rotation.x, targetRotX, 4.0f * deltaTime);

          // Sink down to the floor (HIGH value to stay visible on the ring)
          torso->localTransform.position.y =
              glm::mix(torso->localTransform.position.y, 0.4f, 3.0f * deltaTime);
          torso->localTransform.position.x = fighter->basePosition.x;
          torso->localTransform.position.z = fighter->basePosition.z;

          // Recovery Logic
          if (fighter->stateTimer < 11.0f)
          {
            if (fighter->isPlayer)
            {
              if (kb.justPressed(GLFW_KEY_X))
              {
                fighter->recoveryClicks++;
              }
              // Required clicks: 25 + 15 per subsequent knockdown (Way more clicks)
              int required = 25 + (fighter->knockdownCount - 1) * 15;
              if (fighter->recoveryClicks >= required)
              {
                fighter->state = FighterState::IDLE;
                fighter->currentHealth =
                    fighter->maxHealth * 0.5f; // Recover half health
                fighter->stateTimer = 0.0f;
                if (fighter->soundPlaying)
                {
                  ma_sound_stop(&countSound);
                  fighter->soundPlaying = false;
                }
              }
            }
            else
            {
              // AI recovery: simulate mashing 'X' logic
              // Fix: Make it frame-rate independent by scaling the chance with deltaTime
              float clicksPerSecond = fighter->aiRecoveryChancePerFrame * 60.0f;
              if ((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) < (clicksPerSecond * deltaTime))
              {
                fighter->recoveryClicks++;
              }
              int required = 25 + (fighter->knockdownCount - 1) * 15;
              if (fighter->recoveryClicks >= required)
              {
                fighter->state = FighterState::IDLE;
                fighter->currentHealth = fighter->maxHealth * 0.5f;
                // IMPORTANT: Clear stateTimer so they don't trigger KO logic below
                fighter->stateTimer = 0.0f;
                if (fighter->soundPlaying)
                {
                  ma_sound_stop(&countSound);
                  fighter->soundPlaying = false;
                }
              }
            }
          }

          // Match End: KO if timer > 11s (make 10 last 2 seconds for sound delay)
          // ONLY trigger if they are STILL knocked down
          if (fighter->state == FighterState::KNOCKED_DOWN && fighter->stateTimer > 11.0f)
          {
            fighter->stateTimer = 11.0f; // Clamp to 11 for display/logic
            if (fighter->soundPlaying)
            {
              ma_sound_stop(&countSound);
              fighter->soundPlaying = false;
            }
          }

          // Play count sound ONCE at the start of knockdown
          if (!fighter->soundPlaying && fighter->stateTimer < 0.5f)
          {
            if (audioEngine)
            {
              ma_sound_seek_to_pcm_frame(&countSound,
                                         0); // Restart from beginning
              ma_sound_start(&countSound);
              fighter->soundPlaying = true;
            }
          }
        }
        else
        {
          // Safety: Stop count sound if we are NOT in knockdown state but sound
          // is still flagged
          if (fighter->soundPlaying)
          {
            if (audioEngine)
              ma_sound_stop(&countSound);
            fighter->soundPlaying = false;
          }

          if (fighter->stunnedTimer > 0.0f)
          {
            // Stunned: Reset torso rotation to upright just in case
            torso->localTransform.rotation.x = glm::mix(
                torso->localTransform.rotation.x, 0.0f, 10.0f * deltaTime);
          }
          else
          {
            // Normal state: keep upright
            torso->localTransform.rotation.x = glm::mix(
                torso->localTransform.rotation.x, 0.0f, 10.0f * deltaTime);

            // Determine movement intent
            if (fighter->isPlayer)
            {
              if (isFirstPerson)
              {
                // First-Person: WASD relative to player orientation
                float yaw = torso->localTransform.rotation.y;
                glm::vec3 forward = glm::vec3(std::sin(yaw), 0.0f, std::cos(yaw));
                glm::vec3 right = glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw));
                if (kb.isPressed(GLFW_KEY_W))
                  move -= forward;
                if (kb.isPressed(GLFW_KEY_S))
                  move += forward;
                if (kb.isPressed(GLFW_KEY_A))
                  move -= right;
                if (kb.isPressed(GLFW_KEY_D))
                  move += right;
              }
              else
              {
                // Far-View: WASD relative to arena/broadcaster camera
                if (kb.isPressed(GLFW_KEY_W))
                  move.z -= 1.0f;
                if (kb.isPressed(GLFW_KEY_S))
                  move.z += 1.0f;
                if (kb.isPressed(GLFW_KEY_A))
                  move.x -= 1.0f;
                if (kb.isPressed(GLFW_KEY_D))
                  move.x += 1.0f;
              }
            }
            else if (!isReferee)
            {
              // AI movement + decision logic
              float dx = playerFighter->basePosition.x - fighter->basePosition.x;
              float dz = playerFighter->basePosition.z - fighter->basePosition.z;
              float dist = std::sqrt(dx * dx + dz * dz);

              auto rand01 = []() -> float
              {
                return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
              };

              auto nextDecisionDelay = [&]() -> float
              {
                float span = glm::max(fighter->aiDecisionMax - fighter->aiDecisionMin, 0.01f);
                float raw = fighter->aiDecisionMin + rand01() * span;
                return raw / glm::max(fighter->speedMultiplier, 0.1f);
              };

              bool playerPunchingNow = (playerFighter->leftPunchTimer > 0.0f || playerFighter->rightPunchTimer > 0.0f);
              bool playerStunned = (playerFighter->stunnedTimer > 0.0f);
              bool playerKnockedDown = (playerFighter->state == FighterState::KNOCKED_DOWN);

              // Determine if the player's back is turned towards the AI
              float playerYaw = playerTorso->localTransform.rotation.y;
              glm::vec3 playerForward = glm::normalize(glm::vec3(std::sin(playerYaw), 0.0f, std::cos(playerYaw)));
              glm::vec3 toAI = glm::vec3(-dx, 0.0f, -dz);
              if (glm::length(toAI) > 0.001f)
                toAI = glm::normalize(toAI);
              bool playerBackTurned = (glm::dot(playerForward, toAI) < -0.3f); // Angle > 107 degrees away

              bool aiHealthLow = (fighter->currentHealth < fighter->maxHealth * 0.25f);

              if (playerBackTurned && dist <= 1.5f && fighter->aiDecisionTimer > 0.6f)
              {
                fighter->aiDecisionTimer = 0.5f; // Ready to punch soon, but not instantly, avoiding machine-gun punches
              }

              fighter->aiDecisionTimer -= deltaTime;

              // --- 1. Complex Movement / Spacing (Footsies) ---
              if (playerKnockedDown)
              {
                // If player is down, back away to the corner to wait (or just keep distance)
                if (dist < 2.5f)
                {
                  move = glm::vec3(-dx, 0.0f, -dz);
                  fighter->isDefending = false;
                }
              }
              else if (playerStunned || playerBackTurned)
              {
                // Press the advantage immediately!
                if (dist > 1.2f)
                {
                  move = glm::vec3(dx, 0.0f, dz);
                }
                fighter->isDefending = false;
              }
              else if (aiHealthLow && dist < 2.5f)
              {
                // Health is very low: prioritize running away to survive
                move = glm::vec3(-dx, 0.0f, -dz);
                // Don't defend perfectly; give the player a chance to catch them
                fighter->isDefending = (playerPunchingNow && rand01() < fighter->aiBlockChance);
              }
              else if (playerPunchingNow && dist < 2.2f)
              {
                // Run away (backpedal) actively if the player starts swinging
                move = glm::vec3(-dx, 0.0f, -dz);
                fighter->isDefending = (rand01() < fighter->aiBlockChance * 1.5f);
              }
              else if (dist > fighter->aiApproachDistance)
              {
                // Move toward player
                move = glm::vec3(dx, 0.0f, dz);
                fighter->isDefending = false;
              }
              else
              {
                // In exact decision range -> introduce some idle strafing / circling
                if (fighter->aiDecisionTimer > 0.0f)
                {
                  if (rand01() > 0.95f)
                  {
                    // Occasionally strafe left or right to avoid linear combat
                    float side = (rand01() > 0.5f) ? 1.0f : -1.0f;
                    move = glm::vec3(-dz * side, 0.0f, dx * side);
                    // keep a tiny bit of forward pressure
                    move += glm::vec3(dx, 0.0f, dz) * 0.2f;
                  }
                }
              }

              // --- 2. Action Decisions (Attack, Defend, Idle) ---
              if (fighter->aiDecisionTimer <= 0.0f && !playerKnockedDown)
              {
                int choice = 2; // 0 attack, 1 defend, 2 idle

                // Reactive defense (Block or Counter)
                if (playerPunchingNow)
                {
                  if (dist <= 1.8f && rand01() < fighter->aiBlockChance)
                  {
                    choice = 1; // Block!
                  }
                  else if (dist > 1.8f && rand01() < (fighter->aiAttackWeight * 0.5f))
                  {
                    choice = 0; // Try to step in and counter-attack
                    move = glm::vec3(dx, 0.0f, dz);
                  }
                }
                else if (playerStunned)
                {
                  // Extremely high chance to attack when player is stunned
                  choice = (rand01() < 0.8f) ? 0 : 2;
                }
                else if (playerBackTurned && dist <= 1.8f)
                {
                  // If the player is facing away, the AI should unleash a flurry of hits
                  choice = 0; // unconditionally attack their back
                }
                else
                {
                  float attackW = glm::max(fighter->aiAttackWeight, 0.0f);
                  float defendW = glm::max(fighter->aiDefendWeight, 0.0f);
                  float idleW = glm::max(fighter->aiIdleWeight, 0.0f);
                  float totalW = attackW + defendW + idleW;

                  if (totalW < 0.001f)
                  {
                    attackW = 0.4f;
                    defendW = 0.3f;
                    idleW = 0.3f;
                    totalW = 1.0f;
                  }

                  float roll = rand01() * totalW;
                  if (roll < attackW)
                    choice = 0;
                  else if (roll < attackW + defendW)
                    choice = 1;
                  else
                    choice = 2;
                }

                if (choice == 0)
                {
                  // ATTACK
                  fighter->isDefending = false;

                  if (fighter->nextPunchLeft)
                  {
                    fighter->leftPunchTimer = FighterComponent::PUNCH_DURATION;
                  }
                  else
                  {
                    fighter->rightPunchTimer = FighterComponent::PUNCH_DURATION;
                  }
                  fighter->nextPunchLeft = !fighter->nextPunchLeft;

                  if (audioEngine)
                  {
                    if (!ma_sound_is_playing(&punchSound)) {
                      ma_sound_seek_to_pcm_frame(&punchSound, 0);
                      ma_sound_start(&punchSound);
                    }
                  }

                  float hitDist = glm::distance(fighter->basePosition, playerFighter->basePosition);
                  if (hitDist <= 1.5f)
                  {
                    bool aiPunchedLeft = !fighter->nextPunchLeft; // flipped after trigger
                    bool blockedCorrectSide = false;

                    if (playerFighter->isDefending)
                    {
                      if (aiPunchedLeft)
                      {
                        if (mouse.isPressed(GLFW_MOUSE_BUTTON_RIGHT))
                          blockedCorrectSide = true;
                      }
                      else
                      {
                        if (mouse.isPressed(GLFW_MOUSE_BUTTON_LEFT))
                          blockedCorrectSide = true;
                      }
                    }

                    if (blockedCorrectSide)
                    {
                      fighter->stunnedTimer = FighterComponent::STUN_DURATION;
                      if (audioEngine) {
                        if (!ma_sound_is_playing(&stunSound)) {
                          ma_sound_seek_to_pcm_frame(&stunSound, 0);
                          ma_sound_start(&stunSound);
                        }
                      }
                    }
                    else if (playerFighter->state != FighterState::KNOCKED_DOWN)
                    {
                      float damage = 10.0f * fighter->strengthMultiplier;
                      playerFighter->currentHealth =
                          glm::max(playerFighter->currentHealth - damage, 0.0f);
                    }
                  }

                  // Combo mechanic: at high difficulties (high attack weight), AI has a chance to chain an attack quickly
                  if (rand01() < (fighter->aiAttackWeight * 0.3f))
                  {
                    fighter->aiDecisionTimer = 0.6f; // short timer to combo, but allows human reaction time
                  }
                  else
                  {
                    fighter->aiDecisionTimer = nextDecisionDelay();
                  }
                }
                else if (choice == 1)
                {
                  // DEFEND
                  fighter->isDefending = true;
                  fighter->aiDecisionTimer = nextDecisionDelay();
                }
                else
                {
                  // IDLE
                  fighter->isDefending = false;
                  fighter->aiDecisionTimer = nextDecisionDelay();
                }
              }
            }
            else // isReferee
            {
              // Referee movement: Avoid whichever fighter is closest
              float distToPlayer = glm::distance(fighter->basePosition, playerFighter->basePosition);
              float distToAI = 100.0f;

              glm::vec3 dangerPos = playerFighter->basePosition;
              float minDangerDist = distToPlayer;

              if (cachedAIFighter)
              {
                distToAI = glm::distance(fighter->basePosition, cachedAIFighter->basePosition);
                if (distToAI < minDangerDist)
                {
                  minDangerDist = distToAI;
                  dangerPos = cachedAIFighter->basePosition;
                }
              }

              // If a fighter is closer than 2.0 meters, jog away!
              if (minDangerDist < 2.0f)
              {
                glm::vec3 escapeDir = fighter->basePosition - dangerPos;
                escapeDir.y = 0.0f;
                if (glm::length(escapeDir) < 0.01f)
                  escapeDir = glm::vec3(1.0f, 0.0f, 0.0f);

                // Add a slight pull toward the center (0,0) so the referee can circle around instead of getting stuck in a corner
                move = glm::normalize(escapeDir) + glm::vec3(-fighter->basePosition.x, 0.0f, -fighter->basePosition.z) * 0.3f;
              }
            }

            bool isMoving = (glm::length(move) > 0.0f);

            if (isMoving)
            {
              move = glm::normalize(move);
              float currentSpeed = fighter->isPlayer ? (moveSpeed * fighter->speedMultiplier) : (moveSpeed * 0.7f * fighter->speedMultiplier);
              fighter->basePosition += move * currentSpeed * deltaTime;
              fighter->walkTimer += deltaTime;

              // Restrict to ring bounds
              float ringLimitX = 2.4f;
              float ringLimitZ = 2.4f;
              fighter->basePosition.x =
                  glm::clamp(fighter->basePosition.x, -ringLimitX, ringLimitX);
              fighter->basePosition.z =
                  glm::clamp(fighter->basePosition.z, -ringLimitZ, ringLimitZ);

              // Player rotation: Auto-rotate in Far-View, Mouse-rotate in First-Person
              if (!fighter->isPlayer || !isFirstPerson)
              {
                float targetYaw = 0.0f;
                if (isReferee)
                {
                  // Always face the exact center of the active fighters
                  glm::vec3 fightCenter = playerFighter->basePosition;
                  if (cachedAIFighter)
                  {
                    fightCenter = (fightCenter + cachedAIFighter->basePosition) * 0.5f;
                  }
                  float dx = fightCenter.x - fighter->basePosition.x;
                  float dz = fightCenter.z - fighter->basePosition.z;
                  targetYaw = std::atan2(dx, dz);
                }
                else
                {
                  targetYaw = std::atan2(move.x, move.z);
                }

                float diff = targetYaw - torso->localTransform.rotation.y;
                while (diff > glm::pi<float>())
                  diff -= 2.0f * glm::pi<float>();
                while (diff < -glm::pi<float>())
                  diff += 2.0f * glm::pi<float>();
                torso->localTransform.rotation.y += diff * 12.0f * deltaTime;
              }

              float wobbleTheta =
                  0.08f * std::sin(fighter->walkTimer * legSwingSpeed);
              torso->localTransform.rotation.z = wobbleTheta;

              float pivotY = -0.9f * 0.351f;
              torso->localTransform.position =
                  fighter->basePosition +
                  glm::vec3(pivotY * std::sin(wobbleTheta),
                            pivotY * (1.0f - std::cos(wobbleTheta)), 0.0f);
            }
            else
            {
              fighter->walkTimer = 0.0f;
              torso->localTransform.rotation.z =
                  glm::mix(torso->localTransform.rotation.z, 0.0f, 10.0f * deltaTime);

              // Only reset position to base if NOT knocked down
              if (fighter->state != FighterState::KNOCKED_DOWN)
              {
                torso->localTransform.position = fighter->basePosition;
              }

              // If AI or Referee, rotate to face correct target even when standing still
              if (!fighter->isPlayer && fighter->state != FighterState::KNOCKED_DOWN)
              {
                float targetYaw = 0.0f;
                if (isReferee)
                {
                  glm::vec3 fightCenter = playerFighter->basePosition;
                  if (cachedAIFighter)
                  {
                    fightCenter = (fightCenter + cachedAIFighter->basePosition) * 0.5f;
                  }
                  float dx = fightCenter.x - fighter->basePosition.x;
                  float dz = fightCenter.z - fighter->basePosition.z;
                  targetYaw = std::atan2(dx, dz);
                }
                else
                {
                  float dx = playerFighter->basePosition.x - fighter->basePosition.x;
                  float dz = playerFighter->basePosition.z - fighter->basePosition.z;
                  targetYaw = std::atan2(dx, dz);
                }

                float diff = targetYaw - torso->localTransform.rotation.y;
                while (diff > glm::pi<float>())
                  diff -= 2.0f * glm::pi<float>();
                while (diff < -glm::pi<float>())
                  diff += 2.0f * glm::pi<float>();
                torso->localTransform.rotation.y += diff * 12.0f * deltaTime;
              }
            }

            // Tick down punch timers
            fighter->leftPunchTimer =
                glm::max(fighter->leftPunchTimer - deltaTime, 0.0f);
            fighter->rightPunchTimer =
                glm::max(fighter->rightPunchTimer - deltaTime, 0.0f);

            // Physical Collision Detection
            float collisionRadius = 1.0f;
            for (auto otherEntity : world->getEntities())
            {
              if (otherEntity == torso)
                continue;

              auto *otherFighter = otherEntity->getComponent<FighterComponent>();
              if (otherFighter)
              {
                glm::vec3 otherPos = otherEntity->localTransform.position;
                float dx = fighter->basePosition.x - otherPos.x;
                float dz = fighter->basePosition.z - otherPos.z;
                float distance = std::sqrt(dx * dx + dz * dz);

                if (distance < collisionRadius && distance > 0.001f)
                {
                  float overlap = collisionRadius - distance;
                  glm::vec3 normal = glm::normalize(glm::vec3(dx, 0.0f, dz));
                  fighter->basePosition += normal * overlap;

                  torso->localTransform.position.x = fighter->basePosition.x;
                  torso->localTransform.position.z = fighter->basePosition.z;
                }
              }
            }

            // Animate children (legs + arms + head color)
            for (auto child : world->getEntities())
            {
              if (child->parent != torso)
                continue;

              // Head: swap material pointer between skin and skin_yellow (don't
              // mutate shared material!)
              if (child->name.find("Head") != std::string::npos)
              {
                // Hide player's head in First-Person to prevent internal clipping
                if (fighter->isPlayer && isFirstPerson)
                {
                  child->localTransform.scale = glm::vec3(0.0f);
                }
                else
                {
                  if (isReferee)
                  {
                    child->localTransform.scale = glm::vec3(0.152f);
                  }
                  else
                  {
                    child->localTransform.scale = glm::vec3(0.191f); // Original fighter head scale
                  }
                }

                auto *mr = child->getComponent<MeshRendererComponent>();
                if (mr)
                {
                  Material *skinMat = AssetLoader<Material>::get(fighter->skinMaterialName);
                  if (!skinMat)
                    skinMat = AssetLoader<Material>::get("skin"); // Fallback

                  Material *yellowMat = AssetLoader<Material>::get("skin_yellow");
                  if (skinMat && yellowMat)
                  {
                    mr->material =
                        (fighter->stunnedTimer > 0.0f) ? yellowMat : skinMat;
                  }
                }
              }
              else if (child->name.find("Arm") != std::string::npos)
              {
                // Stun effect for arms too
                auto *mr = child->getComponent<MeshRendererComponent>();
                if (mr)
                {
                  Material *skinMat = AssetLoader<Material>::get("skin");
                  Material *yellowMat = AssetLoader<Material>::get("skin_yellow");
                  if (skinMat && yellowMat)
                  {
                    mr->material = (fighter->stunnedTimer > 0.0f) ? yellowMat : skinMat;
                  }
                }
              }
              else if (child->name.find("Left_Leg") != std::string::npos)
              {
                child->localTransform.rotation.x =
                    legSwingAmplitude * std::sin(fighter->walkTimer * legSwingSpeed);
                child->localTransform.rotation.z = -torso->localTransform.rotation.z;
              }
              else if (child->name.find("Right_Leg") != std::string::npos)
              {
                child->localTransform.rotation.x =
                    -legSwingAmplitude * std::sin(fighter->walkTimer * legSwingSpeed);
                child->localTransform.rotation.z = -torso->localTransform.rotation.z;
              }
              else if (child->name.find("Left_Shoulder") != std::string::npos)
              {
                float targetX = ARM_REST_X;

                // Referee Animation: Raise and lower hand during count
                if (isReferee)
                {
                  FighterComponent *downed = nullptr;
                  if (cachedFighter &&
                      cachedFighter->state == FighterState::KNOCKED_DOWN)
                    downed = cachedFighter;
                  else if (cachedAIFighter &&
                           cachedAIFighter->state == FighterState::KNOCKED_DOWN)
                    downed = cachedAIFighter;

                  if (downed)
                  {
                    // Pump arm every second in sync with the count
                    float fraction =
                        downed->stateTimer - std::floor(downed->stateTimer);
                    // Move from rest to defend position and back within each second
                    float pump = (fraction < 0.5f) ? (fraction * 2.0f)
                                                   : ((1.0f - fraction) * 2.0f);
                    targetX = glm::mix(ARM_REST_X, ARM_DEFEND_X, pump);
                  }
                }
                else
                {
                  // Defend mode: arms DOWN by default, UP only when holding LMB (one
                  // at a time)
                  bool guardLeft =
                      fighter->isPlayer
                          ? (pGuardLeft)
                          : (fighter->isDefending); // AI: full guard when defending

                  if (guardLeft)
                  {
                    targetX = ARM_DEFEND_X;
                  }
                  else if (fighter->leftPunchTimer > 0.0f)
                  {
                    float duration = FighterComponent::PUNCH_DURATION / fighter->speedMultiplier;
                    float t = 1.0f - (fighter->leftPunchTimer / duration);
                    float arc = (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f);
                    targetX = glm::mix(ARM_REST_X, ARM_PUNCH_PEAK, arc);
                  }
                }
                child->localTransform.rotation.x = glm::mix(
                    child->localTransform.rotation.x, targetX, 18.0f * deltaTime);

                // Apply stun color to arms (which are children of shoulders)
                for (auto arm : world->getEntities())
                {
                  if (arm->parent != child)
                    continue;
                  auto *mr = arm->getComponent<MeshRendererComponent>();
                  if (mr)
                  {
                    Material *skinMat = AssetLoader<Material>::get("skin");
                    Material *yellowMat = AssetLoader<Material>::get("skin_yellow");
                    if (skinMat && yellowMat)
                    {
                      mr->material = (fighter->stunnedTimer > 0.0f) ? yellowMat : skinMat;
                    }
                  }
                }
              }
              else if (child->name.find("Right_Shoulder") != std::string::npos)
              {
                float targetX = ARM_REST_X;

                // Defend mode: arms DOWN by default, UP only when holding RMB (one at
                // a time)
                bool guardRight =
                    fighter->isPlayer
                        ? (pGuardRight)
                        : (fighter->isDefending); // AI: full guard when defending

                if (guardRight)
                {
                  targetX = ARM_DEFEND_X;
                }
                else if (fighter->rightPunchTimer > 0.0f)
                {
                  float duration = FighterComponent::PUNCH_DURATION / fighter->speedMultiplier;
                  float t = 1.0f - (fighter->rightPunchTimer / duration);
                  float arc = (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f);
                  targetX = glm::mix(ARM_REST_X, ARM_PUNCH_PEAK, arc);
                }
                child->localTransform.rotation.x = glm::mix(
                    child->localTransform.rotation.x, targetX, 18.0f * deltaTime);

                // Apply stun color to arms (which are children of shoulders)
                for (auto arm : world->getEntities())
                {
                  if (arm->parent != child)
                    continue;
                  auto *mr = arm->getComponent<MeshRendererComponent>();
                  if (mr)
                  {
                    Material *skinMat = AssetLoader<Material>::get("skin");
                    Material *yellowMat = AssetLoader<Material>::get("skin_yellow");
                    if (skinMat && yellowMat)
                    {
                      mr->material = (fighter->stunnedTimer > 0.0f) ? yellowMat : skinMat;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    // ── Call this from PlayState::onImmediateGui() ────────────────────────
    void drawHUD(float deltaTime)
    {
      if (!cachedFighter)
        return;

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
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::SetWindowFontScale(2.0f);
        int displayCount = 10 - (int)cachedFighter->stateTimer;
        if (displayCount < 0)
          displayCount = 0;
        ImGui::Text("   COUNT: %d", displayCount);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("   MASH [X] TO GET UP!");
        int required = 25 + (cachedFighter->knockdownCount - 1) * 15;
        ImGui::ProgressBar((float)cachedFighter->recoveryClicks / required,
                           ImVec2(-1.0f, 20.0f));
        ImGui::Text("   (%d / %d CLICKS)", cachedFighter->recoveryClicks,
                    required);
        ImGui::PopStyleColor();
      }
      else if (cachedFighter->isDefending)
      {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.85f, 1.0f, 1.0f));
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
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.2f, 1.0f));
        ImGui::Text("  [ATTACK MODE]");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::TextUnformatted("  LMB  Left punch");
        ImGui::TextUnformatted("  RMB  Right punch");
        ImGui::TextUnformatted("  [T]  Switch to Defend");
      }

      ImGui::Separator();

      // Player Health Bar
      ImGui::Text("Player HP:");
      ImGui::ProgressBar(cachedFighter->currentHealth / cachedFighter->maxHealth,
                         ImVec2(-1.0f, 0.0f));

      // AI Health Bar (if exists)
      if (cachedAIFighter)
      {
        ImGui::Spacing();
        if (cachedAIFighter->state == FighterState::KNOCKED_DOWN)
        {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
          int aiDisplayCount = 10 - (int)cachedAIFighter->stateTimer;
          if (aiDisplayCount < 0)
            aiDisplayCount = 0;
          ImGui::Text("ENEMY DOWN! COUNT: %d", aiDisplayCount);
          ImGui::PopStyleColor();
        }
        else
        {
          ImGui::Text("Enemy HP:");
          ImGui::ProgressBar(cachedAIFighter->currentHealth /
                                 cachedAIFighter->maxHealth,
                             ImVec2(-1.0f, 0.0f));
        }
      }

      ImGui::End();

      // ── Weather Overlay ─────────────────────────────────────────
      ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
      ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
      const char *weathers[] = {"Sun", "Rain", "Snow"};
      ImGui::Combo("Weather", &our::g_WeatherMode, weathers, IM_ARRAYSIZE(weathers));
      ImGui::End();

      // ── Match Result Overlay ───────────────────────────────────────
      bool playerKO = (cachedFighter->state == FighterState::KNOCKED_DOWN &&
                       cachedFighter->stateTimer >= 11.0f);
      bool aiKO = (cachedAIFighter &&
                   cachedAIFighter->state == FighterState::KNOCKED_DOWN &&
                   cachedAIFighter->stateTimer >= 11.0f);

      if (playerKO || aiKO)
      {
        ImGuiIO &resultIO = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(resultIO.DisplaySize.x * 0.5f, resultIO.DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("RESULT", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

        ImGui::SetWindowFontScale(5.0f);
        if (aiKO)
        {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
          ImGui::Text("KO! YOU WIN!");
        }
        else
        {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
          ImGui::Text("KO! YOU LOSE!");
        }
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        // Auto-return to bracket after 3 seconds
        if (playerKO || aiKO)
        {
          resultTimer += deltaTime;
          if (resultTimer > 3.0f)
          {
            resultTimer = 0.0f;
            auto &tm = our::TournamentManager::getInstance();
            if (aiKO)
            {
              // Win: Advance and show bracket
              tm.currentRound++;
              tm.currentOpponentIndex = 0;
              app->changeState("bracket");
            }
            else
            {
              // Loss: Reset and go to menu
              tm.reset();
              app->changeState("menu");
            }
          }
        }

        ImGui::End();
      }
    }
  };

} // namespace our
