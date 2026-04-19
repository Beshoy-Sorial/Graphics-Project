# 🏆 Boxing Tournament Game: Comprehensive Implementation Roadmap

*A highly detailed, step-by-step guide explaining exactly how to program your final Phase 2 requirement from scratch.*

## Table of Contents
1. [External Libraries (Audio)](#1-external-libraries-audio)
2. [Building the ECS Components](#2-building-the-ecs-components)
3. [The Computer AI (Opponent) Logic](#3-the-computer-ai-opponent-logic)
4. [Combat Logic: Attacks, Blocks & Stuns](#4-combat-logic-attacks-blocks--stuns)
5. [The Knockdown Mini-Game](#5-the-knockdown-mini-game)
6. [Character Hierarchy & JSON](#6-character-hierarchy--json)
7. [Camera System](#7-camera-system)
8. [Tournament & Menu Flow](#8-tournament--menu-flow)

---

## 1. External Libraries (Audio)
Your engine has OpenGL, GLM, ImGui, and Model loaders. The only thing missing is an audio system for crowd cheers, referee counts, and punches.

**We will use `miniaudio` because it requires exactly zero setup.**

### Installation Steps:
1. Download [miniaudio.h from GitHub](https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h).
2. Place it in `source/common/`.
3. Create a new file `source/common/audio.cpp` and put this inside:
    ```cpp
    #define MINIAUDIO_IMPLEMENTATION
    #include "miniaudio.h"
    ```
4. In your system/state where you play sounds (e.g., `PlayState`), initialize it:
    ```cpp
    ma_engine engine;
    ma_engine_init(NULL, &engine);
    // When a punch lands:
    ma_engine_play_sound(&engine, "assets/audio/punch.wav", NULL);
    ```

---

## 2. Building the ECS Components
The Entity-Component-System (ECS) separates your variables (Components) from your logic (Systems/States). First, define the new Components inside `source/common/components/`.

### The `FighterComponent`
This holds the state of the fighters (both Player and Computer).

```cpp
enum class FighterState {
    IDLE,
    ATTACKING_L,  // Throwing Left Punch
    ATTACKING_R,  // Throwing Right Punch
    DEFENDING,    // Holding block
    STUNNED,      // Frozen because their attack was blocked
    KNOCKED_DOWN  // Health is 0
};

class FighterComponent : public Component {
public:
    bool isPlayer = false;          // True for user, False for computer
    float maxHealth = 100.0f;
    float currentHealth = 100.0f;
    
    FighterState state = FighterState::IDLE;
    float stateTimer = 0.0f;        // How long they've been in the current state
    
    // Knockdown mini-game variables
    int knockdownCount = 0;         // How many times they have fallen
    int recoveryClicks = 0;         // How many times 'X' has been pressed
    
    // Serialization
    static std::string getID() { return "Fighter"; }
    void deserialize(const nlohmann::json& data) override;
};
```

---

## 3. The Computer AI (Opponent) Logic
Since you are playing against the computer, we need an `AISystem` (or write the logic directly inside `PlayState::onUpdate`). 

### The AI State Machine
The AI should make decisions based on its distance to the player and its timers.

```cpp
void updateAILogic(World* world, float deltaTime) {
    FighterComponent* ai = findAI(world);
    FighterComponent* player = findPlayer(world);
    
    if (ai->state == FighterState::STUNNED || ai->state == FighterState::KNOCKED_DOWN) {
        return; // The AI cannot do anything right now
    }

    // 1. Calculate Distance
    glm::vec3 aiPos = ai->getOwner()->getLocalToWorldMatrix() * glm::vec4(0,0,0,1);
    glm::vec3 playerPos = player->getOwner()->getLocalToWorldMatrix() * glm::vec4(0,0,0,1);
    float distance = glm::distance(aiPos, playerPos);

    // 2. Behavior Logic
    if (distance > 2.0f) {
        // TOO FAR: Walk towards the player
        glm::vec3 dir = glm::normalize(playerPos - aiPos);
        ai->getOwner()->localTransform.position += dir * moveSpeed * deltaTime;
        // Make the AI look at the player!
    } else {
        // IN RANGE: Decide what to do randomly (every 1 second)
        ai->stateTimer -= deltaTime;
        if (ai->stateTimer <= 0.0f) {
            int randomChoice = rand() % 100; // 0 to 99
            
            if (randomChoice < 40) {
                // 40% chance to attack left
                ai->state = FighterState::ATTACKING_L;
                ai->stateTimer = 0.5f; // Attack animation takes 0.5 sec
            } else if (randomChoice < 80) {
                // 40% chance to attack right
                ai->state = FighterState::ATTACKING_R;
                ai->stateTimer = 0.5f;
            } else {
                // 20% chance to block
                ai->state = FighterState::DEFENDING;
                ai->stateTimer = 1.0f; // Block for 1 second
            }
        }
    }
}
```

---

## 4. Combat Logic: Attacks, Blocks & Stuns
Inside your update loop, you need to write exactly how attacks interact with defenders.

### Checking for a Clash
```cpp
// Check if Player is currently attacking the AI:
if (player->state == FighterState::ATTACKING_L || player->state == FighterState::ATTACKING_R) {
    
    // If the distance is close enough to land a punch...
    if (distance < 2.0f) {
        
        // Scenario 1: AI is actively defending!
        if (ai->state == FighterState::DEFENDING) {
            player->state = FighterState::STUNNED;
            player->stateTimer = 1.0f; // Punish the player! Stunned for 1 sec.
            // Play "blocked shield" sound
        } 
        
        // Scenario 2: AI is NOT defending (it's Idle or attacking slower)
        else {
            ai->currentHealth -= 10.0f; // AI takes damage
            player->state = FighterState::IDLE; // Reset so they don't do infinite damage
            // Play "punch hit" sound
        }
    }
}
```
*Note: Make sure to implement the exact reverse logic for when the AI attacks the Player!*

---

## 5. The Knockdown Mini-Game
When either Fighter's health reaches 0, the game breaks into the mini-game.

### Step-by-Step Implementation:
1. **The Fall:**
   ```cpp
   if (player->currentHealth <= 0 && player->state != FighterState::KNOCKED_DOWN) {
       player->state = FighterState::KNOCKED_DOWN;
       player->knockdownCount++; // Starts at 1
       player->recoveryClicks = 0;
       refereeTimer = 10.0f;
       
       // Visually drop them: Rotate the Torso entity -90 degrees on the X-axis
       player->getOwner()->localTransform.rotation.x = glm::radians(-90.0f);
   }
   ```

2. **The Mini-Game Loop (Assuming Player fell):**
   ```cpp
   if (player->state == FighterState::KNOCKED_DOWN) {
       refereeTimer -= deltaTime;
       
       // Calculate how hard it is this time: 1st drop = 15 clicks. 2nd = 30. 3rd = 60.
       int requiredClicks = 15 * std::pow(2, player->knockdownCount - 1);
       
       // Handle input
       if (app->getKeyboard().isJustPressed(GLFW_KEY_X)) {
           player->recoveryClicks++;
       }
       
       // Check Win
       if (player->recoveryClicks >= requiredClicks) {
           player->state = FighterState::IDLE;
           player->currentHealth = player->maxHealth; // Maximum heal!
           player->getOwner()->localTransform.rotation.x = 0; // Stand up
       }
       
       // Check Lose
       if (refereeTimer <= 0.0f) {
           // GAME OVER. You lost the match. Switch to Game Over State.
       }
   }
   ```
*(You will need ImGui to draw huge numbers on the screen showing `refereeTimer` counting down).*

---

## 6. Character Hierarchy & The "8 Characters" Dilemma

To make animations work without importing complex skeletons, you should use **Hierarchical Modeling**. This means you build a boxer out of separate parts.

### The Question: Do I need 8 different `.obj` files for 8 characters?
If you have 8 characters, and each is made of a Torso, Head, LeftArm, and RightArm, that sounds like 32 different `.obj` files! How do we handle this? 

**Option A (The Standard Way):** Yes, you can find or make 8 completely distinct models online and cut them each into parts.

**Option B (The Smart Way - Color Tinting):** You only need **ONE** set of meshes: just a basic `Torso.obj`, `LeftArm.obj`, `RightArm.obj`, and `Head.obj`. To make 8 different characters, you use the exact same meshes, but you apply 8 completely different **Tinted Materials** to them! 
- Character 1 ("Iron Ivan") Uses the base meshes but with a `torso_red` tinted material.
- Character 2 ("Speedy Sam") Uses the *exact same* basic meshes, but with a `torso_blue` tinted material.
You just define them differently in the JSON configs using different tinted colors in the materials section!

### The JSON Structure
Your JSON scene should look like this (notice how we assign a material!):

```json
{
  "name": "Boxer_Torso",
  "components": [
    { "type": "MeshRenderer", "mesh": "torso", "material": "material_ivan" },
    { "type": "Fighter", "isPlayer": true }
  ],
  "children": [
    {
      "name": "Left_Arm",
      "position": [-0.5, 1.0, 0],
      "components": [ { "type": "MeshRenderer", "mesh": "arm", "material": "material_ivan" } ]
    },
    {
      "name": "Right_Arm",
      "position": [0.5, 1.0, 0],
      "components": [ { "type": "MeshRenderer", "mesh": "arm", "material": "material_ivan" } ]
    },
    {
      "name": "Head",
      "position": [0, 2.0, 0],
      "components": [ { "type": "MeshRenderer", "mesh": "head", "material": "material_ivan_face" } ]
    },
    {
      "name": "Left_Leg",
      "position": [-0.3, -1.0, 0],
      "components": [ { "type": "MeshRenderer", "mesh": "leg", "material": "material_ivan" } ]
    },
    {
      "name": "Right_Leg",
      "position": [0.3, -1.0, 0],
      "components": [ { "type": "MeshRenderer", "mesh": "leg", "material": "material_ivan" } ]
    }
  ]
}
```
**Handling Movement (Separate Legs):** You will use a separate `Leg.obj` and add a `Left_Leg` and `Right_Leg` as children (as shown in the JSON above). When the character moves, you apply a sine-wave rotation to their X-axis to make them look like they are walking.

**Animating the arms:** In your C++ code, when `state == ATTACKING_R`, find the child entity representing the Right Arm and change its `localTransform.rotation.x` back and forth using a simple timer. If they fall down, you rotate the Torso 90 degrees on the X-axis, and all children (arms, head, legs) will fall with it automatically!

---

## 7. Camera System
You need to support two views. 

**Component Setup:** Add a CameraComponent to an empty Entity, and set its parent to the Player's Torso entity.

**Inside `PlayState::onUpdate`:**
```cpp
// Toggle Camera Mode
static bool isFirstPerson = false;
if (app->getKeyboard().isJustPressed(GLFW_KEY_V)) {
    isFirstPerson = !isFirstPerson;
}

Entity* cameraEntity = ... // Get your camera entity
if (isFirstPerson) {
    // Put camera right in front of the face
    cameraEntity->localTransform.position = glm::vec3(0.0f, 1.5f, 0.5f);
} else {
    // Put camera behind the back
    cameraEntity->localTransform.position = glm::vec3(0.0f, 2.5f, 5.0f);
}
```

---

## 8. Tournament & Menu Flow
We need to progress from Quarter Final -> Semi Final -> Final.

### Data Structure (The Roster)
Create a global or State-level array of the 8 characters:
```cpp
std::vector<std::string> allFighters = {
    "Rumble Mike", "Blocker Bob", "Speedy Sam", "Iron Ivan",
    "Knockout Nick", "Jab Jimmy", "Titan Tom", "Hook Harry"
};
```

### The Transition Flow
- **State 1: `MenuState`** -> Uses `ImGui::Button()` for you to pick your character. Once picked, remove the player's choice from `allFighters` and shuffle the remaining 7.
- **State 2: `BracketState`** -> Draws an ImGui window showing the tournament bracket. The player presses "START QUARTER FINAL".
- **State 3: `PlayState`** -> Loads the Ring JSON. Spawns your character. Spawns the computer opponent based on the current bracket stage.
- **End of Match:** If you win, change `TournamentStage++`. If `TournamentStage == 3`, you win the game! Go back to `BracketState` for the next match. If you lose, go to `GameOverState`.
