#pragma once

#include "../ecs/component.hpp"
#include "../mesh/mesh.hpp"
#include "../material/material.hpp"
#include "../asset-loader.hpp"

namespace our {

    // This component denotes that any renderer should draw the given mesh using the given material at the transformation of the owning entity.
    class MeshRendererComponent : public Component {
    public:
        Mesh* mesh = nullptr; // The mesh that should be drawn
        Material* material = nullptr; // The material used to draw the mesh
        std::string materialName; // The name of the material (used for skin swapping)
        // Per-object glow added on top of the material (lit materials only). Materials are shared between
        // entities, so effects that should affect a single entity (e.g. a fighter flashing when hit) use this.
        glm::vec3 emissive = glm::vec3(0.0f);
        bool castShadows = true; // Whether this object is drawn into the shadow map

        // The ID of this component type is "Mesh Renderer"
        static std::string getID() { return "Mesh Renderer"; }

        // Receives the mesh & material from the AssetLoader by the names given in the json object
        void deserialize(const nlohmann::json& data) override;
    };

}