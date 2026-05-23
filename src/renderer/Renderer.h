#pragma once
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vector>
#include <array>
#include <string>
#include <glm/glm.hpp>
#include "Mesh.h"
#include "VulkanContext.h"
#include "ResourceManager.h"

// ============================================================
// Uniform buffer objects
// ============================================================

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 camPos;
    glm::mat4 lightSpaceMatrix;
};

struct ShadowUBO {
    glm::mat4 lightSpaceMatrix;
};

// ============================================================
// Renderer
// ============================================================

class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();

    void drawFrame();
    void updateUniformBuffer(const UniformBufferObject& ubo);

private:

    // --------------------------------------------------------
    // Init groups
    // --------------------------------------------------------
    void initVulkan();
    void initRenderPasses();
    void initResources();
    void initPipelines();
    void initAssets();

    // --------------------------------------------------------
    // Destroy groups
    // --------------------------------------------------------
    void destroySync();
    void destroyAssets();
    void destroyPipelines();
    void destroyResources();
    void destroyVulkan();

    // --------------------------------------------------------
    // Frame
    // --------------------------------------------------------
    void drawHDRPass(VkCommandBuffer cmd);
    void submitFrame(VkCommandBuffer cmd, uint32_t imageIndex);

    VulkanContext* m_vulkanContext;

private:
    VkDevice device() const { return m_vulkanContext->getDevice(); }
    VkPhysicalDevice physicalDevice() const { return m_vulkanContext->getPhysicalDevice(); }
    VkFormat swapchainFormat() const { return m_vulkanContext->getSwapchainFormat(); }
    VkExtent2D swapchainExtent() const { return m_vulkanContext->getSwapchainExtent(); }
    const std::vector<VkImageView>& swapchainImageViews() const { return m_vulkanContext->getSwapchainImageViews(); }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const { return m_vulkanContext->findMemoryType(typeFilter, properties); }

    std::vector<VkFramebuffer> m_framebuffers;

    // --------------------------------------------------------
    // Depth
    // --------------------------------------------------------
    void createDepthResources();
    void destroyDepthResources();
    VkFormat findDepthFormat();

    VkImage        m_depthImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView    m_depthImageView   = VK_NULL_HANDLE;

    // --------------------------------------------------------
    // Main render pass
    // --------------------------------------------------------
    void createRenderPass();
    void createFramebuffers();

    VkRenderPass   m_renderPass   = VK_NULL_HANDLE;
    VkPipeline     m_pipeline     = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    // --------------------------------------------------------
    // Main graphics pipeline
    // --------------------------------------------------------
    void createGraphicsPipeline();

    // --------------------------------------------------------
    // Descriptors
    // --------------------------------------------------------
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSet();
    void destroyDescriptors();

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_descriptorSet       = VK_NULL_HANDLE;

    // --------------------------------------------------------
    // Uniform buffer
    // --------------------------------------------------------
    void createUniformBuffer();

    VkBuffer       m_uniformBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory m_uniformBufferMemory = VK_NULL_HANDLE;
    void*          m_uniformMapped       = nullptr;

    // --------------------------------------------------------
    // Geometry buffers
    // --------------------------------------------------------
    void createVertexBuffer(const std::vector<Vertex>& vertices);
    void createIndexBuffer(const std::vector<uint32_t>& indices);
    void destroyBuffers();

    VkBuffer       m_vertexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer       m_indexBuffer        = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory  = VK_NULL_HANDLE;
    uint32_t       m_indexCount         = 0;

    // --------------------------------------------------------
    // PBR textures
    // --------------------------------------------------------
    void loadPBRTextures();
    void createPBRSampler();
    void destroyPBRTextures();

    VkImage        m_albedoImage      = VK_NULL_HANDLE;
    VkDeviceMemory m_albedoMemory     = VK_NULL_HANDLE;
    VkImageView    m_albedoImageView  = VK_NULL_HANDLE;

    VkImage        m_normalImage      = VK_NULL_HANDLE;
    VkDeviceMemory m_normalMemory     = VK_NULL_HANDLE;
    VkImageView    m_normalImageView  = VK_NULL_HANDLE;

    VkImage        m_roughnessImage      = VK_NULL_HANDLE;
    VkDeviceMemory m_roughnessMemory     = VK_NULL_HANDLE;
    VkImageView    m_roughnessImageView  = VK_NULL_HANDLE;

    VkImage        m_metallicImage      = VK_NULL_HANDLE;
    VkDeviceMemory m_metallicMemory     = VK_NULL_HANDLE;
    VkImageView    m_metallicImageView  = VK_NULL_HANDLE;

    VkSampler m_pbrSampler = VK_NULL_HANDLE;

    // --------------------------------------------------------
    // Texture utilities
    // --------------------------------------------------------
    VkImageView createImageView(VkImage image, VkFormat format);
    void createImageAndView(const std::string& path, bool srgb,
                            VkImage& image, VkDeviceMemory& memory, VkImageView& view);
    void createTextureImage(const std::string& path);
    void createTextureImageView();
    void createTextureSampler();

    VkImage        m_textureImage     = VK_NULL_HANDLE;
    VkDeviceMemory m_textureMemory    = VK_NULL_HANDLE;
    VkImageView    m_textureImageView = VK_NULL_HANDLE;
    VkSampler      m_textureSampler   = VK_NULL_HANDLE;

    // --------------------------------------------------------
    // Command utilities
    // --------------------------------------------------------
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void transitionImageLayout(VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout);

    // --------------------------------------------------------
    // Shadow system
    // --------------------------------------------------------
    void createShadowResources();
    void createShadowRenderPass();
    void createShadowFramebuffer();
    void createShadowDescriptors();
    void createShadowPipeline();
    void drawShadowPass(VkCommandBuffer cmd);
    void destroyShadowResources();
    glm::mat4 getLightSpaceMatrix();

    static constexpr uint32_t SHADOW_MAP_SIZE = 2048;

    VkImage        m_shadowImage          = VK_NULL_HANDLE;
    VkDeviceMemory m_shadowMemory         = VK_NULL_HANDLE;
    VkImageView    m_shadowImageView      = VK_NULL_HANDLE;
    VkSampler      m_shadowSampler        = VK_NULL_HANDLE;
    VkRenderPass   m_shadowRenderPass     = VK_NULL_HANDLE;
    VkFramebuffer  m_shadowFramebuffer    = VK_NULL_HANDLE;
    VkPipeline     m_shadowPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowPipelineLayout = VK_NULL_HANDLE;
    VkBuffer       m_shadowUniformBuffer  = VK_NULL_HANDLE;
    VkDeviceMemory m_shadowUniformMemory  = VK_NULL_HANDLE;
    void*          m_shadowUniformMapped  = nullptr;

    VkDescriptorSetLayout m_shadowDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_shadowDescriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_shadowDescriptorSet       = VK_NULL_HANDLE;

    // --------------------------------------------------------
    // Skybox system
    // --------------------------------------------------------
    void createCubemap();
    void createSkyboxDescriptors();
    void createSkyboxPipeline();
    void drawSkybox(VkCommandBuffer cmd);
    void destroySkyboxResources();

    VkImage        m_cubemapImage     = VK_NULL_HANDLE;
    VkDeviceMemory m_cubemapMemory    = VK_NULL_HANDLE;
    VkImageView    m_cubemapImageView = VK_NULL_HANDLE;
    VkSampler      m_cubemapSampler   = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_skyboxDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_skyboxDescriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_skyboxDescriptorSet       = VK_NULL_HANDLE;

    VkPipeline       m_skyboxPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_skyboxPipelineLayout = VK_NULL_HANDLE;

    // --------------------------------------------------------
    // HDR + tonemapping system
    // --------------------------------------------------------
    void createHDRRenderPass();
    void createHDRResources();
    void createHDRPipeline();
    void createTonemapDescriptors();
    void createTonemapPipeline();
    void createTonemapPass();
    void drawTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex);
    void destroyHDRResources();

    VkImage        m_hdrImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_hdrMemory      = VK_NULL_HANDLE;
    VkImageView    m_hdrImageView   = VK_NULL_HANDLE;
    VkSampler      m_hdrSampler     = VK_NULL_HANDLE;
    VkRenderPass   m_hdrRenderPass  = VK_NULL_HANDLE;
    VkFramebuffer  m_hdrFramebuffer = VK_NULL_HANDLE;

    VkPipeline     m_hdrPipeline     = VK_NULL_HANDLE;
    VkPipelineLayout m_hdrPipelineLayout = VK_NULL_HANDLE;

    VkRenderPass     m_tonemapRenderPass     = VK_NULL_HANDLE;
    VkPipeline       m_tonemapPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_tonemapPipelineLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_tonemapDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_tonemapDescriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_tonemapDescriptorSet       = VK_NULL_HANDLE;

    float m_exposure = 1.0f;
};