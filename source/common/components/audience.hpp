#pragma once
#include "../ecs/component.hpp"

namespace our {
    class AudienceComponent : public Component {
    public:
        float basePositionY = 0.0f;
        float jumpTimer = 0.0f;
        
        static std::string getID() { return "Audience"; }
        void deserialize(const nlohmann::json& data) override {}
    };
}