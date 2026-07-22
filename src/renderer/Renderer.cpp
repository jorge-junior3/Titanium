// Renderer.cpp

#include "Renderer.h"
#include "MeshLoader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#ifndef ASSETS_PATH
#define ASSETS_PATH "assets/"
#endif

// ============================================================
// Memory type helper — delegates to VulkanContext
// ============================================================

uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const {
    return m_vulkanContext->findMemoryType(typeFilter, props);
}

void Renderer::setDirectionalLight(const glm::vec3& dir, const glm::vec3& color) {
    m_lightDir = dir;
    m_lightColor = color;
}

glm::vec3 Renderer::getDirectionalLightDir() const { return m_lightDir; }

glm::vec3 Renderer::getDirectionalLightColor() const { return m_lightColor; }

float Renderer::getLightIntensity() const { return m_lightIntensity; }
bool Renderer::getNormalizeLightDir() const { return m_normalizeLightDir; }

const Renderer::AtmosphereSettings& Renderer::getAtmosphereSettings() const {
    return m_atmosphere;
}

void Renderer::setAtmosphereSettings(const AtmosphereSettings& settings) {
    m_atmosphere = settings;
}

const std::vector<Renderer::SceneNode>& Renderer::getSceneNodes() const {
    return m_sceneNodes;
}

Renderer::SceneNode& Renderer::getSceneNode(size_t index) {
    if (index >= m_sceneNodes.size())
        throw std::out_of_range("Scene node index out of range");
    return m_sceneNodes[index];
}

void Renderer::addCubeNode(const std::string& name) {
    SceneNode node;
    node.name = name;
    node.type = SceneNodeType::Cube;
    node.position = glm::vec3(0.0f);
    node.rotation = glm::vec3(0.0f);
    node.scale    = glm::vec3(1.0f);
    node.color    = glm::vec3(0.8f, 0.8f, 0.8f);
    m_sceneNodes.push_back(std::move(node));
}

void Renderer::removeSceneNode(size_t index) {
    if (index >= m_sceneNodes.size())
        return;
    m_sceneNodes.erase(m_sceneNodes.begin() + index);
}

void Renderer::setSceneNodeName(size_t index, const std::string& name) {
    if (index >= m_sceneNodes.size())
        return;
    m_sceneNodes[index].name = name;
}

void Renderer::setSceneNodeTransform(size_t index, const glm::vec3& position,
                                    const glm::vec3& rotation,
                                    const glm::vec3& scale) {
    if (index >= m_sceneNodes.size())
        return;
    m_sceneNodes[index].position = position;
    m_sceneNodes[index].rotation = rotation;
    m_sceneNodes[index].scale    = scale;
}

void Renderer::setSceneNodeColor(size_t index, const glm::vec3& color) {
    if (index >= m_sceneNodes.size())
        return;
    m_sceneNodes[index].color = color;
}

glm::mat4 Renderer::getNodeModelMatrix(const SceneNode& node) const {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, node.position);
    model = glm::rotate(model, glm::radians(node.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(node.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(node.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, node.scale);
    return model;
}

// ============================================================
// Constructor / Destructor
// ============================================================

Renderer::Renderer(SDL_Window* window) : m_vulkanContext(new VulkanContext(window)), m_window(window) {
    initVulkan();
    initRenderPasses();
    initResources();

    Mesh cubeMesh = MeshLoader::load(std::string(ASSETS_PATH) + "cube.obj");
    loadMesh(cubeMesh.vertices, cubeMesh.indices);
    std::cout << "Loaded default cube mesh: " << cubeMesh.vertices.size()
              << " vertices, " << cubeMesh.indices.size() << " indices" << std::endl;

    addCubeNode("Cube 1");

    initPipelines();
    std::cout << "Renderer ready!" << std::endl;
}

Renderer::~Renderer() {
    if (!m_vulkanContext) return;
    vkDeviceWaitIdle(m_vulkanContext->getDevice());
    destroyPipelines();
    destroyResources();
    destroyVulkan();
}

// ============================================================
// Init groups
// ============================================================

void Renderer::initVulkan() {
    m_vulkanContext->init();
}

void Renderer::initRenderPasses() {
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createShadowResources();
    createHDRResources();
}

void Renderer::initResources() {
    createCubemap();
    createIBL();  
    loadPBRTextures();
    createPBRSampler();
    createUniformBuffer();
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSet();
    createSkyboxDescriptors();
    createTonemapDescriptors();
    // ImGui descriptor pool will be created in initImGui
}

void Renderer::initPipelines() {
    createGraphicsPipeline();
    createGizmoPipeline();
    createSkyboxPipeline();
    createTonemapPipeline();
    initImGui();
}

void Renderer::initImGui() {
    // create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    // Optional features can be enabled by the application (docking may not be available in this build)
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    // init SDL2 backend
    ImGui_ImplSDL2_InitForVulkan(m_window);

    // create descriptor pool for ImGui
    VkDevice device = m_vulkanContext->getDevice();
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &m_imguiDescriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create ImGui descriptor pool");

    // init Vulkan backend for ImGui
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = m_vulkanContext->getInstance();
    init_info.PhysicalDevice = m_vulkanContext->getPhysicalDevice();
    init_info.Device = m_vulkanContext->getDevice();
    init_info.QueueFamily = m_vulkanContext->getGraphicsFamily();
    init_info.Queue = m_vulkanContext->getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_imguiDescriptorPool;
    init_info.DescriptorPoolSize = 0;
    init_info.MinImageCount = static_cast<uint32_t>(m_vulkanContext->getSwapchainImages().size());
    init_info.ImageCount = init_info.MinImageCount;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;

    init_info.PipelineInfoMain.RenderPass = m_tonemapRenderPass;

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_SetMinImageCount(init_info.MinImageCount);
}

void Renderer::destroyImGui() {
    if (m_imguiDescriptorPool != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(m_vulkanContext->getDevice(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
}

// ============================================================
// Public asset loading — called by Engine, not constructor
// ============================================================

void Renderer::loadMesh(const std::vector<Vertex>& vertices,
                        const std::vector<uint32_t>& indices) {
    // destroy old mesh buffers if reloading, but keep the uniform buffer alive
    VkDevice device = m_vulkanContext->getDevice();
    if (m_indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
        vkFreeMemory(device, m_indexBufferMemory, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
        m_indexBufferMemory = VK_NULL_HANDLE;
    }
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
        m_vertexBufferMemory = VK_NULL_HANDLE;
    }
    createVertexBuffer(vertices);
    createIndexBuffer(indices);
}

// ============================================================
// Destroy groups — strict reverse of init order
// ============================================================

void Renderer::destroyPipelines() {
    if (!m_vulkanContext) return;
    destroyShadowResources();
    destroyGizmoResources();
    destroyIBL();
    destroySkyboxResources();
    destroyHDRResources();
    destroyDescriptors();
    VkDevice device = m_vulkanContext->getDevice();
    if (m_pipeline       != VK_NULL_HANDLE) vkDestroyPipeline(device, m_pipeline, nullptr);
    if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    m_pipeline       = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
}

void Renderer::destroyResources() {
    if (!m_vulkanContext) return;
    destroyBuffers();
    destroyPBRTextures();
    destroyDepthResources();
    VkDevice device = m_vulkanContext->getDevice();
    for (auto fb : m_framebuffers)
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(device, fb, nullptr);
    m_framebuffers.clear();
    if (m_renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, m_renderPass, nullptr);
    m_renderPass = VK_NULL_HANDLE;
}

void Renderer::destroyVulkan() {
    destroyImGui();
    delete m_vulkanContext;
    m_vulkanContext = nullptr;
}

// ============================================================
// Swapchain recreation — call on resize or VK_ERROR_OUT_OF_DATE_KHR
// ============================================================

void Renderer::recreateSwapchain() {
    vkDeviceWaitIdle(m_vulkanContext->getDevice());

    // tear down swapchain-dependent resources
    for (auto fb : m_framebuffers)
        vkDestroyFramebuffer(m_vulkanContext->getDevice(), fb, nullptr);
    m_framebuffers.clear();
    destroyDepthResources();
    vkDestroyRenderPass(m_vulkanContext->getDevice(), m_renderPass, nullptr);
    m_renderPass = VK_NULL_HANDLE;

    // rebuild
    m_vulkanContext->recreateSwapchain();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
}

// ============================================================
// Per-frame rendering
// ============================================================

void Renderer::drawFrame() {
    for (int attempt = 0; attempt < 2; ++attempt) {
        VkDevice device   = m_vulkanContext->getDevice();
        VkFence  inFlight = m_vulkanContext->getInFlightFence();

        // wait for previous frame
        vkWaitForFences(device, 1, &inFlight, VK_TRUE, UINT64_MAX);

        // acquire next swapchain image
        uint32_t imageIndex = 0;
        VkResult acquireResult = vkAcquireNextImageKHR(
            device,
            m_vulkanContext->getSwapchain(),
            UINT64_MAX,
            m_vulkanContext->getImageAvailableSemaphore(),
            VK_NULL_HANDLE,
            &imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR ||
            acquireResult == VK_SUBOPTIMAL_KHR ||
            acquireResult == VK_ERROR_SURFACE_LOST_KHR ||
            acquireResult == VK_ERROR_DEVICE_LOST) {
            recreateSwapchain();
            continue;
        }
        if (acquireResult != VK_SUCCESS) {
            std::cerr << "Swapchain acquire failed: " << acquireResult << std::endl;
            return;
        }

        // only reset fence after we know we're submitting
        vkResetFences(device, 1, &inFlight);

        // record
        const auto& cmdBuffers = m_vulkanContext->getCommandBuffers();
        if (imageIndex >= cmdBuffers.size())
            throw std::runtime_error("imageIndex out of range for command buffers");

        VkCommandBuffer cmd = cmdBuffers[imageIndex];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin command buffer");

        drawShadowPass(cmd);
        drawHDRPass(cmd);
        drawTonemapPass(cmd, imageIndex);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
            throw std::runtime_error("Failed to end command buffer");

        // submit and present
        VkResult submitResult = submitFrame(cmd, imageIndex);

        if (submitResult == VK_ERROR_OUT_OF_DATE_KHR ||
            submitResult == VK_SUBOPTIMAL_KHR ||
            submitResult == VK_ERROR_SURFACE_LOST_KHR ||
            submitResult == VK_ERROR_DEVICE_LOST) {
            recreateSwapchain();
            return;
        }
        if (submitResult != VK_SUCCESS) {
            std::cerr << "Swapchain present failed: " << submitResult << std::endl;
            return;
        }
        return;
    }
}

void Renderer::updateUniformBuffer(const UniformBufferObject& ubo) {
    if (!m_uniformMapped)
        throw std::runtime_error("Uniform buffer memory is not mapped");
    m_cachedUBO = ubo;
    memcpy(m_uniformMapped, &ubo, sizeof(ubo));
}

void Renderer::setGizmoNodePos(const glm::vec3& pos) { m_gizmoNodePos = pos; }
void Renderer::setGizmoHoveredAxis(int axis)          { m_gizmoHoveredAxis = axis; }