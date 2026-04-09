#version 450

layout(binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

void main() {
    gl_Position = ubo.lightSpaceMatrix * vec4(inPosition, 1.0);
}