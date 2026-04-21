#pragma once
#include "../ecs/world.hpp"
#include "../components/audience.hpp"
#include "../components/fighter.hpp"
#include "../components/mesh-renderer.hpp"
#include "../miniaudio.h"
#include <glm/glm.hpp>
#include <cmath>

namespace our {
    class AudienceSystem {
        ma_engine* audioEngine = nullptr;
        bool isCheering = false;
    public:
        void setAudioEngine(ma_engine* engine) { audioEngine = engine; }
        
        void update(World* world, float deltaTime) {
            bool someoneGotHit = false;
            bool someoneKnockedDown = false;
            
            
            for(auto entity : world->getEntities()){
                if(auto fighter = entity->getComponent<FighterComponent>()){
                    if(fighter->stunnedTimer > 0.0f) someoneGotHit = true;
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
                if(auto audience = entity->getComponent<AudienceComponent>()){
                    if(someoneGotHit || someoneKnockedDown){
                        audience->jumpTimer += deltaTime * 15.0f; 
                        
                        entity->localTransform.position.y = audience->basePositionY + std::abs(std::sin(audience->jumpTimer)) * 0.4f;
                    } else {
                        audience->jumpTimer = 0.0f;
                        
                        entity->localTransform.position.y = glm::mix(entity->localTransform.position.y, audience->basePositionY, deltaTime * 5.0f);
                    }
                }
            }
        }
    };
}