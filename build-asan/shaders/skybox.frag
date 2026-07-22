#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 camPos;
    mat4 lightSpaceMatrix;
    vec4 lightDir;
    vec4 lightColor;
    vec4 objectColor;
    vec4 skyColor;
    vec4 fogColor;
    vec4 fogParams;
} ubo;

layout(binding = 1) uniform samplerCube skybox;

layout(location = 0) in vec3 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(skybox, fragTexCoord).rgb;
    if (ubo.fogParams.y > 0.5) {
        color = mix(color, ubo.skyColor.rgb, 0.35);
    }
    // no tonemapping here — happens in tonemap pass
    outColor = vec4(color, 1.0);
}