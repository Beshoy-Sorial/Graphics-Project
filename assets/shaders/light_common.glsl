#define MAX_LIGHT_COUNT  16
#define TYPE_DIRECTIONAL  0
#define TYPE_POINT        1
#define TYPE_SPOT         2

struct Light {
    int   type;
    vec3  position;
    vec3  direction;
    vec3  diffuse;
    vec3  specular;
    vec3  ambient;
    float attenuationConstant;
    float attenuationLinear;
    float attenuationQuadratic;
    float innerCutoff;
    float outerCutoff;
};

struct TexturedMaterial {
    sampler2D albedo_tex;
    sampler2D specular_tex;
    sampler2D ambient_occlusion_tex;
    sampler2D roughness_tex;
    sampler2D emissive_tex;
    sampler2D normal_tex;
};


uniform Light lights[MAX_LIGHT_COUNT];
uniform int   light_count;

// ── Shadow mapping (set by the renderer; shadow_enabled stays 0 when shadows are off) ──
uniform int             shadow_enabled;
uniform int             shadow_light;         // index of the light that casts shadows
uniform mat4            light_vp;             // world -> light clip space
uniform sampler2DShadow shadow_map;
uniform float           shadow_texel;         // 1 / shadow map size
uniform float           shadow_normal_offset; // push the lookup along the normal to avoid acne
uniform float           shadow_bias;


// Direction towards the light (L) and the distance / cone attenuation for a light
float lightAttenuation(Light light, vec3 worldPos, out vec3 L){
    if(light.type == TYPE_DIRECTIONAL){
        L = normalize(-light.direction);
        return 1.0;
    }
    vec3  toLight = light.position - worldPos;
    float dist    = length(toLight);
    L = toLight / max(dist, 1e-4);

    float attenuation = 1.0 / (light.attenuationConstant
                             + light.attenuationLinear    * dist
                             + light.attenuationQuadratic * dist * dist);

    if(light.type == TYPE_SPOT){
        float cosTheta = dot(L, normalize(-light.direction));
        attenuation   *= smoothstep(light.outerCutoff, light.innerCutoff, cosTheta);
    }
    return attenuation;
}


// Classic Phong model (the original Phase-2 lighting requirement, used when the HDR pipeline is off)
vec3 calculateLighting(
    vec3  albedo,
    vec3  specTex,
    float ao,
    float shininess,
    vec3  emissive,
    vec3  worldPos,
    vec3  normal,
    vec3  viewDir
){
    vec3 result = emissive;

    for(int i = 0; i < min(light_count, MAX_LIGHT_COUNT); i++){
        Light light = lights[i];

        vec3  L;
        float attenuation = lightAttenuation(light, worldPos, L);

        float NdotL = max(dot(normal, L), 0.0);

        vec3 ambient_contrib  = light.ambient  * albedo * ao;

        vec3 diffuse_contrib  = light.diffuse  * albedo * NdotL;
        float spec            = (NdotL > 0.0)
            ? pow(max(dot(reflect(-L, normal), viewDir), 0.0), shininess)
            : 0.0;
        vec3 specular_contrib = light.specular * specTex * spec;

        result += ambient_contrib + attenuation * (diffuse_contrib + specular_contrib);
    }

    return result;
}


// Percentage-closer filtered shadow lookup: 1 = fully lit, 0 = fully in shadow
float shadowFactor(vec3 worldPos, vec3 normal){
    vec4 lightClip = light_vp * vec4(worldPos + normal * shadow_normal_offset, 1.0);
    if(lightClip.w <= 0.0) return 1.0;
    vec3 p = lightClip.xyz / lightClip.w * 0.5 + 0.5;
    if(p.z >= 1.0 || p.x <= 0.0 || p.y <= 0.0 || p.x >= 1.0 || p.y >= 1.0) return 1.0;

    // 4x4 taps, each one is already bilinearly filtered by the hardware -> soft edges
    float lit = 0.0;
    for(int y = 0; y < 4; y++){
        for(int x = 0; x < 4; x++){
            vec2 offset = (vec2(x, y) - 1.5) * shadow_texel * 1.25;
            lit += texture(shadow_map, vec3(p.xy + offset, p.z - shadow_bias));
        }
    }
    return lit / 16.0;
}


// Enhanced model for the HDR pipeline: normalized Blinn-Phong (energy conserving highlights),
// shadows from the shadow-casting light, all in linear color space.
vec3 calculateLightingEnhanced(
    vec3  albedo,
    vec3  specColor,
    float ao,
    float shininess,
    vec3  worldPos,
    vec3  normal,
    vec3  viewDir,
    out vec3 ambientOut
){
    vec3 direct = vec3(0.0);
    ambientOut  = vec3(0.0);
    float specNormalization = (shininess + 8.0) / 8.0;

    for(int i = 0; i < min(light_count, MAX_LIGHT_COUNT); i++){
        Light light = lights[i];

        vec3  L;
        float attenuation = lightAttenuation(light, worldPos, L);
        ambientOut += light.ambient * albedo * ao;
        if(attenuation <= 0.0001) continue;

        float NdotL = max(dot(normal, L), 0.0);
        if(NdotL <= 0.0) continue;

        if(shadow_enabled == 1 && i == shadow_light){
            attenuation *= shadowFactor(worldPos, normal);
        }

        vec3  H     = normalize(L + viewDir);
        float NdotH = max(dot(normal, H), 0.0);
        float spec  = pow(NdotH, shininess) * specNormalization;

        direct += attenuation * NdotL * (light.diffuse * albedo + light.specular * specColor * spec);
    }
    return direct;
}
