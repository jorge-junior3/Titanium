#include "Renderer.h"
#include <stdexcept>
#include <iostream>

void Renderer::createVertexBuffer(const std::vector<Vertex>& vertices) {
    VkDevice device = m_vulkanContext->getDevice();
    VkDeviceSize size = sizeof(Vertex) * vertices.size();

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &m_vertexBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create vertex buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, m_vertexBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = m_vulkanContext->findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_vertexBufferMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate vertex buffer memory");

    vkBindBufferMemory(device, m_vertexBuffer, m_vertexBufferMemory, 0);

    void* data;
    vkMapMemory(device, m_vertexBufferMemory, 0, size, 0, &data);
    memcpy(data, vertices.data(), size);
    vkUnmapMemory(device, m_vertexBufferMemory);

    std::cout << "Vertex buffer created!" << std::endl;
}

void Renderer::createIndexBuffer(const std::vector<uint32_t>& indices) {
    VkDevice device = m_vulkanContext->getDevice();
    VkDeviceSize size = sizeof(uint32_t) * indices.size();
    m_indexCount      = static_cast<uint32_t>(indices.size());

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &m_indexBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create index buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, m_indexBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = m_vulkanContext->findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_indexBufferMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate index buffer memory");

    vkBindBufferMemory(device, m_indexBuffer, m_indexBufferMemory, 0);

    void* data;
    vkMapMemory(device, m_indexBufferMemory, 0, size, 0, &data);
    memcpy(data, indices.data(), size);
    vkUnmapMemory(device, m_indexBufferMemory);

    std::cout << "Index buffer created!" << std::endl;
}

void Renderer::createUniformBuffer() {
    VkDevice device = m_vulkanContext->getDevice();
    VkDeviceSize size = sizeof(UniformBufferObject);

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &m_uniformBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create uniform buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, m_uniformBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = m_vulkanContext->findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_uniformBufferMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate uniform buffer memory");

    vkBindBufferMemory(device, m_uniformBuffer, m_uniformBufferMemory, 0);
    vkMapMemory(device, m_uniformBufferMemory, 0, size, 0, &m_uniformMapped);
}

void Renderer::destroyBuffers() {
    VkDevice device = m_vulkanContext->getDevice();
    vkDestroyBuffer(device, m_indexBuffer, nullptr);
    vkFreeMemory(device, m_indexBufferMemory, nullptr);
    vkDestroyBuffer(device, m_vertexBuffer, nullptr);
    vkFreeMemory(device, m_vertexBufferMemory, nullptr);
    vkDestroyBuffer(device, m_uniformBuffer, nullptr);
    vkFreeMemory(device, m_uniformBufferMemory, nullptr);
}