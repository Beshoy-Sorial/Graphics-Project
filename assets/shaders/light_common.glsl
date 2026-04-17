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
};


uniform Light lights[MAX_LIGHT_COUNT];
uniform int   light_count;


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
        float attenuation = 1.0;

        if(light.type == TYPE_DIRECTIONAL){
            L = normalize(-light.direction);

        } else {
            vec3  toLight = light.position - worldPos;
            float dist    = length(toLight);
            L = normalize(toLight);

            attenuation = 1.0 / (light.attenuationConstant
                               + light.attenuationLinear    * dist
                               + light.attenuationQuadratic * dist * dist);

            if(light.type == TYPE_SPOT){
                float cosTheta   = dot(L, normalize(-light.direction));
                attenuation     *= smoothstep(light.outerCutoff, light.innerCutoff, cosTheta);
            }
        }

        float NdotL = max(dot(normal, L), 0.0);

        vec3 ambient_contrib  = light.ambient  * albedo * ao;

        vec3 diffuse_contrib  = light.diffuse  * albedo * NdotL
        ;
        float spec            = (NdotL > 0.0)
            ? pow(max(dot(reflect(-L, normal), viewDir), 0.0), shininess)
            : 0.0;
        vec3 specular_contrib = light.specular * specTex * spec;

        result += ambient_contrib + attenuation * (diffuse_contrib + specular_contrib);
    }

    return result;
}
