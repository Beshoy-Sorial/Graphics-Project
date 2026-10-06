#pragma once
#include "../ecs/world.hpp"
#include "../components/audience.hpp"
#include "../components/fighter.hpp"
#include "../components/mesh-renderer.hpp"
#include "../miniaudio.h"
#include <glm/glm.hpp>
#include <cmath>

namespace our {
    // Animates the procedural crowd. "excitement" (0..1) comes from the fight: it spikes on
    // clean hits and knockdowns and slowly calms down. Calm spectators sway, excited ones
    // jump and wave their arms. Every spectator has its own phase and enthusiasm.
    class AudienceSystem {
        ma_engine* audioEngine = nullptr;
        bool isCheering = false;
        float time = 0.0f;
    public:
        void setAudioEngine(ma_engine* engine) { audioEngine = engine; }

        void update(World* world, float deltaTime, float excitement) {
            time += deltaTime;
            bool someoneKnockedDown = false;

            for(auto entity : world->getEntities()){
                if(auto fighter = entity->getComponent<FighterComponent>()){
                    if(fighter->state == FighterState::KNOCKED_DOWN) someoneKnockedDown = true;
                }
            }

            if(someoneKnockedDown && !isCheering && audioEngine) {
                ma_engine_play_sound(audioEngine, "assets/audio/cheer.mp3", NULL);
                isCheering = true;
            } else if (!someoneKnockedDown) {
                isCheering = false;
            }

            for(auto entity : world->getEntities()){
                // Spectator body: sway when calm, jump when excited
                if(auto audience = entity->getComponent<AudienceComponent>()){
                    float e = glm::clamp(excitement * audience->enthusiasm, 0.0f, 1.0f);
                    audience->jumpTimer += deltaTime * (6.0f + 9.0f * e);
                    float jump = std::abs(std::sin(audience->jumpTimer + audience->phase)) * 0.35f * e;
                    entity->localTransform.position.y = audience->basePositionY + jump;
                    entity->localTransform.rotation.z = 0.05f * std::sin(time * 1.3f + audience->phase) * (1.0f - e);
                    continue;
                }
                // Spectator shoulders (the arms are their children): arms down when calm,
                // raised and waving when excited
                if(entity->parent && entity->name.find("Shoulder") != std::string::npos){
                    auto audience = entity->parent->getComponent<AudienceComponent>();
                    if(!audience) continue;
                    float e = glm::clamp(excitement * audience->enthusiasm, 0.0f, 1.0f);
                    bool right = entity->name.find("Right") != std::string::npos;
                    float wave = std::sin(time * 9.0f + audience->phase + (right ? 0.0f : 1.7f)) * 0.35f * e;
                    float target = glm::mix(0.45f, -2.9f, e) + wave;
                    entity->localTransform.rotation.x = glm::mix(entity->localTransform.rotation.x, target,
                                                                 1.0f - std::exp(-10.0f * deltaTime));
                }
            }
        }
    };
}
