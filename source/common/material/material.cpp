#include "material.hpp"

#include "../asset-loader.hpp"
#include "deserialize-utils.hpp"

#include <glad/gl.h>

namespace our {

    // Helper: create a 1x1 texture with a given RGBA color
    static Texture2D* makeDefaultTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a){
        Texture2D* tex = new Texture2D();
        tex->bind();
        uint8_t pixel[4] = {r, g, b, a};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        Texture2D::unbind();
        return tex;
    }

    // This function should setup the pipeline state and set the shader to be used
    void Material::setup() const {
        pipelineState.setup();
        if (shader) shader->use();
    }

    // This function read the material data from a json object
    void Material::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;

        if(data.contains("pipelineState")){
            pipelineState.deserialize(data["pipelineState"]);
        }
        shader = AssetLoader<ShaderProgram>::get(data["shader"].get<std::string>());
        transparent = data.value("transparent", false);
    }

    // This function should call the setup of its parent and
    // set the "tint" uniform to the value in the member variable tint 
    void TintedMaterial::setup() const {
        Material::setup();
        shader->set("tint", tint);
    }

    // This function read the material data from a json object
    void TintedMaterial::deserialize(const nlohmann::json& data){
        Material::deserialize(data);
        if(!data.is_object()) return;
        tint = data.value("tint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // This function should call the setup of its parent and
    // set the "alphaThreshold" uniform to the value in the member variable alphaThreshold
    // Then it should bind the texture and sampler to a texture unit and send the unit number to the uniform variable "tex" 
    void TexturedMaterial::setup() const {
        TintedMaterial::setup();
        shader->set("alphaThreshold", alphaThreshold);
        glActiveTexture(GL_TEXTURE0);
        if(texture) texture->bind();
        if(sampler) sampler->bind(0);
        shader->set("tex", 0);
    }

    // This function read the material data from a json object
    void TexturedMaterial::deserialize(const nlohmann::json& data){
        TintedMaterial::deserialize(data);
        if(!data.is_object()) return;
        alphaThreshold = data.value("alphaThreshold", 0.0f);
        texture = AssetLoader<Texture2D>::get(data.value("texture", ""));
        sampler = AssetLoader<Sampler>::get(data.value("sampler", ""));
    }

    void LitMaterial::setup() const {
        if(!shader) return;
        Material::setup();

        auto bindMap = [&](int unit, Texture2D* tex, Texture2D* fallback, const std::string& name){
        glActiveTexture(GL_TEXTURE0 + unit);
        (tex ? tex : fallback)->bind();
        if(sampler) sampler->bind(unit);
        shader->set(name, unit);
        };

        // 1x1 fallback textures created once; used when a map is not configured
        static Texture2D* white   = makeDefaultTexture(255, 255, 255, 255); // albedo/specular/AO default
        static Texture2D* black   = makeDefaultTexture(  0,   0,   0, 255); // emissive default (no glow)
        static Texture2D* midgray = makeDefaultTexture(128, 128, 128, 255); // roughness default (medium)
        
        bindMap(0, albedo_map,            white,   "material.albedo_tex");
        bindMap(1, specular_map,          white,   "material.specular_tex");
        bindMap(2, ambient_occlusion_map, white,   "material.ambient_occlusion_tex");
        bindMap(3, roughness_map,         midgray, "material.roughness_tex");
        bindMap(4, emissive_map,          black,   "material.emissive_tex");
    }

    void LitMaterial::deserialize(const nlohmann::json& data){
        Material::deserialize(data);
        if(!data.is_object()) return;
        sampler               = AssetLoader<Sampler>::get(data.value("sampler", ""));
        albedo_map            = AssetLoader<Texture2D>::get(data.value("albedo_map",            ""));
        specular_map          = AssetLoader<Texture2D>::get(data.value("specular_map",          ""));
        ambient_occlusion_map = AssetLoader<Texture2D>::get(data.value("ambient_occlusion_map", ""));
        roughness_map         = AssetLoader<Texture2D>::get(data.value("roughness_map",         ""));
        emissive_map          = AssetLoader<Texture2D>::get(data.value("emissive_map",          ""));
        
    }

}
