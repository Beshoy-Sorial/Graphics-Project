#pragma once

#include "../ecs/component.hpp"
#include <glm/glm.hpp>
#include <json/json.hpp>
#include <string>

namespace our {

    enum class LightType {
        DIRECTIONAL,
        POINT,
        SPOT
    };

    class LightComponent : public Component {
    public:
        LightType lightType = LightType::DIRECTIONAL;

        glm::vec3 diffuse  = {1.0f, 1.0f, 1.0f};
        glm::vec3 specular = {1.0f, 1.0f, 1.0f};
        glm::vec3 ambient  = {0.1f, 0.1f, 0.1f};

        // Point and Spot attenuation
        float attenuationConstant  = 1.0f;
        float attenuationLinear    = 0.0f;
        float attenuationQuadratic = 0.0f;

        // Spot cone angles (in radians)
        float innerConeAngle = glm::radians(15.0f);
        float outerConeAngle = glm::radians(30.0f);

        static std::string getID() { return "Light"; }
        void deserialize(const nlohmann::json& data) override;
    };

}
