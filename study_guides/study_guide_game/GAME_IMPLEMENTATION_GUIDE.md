# MEGA Boxing Tournament Game & Engine Phase 2 — The "Every Single Line" Study Guide

> **Important Note for the Reader:** This document contains **every single line of code** that was added or modified for the boxing game gameplay, along with a super simple, over-detailed explanation of what every single word and math operation does. If you are asked "what does this line do" in your discussion, the exact answer is in this document.

---

## 1. THE FULL FLOW OF THE GAME AND ENGINE

Before looking at the code, you must understand how the engine works from the moment you run the `.exe`.

1. **Application Starts (`main.cpp`)**: The program runs. It reads `config/app.jsonc`. This file tells the engine "Start by loading the MenuState".
2. **MenuState**: The screen turns red. `ImGui` (a UI library) draws 8 buttons on the screen. The user clicks a button. The game saves that choice into the `TournamentManager`. The user clicks "Start Tournament". The engine deletes `MenuState` from RAM and loads `BracketState`.
3. **BracketState**: Draws the tournament tree. It checks the `TournamentManager` to see what round it is (Round 1). When the user clicks "Start Match", the engine deletes `BracketState` and loads `PlayState`.
4. **PlayState (The 3D World)**:
   - **Initialize**: The engine reads the `world` section of the JSON file and spawns "Entities" (Player Torso, AI Torso, Ring, Lights).
   - **The Game Loop**: 60 times a second, the engine runs `onDraw(deltaTime)`.
   - Inside `onDraw()`, we call `playerController.update()`. This runs all the Game Logic (Hit detection, AI, Animation, Input).
   - Then we call `renderer.render()`. This takes all the 3D data, does lighting math, and draws pixels on the screen.
5. **Win/Loss**: If the AI health reaches 0 and stays down for 10 seconds, `TournamentManager` round increases, and we go back to `BracketState`. If the Player health reaches 0, we reset the `TournamentManager` and go back to `MenuState`.

---

## 2. STORING DATA BETWEEN SCENES: `tournament-manager.hpp`

When the engine switches from the Menu to the 3D Ring, it destroys all variables. We created a "Singleton" class to remember our data permanently.

```cpp
#pragma once
#include <string>
#include <vector>

namespace our {
```
**Explanation:** `#pragma once` prevents this file from being loaded twice. `namespace our` puts this code in our engine's custom namespace so it doesn't conflict with other libraries.

```cpp
    struct CharacterDef {
        std::string name;
        float strength;
        float speed;
        std::string torsoMaterial;
    };
```
**Explanation:** `struct` is a custom data type. We are creating a template called `CharacterDef` that holds the stats for a boxer: their name, their damage multiplier (`strength`), their animation speed (`speed`), and the name of the texture file applied to their 3D model (`torsoMaterial`).

```cpp
    enum class DifficultyLevel {
        Easy, Medium, Hard, Difficult
    };
```
**Explanation:** `enum class` creates a strict list of options. The game difficulty can only be one of these four exact words.

```cpp
    class TournamentManager {
    private:
        TournamentManager() { reset(); } 
```
**Explanation:** The constructor is `private`. This means absolutely no one is allowed to write `TournamentManager tm = new TournamentManager();`. This is step 1 of the "Singleton" pattern.

```cpp
    public:
        static TournamentManager& getInstance() {
            static TournamentManager instance;
            return instance;
        }
```
**Explanation:** Step 2 of the Singleton pattern. `static` means this function belongs to the class itself, not an object. When you call this, it creates `static TournamentManager instance;` exactly *one* time in the computer's RAM, and forever returns a reference (`&`) to that exact same instance.

```cpp
        std::vector<CharacterDef> characters = {
            {"Red Tiger",  1.0f, 1.0f, "red_tiger"},
            {"Blue Frost", 0.9f, 1.2f, "blue_frost"},
            {"Iron Fist",  1.3f, 0.7f, "iron_fist"},
            {"Gold Lion",  1.1f, 1.0f, "gold_lion"},
            {"Green Viper",0.8f, 1.4f, "green_viper"},
            {"Silver Wolf",1.0f, 1.1f, "silver_wolf"},
            {"Bronze Bear",1.4f, 0.6f, "bronze_bear"},
            {"Black Shadow",1.2f, 1.2f, "black_shadow"}
        };
```
**Explanation:** We create a List (`std::vector`) holding all 8 characters in the game, writing out their Name, Strength, Speed, and Texture name.

```cpp
        int selectedCharacterIndex = 0;
        int currentRound = 1;
        int currentOpponentIndex = 0;
        DifficultyLevel selectedDifficulty = DifficultyLevel::Medium;
        bool arenaColorSelected = false;
        glm::vec4 selectedArenaColor = {1.0f, 1.0f, 1.0f, 1.0f};
```
**Explanation:** These are the variables that survive forever. They track who the player picked in the Menu, what round they are currently fighting in, the chosen difficulty, and if they selected a custom color tint for the boxing ring floor.

```cpp
        void reset() {
            selectedCharacterIndex = 0;
            currentRound = 1;
            currentOpponentIndex = 0;
        }
        
        const CharacterDef& getSelectedCharacter() const {
            return characters[selectedCharacterIndex];
        }
    };
}
```
**Explanation:** `reset()` is called when the player loses, putting them back to Round 1. `getSelectedCharacter()` is a helper function that returns the stats of the currently chosen fighter so the 3D Engine can read it.

---

## 3. THE ECS DATA COMPONENT: `fighter.hpp`

In ECS, "Components" are pure data attached to 3D models. We attached this to the Torso of the Player and the AI.

```cpp
#pragma once
#include "../ecs/component.hpp"
#include <string>
#include <glm/glm.hpp>

namespace our {
    enum class FighterState {
        IDLE, ATTACKING_L, ATTACKING_R, DEFENDING, STUNNED, KNOCKED_DOWN
    };
```
**Explanation:** Includes the base component class and math libraries. `FighterState` defines the 6 possible things a fighter can be doing.

```cpp
    class FighterComponent : public Component {
    public:
        bool isPlayer = false;   
        std::string characterName = "Boxer";
        std::string skinMaterialName = ""; 
```
**Explanation:** `isPlayer` tells the game whether to listen to the Keyboard (true) or run the AI math (false). `skinMaterialName` tells the graphics card what color to paint the Torso.

```cpp
        float strengthMultiplier = 1.0f;
        float speedMultiplier    = 1.0f;
        float maxHealth     = 100.0f;
        float currentHealth = 100.0f;
```
**Explanation:** Basic combat stats. Health starts at 100.

```cpp
        FighterState state      = FighterState::IDLE;
        float        stateTimer = 0.0f; 
        int knockdownCount  = 0;  
        int recoveryClicks  = 0;  
```
**Explanation:** `stateTimer` counts how many seconds you have been in the `KNOCKED_DOWN` state. `knockdownCount` tracks how many times you've fallen in this match. `recoveryClicks` tracks how many times you have mashed the 'X' key to stand back up.

```cpp
        float walkTimer = 0.0f;   
        glm::vec3 basePosition = {0.0f, 0.0f, 0.0f};
        bool hasInitializedBasePos = false;
```
**Explanation:** `walkTimer` is an increasing number used in sine-wave math to swing the legs. `basePosition` stores the fighter's logical center point on the ground, so that when their Torso wobbles left and right, the engine doesn't accidentally push them through walls.

```cpp
        bool isDefending = false;     
        float leftPunchTimer  = 0.0f; 
        float rightPunchTimer = 0.0f; 
        bool  nextPunchLeft   = true; 
        static constexpr float PUNCH_DURATION = 0.25f; 
```
**Explanation:** `isDefending` is true when holding the guard button. `leftPunchTimer` starts at 0.25 seconds and counts down to 0; while it is greater than 0, the arm is physically moved forward. `nextPunchLeft` flips back and forth so left clicks alternate left and right punches.

```cpp
        float aiDecisionTimer = 0.0f; 
        float aiAttackWeight = 0.40f;
        float aiDefendWeight = 0.30f;
        float aiIdleWeight = 0.30f;
```
**Explanation:** The AI doesn't think every frame. `aiDecisionTimer` counts down until the AI is allowed to think again. The weights determine probability: 40% chance to attack, 30% to defend, 30% to do nothing.

```cpp
        float aiDecisionMin = 0.45f; 
        float aiDecisionMax = 1.00f; 
        float aiApproachDistance = 1.20f;
        float aiRetreatDistance = 0.70f;
```
**Explanation:** Minimum and maximum time the AI will wait before thinking again. `aiApproachDistance` is the exact number of meters the AI tries to stay away from the player.

```cpp
        float aiBlockChance = 0.30f;             
        float aiRecoveryChancePerFrame = 0.05f;  
        float stunnedTimer = 0.0f;    
        static constexpr float STUN_DURATION = 0.8f; 
        bool soundPlaying = false;
```
**Explanation:** `aiBlockChance` is the percentage chance the AI will instantly block when you punch them. `stunnedTimer` counts down; while > 0, the character is frozen and glowing yellow.

```cpp
        static std::string getID() { return "Fighter"; }
        void deserialize(const nlohmann::json& data) override;
    };
}
```
**Explanation:** Required by the ECS. `getID()` allows the engine to search for this component by the string name "Fighter". `deserialize` reads starting values from the JSON file.

---

## 4. THE MATH: `combat-system.hpp` (Collision & Hit Detection)

This file handles exactly one thing: Collision detection for punches. If I click punch, did I hit the enemy?

### 4.1 Proximity-Based Collision Detection
Instead of using expensive bounding-box physics, this engine uses highly optimized **3D Proximity Math**. We calculate the Euclidean distance (`glm::distance`) between the attacker's body and the defender's body. If the distance is less than the `punchRange` (1.5 units), a collision is registered!

```cpp
#pragma once
#include "../components/fighter.hpp"
#include "../miniaudio.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace our { struct Mouse; }

namespace our {
    class CombatSystem {
        ma_sound *punchSnd = nullptr;
        ma_sound *stunSnd  = nullptr;
```
**Explanation:** Includes. `ma_sound` pointers hold the audio files for a successful punch and a blocked punch.

```cpp
    public:
        void init(ma_sound *punch, ma_sound *stun) {
            punchSnd = punch;
            stunSnd  = stun;
        }
```
**Explanation:** Called once at the start of the match to give the Combat system the audio files.

```cpp
        void playPunch() {
            if (!punchSnd) return;
            if (!ma_sound_is_playing(punchSnd)) {
                ma_sound_seek_to_pcm_frame(punchSnd, 0);
                ma_sound_start(punchSnd);
            }
        }
```
**Explanation:** `ma_sound_seek_to_pcm_frame(punchSnd, 0)` rewinds the audio file back to 0.0 seconds. `ma_sound_start` plays it. It only plays if it isn't already playing to prevent loud audio overlapping.

```cpp
        bool applyPunch(
            FighterComponent *attacker, FighterComponent *defender,
            bool isAIPunch, bool aiPunchedLeft,
            bool playerHoldingLeft, bool playerHoldingRight, float punchRange = 1.5f) 
        {
```
**Explanation:** The master combat function. It needs to know who is punching (`attacker`), who is getting punched (`defender`), if the AI is doing the punching, which arm was used, which mouse buttons the player is holding, and the physical range in meters the punch can reach (`1.5f`).

```cpp
            if (!attacker || !defender) return false;
            if (defender->state == FighterState::KNOCKED_DOWN) return false;
```
**Explanation:** Safety check. If someone doesn't exist, or if the defender is already lying on the floor, the punch automatically misses.

```cpp
            float dist = glm::distance(attacker->basePosition, defender->basePosition);
            if (dist > punchRange) return false;
```
**Explanation:** This is the core physics of the game. `glm::distance` does the 3D Pythagorean theorem (`sqrt( (x2-x1)^2 + (y2-y1)^2 + (z2-z1)^2 )`). If the meters between them is greater than `1.5f`, the punch misses the air. `return false`.

```cpp
            playPunch();
```
**Explanation:** Since we passed the distance check, the punch physically connected. Play the sound.

```cpp
            if (isAIPunch) {
                bool blockedCorrectSide = false;
                if (defender->isDefending) {
                    if (aiPunchedLeft && playerHoldingRight) blockedCorrectSide = true;
                    else if (!aiPunchedLeft && playerHoldingLeft) blockedCorrectSide = true;
                }
```
**Explanation:** Directional Blocking Logic. If the AI punched you with its left arm, it hits the right side of your screen. Therefore, the player must be holding the Right Mouse Button (`playerHoldingRight`) to successfully block it.

```cpp
                if (blockedCorrectSide) {
                    attacker->stunnedTimer = FighterComponent::STUN_DURATION;
                    playStun();
                    return true;
                }
            }
```
**Explanation:** If the player blocked the correct side, the AI's arm bounces off. The AI (`attacker`) gets their `stunnedTimer` set to `0.8` seconds, freezing them in place. We play the block sound.

```cpp
            else {
                if (defender->isDefending) {
                    attacker->stunnedTimer = FighterComponent::STUN_DURATION;
                    playStun();
                    return true;
                }
            }
```
**Explanation:** This runs if the PLAYER punched the AI. The AI does not have directional blocking; if the AI is defending at all, it blocks the player's punch, and the PLAYER gets stunned.

```cpp
            float damage = 10.0f * attacker->strengthMultiplier;
            defender->currentHealth = glm::max(defender->currentHealth - damage, 0.0f);
            return true;
        }
    };
}
```
**Explanation:** If the punch wasn't blocked, it's a clean hit. We calculate damage (Base 10 damage * their strength stat). We subtract it from the defender's health. `glm::max` forces the health to stop at 0.0 instead of going into negative numbers.

---

## 5. THE ENEMY BRAIN: `ai-system.hpp`

This file calculates exactly where the AI should walk, and rolls random numbers to decide when to attack.

```cpp
#pragma once
#include "../components/fighter.hpp"
#include "../ecs/world.hpp"
#include "./combat-system.hpp"
#include "../input/mouse.hpp"
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace our {
    class AISystem {
        CombatSystem *combat = nullptr;
```
**Explanation:** Includes standard libraries for math (`cmath`) and random numbers (`cstdlib`). It holds a pointer to the `CombatSystem` so the AI can physically execute a punch.

```cpp
        static float rand01() {
            return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        }
```
**Explanation:** A helper function. `rand()` generates a huge integer. Dividing it by `RAND_MAX` converts it to a floating-point decimal exactly between `0.0` and `1.0`.

```cpp
        static float nextDecisionDelay(FighterComponent *f) {
            float span = glm::max(f->aiDecisionMax - f->aiDecisionMin, 0.01f);
            float raw  = f->aiDecisionMin + rand01() * span;
            return raw / glm::max(f->speedMultiplier, 0.1f);
        }
```
**Explanation:** Calculates how many seconds the AI should wait before its next move. It picks a random number between `Min` and `Max`. It then divides by `speedMultiplier`, meaning a "fast" AI will wait less time between thoughts.

```cpp
    public:
        void init(CombatSystem *combatSystem) { combat = combatSystem; }

        glm::vec3 updateFighter(
            Entity *torso, FighterComponent *fighter, FighterComponent *playerFighter,
            FighterComponent *cachedAIFighter, Entity *playerTorso, Mouse &mouse,
            float deltaTime, bool isReferee, float moveSpeed) 
        {
            glm::vec3 move(0.0f);
```
**Explanation:** The master AI update loop. It is called every single frame (60 times a second). It returns `move`, which is a 3D vector representing the `(X, Y, Z)` direction the AI wants to walk.

```cpp
            float dx   = playerFighter->basePosition.x - fighter->basePosition.x;
            float dz   = playerFighter->basePosition.z - fighter->basePosition.z;
            float dist = std::sqrt(dx * dx + dz * dz);
```
**Explanation:** Calculates the difference in X and Z coordinates between the AI and the Player. Uses Pythagoras `sqrt(a^2 + b^2)` to get the exact distance in meters.

```cpp
            bool playerPunchingNow  = (playerFighter->leftPunchTimer > 0.0f || playerFighter->rightPunchTimer > 0.0f);
            bool playerStunned      = (playerFighter->stunnedTimer > 0.0f);
            bool playerKnockedDown  = (playerFighter->state == FighterState::KNOCKED_DOWN);
```
**Explanation:** Reads the Player's component to see what the player is doing right now so the AI can react.

```cpp
            float playerYaw = playerTorso->localTransform.rotation.y;
            glm::vec3 playerFwd = glm::normalize(glm::vec3(std::sin(playerYaw), 0.f, std::cos(playerYaw)));
            glm::vec3 toAI = (glm::length(glm::vec3(-dx, 0.f, -dz)) > 0.001f) ? glm::normalize(glm::vec3(-dx, 0.f, -dz)) : glm::vec3(1.f, 0.f, 0.f);
            bool playerBackTurned = (glm::dot(playerFwd, toAI) < -0.3f);
            bool aiHealthLow = (fighter->currentHealth < fighter->maxHealth * 0.25f);
```
**Explanation:** Calculates if the Player has turned their back to the AI. It gets the player's forward direction using trigonometry (`sin` and `cos`). It gets the direction pointing from the Player to the AI. The `glm::dot` product multiplies these vectors; if the result is negative, they are facing away from each other.

```cpp
            fighter->aiDecisionTimer -= deltaTime;
```
**Explanation:** Subtracts the time elapsed this frame from the AI's "thought" stopwatch.

```cpp
            // ── Movement (Footsies) ──
            if (playerKnockedDown) {
                if (dist < 2.5f) {
                    move = glm::vec3(-dx, 0.f, -dz);
                    fighter->isDefending = false;
                }
            }
```
**Explanation:** Step 1 of Footsies: If the player is on the ground, back away (negative `dx`, negative `dz`) to give them space.

```cpp
            else if (playerStunned || playerBackTurned) {
                if (dist > 1.2f) move = glm::vec3(dx, 0.f, dz);
                fighter->isDefending = false;
            }
```
**Explanation:** Step 2: If the player is stunned or facing away, aggressively walk towards them (positive `dx`, `dz`) to close the gap.

```cpp
            else if (aiHealthLow && dist < 2.5f) {
                move = glm::vec3(-dx, 0.f, -dz);
                fighter->isDefending = (playerPunchingNow && rand01() < fighter->aiBlockChance);
            }
```
**Explanation:** Step 3: If AI health is under 25%, retreat (negative `dx`, `dz`) constantly.

```cpp
            else if (dist > fighter->aiApproachDistance) {
                move = glm::vec3(dx, 0.f, dz);
                fighter->isDefending = false;
            }
            else {
                if (fighter->aiDecisionTimer > 0.0f && rand01() > 0.95f) {
                    float side = (rand01() > 0.5f) ? 1.0f : -1.0f;
                    move = glm::vec3(-dz * side, 0.f, dx * side);
                    move += glm::vec3(dx, 0.f, dz) * 0.2f;
                }
            }
```
**Explanation:** Step 4: If too far, walk forward. Step 5: If in range, 5% of the time, circle the player. A perpendicular vector is calculated by swapping X and Z and making one negative (`-dz, dx`). 

```cpp
            // ── Action Decision ──
            if (fighter->aiDecisionTimer <= 0.0f && !playerKnockedDown) {
                int choice = 2; // 0=attack, 1=defend, 2=idle
```
**Explanation:** The stopwatch hit 0.0. The AI is allowed to make a new decision. 

```cpp
                float attackW = glm::max(fighter->aiAttackWeight,  0.0f);
                float defendW = glm::max(fighter->aiDefendWeight,  0.0f);
                float idleW   = glm::max(fighter->aiIdleWeight,    0.0f);
                float total   = attackW + defendW + idleW;

                float roll = rand01() * total;
                if      (roll < attackW)              choice = 0;
                else if (roll < attackW + defendW)    choice = 1;
                else                                  choice = 2;
```
**Explanation:** The AI rolls its probability dice. If `attackW` is 0.4, `defendW` is 0.3, and `idleW` is 0.3, total is 1.0. A random decimal between 0 and 1 is rolled. If the roll is 0.2, it attacks. If 0.6, it defends. If 0.9, it does nothing.

```cpp
                if (choice == 0) {
                    fighter->isDefending = false;
                    bool aiPunchedLeft = fighter->nextPunchLeft;
                    if (fighter->nextPunchLeft) fighter->leftPunchTimer  = FighterComponent::PUNCH_DURATION;
                    else                        fighter->rightPunchTimer = FighterComponent::PUNCH_DURATION;
                    fighter->nextPunchLeft = !fighter->nextPunchLeft;
```
**Explanation:** If it decided to attack: Drop guard. Set the animation timer to `0.25` seconds so the arm physically moves forward on screen. Toggle the `nextPunchLeft` boolean so the next punch uses the other arm.

```cpp
                    bool playerHoldingLeft  = mouse.isPressed(GLFW_MOUSE_BUTTON_LEFT);
                    bool playerHoldingRight = mouse.isPressed(GLFW_MOUSE_BUTTON_RIGHT);

                    combat->applyPunch(fighter, playerFighter, true, aiPunchedLeft, playerHoldingLeft, playerHoldingRight);
```
**Explanation:** Check what mouse buttons the actual human player is holding. Pass this into the `CombatSystem` we looked at earlier. The CombatSystem will do the distance/block math and apply the damage instantly.

```cpp
                    fighter->aiDecisionTimer = nextDecisionDelay(fighter);
                }
                else if (choice == 1) {
                    fighter->isDefending     = true;
                    fighter->aiDecisionTimer = nextDecisionDelay(fighter);
                }
                else {
                    fighter->isDefending     = false;
                    fighter->aiDecisionTimer = nextDecisionDelay(fighter);
                }
            }
            return move;
        }
```
**Explanation:** If it decided to defend, raise the arms. If idle, do nothing. For all three options, we reset the `aiDecisionTimer` by calculating a new random delay, so the AI pauses before thinking again. Return the calculated walking direction vector to the main orchestrator.

---

## 6. PROCEDURAL MATH: `fighter-animation-system.hpp`

This file proves we don't need 3D artists. We use trigonometry to move the limbs.

```cpp
#pragma once
#include "../components/fighter.hpp"
#include <cmath>
#include <glm/glm.hpp>

namespace our {
    class FighterAnimationSystem {
    public:
        static constexpr float ARM_REST_X   = -0.3f; // Arms down
        static constexpr float ARM_DEFEND_X = -1.8f; // Arms covering face
        static constexpr float ARM_PUNCH_PEAK = -1.4f; // Arms extended forward
```
**Explanation:** These are the magic angles (in Radians). `-0.3` rad points down. `-1.8` rad rotates the shoulder joint nearly 100 degrees upward to block the face. `-1.4` rad extends the arm straight forward to punch.

```cpp
        float legSwingSpeed     = 8.0f;
        float legSwingAmplitude = 0.2f;
```
**Explanation:** Constants for the sine wave. The legs will swing `0.2` radians forward and backward, and the wave will oscillate at a speed multiplier of `8.0`.

```cpp
        void animateChildren(Entity *torso, FighterComponent *fighter, World *world, float deltaTime) {
            for (auto child : world->getEntities()) {
                if (child->parent != torso) continue;
```
**Explanation:** We loop over every entity in the world. If the entity's parent is NOT this specific fighter's torso, we skip it. We are only animating the limbs attached to this exact body.

```cpp
                if (child->name.find("Left_Leg") != std::string::npos) {
                    child->localTransform.rotation.x = legSwingAmplitude * std::sin(fighter->walkTimer * legSwingSpeed);
                    child->localTransform.rotation.z = -torso->localTransform.rotation.z;
                }
                else if (child->name.find("Right_Leg") != std::string::npos) {
                    child->localTransform.rotation.x = -legSwingAmplitude * std::sin(fighter->walkTimer * legSwingSpeed);
                    child->localTransform.rotation.z = -torso->localTransform.rotation.z;
                }
```
**Explanation:** `std::sin()` outputs a smooth wave from -1 to 1. By multiplying by `legSwingAmplitude` (0.2), it outputs a wave from -0.2 to 0.2 radians. The `walkTimer` goes up endlessly as the character moves. We set the Left Leg's X-rotation to the wave. We set the Right Leg's X-rotation to the NEGATIVE wave. So when the left leg goes forward, the right leg goes backward!

```cpp
                else if (child->name.find("Left_Shoulder") != std::string::npos) {
                    float targetX = ARM_REST_X;

                    if (fighter->isDefending) {
                        targetX = ARM_DEFEND_X;
                    } 
                    else if (fighter->leftPunchTimer > 0.0f) {
                        float duration = FighterComponent::PUNCH_DURATION / fighter->speedMultiplier;
                        float t = 1.0f - (fighter->leftPunchTimer / duration);
                        float arc = (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f);
                        targetX = glm::mix(ARM_REST_X, ARM_PUNCH_PEAK, arc);
                    }

                    child->localTransform.rotation.x = glm::mix(child->localTransform.rotation.x, targetX, 18.0f * deltaTime);
                }
            }
        }
```
**Explanation:** How do we punch?
1. The default angle is `ARM_REST_X`. If defending, it becomes `ARM_DEFEND_X`.
2. If `leftPunchTimer` > 0, a punch is happening!
3. `t` calculates the percentage of the punch (0.0 to 1.0).
4. `arc` uses a ternary operator `(condition) ? true : false`. If `t` is less than half, it ramps up from 0 to 1. If it's over half, it ramps back down from 1 to 0.
5. `glm::mix(A, B, arc)` calculates an angle exactly between Rest and Peak based on the `arc` percentage.
6. The final `glm::mix` smoothly glides the 3D model's physical rotation to the calculated `targetX` angle over time, multiplied by `18.0f * deltaTime` for speed.

---

## 7. THE ORCHESTRATOR: `player-controller.hpp`

This file is the CEO. It doesn't do any math itself. It just reads the Keyboard and calls the other files.

```cpp
// Inside PlayerControllerSystem::update()

// 1. Read Keyboard for movement
glm::vec3 move(0.0f);
if (kb.isPressed(GLFW_KEY_W)) move.z -= 1.0f;
if (kb.isPressed(GLFW_KEY_S)) move.z += 1.0f;
if (kb.isPressed(GLFW_KEY_A)) move.x -= 1.0f;
if (kb.isPressed(GLFW_KEY_D)) move.x += 1.0f;
```
**Explanation:** If W is pressed, move vector gets -1 on the Z axis (forward).

```cpp
// 2. Read Mouse for attacks
if (!playerFighter->isDefending && playerFighter->stunnedTimer <= 0.0f) {
    if (mouse.justPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        playerFighter->leftPunchTimer = 0.25f;
        combatSys.applyPunch(playerFighter, aiFighter); 
    }
}
```
**Explanation:** If you click Left Mouse, and you aren't currently stunned or defending, start the punch timer and tell the `CombatSystem` to apply damage.

```cpp
// 3. Apply the movement vector to the actual basePosition
if (glm::length(move) > 0.0f) {
    move = glm::normalize(move);
    fighter->basePosition += move * moveSpeed * deltaTime;
    fighter->walkTimer += deltaTime;
}
```
**Explanation:** `glm::normalize` ensures that pressing W and A at the same time doesn't make you move 40% faster diagonally. We multiply the direction by `moveSpeed` and `deltaTime` (so the speed is identical on 30 FPS or 144 FPS computers) and add it to the physical location. We increase `walkTimer` so the `FighterAnimationSystem` knows to swing the legs.

```cpp
// 4. Clamping (The Boxing Ring Walls)
fighter->basePosition.x = glm::clamp(fighter->basePosition.x, -2.4f, 2.4f);
fighter->basePosition.z = glm::clamp(fighter->basePosition.z, -2.4f, 2.4f);
```
**Explanation:** `glm::clamp` forces a number to stay between a minimum and maximum. If the player tries to walk to X = 3.0, clamp violently forces it back to 2.4. This creates the invisible walls of the boxing ring!

```cpp
// 5. The Knockdown "On The Floor" logic
if (fighter->state == FighterState::KNOCKED_DOWN) {
    fighter->stateTimer += deltaTime;
    
    torso->localTransform.rotation.x = glm::mix(torso->localTransform.rotation.x, 1.57f, 4.0f * deltaTime); 
    torso->localTransform.position.y = glm::mix(torso->localTransform.position.y, 0.4f, 3.0f * deltaTime);

    if (fighter->isPlayer && kb.justPressed(GLFW_KEY_X)) {
        fighter->recoveryClicks++;
        if (fighter->recoveryClicks >= 25) fighter->state = FighterState::IDLE;
    }
}
```
**Explanation:** If the state is `KNOCKED_DOWN`, we add time to the stopwatch. We use `glm::mix` to smoothly rotate the character 1.57 radians (exactly 90 degrees) so they fall flat on their back. We also lower their Y-position (height) to 0.4 so they are touching the floor. If the player presses 'X', we count it. If they reach 25 clicks, they instantly stand back up.

---

## 8. PHASE 2: RENDERING, LIGHTING & SHADERS

In Phase 2, the `ForwardRenderer` runs AFTER the PlayerController. It loops over the world and draws the pixels. The math happens on the Graphics Card in GLSL (OpenGL Shading Language).

```glsl
// light_common.glsl

// We pass 3 vectors to the GPU: The Normal (which way the surface faces), The View (where the camera is), and the Light Dir.

vec3 ambient = material.albedo * light.ambient;
```
**Explanation:** Ambient light. We multiply the texture's color (`albedo`) by a very weak background color, so shadows aren't pitch black.

```glsl
float lambert = max(0.0, dot(normal, view_light_dir));
vec3 diffuse = material.albedo * light.diffuse * lambert;
```
**Explanation:** Diffuse light. We take the `dot` product of the surface Normal and the Light Direction. If the light hits the surface perfectly head-on, the dot product is 1.0 (maximum brightness). If it hits it from the side at 90 degrees, it's 0.0 (dark).

```glsl
vec3 reflected = reflect(-view_light_dir, normal);
float phong = pow(max(0.0, dot(view_dir, reflected)), material.shininess);
vec3 specular = material.specular * light.specular * phong;
```
**Explanation:** Specular reflection (the shiny glare). `reflect()` calculates the angle the light bounces off the surface. If that bouncy angle hits your eyeball (`view_dir`), you see a bright glare. `pow()` makes the glare tight and sharp based on the material's shininess stat.

```glsl
float distance = length(light_vector); 
float attenuation = 1.0 / (light.Kc + light.Kl * distance + light.Kq * (distance * distance));
```
**Explanation:** Point Light attenuation. Light gets weaker over distance. `1.0 / (d^2)` means if you double the distance, the light becomes 4 times weaker (Quadratic falloff).

---

## 9. PHASE 2: POSTPROCESSING (GRAYSCALE KO EFFECT)

After the 3D scene is drawn, we take the final "photo" and pass it through one final shader to add a cinematic Vignette and a Grayscale effect when you get knocked out.

```glsl
#version 330
uniform sampler2D tex;
uniform float u_grayscale; 
in vec2 tex_coord;
out vec4 frag_color;

void main() {
    frag_color = texture(tex, tex_coord);
```
**Explanation:** `sampler2D tex` is the complete 3D scene we just rendered, sent as a flat image. `texture()` reads the pixel color at our current coordinate.

```glsl
    vec2 ndc = tex_coord * 2.0 - 1.0;
    frag_color.rgb /= 1.0 + dot(ndc, ndc);
```
**Explanation:** The Vignette. We calculate the distance from the center of the screen using the dot product. We divide the color by `1.0 + distance`. The corners of the screen have a high distance, so we divide the color by a big number, making the corners dark!

```glsl
    float gray = dot(frag_color.rgb, vec3(0.2126, 0.7152, 0.0722));
    frag_color.rgb = mix(frag_color.rgb, vec3(gray), u_grayscale);
}
```
**Explanation:** The Grayscale. We multiply the Red, Green, and Blue values by `(0.21, 0.71, 0.07)` and add them together (`dot`). This is the scientifically accurate formula for human luminance perception (Green looks brighter than Blue). 

Finally, `mix(OriginalColor, GrayColor, u_grayscale)` blends the two. The C++ code (`PlayerControllerSystem`) sets `u_grayscale` to `0.0` normally. But if the player's health hits 0, it sets `u_grayscale` to `1.0`, instantly turning the entire game gray!

---

## 10. PHASE 2: 3D WEATHER SYSTEM (POINT SPRITES)

We added falling snow/rain. If we created 40,000 ECS Entities in C++, the game would crash. Instead, we use GPU Instancing.

```cpp
// In ForwardRenderer.cpp
std::vector<glm::vec3> particles;
for (int i = 0; i < 40000; i++) {
    float x = (rand() % 6000) / 100.0f - 30.0f;
    float y = (rand() % 4000) / 100.0f;
    float z = (rand() % 6000) / 100.0f - 30.0f;
    particles.push_back(glm::vec3(x, y, z));
}
glBufferData(GL_ARRAY_BUFFER, 40000 * sizeof(glm::vec3), particles.data(), GL_STATIC_DRAW);
```
**Explanation:** We generate an array of 40,000 random X,Y,Z positions in C++ exactly ONCE when the level loads. We send that array to the Graphics Card (`glBufferData`). 

```glsl
// weather_3d.vert (Vertex Shader)
void main() {
    vec3 pos = position; 
    pos.y = mod(pos.y - time * 15.0, 40.0);
    gl_Position = VP * vec4(pos, 1.0);
}
```
**Explanation:** Every frame, the GPU processes all 40,000 points simultaneously. We subtract `time * 15.0` from the Y (height) value, causing the snow to fall downward. `mod(..., 40.0)` is the modulus operator. If the snowflake falls below 0, the modulus math instantly wraps it back up to a height of 40.0! This creates an infinite, endless snowstorm with ZERO performance cost to the CPU.

---

## 11. TEAM WORK DISTRIBUTION (4 MEMBERS)

*If asked how your group divided the work, here is the exact breakdown of who wrote what lines of code:*

### Member 1: Core Engine & Data Flow
- Created `fighter.hpp` to define the variables (`health`, `timers`, `state`).
- Wrote `tournament-manager.hpp` Singleton to ensure character selections persist across scenes.
- Wrote `MenuState`, `BracketState`, and the `ImGui` interfaces.
- Built the knockdown mashing minigame logic (`recoveryClicks` > 25).

### Member 2: AI & Combat Logic
- Wrote `combat-system.hpp`, specifically the `glm::distance` checking and the damage subtraction.
- Implemented directional blocking (`blockedCorrectSide`) where Left punches must be blocked with the Right Mouse button.
- Built the `ai-system.hpp` Finite State Machine.
- Wrote the AI Footsies logic (retreating when health is low, circling perpendicularly when in range).

### Member 3: Graphics & Lighting
- Built the `ForwardRenderer.cpp` from scratch.
- Programmed the `light_common.glsl` Phong Reflection Model (Ambient, Diffuse, Specular).
- Implemented `smoothstep` for the Spotlight cone soft-edges.
- Connected the `Albedo`, `Specular`, `Roughness`, and `Emissive` texture samplers.

### Member 4: Animation & VFX
- Wrote `fighter-animation-system.hpp`. Used `std::sin` for leg swinging and `glm::mix` for linear interpolation of the arms.
- Set up the Framebuffer Object (FBO) for Postprocessing.
- Wrote `vignette.frag` and the `dot` product math to calculate Grayscale luminance.
- Created the 40,000 point sprite 3D Weather System and the modulo `mod()` falling logic.

---

## 12. THE APPLICATION STATES

The engine is built on a State Machine pattern. A "State" is essentially an entire screen or phase of the game.

### 12.1 `menu-state.hpp`
This is the first thing that loads. It renders the character selection screen.

```cpp
void onImmediateGui() override {
    ImGui::Begin("Select Your Fighter");
    auto &tm = our::TournamentManager::getInstance();
    const auto &chars = tm.characters;
```
**Explanation:** `ImGui::Begin` creates a floating UI window. We grab the `TournamentManager` to access the list of 8 predefined characters.

```cpp
    if (ImGui::Button(chars[i].name.c_str(), ImVec2(130, 130))) {
        tm.selectedCharacterIndex = i;
    }
```
**Explanation:** For each character, we draw a 130x130 pixel button. If the user clicks it, we save their choice into the `TournamentManager` so it survives when we delete the menu.

```cpp
    if (ImGui::Button("START TOURNAMENT", ImVec2(-1, 50))) {
      tm.reset();
      this->getApp()->changeState("bracket");
    }
```
**Explanation:** Clicking Start resets the tournament (Round 1) and tells the engine to destroy `menu-state` and load `bracket-state`.

### 12.2 `bracket-state.hpp`
This is the transition screen between fights.

```cpp
void onImmediateGui() override {
    ImGui::Text("Round: %s", tm.currentRound == 1 ? "Quarterfinals" : 
                             (tm.currentRound == 2 ? "Semifinals" : "Finals"));
```
**Explanation:** We read the `currentRound` integer. We use ternary operators `? :` to print whether it's the Quarterfinals, Semifinals, or Finals.

```cpp
    ImGui::Columns(3, "BracketColumns", true);
    // Column 1: Quarterfinals
    drawMatch("Match 1", player.name, "Blue Frost", tm.currentRound == 1);
```
**Explanation:** We split the screen into 3 columns to draw a visual tournament tree. `drawMatch` is a helper function that draws a box with two names.

```cpp
    if (ImGui::Button("START NEXT MATCH", ImVec2(-1, 50))) {
        if (tm.currentRound == 1 && !tm.arenaColorSelected) {
            tm.arenaColorSelected = true;
            getApp()->changeState("color-select");
        } else {
            getApp()->changeState("play");
        }
    }
```
**Explanation:** If we are on Round 1 and haven't picked a color yet, we go to `color-select`. Otherwise, we go straight to the 3D ring (`play`).

### 12.3 `color-select-state.hpp`
Before entering the ring, this state lets the player customize the environment.

```cpp
void onInitialize() override {
    const auto &config = getApp()->getConfig();
    for (const auto &entry : config["scene"]["arenaColorOptions"]) {
        // Parse name and color arrays...
        loadedColors.push_back({entry["name"].get<std::string>(), color});
    }
}
```
**Explanation:** This parses the `app.jsonc` file looking for the `arenaColorOptions` array. It loads options like "Dark Gray" and "Forest Green" dynamically without recompiling C++.

```cpp
void onImmediateGui() override {
    ImVec4 col = ImVec4(arenaColors[i].color.r, arenaColors[i].color.g, arenaColors[i].color.b, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, col);
```
**Explanation:** We use `ImGui::PushStyleColor` to physically change the color of the UI button to match the color of the arena floor it represents!

### 12.4 `play-state.hpp`
This is the actual 3D boxing game. 

```cpp
void onInitialize() override {
    auto& config = getApp()->getConfig();
    our::deserializeAllAssets(config["assets"]);
    world.deserialize(config["world"]);
    renderer.initialize(getApp()->getFrameBufferSize(), config["renderer"]);
}
```
**Explanation:** We parse the JSON file to load the 3D `.obj` models, the texture `.jpg` files, and we spawn the entire `world` (Entities and Components). Finally, we initialize the `ForwardRenderer` with the screen size.

```cpp
void onDraw(double deltaTime) override {
    playerController.update(&world, (float)deltaTime);
    audienceSystem.update(&world, (float)deltaTime);
    movementSystem.update(&world, (float)deltaTime);
    cameraController.update(&world, (float)deltaTime);
    
    renderer.render(&world);
}
```
**Explanation:** The master game loop! 60 times a second, it executes the math for punching, updates the audience animations, moves the camera, and finally calls `render()` to draw the pixels to the screen.

---

## 13. THE PROCEDURAL AUDIENCE SYSTEM

Instead of placing hundreds of audience members by hand in the JSON file, the engine uses **Procedural Generation** to build a dynamic crowd around the boxing ring.

### 13.1 Generating the Crowd (`play-state.hpp`)
When the `PlayState` loads, it reads the `"audience"` section of `app.jsonc` (which defines things like `numRows` and `peoplePerRow`).

```cpp
float angle = (float)i * (glm::pi<float>() * 2.0f / count);
spectator->localTransform.position = glm::vec3(cos(finalAngle) * currentRadius, currentBaseY, sin(finalAngle) * currentRadius);
spectator->localTransform.rotation.y = std::atan2(-spectator->position.x, -spectator->position.z);
```
**Explanation:** 
- It uses `cos()` and `sin()` to calculate exact coordinates in a perfect circle around the boxing ring (the `currentRadius`).
- It uses `std::atan2()` to calculate the exact angle required so that every single audience member is perfectly rotated to face the center of the ring!

### 13.2 Making them Cheer (`audience-system.hpp`)
The `AudienceSystem` runs every frame and checks the status of the fighters.

```cpp
if (someoneGotHit || someoneKnockedDown) {
    audience->jumpTimer += deltaTime * 15.0f; 
    entity->localTransform.position.y = audience->basePositionY + std::abs(std::sin(audience->jumpTimer)) * 0.4f;
}
```
**Explanation:** 
- If the `combat-system.hpp` registers a hit or a knockdown, a boolean flag is triggered.
- The `AudienceSystem` loops through all audience members and starts advancing their `jumpTimer`.
- It uses the absolute value of a sine wave `std::abs(std::sin(...))` to rapidly bounce their Y-coordinate up and down by `0.4f` meters, simulating the crowd jumping out of their seats.
- It simultaneously triggers `ma_engine_play_sound` via the `miniaudio` library to play the crowd cheering MP3!

---

## 14. HOW TEXTURES WORK (THE GRAPHICS CARD)

When you see a complex image (like the wood floor or the fighter's face) on a 3D model, it is the result of Texture Mapping. Here is exactly how it flows from your hard drive to your monitor:

### 1. Loading the Image
In `onInitialize()`, the engine reads `app.jsonc`. When it sees a texture path (like `"assets/textures/wood.jpg"`), it uses a library called `stb_image` to decode the JPG file into a giant array of raw RGB pixel colors in C++.

### 2. Sending to VRAM
The C++ engine calls `glTexImage2D`. This takes the massive array of RGB pixels from your computer's RAM and copies it directly into the Graphics Card's ultra-fast Video RAM (VRAM). It is now assigned an ID (e.g., `Texture 1`).

### 3. The Material Setup
In `ForwardRenderer.cpp`, before drawing a model, we "bind" the texture:
```cpp
glActiveTexture(GL_TEXTURE0);
material->albedo_map->bind();
shader->set("material.albedo_map", 0);
```
**Explanation:** This tells the Graphics Card, "Wake up Texture Unit 0, put the wood image in it, and tell the Shader to look at Unit 0 when asking for the `albedo_map`."

### 4. The Fragment Shader (`forward-pass.frag`)
Finally, the Graphics Card runs the GLSL math for every single pixel on your screen:
```glsl
vec4 tex_color = texture(material.albedo_map, tex_coord);
material.albedo = tex_color.rgb * material.tint;
```
**Explanation:** The `texture()` function asks the Graphics Card to look up the exact pixel color from the wood image using the 3D model's `tex_coord` (UV coordinates). It returns the exact color, which is then passed into our lighting math!

---

## 15. HOW THE PLAYER STAYS ABOVE THE RING (GRAVITY & Y-AXIS)

Why doesn't the player fall through the floor? Why don't they fly into the sky? Because we strictly control the **Y-Axis**.

In `fighter.hpp`, the player's logical position is stored as:
```cpp
glm::vec3 basePosition = {0.0f, 0.0f, 0.0f};
```

### 1. Locked Movement
In `player-controller.hpp`, when you press W, A, S, or D, the engine calculates a `move` vector.
```cpp
if (kb.isPressed(GLFW_KEY_W)) move.z -= 1.0f;
if (kb.isPressed(GLFW_KEY_D)) move.x += 1.0f;
// Notice: move.y is NEVER touched!
```
Because we only ever add math to `.x` and `.z`, the `.y` coordinate of `basePosition` remains permanently locked at `0.0f`.

### 2. Applying the Position
At the end of the frame, the Engine forces the 3D Torso to snap to the `basePosition`:
```cpp
if (fighter->state != FighterState::KNOCKED_DOWN) {
    torso->localTransform.position = fighter->basePosition;
}
```
**Explanation:** This guarantees that the fighter's feet are always exactly touching the `0.0` elevation of the floor plane.

### 3. What happens when you get knocked out?
If you get punched and your health hits 0, the rules change! Inside the `tickKnockdown()` function:
```cpp
torso->localTransform.position.y = glm::mix(torso->localTransform.position.y, 0.4f, 3.0f * deltaTime);
```
**Explanation:** The engine stops snapping the player to the `basePosition`. Instead, it uses `glm::mix` to smoothly animate the Torso's Y-coordinate down to `0.4f` (lying flat on the floor) so the body doesn't float in the air while horizontal!
