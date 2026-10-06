#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include "../material/material.hpp"
#include "../deserialize-utils.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

namespace our
{

    int g_WeatherMode = 0; // 0: Sun, 1: Rain, 2: Snow

    // Small helper: creates a framebuffer whose only color attachment is the given texture
    static GLuint makeColorFrameBuffer(Texture2D *texture)
    {
        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->getOpenGLName(), 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return fbo;
    }

    static ShaderProgram *makeFullscreenShader(const std::string &fragmentShader)
    {
        ShaderProgram *shader = new ShaderProgram();
        shader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
        shader->attach(fragmentShader, GL_FRAGMENT_SHADER);
        shader->link();
        return shader;
    }

    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json &config)
    {
        // First, we store the window size for later use
        this->windowSize = windowSize;

        hdr = config.value("hdr", false);
        weatherEnabled = config.value("weather", false);
        sortOpaque = config.value("sortOpaque", hdr);
        exposure = config.value("exposure", 1.0f);
        if (config.contains("ambient") && config["ambient"].is_object())
        {
            ambientSky = config["ambient"].value("sky", ambientSky);
            ambientGround = config["ambient"].value("ground", ambientGround);
        }
        if (config.contains("fog") && config["fog"].is_object())
        {
            fogColor = config["fog"].value("color", fogColor);
            fogDensity = config["fog"].value("density", fogDensity);
        }

        // Then we check if there is a sky texture in the configuration
        if (config.contains("sky"))
        {
            // First, we create a sphere which will be used to draw the sky
            // (a detailed panorama needs more segments, otherwise the faceted sphere visibly warps the image)
            int skySegments = config.value("skySegments", 16);
            this->skySphere = mesh_utils::sphere(glm::ivec2(skySegments, skySegments / 2 < 16 ? 16 : skySegments / 2));
            skyTint = config.value("skyTint", skyTint);

            // We can draw the sky using the same shader used to draw textured objects
            ShaderProgram *skyShader = new ShaderProgram();
            skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
            skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
            skyShader->link();

            // TODO: (Req 10) Pick the correct pipeline state to draw the sky
            //  Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth funtion should we pick?
            //  We will draw the sphere from the inside, so what options should we pick for the face culling.
            PipelineState skyPipelineState{};
            skyPipelineState.depthTesting.enabled = true;
            skyPipelineState.depthTesting.function = GL_LEQUAL;
            skyPipelineState.faceCulling.enabled = true;
            skyPipelineState.faceCulling.culledFace = GL_FRONT;

            // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
            std::string skyTextureFile = config.value<std::string>("sky", "");
            Texture2D *skyTexture = texture_utils::loadImage(skyTextureFile, false);

            // Setup a sampler for the sky
            Sampler *skySampler = new Sampler();
            skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
            skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Combine all the aforementioned objects (except the mesh) into a material
            this->skyMaterial = new TexturedMaterial();
            this->skyMaterial->shader = skyShader;
            this->skyMaterial->texture = skyTexture;
            this->skyMaterial->sampler = skySampler;
            this->skyMaterial->pipelineState = skyPipelineState;
            this->skyMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            this->skyMaterial->alphaThreshold = 1.0f;
            this->skyMaterial->transparent = false;
        }

        // The HDR pipeline always needs a final postprocess pass (to tone map the floating point image)
        std::string postprocessShaderFile = config.value<std::string>("postprocess", "");
        if (hdr && postprocessShaderFile.empty())
            postprocessShaderFile = "assets/shaders/postprocess/arena-final.frag";

        // Then we check if there is a postprocessing shader in the configuration
        if (!postprocessShaderFile.empty())
        {
            // TODO: (Req 11) Create a framebuffer
            glGenFramebuffers(1, &postprocessFrameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);

            // TODO: (Req 11) Create a color and a depth texture and attach them to the framebuffer
            //  Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
            //  The depth format can be (Depth component with 24 bits).
            //  (The HDR pipeline uses 16-bit floating point colors so lights can be brighter than 1.0)
            colorTarget = texture_utils::empty(hdr ? GL_RGBA16F : GL_RGBA8, windowSize);
            depthTarget = texture_utils::empty(GL_DEPTH_COMPONENT24, windowSize);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(), 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(), 0);

            // TODO: (Req 11) Unbind the framebuffer just to be safe
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Create a vertex array to use for drawing the texture
            glGenVertexArrays(1, &postProcessVertexArray);

            // Create a sampler to use for sampling the scene texture in the post processing shader
            Sampler *postprocessSampler = new Sampler();
            postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Create the post processing shader
            ShaderProgram *postprocessShader = makeFullscreenShader(postprocessShaderFile);

            // Create a post processing material
            postprocessMaterial = new TexturedMaterial();
            postprocessMaterial->shader = postprocessShader;
            postprocessMaterial->texture = colorTarget;
            postprocessMaterial->sampler = postprocessSampler;
            postprocessMaterial->tint = glm::vec4(1.0f);
            postprocessMaterial->alphaThreshold = 0.0f;
            postprocessMaterial->transparent = false;
            // The default options are fine but we don't need to interact with the depth buffer
            // so it is more performant to disable the depth mask
            postprocessMaterial->pipelineState.depthMask = false;

            if (hdr)
                initializeHDR(config);
        }
        else
        {
            hdr = false; // HDR without a postprocess pass is not possible
        }

        // Shadow mapping (independent of HDR)
        if (config.contains("shadows") && config["shadows"].is_object() && config["shadows"].value("enabled", true))
        {
            const auto &shadowConfig = config["shadows"];
            shadowsEnabled = true;
            shadowMapSize = shadowConfig.value("size", shadowMapSize);
            shadowNormalOffset = shadowConfig.value("normalOffset", shadowNormalOffset);
            shadowBias = shadowConfig.value("bias", shadowBias);

            shadowMap = texture_utils::empty(GL_DEPTH_COMPONENT24, glm::ivec2(shadowMapSize));
            shadowMap->bind();
            // Hardware depth comparison: sampling with a sampler2DShadow returns a filtered 0..1 "lit" factor
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float white[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // outside the map = fully lit
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, white);
            Texture2D::unbind();

            glGenFramebuffers(1, &shadowFrameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFrameBuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap->getOpenGLName(), 0);
            glDrawBuffer(GL_NONE); // depth only
            glReadBuffer(GL_NONE);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                std::cerr << "Shadow framebuffer is incomplete, shadows disabled" << std::endl;
                shadowsEnabled = false;
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            shadowShader = new ShaderProgram();
            shadowShader->attach("assets/shaders/shadow-depth.vert", GL_VERTEX_SHADER);
            shadowShader->attach("assets/shaders/shadow-depth.frag", GL_FRAGMENT_SHADER);
            shadowShader->link();
        }

        // Initialize 3D Weather System
        glGenVertexArrays(1, &weatherVAO);
        glGenBuffers(1, &weatherVBO);
        glBindVertexArray(weatherVAO);
        glBindBuffer(GL_ARRAY_BUFFER, weatherVBO);

        // Create random resting points in [(-30, 0, -30) to (+30, 40, +30)]
        std::vector<glm::vec3> particles;
        particles.reserve(numParticles);
        for (int i = 0; i < numParticles; i++)
        {
            float x = (rand() % 6000) / 100.0f - 30.0f;
            float y = (rand() % 4000) / 100.0f;
            float z = (rand() % 6000) / 100.0f - 30.0f;
            particles.push_back(glm::vec3(x, y, z));
        }

        glBufferData(GL_ARRAY_BUFFER, numParticles * sizeof(glm::vec3), particles.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(glm::vec3), 0);
        glBindVertexArray(0);

        weatherShader = new ShaderProgram();
        weatherShader->attach("assets/shaders/weather_3d.vert", GL_VERTEX_SHADER);
        weatherShader->attach("assets/shaders/weather_3d.frag", GL_FRAGMENT_SHADER);
        weatherShader->link();
    }

    void ForwardRenderer::initializeHDR(const nlohmann::json &config)
    {
        // Multisampled framebuffer: the scene is rendered here, then resolved (averaged) into colorTarget
        msaaSamples = config.value("msaa", 0);
        if (msaaSamples > 1)
        {
            GLint maxSamples = 0;
            glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
            msaaSamples = std::min(msaaSamples, (int)maxSamples);

            glGenFramebuffers(1, &msaaFrameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, msaaFrameBuffer);
            glGenRenderbuffers(1, &msaaColorBuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, msaaColorBuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, GL_RGBA16F, windowSize.x, windowSize.y);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColorBuffer);
            glGenRenderbuffers(1, &msaaDepthBuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthBuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, GL_DEPTH_COMPONENT24, windowSize.x, windowSize.y);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepthBuffer);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                std::cerr << "MSAA framebuffer is incomplete, MSAA disabled" << std::endl;
                glDeleteFramebuffers(1, &msaaFrameBuffer);
                glDeleteRenderbuffers(1, &msaaColorBuffer);
                glDeleteRenderbuffers(1, &msaaDepthBuffer);
                msaaFrameBuffer = msaaColorBuffer = msaaDepthBuffer = 0;
                msaaSamples = 0;
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        else
        {
            msaaSamples = 0;
        }

        // Bloom works at half resolution: cheaper and the blur reaches further
        if (config.contains("bloom") && config["bloom"].is_object() && config["bloom"].value("enabled", true))
        {
            const auto &bloomConfig = config["bloom"];
            bloomEnabled = true;
            bloomStrength = bloomConfig.value("strength", bloomStrength);
            bloomThreshold = bloomConfig.value("threshold", bloomThreshold);
            bloomKnee = bloomConfig.value("knee", bloomKnee);
            bloomIterations = bloomConfig.value("iterations", bloomIterations);
            bloomSize = glm::max(windowSize / 2, glm::ivec2(1));
            for (int i = 0; i < 2; i++)
            {
                bloomTextures[i] = texture_utils::empty(GL_RGBA16F, bloomSize);
                bloomFrameBuffers[i] = makeColorFrameBuffer(bloomTextures[i]);
            }
            bloomExtractShader = makeFullscreenShader("assets/shaders/postprocess/bloom-extract.frag");
            bloomBlurShader = makeFullscreenShader("assets/shaders/postprocess/bloom-blur.frag");
        }
    }

    void ForwardRenderer::destroy()
    {
        // Delete all objects related to the sky
        if (skyMaterial)
        {
            delete skySphere;
            delete skyMaterial->shader;
            delete skyMaterial->texture;
            delete skyMaterial->sampler;
            delete skyMaterial;
            skyMaterial = nullptr;
            skySphere = nullptr;
        }
        // Delete all objects related to post processing
        if (postprocessMaterial)
        {
            glDeleteFramebuffers(1, &postprocessFrameBuffer);
            glDeleteVertexArrays(1, &postProcessVertexArray);
            delete colorTarget;
            delete depthTarget;
            delete postprocessMaterial->sampler;
            delete postprocessMaterial->shader;
            delete postprocessMaterial;
            postprocessMaterial = nullptr;
            colorTarget = depthTarget = nullptr;
        }
        // Delete the HDR / MSAA / bloom objects
        if (msaaFrameBuffer)
        {
            glDeleteFramebuffers(1, &msaaFrameBuffer);
            glDeleteRenderbuffers(1, &msaaColorBuffer);
            glDeleteRenderbuffers(1, &msaaDepthBuffer);
            msaaFrameBuffer = msaaColorBuffer = msaaDepthBuffer = 0;
        }
        for (int i = 0; i < 2; i++)
        {
            if (bloomFrameBuffers[i])
                glDeleteFramebuffers(1, &bloomFrameBuffers[i]);
            delete bloomTextures[i];
            bloomFrameBuffers[i] = 0;
            bloomTextures[i] = nullptr;
        }
        delete bloomExtractShader;
        delete bloomBlurShader;
        bloomExtractShader = bloomBlurShader = nullptr;
        // Delete the shadow map objects
        if (shadowFrameBuffer)
            glDeleteFramebuffers(1, &shadowFrameBuffer);
        shadowFrameBuffer = 0;
        delete shadowMap;
        delete shadowShader;
        shadowMap = nullptr;
        shadowShader = nullptr;

        // Delete 3D Weather
        glDeleteVertexArrays(1, &weatherVAO);
        glDeleteBuffers(1, &weatherVBO);
        if (weatherShader)
            delete weatherShader;
        weatherShader = nullptr;
    }

    // Computes the view-projection matrix of the light used to render the shadow map.
    // Returns false for light types that we don't support (point lights would need a cube map).
    bool ForwardRenderer::computeShadowMatrix(LightComponent *light, glm::mat4 &lightVP) const
    {
        glm::mat4 ltw = light->getOwner()->getLocalToWorldMatrix();
        glm::vec3 position = glm::vec3(ltw * glm::vec4(0, 0, 0, 1));
        glm::vec3 direction = glm::normalize(glm::vec3(ltw * glm::vec4(0, 0, -1, 0)));
        glm::vec3 up = (std::abs(direction.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

        if (light->lightType == LightType::SPOT)
        {
            float fov = std::min(light->outerConeAngle * 2.0f * 1.15f, glm::radians(170.0f));
            lightVP = glm::perspective(fov, 1.0f, 0.5f, 40.0f) *
                      glm::lookAt(position, position + direction, up);
            return true;
        }
        if (light->lightType == LightType::DIRECTIONAL)
        {
            float r = light->shadowRadius;
            glm::vec3 center(0.0f);
            lightVP = glm::ortho(-r, r, -r, r, 0.1f, 80.0f) *
                      glm::lookAt(center - direction * 40.0f, center, up);
            return true;
        }
        return false;
    }

    void ForwardRenderer::renderShadowMap(int shadowLightIndex, const glm::mat4 &lightVP)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFrameBuffer);
        glViewport(0, 0, shadowMapSize, shadowMapSize);
        glDepthMask(GL_TRUE);
        glClearDepth(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE); // many of our models are not closed meshes
        // Slope-scaled depth bias removes "shadow acne" (surfaces wrongly shadowing themselves)
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 3.0f);

        shadowShader->use();
        for (const auto &command : opaqueCommands)
        {
            if (!command.castShadows)
                continue;
            // Skip objects that are clearly outside the light's view (e.g. most of the crowd)
            glm::vec4 clip = lightVP * glm::vec4(command.center, 1.0f);
            if (clip.w <= 0.0f)
                continue;
            glm::vec2 ndc = glm::vec2(clip) / clip.w;
            if (std::abs(ndc.x) > 1.6f || std::abs(ndc.y) > 1.6f)
                continue;
            shadowShader->set("transform", lightVP * command.localToWorld);
            command.mesh->draw();
        }

        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Uniforms shared by every object drawn with the same program (sent once per frame per program)
    void ForwardRenderer::prepareProgram(Material *material, const glm::vec3 &cameraPosition,
                                         int shadowLightIndex, const glm::mat4 &lightVP)
    {
        ShaderProgram *shader = material->shader;
        if (!preparedPrograms.insert(shader).second)
            return;

        // Shaders encode the weather as 0 = none, 1 = sun, 2 = rain, 3 = snow so that a shader whose
        // uniform was never set (value 0) is not affected by the weather.
        shader->set("weatherMode", weatherEnabled ? g_WeatherMode + 1 : 0);
        shader->set("hdr_pipeline", hdr ? 1 : 0);

        if (!dynamic_cast<LitMaterial *>(material))
            return;

        shader->set("camera_position", cameraPosition);
        shader->set("ambient_sky", ambientSky);
        shader->set("ambient_ground", ambientGround);
        shader->set("fog_color", fogColor);
        shader->set("fog_density", fogDensity);

        shader->set("shadow_enabled", shadowLightIndex >= 0 ? 1 : 0);
        shader->set("shadow_light", shadowLightIndex);
        shader->set("light_vp", lightVP);
        shader->set("shadow_texel", 1.0f / (float)shadowMapSize);
        shader->set("shadow_normal_offset", shadowNormalOffset);
        shader->set("shadow_bias", shadowBias);

        int count = static_cast<int>(lights.size());
        if (count > 16)
            count = 16;
        shader->set("light_count", count);
        for (int i = 0; i < count; i++)
        {
            LightComponent *lc = lights[i];
            std::string base = "lights[" + std::to_string(i) + "].";
            shader->set(base + "type", static_cast<int>(lc->lightType));
            glm::mat4 ltw = lc->getOwner()->getLocalToWorldMatrix();
            shader->set(base + "position", glm::vec3(ltw * glm::vec4(0, 0, 0, 1)));
            shader->set(base + "direction", glm::normalize(glm::vec3(ltw * glm::vec4(0, 0, -1, 0))));
            shader->set(base + "diffuse", lc->diffuse);
            shader->set(base + "specular", lc->specular);
            shader->set(base + "ambient", lc->ambient);
            shader->set(base + "attenuationConstant", lc->attenuationConstant);
            shader->set(base + "attenuationLinear", lc->attenuationLinear);
            shader->set(base + "attenuationQuadratic", lc->attenuationQuadratic);
            shader->set(base + "innerCutoff", glm::cos(lc->innerConeAngle));
            shader->set(base + "outerCutoff", glm::cos(lc->outerConeAngle));
        }
    }

    void ForwardRenderer::drawCommand(const RenderCommand &command, const glm::mat4 &VP, const glm::vec3 &cameraPosition,
                                      int shadowLightIndex, const glm::mat4 &lightVP)
    {
        command.material->setup();
        prepareProgram(command.material, cameraPosition, shadowLightIndex, lightVP);
        command.material->shader->set("transform", VP * command.localToWorld);
        if (dynamic_cast<LitMaterial *>(command.material))
        {
            ShaderProgram *shader = command.material->shader;
            shader->set("object_to_world", command.localToWorld);
            shader->set("object_to_world_inv_transpose", glm::transpose(glm::inverse(command.localToWorld)));
            shader->set("object_emissive", command.emissive);
        }
        command.mesh->draw();
    }

    void ForwardRenderer::renderBloom()
    {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glColorMask(true, true, true, true);
        glViewport(0, 0, bloomSize.x, bloomSize.y);
        glBindVertexArray(postProcessVertexArray);
        glActiveTexture(GL_TEXTURE0);
        glBindSampler(0, 0);

        // 1) Keep only the bright parts of the image (and downsample to half resolution)
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFrameBuffers[0]);
        bloomExtractShader->use();
        colorTarget->bind();
        bloomExtractShader->set("tex", 0);
        bloomExtractShader->set("threshold", bloomThreshold);
        bloomExtractShader->set("knee", bloomKnee);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 2) Blur them with a separable gaussian blur, ping-ponging between the two bloom buffers
        bloomBlurShader->use();
        bloomBlurShader->set("tex", 0);
        for (int i = 0; i < bloomIterations; i++)
        {
            float spread = 1.0f + (float)i; // every iteration reaches a bit further
            glBindFramebuffer(GL_FRAMEBUFFER, bloomFrameBuffers[1]);
            bloomTextures[0]->bind();
            bloomBlurShader->set("direction", glm::vec2(spread / bloomSize.x, 0.0f));
            glDrawArrays(GL_TRIANGLES, 0, 3);

            glBindFramebuffer(GL_FRAMEBUFFER, bloomFrameBuffers[0]);
            bloomTextures[1]->bind();
            bloomBlurShader->set("direction", glm::vec2(0.0f, spread / bloomSize.y));
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowSize.x, windowSize.y);
    }

    void ForwardRenderer::render(World *world)
    {
        // First of all, we search for a camera and for all the mesh renderers
        CameraComponent *camera = nullptr;
        opaqueCommands.clear();
        transparentCommands.clear();
        lights.clear();
        preparedPrograms.clear();
        for (auto entity : world->getEntities())
        {
            // If we hadn't found a camera yet, we look for a camera in this entity
            if (!camera)
                camera = entity->getComponent<CameraComponent>();
            // Collect lights
            if (auto light = entity->getComponent<LightComponent>(); light)
            {
                lights.push_back(light);
            }
            // If this entity has a mesh renderer component
            if (auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer)
            {
                // We construct a command from it
                RenderCommand command;
                command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
                command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
                command.mesh = meshRenderer->mesh;
                command.material = meshRenderer->material;
                command.emissive = meshRenderer->emissive;
                command.castShadows = meshRenderer->castShadows;
                command.depth = 0.0f;
                // Skip if mesh, material, or shader is missing (asset typo / load failure)
                if (!command.mesh || !command.material || !command.material->shader) continue;
                // if it is transparent, we add it to the transparent commands list
                if (command.material->transparent)
                {
                    transparentCommands.push_back(command);
                }
                else
                {
                    // Otherwise, we add it to the opaque command list
                    opaqueCommands.push_back(command);
                }
            }
        }

        // If there is no camera, we return (we cannot render without a camera)
        if (camera == nullptr)
            return;

        // TODO: (Req 9) Modify the following line such that "cameraForward" contains a vector pointing the camera forward direction
        //  HINT: See how you wrote the CameraComponent::getViewMatrix, it should help you solve this one
        glm::mat4 cameraLocalToWorld = camera->getOwner()->getLocalToWorldMatrix();
        glm::vec3 cameraPosition = glm::vec3(cameraLocalToWorld * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        glm::vec3 cameraForward = glm::vec3(cameraLocalToWorld * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
        std::sort(transparentCommands.begin(), transparentCommands.end(), [cameraForward, cameraPosition](const RenderCommand &first, const RenderCommand &second)
                  {
            //TODO: (Req 9) Finish this function
            // HINT: the following return should return true "first" should be drawn before "second".
            return glm::dot(first.center - cameraPosition, cameraForward) > glm::dot(second.center - cameraPosition, cameraForward); });

        // Optimization: draw opaque objects front-to-back so the depth test rejects hidden pixels
        // before their (expensive) lighting is computed.
        if (sortOpaque)
        {
            for (auto &command : opaqueCommands)
                command.depth = glm::dot(command.center - cameraPosition, cameraForward);
            std::sort(opaqueCommands.begin(), opaqueCommands.end(), [](const RenderCommand &a, const RenderCommand &b)
                      { return a.depth < b.depth; });
        }

        // â”€â”€ Shadow pass â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        int shadowLightIndex = -1;
        glm::mat4 lightVP(1.0f);
        if (shadowsEnabled)
        {
            for (int i = 0; i < (int)lights.size() && i < 16; i++)
            {
                if (lights[i]->castShadows && computeShadowMatrix(lights[i], lightVP))
                {
                    shadowLightIndex = i;
                    break;
                }
            }
            if (shadowLightIndex >= 0)
                renderShadowMap(shadowLightIndex, lightVP);
        }

        // TODO: (Req 9) Get the camera ViewProjection matrix and store it in VP
        glm::mat4 VP = camera->getProjectionMatrix(windowSize) * camera->getViewMatrix();

        // TODO: (Req 9) Set the OpenGL viewport using viewportStart and viewportSize
        glViewport(0, 0, windowSize.x, windowSize.y);

        // TODO: (Req 9) Set the clear color to black and the clear depth to 1
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f);

        // TODO: (Req 9) Set the color mask to true and the depth mask to true (to ensure the glClear will affect the framebuffer)
        glColorMask(true, true, true, true);
        glDepthMask(true);

        // If there is a postprocess material, bind the framebuffer
        if (postprocessMaterial)
        {
            // TODO: (Req 11) bind the framebuffer
            // (with MSAA we first draw into the multisampled framebuffer and resolve it afterwards)
            glBindFramebuffer(GL_FRAMEBUFFER, msaaFrameBuffer ? msaaFrameBuffer : postprocessFrameBuffer);
        }

        // TODO: (Req 9) Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Make the shadow map available to the lit shader on texture unit 5
        if (shadowLightIndex >= 0)
        {
            glActiveTexture(GL_TEXTURE5);
            shadowMap->bind();
            glBindSampler(5, 0); // use the texture's own (comparison) parameters
            glActiveTexture(GL_TEXTURE0);
        }

        // TODO: (Req 9) Draw all the opaque commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for (const auto &command : opaqueCommands)
        {
            drawCommand(command, VP, cameraPosition, shadowLightIndex, lightVP);
        }

        // If there is a sky material, draw the sky
        if (this->skyMaterial)
        {
            glm::vec4 weatherTint = glm::vec4(1.0f);
            if (weatherEnabled)
            {
                if (g_WeatherMode == 0) {
                    weatherTint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Bright sunny sky
                } else if (g_WeatherMode == 1) {
                    weatherTint = glm::vec4(0.15f, 0.15f, 0.25f, 1.0f); // Dark night
                } else if (g_WeatherMode == 2) {
                    weatherTint = glm::vec4(0.7f, 0.7f, 0.75f, 1.0f); // Overcast snow
                }
            }
            skyMaterial->tint = weatherTint * skyTint;

            // TODO: (Req 10) setup the sky material
            skyMaterial->setup();
            prepareProgram(skyMaterial, cameraPosition, shadowLightIndex, lightVP);

            // TODO: (Req 10) Get the camera position

            // TODO: (Req 10) Create a model matrix for the sy such that it always follows the camera (sky sphere center = camera position)
            glm::mat4 model = glm::translate(glm::mat4(1.0f), cameraPosition);

            // TODO: (Req 10) We want the sky to be drawn behind everything (in NDC space, z=1)
            //  We can acheive the is by multiplying by an extra matrix after the projection but what values should we put in it?
            glm::mat4 alwaysBehindTransform = glm::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f);
            // TODO: (Req 10) set the "transform" uniform
            skyMaterial->shader->set("transform", alwaysBehindTransform * VP * model);

            // TODO: (Req 10) draw the sky sphere
            skySphere->draw();
        }
        // TODO: (Req 9) Draw all the transparent commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for (const auto &command : transparentCommands)
        {
            drawCommand(command, VP, cameraPosition, shadowLightIndex, lightVP);
        }

        // --- Render 3D Weather System ---
        if (weatherEnabled && (g_WeatherMode == 1 || g_WeatherMode == 2))
        {
            weatherShader->use();
            weatherShader->set("VP", VP);
            weatherShader->set("time", (float)glfwGetTime());
            weatherShader->set("weatherMode", g_WeatherMode);
            weatherShader->set("hdr_pipeline", hdr ? 1 : 0);

            // Enable point size scaling in shader
            glEnable(GL_PROGRAM_POINT_SIZE);
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE); // Don't clip against other particles

            glBindVertexArray(weatherVAO);
            glDrawArrays(GL_POINTS, 0, numParticles);
            glBindVertexArray(0);

            glDisable(GL_PROGRAM_POINT_SIZE);
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE); // Re-enable depth writing
        }

        // If there is a postprocess material, apply postprocessing
        if (postprocessMaterial)
        {
            // Resolve the multisampled image (average the samples of every pixel) into the color target
            if (msaaFrameBuffer)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFrameBuffer);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);
                glBlitFramebuffer(0, 0, windowSize.x, windowSize.y, 0, 0, windowSize.x, windowSize.y,
                                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }

            if (bloomEnabled)
                renderBloom();

            // TODO: (Req 11) Return to the default framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
            postprocessMaterial->setup();
            if (hdr)
            {
                ShaderProgram *shader = postprocessMaterial->shader;
                glActiveTexture(GL_TEXTURE1);
                (bloomEnabled ? bloomTextures[0] : colorTarget)->bind();
                glBindSampler(1, 0);
                glActiveTexture(GL_TEXTURE0);
                shader->set("bloom_tex", 1);
                shader->set("bloom_strength", bloomEnabled ? bloomStrength : 0.0f);
                shader->set("exposure", exposure);
                shader->set("time", (float)glfwGetTime());
            }

            glBindVertexArray(postProcessVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }

    void ForwardRenderer::setGrayscale(float intensity)
    {
        if (postprocessMaterial && postprocessMaterial->shader)
        {
            postprocessMaterial->shader->use();
            postprocessMaterial->shader->set("u_grayscale", intensity);
        }
    }

}
