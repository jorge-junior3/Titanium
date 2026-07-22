#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 camPos;
    mat4 lightSpaceMatrix;
    vec4 lightDir;
    vec4 lightColor;
} ubo;

layout(location = 0) out vec3 fragTexCoord;

// unit cube positions — no vertex buffer needed
vec3 positions[36] = vec3[](
    // +X right
    vec3( 1,-1,-1), vec3( 1, 1,-1), vec3( 1, 1, 1),
    vec3( 1, 1, 1), vec3( 1,-1, 1), vec3( 1,-1,-1),
    // -X left
    vec3(-1,-1, 1), vec3(-1, 1, 1), vec3(-1, 1,-1),
    vec3(-1, 1,-1), vec3(-1,-1,-1), vec3(-1,-1, 1),
    // +Y top
    vec3(-1, 1,-1), vec3(-1, 1, 1), vec3( 1, 1, 1),
    vec3( 1, 1, 1), vec3( 1, 1,-1), vec3(-1, 1,-1),
    // -Y bottom
    vec3(-1,-1, 1), vec3(-1,-1,-1), vec3( 1,-1,-1),
    vec3( 1,-1,-1), vec3( 1,-1, 1), vec3(-1,-1, 1),
    // +Z front
    vec3(-1,-1, 1), vec3( 1,-1, 1), vec3( 1, 1, 1),
    vec3( 1, 1, 1), vec3(-1, 1, 1), vec3(-1,-1, 1),
    // -Z back
    vec3( 1,-1,-1), vec3(-1,-1,-1), vec3(-1, 1,-1),
    vec3(-1, 1,-1), vec3( 1, 1,-1), vec3( 1,-1,-1)
);

void main() {
    fragTexCoord = positions[gl_VertexIndex];

    // remove translation from view matrix — skybox stays centered on camera
    mat4 viewNoTranslation = mat4(mat3(ubo.view));
    vec4 pos = ubo.proj * viewNoTranslation * vec4(positions[gl_VertexIndex], 1.0);

    // set z = w so depth is always 1.0 — skybox renders behind everything
    gl_Position = pos.xyww;
}