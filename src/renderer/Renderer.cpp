#include "Renderer.h"
#include <iostream>
#include <stdexcept>

#ifndef ASSETS_PATH
#define ASSETS_PATH "assets/"
#endif

// ============================================================
// Memory type helper — delegates to VulkanContext
// ============================================================

uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    return m_vulkanContext->findMemoryType(typeFilter, props);
}

// ============================================================
// Constructor / Destructor
// ============================================================

Renderer::Renderer(SDL_Window* window) : m_vulkanContext(new VulkanContext(window)) {
    initVulkan();
    initRenderPasses();
    initResources();
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
    loadPBRTextures();
    createPBRSampler();
    createUniformBuffer();
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSet();
    createSkyboxDescriptors();
    createTonemapDescriptors();
}

void Renderer::initPipelines() {
    createGraphicsPipeline();
    createSkyboxPipeline();
    createTonemapPipeline();
}

// ============================================================
// Public asset loading — called by Engine, not constructor
// ============================================================

void Renderer::loadMesh(const std::vector<Vertex>& vertices,
                        const std::vector<uint32_t>& indices) {
    // destroy old buffers if reloading
    destroyBuffers();
    createVertexBuffer(vertices);
    createIndexBuffer(indices);
}

// ============================================================
// Destroy groups — strict reverse of init order
// ============================================================

void Renderer::destroyPipelines() {
    if (!m_vulkanContext) return;
    destroyShadowResources();
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
    VkDevice device   = m_vulkanContext->getDevice();
    VkFence  inFlight = m_vulkanContext->getInFlightFence();

    // wait for previous frame
    vkWaitForFences(device, 1, &inFlight, VK_TRUE, UINT64_MAX);

    // acquire next swapchain image
    uint32_t imageIndex;
    VkResult acquireResult = vkAcquireNextImageKHR(
        device,
        m_vulkanContext->getSwapchain(),
        UINT64_MAX,
        m_vulkanContext->getImageAvailableSemaphore(),
        VK_NULL_HANDLE,
        &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Failed to acquire swapchain image");

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
        submitResult == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (submitResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }
}

void Renderer::updateUniformBuffer(const UniformBufferObject& ubo) {
    if (m_uniformMapped)
        memcpy(m_uniformMapped, &ubo, sizeof(ubo));
}