# PLAY STATE DEEP DIVE (`play-state.hpp`)

This file is the absolute core of the 3D Boxing Game. Once the menus and brackets are finished, this State takes over to load the 3D world, spawn the audience, configure the AI difficulty, and run the 60-FPS physics/rendering loop.

Here is a step-by-step breakdown of how `play-state.hpp` works.

---

## 1. Initialization and JSON Parsing

When the match starts, `onInitialize()` is the very first function to run.

```cpp
auto &config = getApp()->getConfig()["scene"];
if (config.contains("assets")) {
    our::deserializeAllAssets(config["assets"]);
}
if (config.contains("world")) {
    world.deserialize(config["world"]);
}
```
**Explanation:** The engine does not hardcode 3D models into C++. It reads the `app.jsonc` file. 
- `deserializeAllAssets` loads all the `.obj` models, `.jpg` textures, and shaders into the Graphics Card VRAM.
- `world.deserialize` creates the Entity-Component-System (ECS). It spawns the invisible entities (like the Player_Torso and AI_Torso) and attaches data components to them.

```cpp
auto size = getApp()->getFrameBufferSize();
renderer.initialize(size, config["renderer"]);

ma_engine_init(NULL, &audioEngine);
playerController.setAudioEngine(&audioEngine);
```
**Explanation:** It gives the window's screen size (e.g. 1920x1080) to the `ForwardRenderer` so it knows how many pixels to draw. It also spins up the `miniaudio` sound engine and passes it to the `playerController`.

---

## 2. The Procedural Audience Generation

Instead of writing 200 audience members by hand in the JSON file, `play-state` generates them mathematically using trigonometry.

```cpp
for(int row = 0; row < numRows; row++) {
    int count = peoplePerRow[row];
    float currentRadius = startRadius + (row * rowSpacing);
    float currentBaseY = row * heightStep; 
```
**Explanation:** It loops through concentric rings (rows). Every row gets pushed further back (`startRadius + rowSpacing`) and raised higher up into the air (`heightStep`) so they look like stadium bleachers.

```cpp
    for(int i = 0; i < count; i++) {
        float angle = (float)i * (glm::pi<float>() * 2.0f / count);
        float randomOffset = ((rand() % 100 / 100.0f) - 0.5f) * angleRandomOffset;
        float finalAngle = angle + randomOffset;
```
**Explanation:** `2 * pi` is a full circle (360 degrees in radians). It divides the circle by the number of people to space them perfectly evenly. It then adds a tiny `randomOffset` so they don't look like perfect robots.

```cpp
        spectator->localTransform.position = glm::vec3(cos(finalAngle) * currentRadius, currentBaseY, sin(finalAngle) * currentRadius);
        spectator->localTransform.rotation.y = std::atan2(-spectator->localTransform.position.x, -spectator->localTransform.position.z);
```
**Explanation:** 
- `cos()` and `sin()` convert the circular angle into X and Z physical 3D coordinates.
- `atan2` calculates the exact angle needed to make the 3D model turn its head to stare directly at the center of the ring `(0,0,0)`.

---

## 3. Applying the Difficulty & Tournament Stats

The ECS world loaded blank "Fighters" from the JSON. Now, we must inject the stats chosen in the Menu State.

```cpp
auto &tm = our::TournamentManager::getInstance();
const auto &selected = tm.getSelectedCharacter();

our::CharacterDef aiDef = tm.characters[1]; // Round 1: Blue Frost
if (tm.currentRound == 2) aiDef = tm.characters[3]; // Round 2: Gold Lion
```
**Explanation:** It grabs the Singleton `TournamentManager` to find out what character the player chose, and manually decides who the opponent should be based on the `currentRound`.

```cpp
aiDef.strength *= (0.8f + tm.currentRound * 0.2f);
aiDef.speed *= (0.8f + tm.currentRound * 0.1f);
```
**Explanation:** The game dynamically gets harder! For every round you win, the AI's strength goes up by 20% and its speed goes up by 10%.

```cpp
case our::DifficultyLevel::Hard:
    fighter->aiAttackWeight = 0.60f;
    fighter->aiDefendWeight = 0.35f;
    fighter->aiIdleWeight = 0.05f;
    fighter->aiBlockChance = 0.60f;
    break;
```
**Explanation:** After finding the AI Entity in the world, it modifies its `FighterComponent` brain. On "Hard" difficulty, the AI has a 60% chance to attack every time it thinks, only a 5% chance to do nothing, and a 60% chance to instantly block your punches.

---

## 4. Setting the Custom Arena Color

```cpp
if (tm.arenaColorSelected) {
    for (auto entity : world.getEntities()) {
        if (entity->name == "Ring" || entity->name == "Floor") {
            auto* litMat = dynamic_cast<our::LitMaterial*>(mr->material);
            if (litMat) litMat->tint = glm::vec3(tm.selectedArenaColor);
        }
    }
}
```
**Explanation:** If the player picked a color in the `color-select-state`, the engine loops over all entities until it finds the "Ring" and "Floor". It then modifies their `LitMaterial` shader tint to instantly paint the floor the chosen color.

---

## 5. The Master Game Loop (`onDraw`)

Once setup is done, this function runs 60 times a second.

```cpp
void onDraw(double deltaTime) override {
    lastDeltaTime = (float)deltaTime;
    
    playerController.update(&world, (float)deltaTime);
    audienceSystem.update(&world, (float)deltaTime);
    movementSystem.update(&world, (float)deltaTime);
    cameraController.update(&world, (float)deltaTime);
    
    renderer.render(&world);
```
**Explanation:** 
- `deltaTime` is the exact fraction of a second since the last frame (usually 0.016s). It ensures physics calculations run at the exact same speed on slow and fast computers.
- It calls `update()` on all the isolated Systems we built (`playerController` for input, `audienceSystem` for making the crowd jump).
- Finally, it gives the updated 3D world to the `renderer` to calculate the lighting and draw the pixels to your monitor.

---

## 6. Memory Cleanup (`onDestroy`)

When the match ends (someone gets knocked out or the player presses Escape), we must give RAM back to the operating system.

```cpp
void onDestroy() override {
    renderer.destroy();
    cameraController.exit();
    playerController.resetCache();
    world.clear();
    our::clearAllAssets();
    ma_engine_uninit(&audioEngine);
}
```
**Explanation:** 
- It deletes all ECS Entities.
- It wipes all cached C++ pointers so the next match doesn't crash from reading dead memory (`resetCache()`).
- `clearAllAssets()` deletes the 3D models and textures from the Graphics Card VRAM.
- `ma_engine_uninit` shuts down the sound card connection.
