#version 450

layout(binding = 1) uniform sampler2D albedoMap;
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2D roughnessMap;
layout(binding = 4) uniform sampler2D metallicMap;
layout(binding = 5) uniform sampler2DShadow shadowMap;
layout(binding = 6) uniform samplerCube irradianceMap;
layout(binding = 7) uniform samplerCube prefilteredMap;
layout(binding = 8) uniform sampler2D   brdfLUT;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 camPos;
    mat4 lightSpaceMatrix;
    vec4 lightDir;
    vec4 lightColor;
} ubo;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 camPos;
layout(location = 4) in vec3 TBN0;
layout(location = 5) in vec3 TBN1;
layout(location = 6) in vec3 TBN2;
layout(location = 7) in vec4 fragPosLightSpace;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float D_GGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 F_SchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
              * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 sampleFakeEnv(vec3 dir, float roughness) {
    vec3 skyZenith  = vec3(0.1, 0.3, 0.8);
    vec3 skyHorizon = vec3(0.8, 0.85, 1.0);
    vec3 ground     = vec3(0.08, 0.06, 0.05);
    float upness    = dir.y;
    vec3 sky        = mix(skyHorizon, skyZenith, max(upness, 0.0));
    vec3 env        = mix(ground, sky, smoothstep(-0.1, 0.1, upness));
    vec3 avgSky     = mix(skyHorizon, skyZenith, 0.3);
    return mix(env, avgSky, roughness * roughness);
}

// PCF shadow sampling — samples a 3x3 kernel around the shadow coord
// to produce soft shadow edges instead of hard aliased ones
float shadowPCF(vec4 fragPosLS, float NdotL) {
    // perspective divide — brings coords into [-1, 1]
    vec3 proj = fragPosLS.xyz / fragPosLS.w;

    // remap XY from [-1,1] to [0,1] for texture lookup
    vec2 shadowUV = proj.xy * 0.5 + 0.5;

    // fragment is outside the shadow map — no shadow
    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 ||
        shadowUV.y < 0.0 || shadowUV.y > 1.0 ||
        proj.z > 1.0)
        return 1.0;

    // bias prevents shadow acne — steeper angles need more bias
    float bias    = max(0.005 * (1.0 - NdotL), 0.001);
    float depth   = proj.z - bias;

    // 3x3 PCF kernel — average 9 shadow samples
    float shadow     = 0.0;
    vec2  texelSize  = vec2(1.0 / 2048.0); // match shadow map resolution
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec3 sampleCoord = vec3(shadowUV + vec2(x, y) * texelSize, depth);
            shadow += texture(shadowMap, sampleCoord);
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main() {
    vec3 T = normalize(TBN0);
    vec3 B = normalize(TBN1);
    vec3 Ngeom = normalize(TBN2);
    T = normalize(T - Ngeom * dot(Ngeom, T));
    B = normalize(cross(Ngeom, T));
    mat3 TBN = mat3(T, B, Ngeom);

    vec3  albedo    = pow(texture(albedoMap,    fragUV).rgb, vec3(2.2));
    vec3  normalTex = texture(normalMap,        fragUV).rgb * 2.0 - 1.0;
    float normalScale = 4.0;
    normalTex.xy *= normalScale;
    normalTex = normalize(normalTex);
    float roughness = texture(roughnessMap,     fragUV).r;
    float metallic  = texture(metallicMap,      fragUV).r;

    vec3 N = normalize(TBN * normalTex);
    vec3 V = normalize(camPos - fragPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // directional light based on shared light direction and radiance
    vec3  L        = normalize(ubo.lightDir.xyz);
    vec3  H        = normalize(V + L);
    vec3  radiance = ubo.lightColor.rgb;

    float NDF = D_GGX(N, H, roughness);
    float G   = G_Smith(N, V, L, roughness);
    vec3  F   = F_Schlick(max(dot(H, V), 0.0), F0);

    vec3  num     = NDF * G * F;
    float denom   = 4.0 * max(dot(N, V), 0.0)
                        * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = num / denom;

    vec3  kD    = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);

    // shadow — modulates direct light only, not ambient
    float shadow = shadowPCF(fragPosLightSpace, NdotL);
    vec3  Lo     = (kD * albedo / PI + specular) * radiance * NdotL * shadow;

    vec3 F_ibl      = F_SchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD_ibl     = (vec3(1.0) - F_ibl) * (1.0 - metallic);
    // diffuse IBL
    vec3 irradiance  = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL  = kD_ibl * irradiance * albedo;

    // specular IBL — split sum
    float NdotV_clamped = max(dot(N, V), 0.0);
    vec3  prefilteredColor = textureLod(prefilteredMap, R,
    roughness * float(5 - 1)).rgb; // 5 = IBL_PREFILTER_MIPS
    vec2  brdf      = texture(brdfLUT, vec2(NdotV_clamped, roughness)).rg;
    vec3  specIBL   = prefilteredColor * (F_ibl * brdf.x + brdf.y);

    vec3 ambient = (diffuseIBL + specIBL) * 0.8;
    vec3 color   = ambient + Lo;
    outColor     = vec4(color, 1.0);
}