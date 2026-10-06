#pragma once
#include "../ecs/component.hpp"

namespace our {
    class AudienceComponent : public Component {
    public:
        float basePositionY = 0.0f;
        float jumpTimer = 0.0f;
        float phase = 0.0f;       // random per spectator so the crowd doesn't move in lockstep
        float enthusiasm = 1.0f;  // random per spectator (some people cheer harder than others)

        static std::string getID() { return "Audience"; }
        void deserialize(const nlohmann::json& data) override {}
    };
}
