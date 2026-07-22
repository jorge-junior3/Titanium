// Renderer.h

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
    glm::vec4 lightDir;
    glm::vec4 lightColor;
    glm::vec4 objectColor;
    glm::vec4 skyColor;
    glm::vec4 fogColor;
    glm::vec4 fogParams; // x = density, y = enabled ? 1 : 0
};

struct ShadowUBO {
    glm::mat4 model;
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
    void loadMesh(const std::vector<Vertex>& vertices,
                  const std::vector<uint32_t>& indices);
    void recreateSwapchain();
    void updateUniformBuffer(const UniformBufferObject& ubo);
    glm::mat4 getLightSpaceMatrix() const;
    void setDirectionalLight(const glm::vec3& dir, const glm::vec3& color);
    glm::vec3 getDirectionalLightDir() const;
    glm::vec3 getDirectionalLightColor() const;
    float getLightIntensity() const;
    bool getNormalizeLightDir() const;
    void setLightIntensity(float intensity) { m_lightIntensity = intensity; }
    void setNormalizeLightDir(bool normalize) { m_normalizeLightDir = normalize; }
    void setExposure(float e) { m_exposure = e; }
    float getExposure() const { return m_exposure; }

    enum class SceneNodeType {
        Cube,
    };

    struct SceneNode {
        std::string name;
        SceneNodeType type = SceneNodeType::Cube;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale    = glm::vec3(1.0f);
        glm::vec3 color    = glm::vec3(1.0f);
    };

    struct AtmosphereSettings {
        bool enabled = true;
        glm::vec3 skyColor = glm::vec3(0.15f, 0.25f, 0.45f);
        glm::vec3 fogColor = glm::vec3(0.65f, 0.70f, 0.75f);
        float fogDensity = 0.02f;
    };

    const std::vector<SceneNode>& getSceneNodes() const;
    SceneNode& getSceneNode(size_t index);
    void addCubeNode(const std::string& name);
    void removeSceneNode(size_t index);
    void setSceneNodeName(size_t index, const std::string& name);
    void setSceneNodeTransform(size_t index, const glm::vec3& position,
                               const glm::vec3& rotation,
                               const glm::vec3& scale);
    void setSceneNodeColor(size_t index, const glm::vec3& color);

    const AtmosphereSettings& getAtmosphereSettings() const;
    void setAtmosphereSettings(const AtmosphereSettings& settings);

    void updateGizmoBuffers(const std::vector<GizmoVertex>& vertices);
    void setGizmoNodePos(const glm::vec3& pos);
    void setGizmoHoveredAxis(int axis);

    glm::mat4 getNodeModelMatrix(const SceneNode& node) const;

private:

    // --------------------------------------------------------
    // Init groups
    // --------------------------------------------------------
    void initVulkan();
    void initRenderPasses();
    void initResources();
    void initPipelines();
    void initImGui();
    void destroyImGui();
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
    VkResult submitFrame(VkCommandBuffer cmd, uint32_t imageIndex);

    VulkanContext* m_vulkanContext;

private:
    VkDevice device() const { return m_vulkanContext->getDevice(); }
    VkPhysicalDevice physicalDevice() const { return m_vulkanContext->getPhysicalDevice(); }
    VkFormat swapchainFormat() const { return m_vulkanContext->getSwapchainFormat(); }
    VkExtent2D swapchainExtent() const { return m_vulkanContext->getSwapchainExtent(); }
    const std::vector<VkImageView>& swapchainImageViews() const { return m_vulkanContext->getSwapchainImageViews(); }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

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

    // ImGui
    VkDescriptorPool      m_imguiDescriptorPool = VK_NULL_HANDLE;
    SDL_Window*           m_window = nullptr;

    // UI-driven lighting state
    glm::vec3 m_lightDir = glm::vec3(0.3f, -1.0f, 0.2f);
    glm::vec3 m_lightColor = glm::vec3(12.0f, 11.2f, 10.4f);
    float m_lightIntensity = 1.0f;
    bool m_normalizeLightDir = true;

    float m_exposure = 1.0f;

    // Cached per-frame base uniform state (view/proj/light)
    UniformBufferObject m_cachedUBO{};

    glm::vec3 m_gizmoNodePos    = glm::vec3(0.0f);
    int       m_gizmoHoveredAxis = -1;

    std::vector<SceneNode> m_sceneNodes;
    AtmosphereSettings m_atmosphere;

    // --------------------------------------------------------
    // Gizmo pipeline
    // --------------------------------------------------------
    void createGizmoPipeline();
    void drawGizmo(VkCommandBuffer cmd, const glm::mat4& view,
                   const glm::mat4& proj, const glm::vec3& nodePos,
                   int hoveredAxis);
    void destroyGizmoResources();

    VkPipeline       m_gizmoPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_gizmoPipelineLayout = VK_NULL_HANDLE;
    VkBuffer         m_gizmoVertexBuffer   = VK_NULL_HANDLE;
    VkDeviceMemory   m_gizmoVertexMemory   = VK_NULL_HANDLE;
    uint32_t         m_gizmoVertexCount    = 0;

    // --------------------------------------------------------
    // IBL
    // --------------------------------------------------------
    void createIBL();
    void createIrradianceMap();
    void createPrefilteredMap();
    void createBRDFLut();
    void destroyIBL();

    static constexpr uint32_t IBL_PREFILTER_MIPS = 5;

    VkImage        m_irradianceImage        = VK_NULL_HANDLE;
    VkDeviceMemory m_irradianceMemory       = VK_NULL_HANDLE;
    VkImageView    m_irradianceImageView    = VK_NULL_HANDLE;
    VkImageView    m_irradianceStorageView  = VK_NULL_HANDLE;
    VkSampler      m_irradianceSampler      = VK_NULL_HANDLE;

    VkImage        m_prefilteredImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_prefilteredMemory      = VK_NULL_HANDLE;
    VkImageView    m_prefilteredImageView   = VK_NULL_HANDLE;
    VkSampler      m_prefilteredSampler     = VK_NULL_HANDLE;

    VkImage        m_brdfLutImage           = VK_NULL_HANDLE;
    VkDeviceMemory m_brdfLutMemory          = VK_NULL_HANDLE;
    VkImageView    m_brdfLutImageView       = VK_NULL_HANDLE;
    VkSampler      m_brdfLutSampler         = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_iblComputeDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_iblComputeDescriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_irradianceDescriptorSet       = VK_NULL_HANDLE;
};