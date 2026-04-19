# 🥊 Boxing Game Implementation Tasks

This is your step-by-step checklist for building the Boxing Game, specifically tailored for the **Separate Torso and Legs** approach. Check off tasks as you complete them!

## Phase 1: Asset Gathering & Scene Setup
- [x] **Find Base Models:** Download low/mid-poly `.obj` models for a `Torso`, a `Leg` (can be used for both left and right), an `Arm` (can be used for both left and right), and a `Head`.
- [x] **Define Colors:** Instead of preparing texture files, define 8 uniquely colored tinted materials in your JSON config (e.g., `torso_red`, `torso_blue`) to represent your 8 characters.
- [x] **Referee Setup:** The referee will just be another instance of the standard fighter model, but using a pure white tinted material to stand out.
- [ ] **Audio Assets:** Download basic `.wav` files for a punch landing, a block, a crowd cheer, and a referee bell.
- [x] **JSON Scene Creation:** Create your first boxing ring JSON file. Add the Player entity using the hierarchical structure: `Torso` as the parent, with `Left_Arm`, `Right_Arm`, `Left_Leg`, `Right_Leg`, and `Head` as children.

## Phase 2: Core Components & ECS
- [x] **Audio Integration:** Setup `miniaudio` in `PlayState` (Completed!).
- [x] **Create `FighterComponent`:** Add variables for `isPlayer`, `health`, `state` (Idle, Attacking, Defending, etc.), and `stateTimer`.
- [x] **Basic Player Movement:** Read keyboard input (W, A, S, D) to move the player's Torso entity. Apply a sine-wave rotation to the local X-axis of the `Left_Leg` and `Right_Leg` entities while moving to simulate walking.
- [x] **Basic Punching Animation:** When clicking the mouse or pressing a punch key, use code to quickly rotate the `Left_Arm` or `Right_Arm` entity on its local X-axis, then return it to its resting position.

## Phase 3: AI & Combat Logic
- [x] **Spawn Opponent:** Add a second Fighter to your JSON scene to act as the AI.
- [x] **AI Movement System:** Write logic so the AI calculates the distance to the player and slowly moves toward them if they are too far away.
- [x] **AI Decision System:** Once in range, make the AI randomly choose between Attacking, Defending, or waiting idle based on timers.
- [x] **Clash Detection (Hitboxes):** When a punch animation happens, check the distance between fighters. If they are close, check if the defender's state is "Defending".
    - [x] **Hit:** Reduce health, play punch sound, stagger defender.
    - [x] **Block:** Play block sound, stun the attacker briefly.

## Phase 4: The Knockdown Mini-Game
- [x] **Fall Animation:** When a fighter's health hits 0, rotate their Torso 90 degrees backward so they fall flat.
- [ ] **Referee Timer:** Start a 10-second countdown and draw it on the screen using `ImGui`.
- [ ] **Recovery Mechanic:** If it's the player, require them to mash the 'X' key a certain number of times to stand back up. Increase required clicks for every subsequent fall.
- [ ] **Match End Condition:** If the timer runs out before standing up, trigger a KO and end the match.

## Phase 5: Tournament Flow & Camera
- [ ] **Camera Toggle:** Add a CameraComponent attached to the Player's Torso. Map a key (e.g., 'V') to switch its local position between First-Person (in front of face) and Third-Person (behind the back).
- [x] **Menu State:** Create a starting screen using `ImGui` where the player can click to select which of the 8 characters they want to play as.
- [x] **Bracket State:** Create a screen showing the tournament bracket.
- [x] **Progression Logic:** When you win a match, return to the Bracket State, update the bracket, and load the next opponent. Win 3 matches to beat the game!
