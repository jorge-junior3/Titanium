#version 450

layout(location = 0) out vec2 fragUV;

void main() {
    // Generate a fullscreen triangle using vertex index
    // Triangle vertices: (0,0), (2,0), (0,2) in clip space
    float x = (gl_VertexIndex & 1) == 0 ? -1.0 : 3.0;
    float y = (gl_VertexIndex & 2) == 0 ? -1.0 : 3.0;
    
    gl_Position = vec4(x, y, 0.0, 1.0);
    
    // Map clip coordinates to UV coordinates [0,1]
    fragUV = vec2((x + 1.0) * 0.5, (1.0 - y) * 0.5);
}