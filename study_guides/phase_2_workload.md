# Phase 2 Workload Distribution

This document outlines the division of labor and specific technical contributions of the 4 team members during Phase 2 of the engine development and game integration process. 

The workload was carefully divided to ensure equal contribution across rendering, physics, artificial intelligence, and engine architecture.

---

### **Team Member 1: Core Graphics & Lighting Engineering**
**Focus:** Illumination models, material pipelines, and fundamental GPU shader math.
* **Lighting Shaders (`lit.vert`, `lit.frag`):** Implemented the core Phong/Blinn-Phong illumination models, translating mathematical light equations into GLSL.
* **Light Attenuation:** Engineered the quadratic attenuation formulas for Point and Spot lights to create realistic light falloff in the boxing corners.
* **LitMaterial Component:** Designed the C++ ECS component to bind multiple texture maps (Albedo, Specular, Roughness, Ambient Occlusion) to the GPU texturing units.
* **JSON Deserialization:** Wrote the JSON parsing logic allowing the engine to dynamically load lights and textures from `app.jsonc` without recompilation.

---

### **Team Member 2: Rendering Architecture & Post-Processing**
**Focus:** Framebuffers, screen-space effects, and visual polish.
* **Framebuffer Management:** Handled the creation and destruction of off-screen OpenGL Framebuffer Objects (FBOs) in the `ForwardRenderer`.
* **Post-Processing Pipeline:** Built the architecture to draw the rendered scene onto a full-screen quad for final manipulation.
* **Cinematic Shaders (`warm-grade.frag`, `vignette.frag`):** Wrote the GLSL shaders that color-grade the arena into a warm, cinematic gold and apply edge-darkening vignettes.
* **Knockout Grayscale Hook:** Connected the C++ game logic to the post-processing material, allowing the `u_grayscale` uniform to instantly drain color from the screen during a knockout.

---

### **Team Member 3: AI Architecture & Combat Physics**
**Focus:** Finite State Machines, hit detection, and enemy intelligence.
* **AI Subsystem (`ai-system.hpp`):** Designed the non-player character (NPC) state machine. Implemented the logic allowing the AI to strafe, circle the player, randomly block, and counter-attack.
* **Hit Detection Math (`combat-system.hpp`):** Implemented the 3D distance and bounding-box math that accurately detects when a punch connects with a torso, calculating `glm::distance` between fist and body.
* **Stun & Recovery Mechanics:** Built the timing logic that forces a fighter into a `STUNNED` state upon taking a heavy hit, locking them out of animations.
* **Audio Integration:** Connected the hit-detection logic to the `miniaudio` library to dynamically trigger punch and crowd impact sounds.

---

### **Team Member 4: Procedural Animation & Orchestration**
**Focus:** Mathematical skeleton manipulation, game flow, and environmental effects.
* **Procedural Animation (`fighter-animation-system.hpp`):** Bypassed the need for external animation files by using pure math (`sin()` waves and `glm::mix()`) to procedurally animate punches, walking, and falling sequences directly on the GPU/CPU.
* **Tournament Manager (`tournament-manager.hpp`):** Programmed the overall game loop, managing brackets, opponent scaling (increasing AI health/aggression per round), and state transitions.
* **Player Controller Refactor:** Acted as the software architect to modularize the monolithic 1,200-line controller into clean, maintainable systems.
* **3D Weather Particle System:** Engineered the randomized coordinate generation (`glDrawArrays(GL_POINTS)`) that simulates rain and snow falling across the arena.
