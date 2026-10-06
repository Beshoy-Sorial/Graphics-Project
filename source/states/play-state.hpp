#pragma once

#include <application.hpp>
#include <systems/audience-system.hpp>
#include <components/audience.hpp>
#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/movement.hpp>
#include <systems/player-controller.hpp>
#include <asset-loader.hpp>
#include <miniaudio.h>
#include <vector>
#include "../common/tournament-manager.hpp"

// This state shows how to use the ECS framework and deserialization.
class Playstate : public our::State
{

    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::PlayerControllerSystem playerController;
    our::AudienceSystem audienceSystem;
    ma_engine audioEngine;
    float lastDeltaTime = 0.016f;

    void onInitialize() override
    {
        // First of all, we get the scene configuration from the app config
        auto &config = getApp()->getConfig()["scene"];
        // If we have assets in the scene config, we deserialize them
        if (config.contains("assets"))
        {
            our::deserializeAllAssets(config["assets"]);
        }
        // If we have a world in the scene config, we use it to populate our world
        if (config.contains("world"))
        {
            world.deserialize(config["world"]);
        }
        // We initialize the camera controller system since it needs a pointer to the app
        cameraController.enter(getApp());
        // We initialize the player controller system since it needs a pointer to the app
        playerController.enter(getApp());
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);

        // Initialize the audio engine
        ma_engine_init(NULL, &audioEngine);
        playerController.setAudioEngine(&audioEngine);

      
        audienceSystem.setAudioEngine(&audioEngine);

        int numRows = 3;
        std::vector<int> peoplePerRow = {40, 50, 60};
        float startRadius = 7.5f;
        float rowSpacing = 1.5f;
        float heightStep = 0.9f;
        float angleRandomOffset = 0.2f;
        std::vector<std::string> torsoColors = {
            "aud_red", "aud_blue", "aud_green", "aud_yellow",
            "aud_purple", "aud_cyan", "aud_orange", "aud_black"
        };

        if (config.contains("audience") && config["audience"].is_object()) {
            auto &audienceConfig = config["audience"];
            numRows = audienceConfig.value("numRows", numRows);
            startRadius = audienceConfig.value("startRadius", startRadius);
            rowSpacing = audienceConfig.value("rowSpacing", rowSpacing);
            heightStep = audienceConfig.value("heightStep", heightStep);
            angleRandomOffset = audienceConfig.value("angleRandomOffset", angleRandomOffset);

            if (audienceConfig.contains("peoplePerRow") && audienceConfig["peoplePerRow"].is_array()) {
                peoplePerRow.clear();
                for (const auto &count : audienceConfig["peoplePerRow"]) {
                    if (count.is_number_integer()) peoplePerRow.push_back(count.get<int>());
                }
            }

            if (audienceConfig.contains("torsoMaterials") && audienceConfig["torsoMaterials"].is_array()) {
                torsoColors.clear();
                for (const auto &materialName : audienceConfig["torsoMaterials"]) {
                    if (materialName.is_string()) torsoColors.push_back(materialName.get<std::string>());
                }
            }
        }

        
        // Crowd members are always far from the camera: use the low-poly LOD meshes when available
        auto crowdMesh = [](const std::string &name) {
            our::Mesh *lod = our::AssetLoader<our::Mesh>::get(name + "_lod");
            return lod ? lod : our::AssetLoader<our::Mesh>::get(name);
        };
        our::Mesh *crowdTorso = crowdMesh("torso");
        our::Mesh *crowdHead = our::AssetLoader<our::Mesh>::get("head");
        our::Mesh *crowdLeftArm = crowdMesh("left_arm"), *crowdRightArm = crowdMesh("right_arm");
        our::Mesh *crowdLeftLeg = crowdMesh("left_leg"), *crowdRightLeg = crowdMesh("right_leg");
        our::Material *crowdSkin = our::AssetLoader<our::Material>::get("aud_skin");

        // Bleachers: a ring of steps under every row so the crowd doesn't float in the air
        auto stepTop = [&](int row) { return 0.35f + row * heightStep; };
        our::Mesh *cube = our::AssetLoader<our::Mesh>::get("cube");
        our::Material *bleacherMaterial = our::AssetLoader<our::Material>::get("bleacher");
        if (cube && bleacherMaterial) {
            for (int row = 0; row < numRows; row++) {
                float radius = startRadius + row * rowSpacing;
                float topY = stepTop(row);
                const int segments = 40 + row * 8;
                float segmentLength = glm::two_pi<float>() * radius / segments * 1.04f; // slight overlap, no gaps
                for (int s = 0; s < segments; s++) {
                    float angle = (s + 0.5f) * glm::two_pi<float>() / segments;
                    our::Entity *step = world.add();
                    step->name = "Bleacher";
                    float halfHeight = (topY + 0.1f) * 0.5f;
                    step->localTransform.position = glm::vec3(std::cos(angle) * radius, topY - halfHeight, std::sin(angle) * radius);
                    step->localTransform.rotation.y = -angle;
                    // cube.obj spans -1..1, so the scale is half the size
                    step->localTransform.scale = glm::vec3(rowSpacing * 0.5f, halfHeight, segmentLength * 0.5f);
                    auto *mr = step->addComponent<our::MeshRendererComponent>();
                    mr->mesh = cube;
                    mr->material = bleacherMaterial;
                    mr->castShadows = false;
                }
            }
        }

        // Creates one body part of a spectator (same hierarchy and offsets as the fighters in app.jsonc)
        auto addPart = [&](our::Entity *parent, const char *name, our::Mesh *mesh, our::Material *material,
                           glm::vec3 position, glm::vec3 rotation, float scale) {
            our::Entity *part = world.add();
            part->name = name;
            part->parent = parent;
            part->localTransform.position = position;
            part->localTransform.rotation = rotation;
            part->localTransform.scale = glm::vec3(scale);
            if (mesh) {
                auto *mr = part->addComponent<our::MeshRendererComponent>();
                mr->mesh = mesh;
                mr->material = material;
                mr->castShadows = false; // outside the spotlight anyway; keeps the shadow pass cheap
            }
            return part;
        };

        for(int row = 0; row < numRows; row++) {
            int count = row < (int)peoplePerRow.size() ? peoplePerRow[row] : 0;
            if (count <= 0) continue;
            float currentRadius = startRadius + (row * rowSpacing);

            for(int i = 0; i < count; i++) {
                float angle = (float)i * (glm::pi<float>() * 2.0f / count);
                float randomOffset = ((rand() % 100 / 100.0f) - 0.5f) * angleRandomOffset;
                float finalAngle = angle + randomOffset;

                // Slight size variation so the crowd doesn't look cloned
                float size = 0.30f + (rand() % 100) / 100.0f * 0.05f;
                // The feet are ~2.9 units below the torso origin (in torso space): stand on the step
                float baseY = stepTop(row) + 2.9f * size;

                our::Entity* spectator = world.add();
                spectator->name = "Audience";
                spectator->localTransform.position = glm::vec3(cos(finalAngle) * currentRadius, baseY, sin(finalAngle) * currentRadius);
                // Face the ring (bodies look along their local +Z)
                spectator->localTransform.rotation.y = std::atan2(-spectator->localTransform.position.x, -spectator->localTransform.position.z);
                spectator->localTransform.scale = glm::vec3(size);

                auto* audComp = spectator->addComponent<our::AudienceComponent>();
                audComp->basePositionY = baseY;
                audComp->phase = (rand() % 1000) / 1000.0f * glm::two_pi<float>();
                audComp->enthusiasm = 0.6f + (rand() % 100) / 100.0f * 0.7f;

                int colorIndex = torsoColors.empty() ? 0 : rand() % (int)torsoColors.size();
                std::string selectedColor = torsoColors.empty() ? "aud_red" : torsoColors[colorIndex];
                our::Material *shirt = our::AssetLoader<our::Material>::get(selectedColor);

                auto* mrTorso = spectator->addComponent<our::MeshRendererComponent>();
                mrTorso->mesh = crowdTorso;
                mrTorso->material = shirt;
                mrTorso->castShadows = false;

                addPart(spectator, "Head", crowdHead, crowdSkin, glm::vec3(-0.18f, 1.102f, -0.244f), glm::vec3(0.0f, -glm::half_pi<float>(), 0.0f), 0.191f);
                our::Entity *leftShoulder  = addPart(spectator, "Left_Shoulder",  nullptr, nullptr, glm::vec3( 0.6f, 1.0f, 0.0f), glm::vec3(0.45f, 0.0f, 0.0f), 1.0f);
                our::Entity *rightShoulder = addPart(spectator, "Right_Shoulder", nullptr, nullptr, glm::vec3(-0.6f, 1.0f, 0.0f), glm::vec3(0.45f, 0.0f, 0.0f), 1.0f);
                addPart(leftShoulder,  "Left_Arm",  crowdLeftArm,  crowdSkin, glm::vec3(-0.223f, -1.792f, -0.216f), glm::vec3(0.0f), 0.074f);
                addPart(rightShoulder, "Right_Arm", crowdRightArm, crowdSkin, glm::vec3( 0.223f, -1.792f, -0.216f), glm::vec3(0.0f), 0.074f);
                addPart(spectator, "Left_Leg",  crowdLeftLeg,  shirt, glm::vec3( 0.368f, -2.92f, -0.135f), glm::vec3(0.0f), 0.28f);
                addPart(spectator, "Right_Leg", crowdRightLeg, shirt, glm::vec3(-0.368f, -2.92f, -0.135f), glm::vec3(0.0f), 0.28f);
            }
        }
        // Apply selected character identity to the player
        auto &tm = our::TournamentManager::getInstance();
        const auto &selected = tm.getSelectedCharacter();

        // Determine AI opponent based on round
        our::CharacterDef aiDef = tm.characters[1]; // Round 1: Blue Frost
        if (tm.currentRound == 2)
            aiDef = tm.characters[3]; // Round 2: Gold Lion
        if (tm.currentRound >= 3)
            aiDef = tm.characters[7]; // Final: Black Shadow

        // Scale AI stats by round
        aiDef.strength *= (0.8f + tm.currentRound * 0.2f);
        aiDef.speed *= (0.8f + tm.currentRound * 0.1f);

        // Apply selected difficulty
        switch (tm.selectedDifficulty)
        {
        case our::DifficultyLevel::Easy:
            aiDef.strength *= 0.85f;
            aiDef.speed *= 0.90f;
            break;
        case our::DifficultyLevel::Medium:
            aiDef.strength *= 1.05f;
            aiDef.speed *= 1.05f;
            break;
        case our::DifficultyLevel::Hard:
            aiDef.strength *= 1.25f;
            aiDef.speed *= 1.20f;
            break;
        case our::DifficultyLevel::Difficult:
            aiDef.strength *= 1.50f;
            aiDef.speed *= 1.35f;
            break;
        }

        for (auto entity : world.getEntities())
        {
            // Check for Player
            if (entity->name.find("Player_Torso") != std::string::npos ||
                (entity->getComponent<our::FighterComponent>() && entity->getComponent<our::FighterComponent>()->isPlayer))
            {

                auto *fighter = entity->getComponent<our::FighterComponent>();
                if (fighter)
                {
                    fighter->characterName = selected.name;
                    fighter->strengthMultiplier = selected.strength;
                    fighter->speedMultiplier = selected.speed;
                    fighter->skinMaterialName = selected.torsoMaterial;
                }

                auto *mr = entity->getComponent<our::MeshRendererComponent>();
                if (mr)
                {
                    mr->materialName = selected.torsoMaterial;
                    mr->material = our::AssetLoader<our::Material>::get(selected.torsoMaterial);
                }
            }
            // Check for AI (Assuming it's called AI_Torso in test.jsonc)
            else if (entity->name.find("AI_Torso") != std::string::npos ||
                     (entity->getComponent<our::FighterComponent>() && !entity->getComponent<our::FighterComponent>()->isPlayer && entity->name.find("Referee") == std::string::npos))
            {

                auto *fighter = entity->getComponent<our::FighterComponent>();
                if (fighter)
                {
                    fighter->characterName = aiDef.name;
                    fighter->strengthMultiplier = aiDef.strength;
                    fighter->speedMultiplier = aiDef.speed;
                    fighter->skinMaterialName = aiDef.torsoMaterial;
                    // Behavior profile by selected difficulty
                    switch (tm.selectedDifficulty)
                    {
                    case our::DifficultyLevel::Easy:
                        fighter->aiAttackWeight = 0.30f;
                        fighter->aiDefendWeight = 0.30f;
                        fighter->aiIdleWeight = 0.40f;
                        fighter->aiDecisionMin = 0.60f;
                        fighter->aiDecisionMax = 1.20f;
                        fighter->aiApproachDistance = 1.35f;
                        fighter->aiRetreatDistance = 0.55f;
                        fighter->aiBlockChance = 0.25f;
                        fighter->aiRecoveryChancePerFrame = 0.05f;
                        fighter->aiWindupTime = 0.42f;
                        break;

                    case our::DifficultyLevel::Medium:
                        fighter->aiAttackWeight = 0.45f;
                        fighter->aiDefendWeight = 0.35f;
                        fighter->aiIdleWeight = 0.20f;
                        fighter->aiDecisionMin = 0.35f;
                        fighter->aiDecisionMax = 0.80f;
                        fighter->aiApproachDistance = 1.20f;
                        fighter->aiRetreatDistance = 0.70f;
                        fighter->aiBlockChance = 0.40f;
                        fighter->aiRecoveryChancePerFrame = 0.08f;
                        fighter->aiWindupTime = 0.30f;
                        break;

                    case our::DifficultyLevel::Hard:
                        fighter->aiAttackWeight = 0.60f;
                        fighter->aiDefendWeight = 0.35f;
                        fighter->aiIdleWeight = 0.05f;
                        fighter->aiDecisionMin = 0.20f;
                        fighter->aiDecisionMax = 0.50f;
                        fighter->aiApproachDistance = 1.15f;
                        fighter->aiRetreatDistance = 0.80f;
                        fighter->aiBlockChance = 0.60f;
                        fighter->aiRecoveryChancePerFrame = 0.12f;
                        fighter->aiWindupTime = 0.22f;
                        break;

                    case our::DifficultyLevel::Difficult:
                        fighter->aiAttackWeight = 0.75f;
                        fighter->aiDefendWeight = 0.25f;
                        fighter->aiIdleWeight = 0.00f;
                        fighter->aiDecisionMin = 0.10f;
                        fighter->aiDecisionMax = 0.30f;
                        fighter->aiApproachDistance = 1.10f;
                        fighter->aiRetreatDistance = 0.85f;
                        fighter->aiBlockChance = 0.80f;
                        fighter->aiRecoveryChancePerFrame = 0.15f;
                        fighter->aiWindupTime = 0.16f;
                        break;
                    }
                }

                auto *mr = entity->getComponent<our::MeshRendererComponent>();
                if (mr)
                {
                    mr->materialName = aiDef.torsoMaterial;
                    mr->material = our::AssetLoader<our::Material>::get(aiDef.torsoMaterial);
                }
            }
        }

        // Apply the selected arena colour tint to the Ring canvas and Floor.
        // Only when arenaColorSelected=true; otherwise keep the default white
        // tint so the scene renders with natural colours before any choice is made.
        if (tm.arenaColorSelected) {
            for (auto entity : world.getEntities()) {
                if (entity->name == "Ring" || entity->name == "Floor") {
                    auto* mr = entity->getComponent<our::MeshRendererComponent>();
                    if (mr) {
                        auto* litMat = dynamic_cast<our::LitMaterial*>(mr->material);
                        if (litMat) {
                            litMat->tint = glm::vec3(tm.selectedArenaColor);
                        }
                    }
                }
            }
        }
        // (Without a colour pick the ring keeps its texture and the floor keeps its parquet texture.)
    }

    void onDraw(double deltaTime) override
    {
        // The first frame after loading includes the loading time, and the window can be dragged:
        // clamp the time step so nothing teleports or jumps.
        float dt = glm::min((float)deltaTime, 1.0f / 20.0f);
        lastDeltaTime = dt;
        // Run the player controller FIRST so movement is applied before rendering
        playerController.update(&world, dt);
        audienceSystem.update(&world, dt, playerController.getExcitement());
        // Run other systems
        movementSystem.update(&world, dt);
        // Render the scene
        renderer.render(&world);

        // Get a reference to the keyboard object
        auto &keyboard = getApp()->getKeyboard();

        if (keyboard.justPressed(GLFW_KEY_ESCAPE))
        {
            // If the escape key is pressed in this frame, go to the menu state
            getApp()->changeState("menu");
        }
    }

    void onImmediateGui() override
    {
        // Draw the HUD (health bars, stance, hit pop-ups, knockdown count, KO screen).
        // Passing &renderer allows drawHUD to toggle the grayscale postprocess
        // effect automatically when someone is knocked down.
        playerController.drawHUD(lastDeltaTime, &renderer);
    }

    void onDestroy() override
    {
        // Don't forget to destroy the renderer
        renderer.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Reset cached fighter pointers BEFORE clearing the world.
        // drawHUD() runs in onImmediateGui (before update() each frame), so on the first
        // frame of a new match the cache could still hold dangling pointers from the old world.
        playerController.resetCache();
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();

        // Release the sounds, then uninitialize the audio engine
        playerController.releaseAudio();
        ma_engine_uninit(&audioEngine);
    }
};