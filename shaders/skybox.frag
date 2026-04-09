#version 450

layout(binding = 1) uniform samplerCube skybox;

layout(location = 0) in vec3 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(skybox, fragTexCoord).rgb;

    // same tonemapping + gamma as main shader so they match
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}