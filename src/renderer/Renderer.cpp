#include "Renderer.h"
#include "MeshLoader.h"
#include <iostream>

#ifndef ASSETS_PATH
#define ASSETS_PATH "assets/"
#endif

// ============================================================
// Constructor / Destructor
// ============================================================

Renderer::Renderer(SDL_Window* window) : m_window(window) {
    initVulkan();
    initRenderPasses();
    initResources();
    initPipelines();
    initAssets();
    std::cout << "Renderer ready!" << std::endl;
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(m_device);
    destroySync();
    vkDeviceWaitIdle(m_device);
    destroyAssets();
    vkDeviceWaitIdle(m_device);
    destroyPipelines();
    vkDeviceWaitIdle(m_device);
    destroyResources();
    vkDeviceWaitIdle(m_device);
    destroyVulkan();
}

// ============================================================
// Init groups
// ============================================================

void Renderer::initVulkan() {
    createInstance();
    pickPhysicalDevice();
    createLogicalDevice();
    createSurface();
    createSwapchain();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
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

void Renderer::initAssets() {
    Mesh mesh = MeshLoader::load(std::string(ASSETS_PATH) + "cube.obj");
    createVertexBuffer(mesh.vertices);
    createIndexBuffer(mesh.indices);
}

// ============================================================
// Destroy groups — mirror of init, reverse order
// ============================================================

void Renderer::destroySync() {
    vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
    vkDestroySemaphore(m_device, m_renderFinished, nullptr);
    vkDestroyFence(m_device, m_inFlight, nullptr);
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
}

void Renderer::destroyAssets() {
    destroyBuffers();
    destroyPBRTextures();
}

void Renderer::destroyPipelines() {
    destroyShadowResources();    // handles shadow pipeline
    destroySkyboxResources();    // handles skybox pipeline + layout
    destroyHDRResources();       // handles tonemap pipeline + layout
    destroyDescriptors();
}

void Renderer::destroyResources() {
    destroyDepthResources();
    for (auto fb : m_framebuffers)
        vkDestroyFramebuffer(m_device, fb, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
}

void Renderer::destroyVulkan() {
    destroySwapchain();
}

// ============================================================
// Per-frame rendering
// ============================================================

void Renderer::drawFrame() {
    std::cout << "drawFrame start" << std::endl; std::cout << std::flush;
    vkWaitForFences(m_device, 1, &m_inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_inFlight);
    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                          m_imageAvailable, VK_NULL_HANDLE, &imageIndex);
    std::cout << "imageIndex: " << imageIndex << std::endl; std::cout << std::flush;
    VkCommandBuffer cmd = m_commandBuffers[imageIndex];
    std::cout << "cmd: " << cmd << std::endl; std::cout << std::flush;
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);
    std::cout << "before drawShadowPass" << std::endl; std::cout << std::flush;
    drawShadowPass(cmd);
    std::cout << "before drawHDRPass" << std::endl; std::cout << std::flush;
    drawHDRPass(cmd);
    std::cout << "before drawTonemapPass" << std::endl; std::cout << std::flush;
    drawTonemapPass(cmd, imageIndex);
    std::cout << "before vkEndCommandBuffer" << std::endl; std::cout << std::flush;
    vkEndCommandBuffer(cmd);
    std::cout << "before submitFrame" << std::endl; std::cout << std::flush;
    submitFrame(cmd, imageIndex);
}

void Renderer::updateUniformBuffer(const UniformBufferObject& ubo) {
    memcpy(m_uniformMapped, &ubo, sizeof(ubo));
}