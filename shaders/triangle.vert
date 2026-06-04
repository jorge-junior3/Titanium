#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 camPos;
    mat4 lightSpaceMatrix;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 camPos;
layout(location = 4) out vec3 TBN0;
layout(location = 5) out vec3 TBN1;
layout(location = 6) out vec3 TBN2;
layout(location = 7) out vec4 fragPosLightSpace;

void main() {
    vec4 worldPos     = ubo.model * vec4(inPosition, 1.0);
    gl_Position       = ubo.proj * ubo.view * worldPos;
    fragPos           = worldPos.xyz;
    fragUV            = inUV;
    camPos            = ubo.camPos.xyz;
    fragPosLightSpace = ubo.lightSpaceMatrix * worldPos;

    mat3 normalMat = mat3(transpose(inverse(ubo.model)));
    vec3 T = normalize(normalMat * inTangent);
    vec3 B = normalize(normalMat * inBitangent);
    vec3 N = normalize(normalMat * inNormal);
    fragNormal = N;
    TBN0 = T;
    TBN1 = B;
    TBN2 = N;
}