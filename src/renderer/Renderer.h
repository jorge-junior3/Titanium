#pragma once
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vector>
#include <array>
#include <string>
#include "Mesh.h"
#include <glm/glm.hpp>

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 camPos;
    glm::mat4 lightSpaceMatrix;
};



class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();
    void drawFrame();
    void updateUniformBuffer(const UniformBufferObject& ubo);

private:
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSurface();
    void createSwapchain();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createGraphicsPipeline();  // ← was missing

    VkInstance       m_instance       = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice         m_device         = VK_NULL_HANDLE;
    VkQueue          m_graphicsQueue  = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface        = VK_NULL_HANDLE;
    VkSwapchainKHR   m_swapchain      = VK_NULL_HANDLE;
    VkFormat         m_swapchainFormat;
    VkExtent2D       m_swapchainExtent;
    VkRenderPass     m_renderPass     = VK_NULL_HANDLE;
    VkCommandPool    m_commandPool    = VK_NULL_HANDLE;
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;  // ← was missing
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;  // ← was missing

    std::vector<VkImage>         m_swapchainImages;
    std::vector<VkImageView>     m_swapchainImageViews;
    std::vector<VkFramebuffer>   m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;

    VkSemaphore m_imageAvailable = VK_NULL_HANDLE;
    VkSemaphore m_renderFinished = VK_NULL_HANDLE;
    VkFence     m_inFlight       = VK_NULL_HANDLE;

    // Shadow mapping
    

    uint32_t    m_graphicsFamily = 0;
    SDL_Window* m_window         = nullptr;

    void createVertexBuffer(const std::vector<Vertex>& vertices);
    void createIndexBuffer(const std::vector<uint32_t>& indices);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkBuffer       m_vertexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer       m_indexBuffer        = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory  = VK_NULL_HANDLE;
    uint32_t       m_indexCount         = 0;
    
    void createDescriptorSetLayout();
    void createUniformBuffer();
    void createDescriptorPool();
    void createDescriptorSet();

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_descriptorSet       = VK_NULL_HANDLE;
    VkBuffer              m_uniformBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory        m_uniformBufferMemory = VK_NULL_HANDLE;
    void*                 m_uniformMapped       = nullptr;

    VkImage        m_depthImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView    m_depthImageView   = VK_NULL_HANDLE;

    void createDepthResources();
    VkFormat findDepthFormat();

    // add to private members:
    VkImage        m_textureImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_textureMemory      = VK_NULL_HANDLE;
    VkImageView    m_textureImageView   = VK_NULL_HANDLE;
    VkSampler      m_textureSampler     = VK_NULL_HANDLE;

    // add private methods:
    void createTextureImage(const std::string& path);
    void createTextureImageView();
    void createTextureSampler();
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);
    void transitionImageLayout(VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image,
                           uint32_t width, uint32_t height);
    // PBR textures — one set per map
    VkImage        m_albedoImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_albedoMemory      = VK_NULL_HANDLE;
    VkImageView    m_albedoImageView   = VK_NULL_HANDLE;

    VkImage        m_normalImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_normalMemory      = VK_NULL_HANDLE;
    VkImageView    m_normalImageView   = VK_NULL_HANDLE;

    VkImage        m_roughnessImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_roughnessMemory      = VK_NULL_HANDLE;
    VkImageView    m_roughnessImageView   = VK_NULL_HANDLE;

    VkImage        m_metallicImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_metallicMemory      = VK_NULL_HANDLE;
    VkImageView    m_metallicImageView   = VK_NULL_HANDLE;

    VkSampler      m_pbrSampler = VK_NULL_HANDLE;

    void loadPBRTextures();
    VkImageView createImageView(VkImage image, VkFormat format);
    void createImageAndView(const std::string& path, bool srgb,
                            VkImage& image, VkDeviceMemory& memory, VkImageView& view);
    void createPBRSampler();
    static constexpr uint32_t SHADOW_MAP_SIZE = 2048;
    VkImage        m_shadowImage           = VK_NULL_HANDLE;
    VkDeviceMemory m_shadowMemory          = VK_NULL_HANDLE;
    VkImageView    m_shadowImageView       = VK_NULL_HANDLE;
    VkSampler      m_shadowSampler         = VK_NULL_HANDLE;
    VkRenderPass   m_shadowRenderPass      = VK_NULL_HANDLE;
    VkFramebuffer  m_shadowFramebuffer     = VK_NULL_HANDLE;
    VkPipeline     m_shadowPipeline        = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowPipelineLayout = VK_NULL_HANDLE;
    VkBuffer       m_shadowUniformBuffer   = VK_NULL_HANDLE;
    VkDeviceMemory m_shadowUniformMemory   = VK_NULL_HANDLE;
    void*          m_shadowUniformMapped   = nullptr;

    struct ShadowUBO {
        glm::mat4 lightSpaceMatrix;
    };

    VkDescriptorSetLayout m_shadowDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_shadowDescriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_shadowDescriptorSet       = VK_NULL_HANDLE;

    void createShadowResources();
    void createShadowRenderPass();
    void createShadowFramebuffer();
    void createShadowPipeline();
    void createShadowDescriptors();
    void drawShadowPass(VkCommandBuffer cmd);
    glm::mat4 getLightSpaceMatrix();

    // skybox
    VkImage        m_cubemapImage      = VK_NULL_HANDLE;
    VkDeviceMemory m_cubemapMemory     = VK_NULL_HANDLE;
    VkImageView    m_cubemapImageView  = VK_NULL_HANDLE;
    VkSampler      m_cubemapSampler    = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_skyboxDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_skyboxDescriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_skyboxDescriptorSet       = VK_NULL_HANDLE;

    VkPipeline       m_skyboxPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_skyboxPipelineLayout = VK_NULL_HANDLE;

    void createCubemap();
    void createSkyboxPipeline();
    void createSkyboxDescriptors();
    void drawSkybox(VkCommandBuffer cmd);
};