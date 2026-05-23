#pragma once
#include <vulkan/vulkan.hpp>
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct Vertex;
struct UniformBufferObject;

class VulkanContext;

class ResourceManager {
public:
    ResourceManager(VulkanContext* vulkanContext);
    ~ResourceManager();

    void init();
    void cleanup();

    // Buffers
    void createVertexBuffer(const std::vector<Vertex>& vertices);
    void createIndexBuffer(const std::vector<uint32_t>& indices);
    void createUniformBuffer();
    void destroyBuffers();

    VkBuffer getVertexBuffer() const { return m_vertexBuffer; }
    VkBuffer getIndexBuffer() const { return m_indexBuffer; }
    VkBuffer getUniformBuffer() const { return m_uniformBuffer; }
    uint32_t getIndexCount() const { return m_indexCount; }
    void* getUniformMapped() const { return m_uniformMapped; }

    // Textures
    void loadPBRTextures();
    void createPBRSampler();
    void destroyPBRTextures();

    VkSampler getPBRSampler() const { return m_pbrSampler; }
    VkImageView getAlbedoImageView() const { return m_albedoImageView; }
    VkImageView getNormalImageView() const { return m_normalImageView; }
    VkImageView getRoughnessImageView() const { return m_roughnessImageView; }
    VkImageView getMetallicImageView() const { return m_metallicImageView; }

    // Descriptors
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSet();
    void destroyDescriptors();

    VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
    VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }

private:
    VulkanContext* m_vulkanContext;

    // Buffers
    VkBuffer       m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer       m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    uint32_t       m_indexCount = 0;
    VkBuffer       m_uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_uniformBufferMemory = VK_NULL_HANDLE;
    void*          m_uniformMapped = nullptr;

    // PBR textures
    VkImage        m_albedoImage = VK_NULL_HANDLE;
    VkDeviceMemory m_albedoMemory = VK_NULL_HANDLE;
    VkImageView    m_albedoImageView = VK_NULL_HANDLE;
    VkImage        m_normalImage = VK_NULL_HANDLE;
    VkDeviceMemory m_normalMemory = VK_NULL_HANDLE;
    VkImageView    m_normalImageView = VK_NULL_HANDLE;
    VkImage        m_roughnessImage = VK_NULL_HANDLE;
    VkDeviceMemory m_roughnessMemory = VK_NULL_HANDLE;
    VkImageView    m_roughnessImageView = VK_NULL_HANDLE;
    VkImage        m_metallicImage = VK_NULL_HANDLE;
    VkDeviceMemory m_metallicMemory = VK_NULL_HANDLE;
    VkImageView    m_metallicImageView = VK_NULL_HANDLE;
    VkSampler      m_pbrSampler = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet       m_descriptorSet = VK_NULL_HANDLE;

    // Utility
    VkImageView createImageView(VkImage image, VkFormat format);
    void createImageAndView(const std::string& path, bool srgb, VkImage& image, VkDeviceMemory& memory, VkImageView& view);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
};