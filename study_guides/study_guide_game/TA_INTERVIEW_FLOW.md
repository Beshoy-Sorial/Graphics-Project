# TA INTERVIEW QUESTION: "What is the exact flow of the game when it starts?"

*If the TA asks you exactly how the engine boots up and transitions between scenes, read this step-by-step answer. It covers the architecture perfectly.*

---

## 1. The Boot Process (`main.cpp`)
When you launch the `.exe`, the code enters `main()`. 
- The very first thing it does is open and read the `config/app.jsonc` file.
- It passes this JSON data into `our::Application::run()`.
- The engine uses the JSON to know exactly how big to make the GLFW Window (e.g., 1280x720) and initializes the OpenGL graphics context.
- It checks the `"start-scene"` variable in the JSON, which tells the engine's **State Machine** to load the `"menu"` state first.

## 2. The Menu State (`menu-state.hpp`)
- The engine loads `MenuState`. It sets up an `ImGui` interface displaying the 8 character options.
- When the player clicks a character, difficulty, and weather condition, the data is saved into a **Singleton** class called `TournamentManager`.
- Because `TournamentManager` is a Singleton, it lives permanently in the computer's RAM. This allows the data to survive even after the Menu State is deleted!
- Clicking "START" tells the State Machine to destroy `MenuState` and load `BracketState`.

## 3. The Bracket State (`bracket-state.hpp`)
- This acts as the transition screen. It reads `TournamentManager::currentRound` to figure out if you are in the Quarterfinals, Semifinals, or Finals.
- It dynamically assigns your opponent based on the round number.
- Clicking "FIGHT" tells the State Machine to load `ColorSelectState` (if it's the very first round) or jump straight to `PlayState` (if it's a later round).

## 4. The Color Select State (`color-select-state.hpp`)
- This is a brief UI screen where you pick the color of the boxing ring floor.
- It parses the `arenaColorOptions` array from the JSON file and generates colored buttons.
- Clicking "START MATCH" saves your hex-color choice into the `TournamentManager` and loads `PlayState`.

## 5. The Play State (`play-state.hpp`)
*This is where the heavy lifting happens.*
- **Initialization:** Inside `onInitialize()`, the engine reads the `app.jsonc` file again. It sends all the `.obj` models and `.jpg` textures to the Graphics Card. It then builds the ECS (Entity-Component-System) World, spawning invisible GameObjects (Entities) and attaching data (Components) to them.
- **Dynamic Math:** It grabs the chosen fighter stats from `TournamentManager`. It calculates the AI's difficulty and scales their speed/health based on what round it is. It procedurally generates the 3D audience in a circle using `sin()` and `cos()`.
- **The Game Loop:** The engine enters `onDraw()`, which runs 60 times a second. It executes the logic systems in a strict order:
  1. `PlayerControllerSystem` (Reads keyboard/mouse)
  2. `CombatSystem` (Calculates punch distance math)
  3. `AISystem` (Calculates enemy movement)
  4. `FighterAnimationSystem` (Calculates limb rotations)
  5. `ForwardRenderer` (Draws the 3D pixels and lighting to the screen)

## 6. The End Condition
- If the AI's health hits 0, `TournamentManager::currentRound` increases by 1, and the State Machine returns to `BracketState`.
- If the Player's health hits 0, `TournamentManager` is reset to 0, and the State Machine kicks you all the way back to `MenuState` to start over!
