#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <array>

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;
    glm::vec3 bitangent;
    
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, 6> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 6> attrs{};

        attrs[0].binding  = 0;
        attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset   = offsetof(Vertex, position);

        attrs[1].binding  = 0;
        attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[1].offset   = offsetof(Vertex, color);

        attrs[2].binding  = 0;
        attrs[2].location = 2;
        attrs[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[2].offset   = offsetof(Vertex, normal);

        attrs[3].binding  = 0;
        attrs[3].location = 3;
        attrs[3].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[3].offset   = offsetof(Vertex, uv);

        attrs[4].binding  = 0;
        attrs[4].location = 4;
        attrs[4].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[4].offset   = offsetof(Vertex, tangent);

        attrs[5].binding  = 0;
        attrs[5].location = 5;
        attrs[5].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[5].offset   = offsetof(Vertex, bitangent);

        return attrs;
    }

};
struct Mesh {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
};