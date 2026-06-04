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
    vec2 texelSize = vec2(1.0) / vec2(textureSize(hdrImage, 0));
    float threshold = 3.0;
    float bloomIntensity = 1.8;

    vec3 hdr = texture(hdrImage, fragUV).rgb;
    vec3 bloom = vec3(0.0);
    vec3 samp;
    vec3 bright;

    samp = texture(hdrImage, fragUV + vec2(-1.0, -1.0) * texelSize).rgb;
    bright = max(samp - vec3(threshold), vec3(0.0));
    bloom += bright * 0.0625;

    samp = texture(hdrImage, fragUV + vec2(0.0, -1.0) * texelSize).rgb;
    bright = max(samp - vec3(threshold), vec3(0.0));
    bloom += bright * 0.125;

    samp = texture(hdrImage, fragUV + vec2(1.0, -1.0) * texelSize).rgb;
    bright = max(samp - vec3(threshold), vec3(0.0));
    bloom += bright * 0.0625;

    samp = texture(hdrImage, fragUV + vec2(-1.0, 0.0) * texelSize).rgb;
    bright = max(samp - vec3(threshold), vec3(0.0));
    bloom += bright * 0.125;

    samp = texture(hdrImage, fragUV).rgb;
    bright = max(samp - vec3(threshold), vec3(0.0));
    bloom += bright * 0.25;

    samp = texture(hdrImage, fragUV + vec2(1.0, 0.0) * texelSize).rgb;
    bright = max(samp - vec3(threshold), vec3(0.0));
    bloom += bright * 0.125;

    samp = texture(hdrImage, fragUV + vec2(-1.0, 1.0) * texelSize).rgb;
    bright = max(samp - vec3(threshold), vec3(0.0));
    bloom += bright * 0.0625;

    samp = texture(hdrImage, fragUV + vec2(0.0, 1.0) * texelSize).rgb;
    bright = max(samp - vec3(threshold), vec3(0.0));
    bloom += bright * 0.125;

    samp = texture(hdrImage, fragUV + vec2(1.0, 1.0) * texelSize).rgb;
    bright = max(samp - vec3(threshold), vec3(0.0));
    bloom += bright * 0.0625;

    bloom *= bloomIntensity;
    vec3 combined = hdr + bloom;
    combined *= push.exposure;
    vec3 color = ACESFilmic(combined);
    color      = pow(color, vec3(1.0 / 2.2));
    outColor   = vec4(color, 1.0);
}