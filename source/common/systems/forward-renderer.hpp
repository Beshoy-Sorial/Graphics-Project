#pragma once

#include "../ecs/world.hpp"
#include "../components/camera.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/light.hpp"
#include "../asset-loader.hpp"

#include <glad/gl.h>
#include <vector>
#include <algorithm>
#include <unordered_set>

namespace our
{
    extern int g_WeatherMode; // 0: Sun, 1: Rain, 2: Snow

    // The render command stores command that tells the renderer that it should draw
    // the given mesh at the given localToWorld matrix using the given material
    // The renderer will fill this struct using the mesh renderer components
    struct RenderCommand
    {
        glm::mat4 localToWorld;
        glm::vec3 center;
        Mesh *mesh;
        Material *material;
        glm::vec3 emissive;  // Per-object glow (see MeshRendererComponent::emissive)
        bool castShadows;
        float depth;         // Distance along the camera forward axis (used for sorting)
    };

    // A forward renderer is a renderer that draw the object final color directly to the framebuffer
    // In other words, the fragment shader in the material should output the color that we should see on the screen
    // This is different from more complex renderers that could draw intermediate data to a framebuffer before computing the final color
    // In this project, we only need to implement a forward renderer
    //
    // Optional "enhanced" features are enabled from the renderer config (all off by default, so the
    // requirement tests render exactly as before):
    //   "hdr": true              -> render into a floating point buffer, tone map in the postprocess shader
    //   "msaa": 4                -> multisample anti-aliasing (HDR pipeline only)
    //   "bloom": { ... }         -> glow around very bright pixels (HDR pipeline only)
    //   "shadows": { ... }       -> shadow map for the first light with "castShadows": true
    //   "ambient", "fog"         -> hemisphere ambient light and distance fog for lit materials
    //   "weather": true          -> apply the global weather mode (sun / rain / snow)
    class ForwardRenderer
    {
        // These window size will be used on multiple occasions (setting the viewport, computing the aspect ratio, etc.)
        glm::ivec2 windowSize;
        // These are two vectors in which we will store the opaque and the transparent commands.
        // We define them here (instead of being local to the "render" function) as an optimization to prevent reallocating them every frame
        std::vector<RenderCommand> opaqueCommands;
        std::vector<RenderCommand> transparentCommands;
        std::vector<LightComponent *> lights;
        // Objects used for rendering a skybox
        Mesh *skySphere = nullptr;
        TexturedMaterial *skyMaterial = nullptr;
        glm::vec4 skyTint = glm::vec4(1.0f);
        // Objects used for Postprocessing
        GLuint postprocessFrameBuffer = 0, postProcessVertexArray = 0;
        Texture2D *colorTarget = nullptr, *depthTarget = nullptr;
        TexturedMaterial *postprocessMaterial = nullptr;

        // Objects used for 3D Weather
        GLuint weatherVAO = 0, weatherVBO = 0;
        ShaderProgram *weatherShader = nullptr;
        int numParticles = 40000;
        bool weatherEnabled = false;

        // ── HDR pipeline ─────────────────────────────────────────────────────
        bool hdr = false;
        bool sortOpaque = false;
        float exposure = 1.0f;
        int msaaSamples = 0;
        GLuint msaaFrameBuffer = 0, msaaColorBuffer = 0, msaaDepthBuffer = 0;

        // ── Bloom ────────────────────────────────────────────────────────────
        bool bloomEnabled = false;
        float bloomStrength = 0.06f, bloomThreshold = 1.0f, bloomKnee = 0.5f;
        int bloomIterations = 4;
        glm::ivec2 bloomSize = {0, 0};
        GLuint bloomFrameBuffers[2] = {0, 0};
        Texture2D *bloomTextures[2] = {nullptr, nullptr};
        ShaderProgram *bloomExtractShader = nullptr, *bloomBlurShader = nullptr;

        // ── Shadows ──────────────────────────────────────────────────────────
        bool shadowsEnabled = false;
        int shadowMapSize = 2048;
        float shadowNormalOffset = 0.03f, shadowBias = 0.0004f;
        GLuint shadowFrameBuffer = 0;
        Texture2D *shadowMap = nullptr;
        ShaderProgram *shadowShader = nullptr;

        // ── Environment (lit materials) ──────────────────────────────────────
        glm::vec3 ambientSky = glm::vec3(0.0f), ambientGround = glm::vec3(0.0f);
        glm::vec3 fogColor = glm::vec3(0.0f);
        float fogDensity = 0.0f;

        // Uniforms that are the same for every object (lights, camera, fog...) only need to be sent once per
        // shader program per frame. This set remembers which programs were already prepared this frame.
        std::unordered_set<ShaderProgram *> preparedPrograms;

        void initializeHDR(const nlohmann::json &config);
        void renderShadowMap(int shadowLightIndex, const glm::mat4 &lightVP);
        void renderBloom();
        bool computeShadowMatrix(LightComponent *light, glm::mat4 &lightVP) const;
        void prepareProgram(Material *material, const glm::vec3 &cameraPosition,
                            int shadowLightIndex, const glm::mat4 &lightVP);
        void drawCommand(const RenderCommand &command, const glm::mat4 &VP, const glm::vec3 &cameraPosition,
                         int shadowLightIndex, const glm::mat4 &lightVP);

    public:
        // Initialize the renderer including the sky and the Postprocessing objects.
        // windowSize is the width & height of the window (in pixels).
        void initialize(glm::ivec2 windowSize, const nlohmann::json &config);
        // Clean up the renderer
        void destroy();
        // This function should be called every frame to draw the given world
        void render(World *world);
        // Set u_grayscale uniform on the postprocess shader.
        // intensity: 0.0 = full colour, 1.0 = full grayscale (player KO).
        void setGrayscale(float intensity);
    };

}
