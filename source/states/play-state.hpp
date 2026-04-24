#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/movement.hpp>
#include <systems/player-controller.hpp>
#include <asset-loader.hpp>
#include <miniaudio.h>
#include "../common/tournament-manager.hpp"

// This state shows how to use the ECS framework and deserialization.
class Playstate : public our::State
{

    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::PlayerControllerSystem playerController;
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
    }

    void onDraw(double deltaTime) override
    {
        lastDeltaTime = (float)deltaTime;
        // Run the player controller FIRST so movement is applied before rendering
        playerController.update(&world, (float)deltaTime);
        // Run other systems
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);
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
        // Draw the ATTACK / DEFEND mode HUD in the top-right corner
        playerController.drawHUD(lastDeltaTime);
    }

    void onDestroy() override
    {
        // Don't forget to destroy the renderer
        renderer.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();

        // Uninitialize the audio engine
        ma_engine_uninit(&audioEngine);
    }
};