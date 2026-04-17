#include "light.hpp"
#include "../deserialize-utils.hpp"

namespace our {

    void LightComponent::deserialize(const nlohmann::json& data) {
        if(!data.is_object()) return;

        std::string type = data.value("lightType", "directional");
        if(type == "directional")     lightType = LightType::DIRECTIONAL;
        else if(type == "point")      lightType = LightType::POINT;
        else if(type == "spot")       lightType = LightType::SPOT;

        diffuse  = data.value("diffuse",  glm::vec3(1.0f, 1.0f, 1.0f));
        specular = data.value("specular", glm::vec3(1.0f, 1.0f, 1.0f));
        ambient  = data.value("ambient",  glm::vec3(0.1f, 0.1f, 0.1f));

        attenuationConstant  = data.value("attenuationConstant",  1.0f);
        attenuationLinear    = data.value("attenuationLinear",    0.0f);
        attenuationQuadratic = data.value("attenuationQuadratic", 0.0f);

        innerConeAngle = glm::radians(data.value("innerConeAngle", 15.0f));
        outerConeAngle = glm::radians(data.value("outerConeAngle", 30.0f));
    }

}
