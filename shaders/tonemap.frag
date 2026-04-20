#version 450

layout(binding = 0) uniform sampler2D hdrImage;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    float exposure;
} push;

vec3 ACESFilmic(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr   = texture(hdrImage, fragUV).rgb;
    hdr       *= push.exposure;
    vec3 color = ACESFilmic(hdr);
    color      = pow(color, vec3(1.0 / 2.2));
    outColor   = vec4(color, 1.0);
}