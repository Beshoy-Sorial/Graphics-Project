# Boxing Tournament Game — Ultra Detailed Study Guide

> **Who is this for?** Someone who has never written a game before and needs to understand every single line of code, every design decision, and the entire flow of the program in order to explain it to someone else.

---

## 🗺️ PART 1 — The Big Picture: What Are We Building and How?

### What is the Game?

We are building a **3D first-person boxing tournament game** where:
1. The player picks one of 8 fighters from a menu.
2. A bracket shows the 3-round tournament.
3. Each round loads a 3D arena where the player fights an AI opponent.
4. Win = advance to next round. Lose = game over, return to menu.

### What Engine Are We Using?

We are NOT using Unity, Unreal, or Godot. We are building on top of a **custom C++ OpenGL engine** that was built throughout the semester. It already handles:
- Drawing 3D meshes with shaders.
- Loading scenes from JSON files.
- A camera system.
- An Entity-Component-System (ECS) architecture.

**Our job**: Add a `FighterComponent`, a `PlayerControllerSystem`, a `TournamentManager`, and three game states (Menu, Bracket, Play).

### The Program Flow (State Machine)

The entire game is controlled by a **State Machine**. Think of it as a TV remote — only one channel (state) is active at a time:

```
┌─────────────────────────────────────────────────────────────────────┐
│                         PROGRAM STARTS                              │
│         Loads config/app.jsonc → changes state to "menu"           │
└───────────────────────────┬─────────────────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────────────────┐
│  MenuState  ("menu")                                                │
│  - Draws a solid RED background                                     │
│  - Shows 8 character buttons via ImGui                              │
│  - Player clicks a character → saves to TournamentManager           │
│  - Player clicks "START TOURNAMENT" → changes state to "bracket"   │
└───────────────────────────┬─────────────────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────────────────┐
│  BracketState  ("bracket")                                          │
│  - Reads TournamentManager.currentRound (1, 2, or 3)               │
│  - Draws the tournament tree showing who fights who                 │
│  - "START NEXT MATCH" button → changes state to "play"             │
│  - After Round 3, shows CHAMPION screen → reset → go to "menu"     │
└───────────────────────────┬─────────────────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────────────────┐
│  PlayState  ("play")                                                │
│  - Loads the 3D boxing ring world from config/app.jsonc             │
│  - Reads TournamentManager to set player & AI colors/stats          │
│  - Runs every frame:                                                │
│      → PlayerControllerSystem.update() (combat, animation, AI)     │
│      → ForwardRenderer.render() (draws everything to the screen)   │
│  - Player wins (AI KO'd): currentRound++ → go to "bracket"        │
│  - Player loses (Player KO'd): reset() → go to "menu"             │
└─────────────────────────────────────────────────────────────────────┘
```

### How Does the Frame Loop Work?

In `source/common/application.cpp`, the core game loop runs every frame:

```cpp
// This runs 60 times per second in an infinite loop
while(!glfwWindowShouldClose(window)) {
    glfwPollEvents();           // Step 1: Read mouse/keyboard from OS
    ImGui::NewFrame();          // Step 2: Start a new ImGui overlay frame
    currentState->onImmediateGui(); // Step 3: Draw HUD/menus (ImGui)
    ImGui::Render();            // Step 4: Finalize ImGui commands
    currentState->onDraw(deltaTime); // Step 5: Update game logic + render 3D
    ImGui_ImplOpenGL3_RenderDrawData(); // Step 6: Put ImGui on screen
    glfwSwapBuffers(window);    // Step 7: Flip back buffer → show frame
    keyboard.update();          // Step 8: Update "was just pressed" states
    mouse.update();
}
```

`deltaTime` = time since last frame (usually ~0.016 seconds at 60fps). **All movement is multiplied by deltaTime** so the game runs at the same speed regardless of FPS.

---

## 📁 PART 2 — New Files We Created

| File | Purpose |
|---|---|
| `source/common/components/fighter.hpp` | The FighterComponent data container |
| `source/common/components/fighter.cpp` | Deserialization from JSON |
| `source/common/systems/player-controller.hpp` | All combat, animation, AI logic |
| `source/common/tournament-manager.hpp` | Singleton storing round & character selection |
| `source/states/menu-state.hpp` | Character selection screen |
| `source/states/bracket-state.hpp` | Tournament bracket screen |
| `source/states/play-state.hpp` | The actual 3D fight state |
| `config/app.jsonc` | The 3D world (ring, fighters, camera) in JSON |

---

## 📖 PART 3 — `FighterComponent` (fighter.hpp & .cpp)

This file defines a new **Component** (a data sticky note) we attach to each fighter entity.

### Why a Component and not just a class?

In ECS, the **World** (the scene) contains entities. A system can only find data if it is stored as a component on an entity. By making `FighterComponent` a proper component, the `PlayerControllerSystem` can loop over all entities with `entity->getComponent<FighterComponent>()` to find the player and AI.

### The FighterState Enum

```cpp
// An enum (short for enumeration) is like a multiple-choice list of options.
// A fighter can only ever be in exactly ONE of these states at a time.
enum class FighterState {
    IDLE,           // Doing nothing — just standing
    WALKING,        // Moving across the ring
    PUNCHING,       // Mid-punch animation
    DEFENDING,      // Arms raised to block
    STUNNED,        // Hit a block — briefly frozen
    KNOCKED_DOWN    // Health reached 0 — lying on the floor
};
```

### Key Variables in FighterComponent

```cpp
class FighterComponent : public Component {
public:
    // ── IDENTITY ─────────────────────────────────────────────────────────
    // Is a human pressing keys to control this? If false, the AI controls it.
    bool isPlayer = false;

    // ── HEALTH ───────────────────────────────────────────────────────────
    float maxHealth = 100.0f;       // Starting health (set from JSON)
    float currentHealth = 100.0f;  // Current remaining health

    // ── CHARACTER STATS ──────────────────────────────────────────────────
    std::string characterName = "Fighter";

    // 1.0 = normal. 1.4 = 40% stronger (deals more damage).
    float strengthMultiplier = 1.0f;

    // 1.0 = normal. 1.4 = 40% faster (punches faster, moves faster).
    float speedMultiplier = 1.0f;

    // Name of the torso material (e.g. "torso_red") for color identification
    std::string skinMaterialName = "skin";

    // ── COMBAT TIMERS ────────────────────────────────────────────────────
    // These count DOWN from a starting value each frame.
    // While above 0, the punch animation is playing.
    float leftPunchTimer = 0.0f;
    float rightPunchTimer = 0.0f;

    // How long the fighter is frozen (yellow flash). Set to 1.0 on block.
    float stunnedTimer = 0.0f;

    // ── AI BRAIN ─────────────────────────────────────────────────────────
    // The AI doesn't react every frame — it "thinks" after this timer runs out.
    float aiDecisionTimer = 0.0f;
    bool isDefending = false;    // Is the AI currently blocking?
    bool nextPunchLeft = true;   // Alternates left and right punches

    // ── KNOCKDOWN MINIGAME ───────────────────────────────────────────────
    int knockdownCount = 0;       // How many times has this fighter been KO'd?
    float stateTimer = 0.0f;      // Counts up while knocked down (max = 11s)
    int recoveryClicks = 0;       // How many times player pressed 'X' to stand

    // Prevents referee countdown audio from replaying every frame
    bool soundPlaying = false;

    // Tracks where the fighter started in the world (for hit-distance math)
    glm::vec3 basePosition = {0.0f, 0.0f, 0.0f};
    bool hasInitializedBasePos = false;

    FighterState state = FighterState::IDLE;

    // Constants: These never change. constexpr means the compiler calculates them.
    static constexpr float PUNCH_DURATION = 0.3f; // One punch = 0.3 seconds
    static constexpr float STUN_DURATION  = 1.0f; // Stun lasts 1.0 second
};
```

**The `basePosition` trick:** The torso entity moves around in the world, but its position changes during animation (bobbing, tilting). `basePosition` is set once at the start and only changed by actual movement, giving us a clean center point for measuring distances between fighters.

### How does it load from JSON? (fighter.cpp)

When `app.jsonc` is loaded, the engine calls `deserialize` on our component. We simply read the values from the JSON text and assign them to our variables:

```cpp
void FighterComponent::deserialize(const nlohmann::json& data) {
    if (!data.is_object()) return;
    
    // Read the "isPlayer" true/false from JSON. If it's missing, keep the current value.
    isPlayer = data.value("isPlayer", isPlayer);
    
    // Read stats and names
    characterName = data.value("characterName", characterName);
    skinMaterialName = data.value("skinMaterialName", skinMaterialName);
    strengthMultiplier = data.value("strengthMultiplier", strengthMultiplier);
    speedMultiplier = data.value("speedMultiplier", speedMultiplier);
    
    maxHealth = data.value("maxHealth", maxHealth);
    currentHealth = maxHealth; // Always start at full health
}
```

---

## 📖 PART 4 — `TournamentManager` (tournament-manager.hpp)

### The Problem It Solves

When the game transitions from `MenuState` to `PlayState`, the ECS World is destroyed and rebuilt from scratch. All local variables in states are wiped. We need a way to **remember** which character was selected and which round we are on.

### The Singleton Pattern

A Singleton is a C++ class designed so that **only one instance ever exists** in memory for the entire lifetime of the program. Every file that needs data asks the same singleton for it.

```cpp
namespace our {

    // CharacterDef holds all unique data for one character.
    struct CharacterDef {
        std::string name;          // e.g., "Gold Lion"
        std::string torsoMaterial; // e.g., "torso_yellow" (links to JSON material)
        glm::vec4 buttonColor;     // RGBA color for the menu button (R,G,B,A from 0.0-1.0)
        float strength;            // Damage multiplier
        float speed;               // Punch/movement speed multiplier
    };

    class TournamentManager {
    public:
        // This is how you get the ONE and ONLY instance from anywhere in the code.
        // 'static' means only one copy exists. 'static TournamentManager instance'
        // is created the first time this function is called and lives forever.
        static TournamentManager& getInstance() {
            static TournamentManager instance; // Created ONCE, lives forever
            return instance;                   // Returns a reference (not a copy!)
        }

        int selectedCharacterIndex = 0; // Which fighter did the player click?
        int currentOpponentIndex = 0;
        int currentRound = 1; // 1 = Quarterfinals, 2 = Semifinals, 3 = Finals

        // The roster of all 8 fighters. Each entry is a CharacterDef struct.
        const std::vector<CharacterDef> characters = {
            {"Red Dragon",   "torso_red",    {0.8f,0.1f,0.1f,1.0f}, 1.2f, 1.0f},
            {"Blue Frost",   "torso_blue",   {0.1f,0.3f,0.8f,1.0f}, 1.0f, 1.2f},
            {"Green Viper",  "torso_green",  {0.1f,0.7f,0.1f,1.0f}, 0.9f, 1.4f},
            {"Gold Lion",    "torso_yellow", {0.8f,0.7f,0.1f,1.0f}, 1.4f, 0.8f},
            {"Purple Night", "torso_purple", {0.5f,0.1f,0.7f,1.0f}, 1.1f, 1.1f},
            {"Cyan Storm",   "torso_cyan",   {0.1f,0.7f,0.8f,1.0f}, 1.0f, 1.3f},
            {"Orange Heat",  "torso_orange", {0.9f,0.5f,0.1f,1.0f}, 1.3f, 0.9f},
            {"Black Shadow", "torso_black",  {0.2f,0.2f,0.2f,1.0f}, 1.2f, 1.2f}
        };

        // A helper function to get whichever character is selected.
        const CharacterDef& getSelectedCharacter() const {
            return characters[selectedCharacterIndex];
        }

        // Resets round progress (called when player loses or starts new game)
        void reset() {
            currentRound = 1;
            currentOpponentIndex = 0;
            // Note: We intentionally don't reset selectedCharacterIndex
            // so the player can retry with the same character.
        }

    private:
        // The constructor is PRIVATE. This prevents anyone from writing:
        // TournamentManager tm; // <-- This would FAIL to compile.
        // The only way to get the instance is through getInstance().
        TournamentManager() = default;
    };
}
```

**Why const vector?** The `characters` vector has the keyword `const`, meaning no one can accidentally modify or delete characters from the list. It is read-only.

---

## 📖 PART 5 — `MenuState` (menu-state.hpp)

The menu screen has two jobs: draw the red background and show the character selection grid.

### Background: Solid Red

```cpp
// glClearColor sets what color to fill the screen with when we "erase" it.
// Values are R, G, B, Alpha — each from 0.0 (none) to 1.0 (full).
// 0.8 Red, 0.1 Green, 0.1 Blue → a strong red color.
glClearColor(0.8f, 0.1f, 0.1f, 1.0f);

// GL_COLOR_BUFFER_BIT = clear the color pixels.
// GL_DEPTH_BUFFER_BIT = clear the depth buffer (which object is in front).
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```

### Character Buttons with ImGui

ImGui (Immediate Mode GUI) is a library that lets you draw buttons, text, and windows with just C++ function calls — no HTML, no XML.

```cpp
void onImmediateGui() override {
    ImGuiIO& io = ImGui::GetIO(); // io holds screen size, mouse position, etc.

    // Set where the window appears: centered on screen.
    // ImVec2(0.5f, 0.5f) is the "pivot" — the window's center point is placed
    // at the given screen position.
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f)
    );
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Always);
    ImGui::Begin("Select Your Fighter", nullptr, ImGuiWindowFlags_NoResize);

    // Get a reference to the TournamentManager singleton.
    auto& tm = our::TournamentManager::getInstance();
    const auto& chars = tm.characters;

    // Draw 4 columns in a grid layout.
    ImGui::Columns(4, "CharacterGrid", false);

    // Loop through all 8 characters.
    for (int i = 0; i < (int)chars.size(); ++i) {
        ImGui::PushID(i); // Each UI element needs a unique ID.

        // Set the button color to the character's signature color.
        ImVec4 col = ImVec4(chars[i].buttonColor.r, chars[i].buttonColor.g,
                            chars[i].buttonColor.b, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, col);

        // Slightly brighter version for when mouse hovers over the button.
        ImVec4 hoverCol = ImVec4(col.x*1.2f, col.y*1.2f, col.z*1.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverCol);

        // If this is the currently selected character, draw a thick white border.
        bool isSelected = (tm.selectedCharacterIndex == i);
        if (isSelected) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1,1,1,1));
        }

        // Draw the button. ImGui::Button returns true if it was clicked.
        if (ImGui::Button(chars[i].name.c_str(), ImVec2(130, 130))) {
            // Player clicked this button → save choice to singleton.
            tm.selectedCharacterIndex = i;
        }

        // We must "pop" everything we "pushed" (LIFO stack pattern).
        if (isSelected) { ImGui::PopStyleColor(); ImGui::PopStyleVar(); }
        ImGui::PopStyleColor(2); // Pop the 2 PushStyleColor calls (button + hover)
        ImGui::PopID();
        ImGui::NextColumn(); // Move to next column in the grid
    }
    ImGui::Columns(1); // Reset to single column

    // Show the selected character's stats as text.
    const auto& selected = tm.getSelectedCharacter();
    ImGui::Text("Selected: %s", selected.name.c_str());
    ImGui::Text("Stats: Strength %.1fx, Speed %.1fx", selected.strength, selected.speed);

    // The big green START button. ImVec2(-1, 50) = full width, 50px tall.
    if (ImGui::Button("START TOURNAMENT", ImVec2(-1, 50))) {
        tm.reset();                          // Wipe old round progress
        getApp()->changeState("bracket");    // Go to bracket screen
    }
    ImGui::End();
}
```

---

## 📖 PART 6 — `BracketState` (bracket-state.hpp)

The bracket screen reads the `currentRound` from the Singleton and draws the tournament tree.
If the player reached round 4 (meaning they won round 3), it shows the "YOU ARE THE CHAMPION" screen.

```cpp
void onImmediateGui() override {
    auto& tm = our::TournamentManager::getInstance();

    if (tm.currentRound > 3) {
        // CHAMPION SCREEN
        ImGui::Text("YOU ARE THE CHAMPION!");
        if (ImGui::Button("MAIN MENU")) {
            tm.reset();
            getApp()->changeState("menu");
        }
        return; // Don't draw the rest of the bracket
    }

    // NORMAL BRACKET SCREEN
    ImGui::Text("ROUND %d", tm.currentRound);
    
    // We figure out the opponent based on the round:
    // Round 1 (Quarterfinals) -> Blue Frost
    // Round 2 (Semifinals) -> Gold Lion
    // Round 3 (Finals) -> Black Shadow

    // ... ImGui text drawing omitted for brevity ...

    if (ImGui::Button("START NEXT MATCH", ImVec2(200, 50))) {
        // Transition to the actual 3D fighting scene!
        getApp()->changeState("play");
    }
}
```

---

## 📖 PART 7 — `PlayState` (play-state.hpp)

This is the state where the actual 3D fighting occurs. When it loads, it does a very important setup phase: **Applying the Singleton data to the 3D world.**

```cpp
void onInitialize() override {
    // 1. Load the "app.jsonc" world file (which has a generic "Player" and "AI").
    auto& config = getApp()->getConfig()["scene"];
    world.deserialize(config["world"]);

    // 2. Fetch the player's chosen character from the Singleton.
    auto& tm = our::TournamentManager::getInstance();
    const auto& selected = tm.getSelectedCharacter();

    // 3. Figure out the AI opponent based on the current round.
    our::CharacterDef aiDef = tm.characters[1]; // Round 1: Blue Frost
    if (tm.currentRound == 2) aiDef = tm.characters[3]; // Round 2: Gold Lion
    if (tm.currentRound >= 3) aiDef = tm.characters[7]; // Final: Black Shadow

    // 4. SCALE THE AI STATS TO MAKE LATER ROUNDS HARDER!
    // For Round 3: strength is multiplied by (0.8 + 3 * 0.2) = 1.4x!
    aiDef.strength *= (0.8f + tm.currentRound * 0.2f);
    aiDef.speed *= (0.8f + tm.currentRound * 0.1f);

    // 5. Loop through every single object in the 3D world.
    for (auto entity : world.getEntities()) {
        
        // If this is the Player...
        if (entity->getComponent<our::FighterComponent>() && 
            entity->getComponent<our::FighterComponent>()->isPlayer) {
            
            auto* fighter = entity->getComponent<our::FighterComponent>();
            // Apply the chosen stats to the fighter logic component!
            fighter->characterName = selected.name;
            fighter->strengthMultiplier = selected.strength;
            fighter->speedMultiplier = selected.speed;
            fighter->skinMaterialName = selected.torsoMaterial;

            // Apply the chosen color to the visual mesh renderer!
            auto* mr = entity->getComponent<our::MeshRendererComponent>();
            if (mr) {
                mr->materialName = selected.torsoMaterial;
                mr->material = our::AssetLoader<our::Material>::get(selected.torsoMaterial);
            }
        }
        
        // ... (Same thing happens for the AI, applying aiDef stats) ...
    }
}
```

---

## 📖 PART 8 — `PlayerControllerSystem` (player-controller.hpp)

This is the "Brain" of the game. It runs 60 times a second and handles combat, movement, AI, and animations. It is the most complex file in the project.

### A. Distance-Based Hit Detection
Most professional games use complex "Physics Collision Boxes" attached to fists. To keep our engine lightweight, we use a simpler trick: **Distance Mathematics**.

If you punch, and the distance between your torso and the enemy torso is less than 1.5 meters, the punch lands!

```cpp
// glm::distance measures the 3D distance between two points in meters.
float dist = glm::distance(playerFighter->basePosition, aiFighter->basePosition);

if (dist <= 1.5f) {
    if (aiFighter->isDefending) {
        // BLOCKED!
        // The attacker gets "stunned" for 1 second.
        playerFighter->stunnedTimer = FighterComponent::STUN_DURATION;
        ma_engine_play_sound(audioEngine, "assets/audio/stun.mp3", NULL);
    } else {
        // HIT!
        // Base damage is 10. Multiply it by the attacker's strength stat.
        float damage = 10.0f * playerFighter->strengthMultiplier;
        
        // Subtract health. glm::max ensures health never goes below 0.
        aiFighter->currentHealth = glm::max(aiFighter->currentHealth - damage, 0.0f);
    }
}
```

### B. Procedural Animations (Math-Based Movement)
We did not import pre-animated files from Blender (`.fbx` or `.dae`). Instead, we animate the characters live using mathematical functions.

#### 1. Walking (Sine Waves)
When you walk, legs swing back and forth. A **Sine Wave** (`std::sin()`) creates a smooth curve that oscillates up and down from 1 to -1. By plugging time into a sine wave, we get perfect swinging motion.

```cpp
// Walk timer increases every frame
fighter->walkTimer += deltaTime * 10.0f * fighter->speedMultiplier;

if (child->name.find("Left_Leg") != std::string::npos) {
    // Rotate leg forward and backward using a sine wave
    child->localTransform.rotation.x = legSwingAmplitude * std::sin(fighter->walkTimer);
} else if (child->name.find("Right_Leg") != std::string::npos) {
    // Right leg does the exact OPPOSITE (negative sine wave)
    child->localTransform.rotation.x = -legSwingAmplitude * std::sin(fighter->walkTimer);
}
```

#### 2. Punching (Linear Interpolation / `glm::mix`)
A punch needs to extend the arm outward, then quickly retract it. We use `glm::mix`, which blends two numbers together based on a percentage.

```cpp
// 't' is a percentage from 1.0 down to 0.0 based on the punch timer.
float t = 1.0f - (fighter->leftPunchTimer / duration);

// 'arc' creates the in-and-out motion.
// If 't' is < 0.5 (punching OUT), arc goes from 0 to 1.
// If 't' is > 0.5 (pulling BACK), arc goes from 1 to 0.
float arc = (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f);

// Blend the arm's resting position with its fully extended punch position based on the arc.
float targetX = glm::mix(ARM_REST_X, ARM_PUNCH_PEAK, arc);

// Apply the rotation smoothly over time.
child->localTransform.rotation.x = glm::mix(child->localTransform.rotation.x, targetX, 18.0f * deltaTime);
```

### C. Visual Feedback: Stun Colors
When a fighter gets stunned, their arms instantly turn yellow.

```cpp
if (child->name.find("Arm") != std::string::npos) {
    auto* mr = child->getComponent<MeshRendererComponent>();
    if (mr) {
        Material* skinMat = AssetLoader<Material>::get("skin");
        Material* yellowMat = AssetLoader<Material>::get("skin_yellow");
        
        // The '?' is a ternary operator (a one-line IF statement).
        // If stunnedTimer > 0, use yellowMat. Else, use skinMat.
        mr->material = (fighter->stunnedTimer > 0.0f) ? yellowMat : skinMat;
    }
}
```

### D. The Knockdown Minigame
When health hits 0, the fighter enters the `KNOCKED_DOWN` state. They fall backward, and a timer starts counting to 10. The player must mash the 'X' key to stand back up.

```cpp
if (fighter->state == FighterState::KNOCKED_DOWN) {
    // 1. Fall animation: Rotate torso 90 degrees (half_pi) backward smoothly.
    float targetRotX = glm::half_pi<float>();
    torso->localTransform.rotation.x = glm::mix(torso->localTransform.rotation.x, targetRotX, 4.0f * deltaTime);

    // 2. Mashing X Logic
    if (kb.justPressed(GLFW_KEY_X)) {
        fighter->recoveryClicks++; // Add 1 click
    }

    // 3. How many clicks are required?
    // 25 clicks for the first knockdown. 
    // +15 extra clicks for every subsequent knockdown!
    int required = 25 + (fighter->knockdownCount - 1) * 15;

    // 4. Did they click enough?
    if (fighter->recoveryClicks >= required) {
        fighter->state = FighterState::IDLE;        // Stand up!
        fighter->currentHealth = fighter->maxHealth * 0.5f; // Recover 50% HP
    }

    // 5. If 11 seconds pass (10s count + 1s for audio to finish), it's a KO!
    if (fighter->stateTimer > 11.0f) {
        // Player KO'd flag triggers match end overlay
    }
}
```

---

## 📖 PART 9 — JSON Hierarchy (`config/app.jsonc`)

The 3D fighters aren't single models. They are built out of individual pieces (Torso, Shoulders, Arms, Legs, Head) stacked together using **Hierarchy**.

In `app.jsonc`, objects inside the `"children"` array inherit the position and rotation of their parent. This is why when we rotate the Torso, the legs and arms move with it!

```json
{
    "name": "Player_Torso",
    "position": [-1.3, 1.50, 0.0],
    "components": [
        { "type": "Mesh Renderer", "mesh": "torso", "material": "torso_red" },
        { "type": "Fighter", "isPlayer": true, "maxHealth": 100 }
    ],
    "children": [
        {
            "name": "Player_Head",
            "position": [-0.180, 1.102, -0.244],
            "components": [{ "type": "Mesh Renderer", "mesh": "head", "material": "skin" }]
        },
        {
            "name": "Player_Left_Shoulder",
            "position": [0.6, 1.0, 0.0],
            "children": [
                {
                    // The Arm is a child of the Shoulder. 
                    // When the Shoulder rotates during a punch, the Arm swings with it!
                    "name": "Player_Left_Arm",
                    "position": [-0.223, -1.792, -0.216],
                    "components": [{ "type": "Mesh Renderer", "mesh": "left_arm", "material": "skin" }]
                }
            ]
        }
    ]
}
```

---

## Conclusion Summary for Exams/Presentations

If you are asked, *"How did you build this game?"* Here is your 5-point summary:

1.  **Architecture:** We used an **Entity-Component-System (ECS)** to separate raw data (`FighterComponent`) from the math logic (`PlayerControllerSystem`).
2.  **Combat & Collision:** We didn't use physics engines. We used basic math (`glm::distance`) between the two Torso coordinates to see if a punch was close enough to land. Damage was calculated using math multipliers.
3.  **Animation:** We didn't import pre-made animations. We used mathematical formulas (`std::sin` for walking, `glm::mix` for punching) to rotate hierarchical body parts in real-time.
4.  **Game State & Memory:** We used a **Singleton Pattern** (`TournamentManager`) to store data outside of the ECS world, allowing character choices and round numbers to persist between different screens.
5.  **Scene Building:** We used a **JSON Hierarchy** so that limbs are "children" of shoulders and torsos, ensuring that when the main body moves, the whole character moves together flawlessly.
