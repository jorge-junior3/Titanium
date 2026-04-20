#version 450

layout(binding = 1) uniform samplerCube skybox;

layout(location = 0) in vec3 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(skybox, fragTexCoord).rgb;
    // no tonemapping here — happens in tonemap pass
    outColor = vec4(color, 1.0);
}