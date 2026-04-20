#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "Renderer.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include "MeshLoader.h"
#include <glm/gtc/matrix_transform.hpp>

#ifndef ASSETS_PATH
#define ASSETS_PATH "assets/"
#endif

VkFormat Renderer::findDepthFormat() {
    // prefer D32, fall back to D32+stencil, then D24+stencil
    for (VkFormat format : {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    }) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return format;
    }
    throw std::runtime_error("Failed to find supported depth format");
}

void Renderer::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    // create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = m_swapchainExtent.width;
    imageInfo.extent.height = m_swapchainExtent.height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = depthFormat;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image");

    // allocate memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, m_depthImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_depthImageMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate depth image memory");

    vkBindImageMemory(m_device, m_depthImage, m_depthImageMemory, 0);

    // create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_depthImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = depthFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image view");

    std::cout << "Depth resources created!" << std::endl;
}

Renderer::Renderer(SDL_Window* window) : m_window(window) {
    std::cout << "1 createInstance" << std::endl;
    createInstance();
    std::cout << "2 pickPhysicalDevice" << std::endl;
    pickPhysicalDevice();
    std::cout << "3 createLogicalDevice" << std::endl;
    createLogicalDevice();
    std::cout << "4 createSurface" << std::endl;
    createSurface();
    std::cout << "5 createSwapchain" << std::endl;
    createSwapchain();
    std::cout << "6 createDepthResources" << std::endl;
    createDepthResources();
    std::cout << "7 createRenderPass" << std::endl;
    createRenderPass();
    std::cout << "8 createFramebuffers" << std::endl;
    createFramebuffers();
    std::cout << "9 createCommandPool" << std::endl;
    createCommandPool();
    std::cout << "10 createCommandBuffers" << std::endl;
    createCommandBuffers();
    std::cout << "11 createSyncObjects" << std::endl;
    createSyncObjects();
    std::cout << "12 createShadowResources" << std::endl;
    createShadowResources();
    std::cout << "13 createHDRResources" << std::endl;
    createHDRResources();          // ← must come before createTonemapDescriptors
    std::cout << "14 createCubemap" << std::endl;
    createCubemap();
    std::cout << "15 loadPBRTextures" << std::endl;
    loadPBRTextures();
    std::cout << "16 createPBRSampler" << std::endl;
    createPBRSampler();
    std::cout << "17 createDescriptorSetLayout" << std::endl;
    createDescriptorSetLayout();
    std::cout << "18 createUniformBuffer" << std::endl;
    createUniformBuffer();
    std::cout << "19 createDescriptorPool" << std::endl;
    createDescriptorPool();
    std::cout << "20 createDescriptorSet" << std::endl;
    createDescriptorSet();
    std::cout << "21 createSkyboxDescriptors" << std::endl;
    createSkyboxDescriptors();
    std::cout << "22 createTonemapDescriptors" << std::endl;
    createTonemapDescriptors();    // ← HDR image exists now, safe to write descriptor
    std::cout << "23 createGraphicsPipeline" << std::endl;
    createGraphicsPipeline();
    std::cout << "24 createSkyboxPipeline" << std::endl;
    createSkyboxPipeline();
    std::cout << "25 createTonemapPipeline" << std::endl;
    createTonemapPipeline();
    std::cout << "26 loading mesh" << std::endl;
    Mesh mesh = MeshLoader::load(std::string(ASSETS_PATH) + "cube.obj");
    std::cout << "27 createVertexBuffer" << std::endl;
    createVertexBuffer(mesh.vertices);
    std::cout << "28 createIndexBuffer" << std::endl;
    createIndexBuffer(mesh.indices);
    std::cout << "Renderer ready!" << std::endl;
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(m_device); // wait for GPU to finish before destroying

    vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
    vkDestroySemaphore(m_device, m_renderFinished, nullptr);
    vkDestroyFence(m_device, m_inFlight, nullptr);
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    for (auto fb : m_framebuffers)
        vkDestroyFramebuffer(m_device, fb, nullptr);
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
    vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_uniformBufferMemory, nullptr);
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage(m_device, m_depthImage, nullptr);
    vkFreeMemory(m_device, m_depthImageMemory, nullptr);
    vkDestroySampler(m_device, m_pbrSampler, nullptr);

    vkDestroyPipeline(m_device, m_shadowPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_shadowPipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_shadowRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_shadowFramebuffer, nullptr);
    vkDestroyImageView(m_device, m_shadowImageView, nullptr);
    vkDestroyImage(m_device, m_shadowImage, nullptr);
    vkFreeMemory(m_device, m_shadowMemory, nullptr);
    vkDestroySampler(m_device, m_shadowSampler, nullptr);
    vkDestroyBuffer(m_device, m_shadowUniformBuffer, nullptr);
    vkFreeMemory(m_device, m_shadowUniformMemory, nullptr);
    vkDestroyDescriptorPool(m_device, m_shadowDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_shadowDescriptorSetLayout, nullptr);

    vkDestroyPipeline(m_device, m_skyboxPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_skyboxPipelineLayout, nullptr);
    vkDestroyDescriptorPool(m_device, m_skyboxDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_skyboxDescriptorSetLayout, nullptr);
    vkDestroySampler(m_device, m_cubemapSampler, nullptr);
    vkDestroyImageView(m_device, m_cubemapImageView, nullptr);
    vkDestroyImage(m_device, m_cubemapImage, nullptr);
    vkFreeMemory(m_device, m_cubemapMemory, nullptr);   

    vkDestroyPipeline(m_device, m_tonemapPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_tonemapPipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_tonemapRenderPass, nullptr);
    vkDestroyRenderPass(m_device, m_hdrRenderPass, nullptr);
    vkDestroyDescriptorPool(m_device, m_tonemapDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_tonemapDescriptorSetLayout, nullptr);
    vkDestroySampler(m_device, m_hdrSampler, nullptr);
    vkDestroyImageView(m_device, m_hdrImageView, nullptr);
    vkDestroyImage(m_device, m_hdrImage, nullptr);
    vkFreeMemory(m_device, m_hdrMemory, nullptr);
    vkDestroyFramebuffer(m_device, m_hdrFramebuffer, nullptr);



    auto destroyTex = [&](VkImage img, VkDeviceMemory mem, VkImageView view) {
        vkDestroyImageView(m_device, view, nullptr);
        vkDestroyImage(m_device, img, nullptr);
        vkFreeMemory(m_device, mem, nullptr);
    };
    destroyTex(m_albedoImage,    m_albedoMemory,    m_albedoImageView);
    destroyTex(m_normalImage,    m_normalMemory,    m_normalImageView);
    destroyTex(m_roughnessImage, m_roughnessMemory, m_roughnessImageView);
    destroyTex(m_metallicImage,  m_metallicMemory,  m_metallicImageView);

    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    for (auto view : m_swapchainImageViews)
        vkDestroyImageView(m_device, view, nullptr);
    if (m_swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    if (m_surface   != VK_NULL_HANDLE) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_device    != VK_NULL_HANDLE) vkDestroyDevice(m_device, nullptr);
    if (m_instance  != VK_NULL_HANDLE) vkDestroyInstance(m_instance, nullptr);
}

void Renderer::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "MyEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "MyEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    uint32_t extCount = 0;
    SDL_Vulkan_GetInstanceExtensions(m_window, &extCount, nullptr);
    std::vector<const char*> extensions(extCount);
    SDL_Vulkan_GetInstanceExtensions(m_window, &extCount, extensions.data());

    // Required for MoltenVK on Mac
    extensions.push_back("VK_KHR_portability_enumeration");
    extensions.push_back("VK_KHR_get_physical_device_properties2");

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount       = 0;
    createInfo.flags                  |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance");
}
void Renderer::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0)
        throw std::runtime_error("No Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    // pick the first available GPU
    m_physicalDevice = devices[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    std::cout << "GPU: " << props.deviceName << std::endl;
}

void Renderer::createLogicalDevice() {
    // find a queue family that supports graphics
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, families.data());

    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphicsFamily = i;
            break;
        }
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = m_graphicsFamily;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &priority;

    // required on Mac/MoltenVK
    std::vector<const char*> deviceExtensions = {
        "VK_KHR_swapchain",
        "VK_KHR_portability_subset"
    };

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount    = 1;
    deviceInfo.pQueueCreateInfos       = &queueInfo;
    deviceInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device");

    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    std::cout << "Logical device created!" << std::endl;
}
void Renderer::createSurface() {
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface))
        throw std::runtime_error("Failed to create window surface");
    std::cout << "Surface created!" << std::endl;
}

void Renderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);

    // pick format
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }

    // pick present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync, always available

    // pick extent (resolution)
    m_swapchainExtent = caps.currentExtent;
    m_swapchainFormat = chosenFormat.format;

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR swapInfo{};
    swapInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapInfo.surface          = m_surface;
    swapInfo.minImageCount    = imageCount;
    swapInfo.imageFormat      = chosenFormat.format;
    swapInfo.imageColorSpace  = chosenFormat.colorSpace;
    swapInfo.imageExtent      = m_swapchainExtent;
    swapInfo.imageArrayLayers = 1;
    swapInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform     = caps.currentTransform;
    swapInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapInfo.presentMode      = presentMode;
    swapInfo.clipped          = VK_TRUE;
    swapInfo.oldSwapchain     = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &swapInfo, nullptr, &m_swapchain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain");

    // get swapchain images
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());

    // create image views
    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = m_swapchainFormat;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image view");
    }

    std::cout << "Swapchain created! Images: " << m_swapchainImages.size() << std::endl;
}
void Renderer::createRenderPass() {
    VkFormat depthFormat = findDepthFormat();

    // color attachment (unchanged)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = m_swapchainFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // depth attachment — new
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = depthFormat;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE; // don't need it after frame
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;   // ← wire in depth

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                             | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");

    std::cout << "Render pass created!" << std::endl;
}

void Renderer::createFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());

    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            m_swapchainImageViews[i],
            m_depthImageView          // ← same depth view for all frames (one frame in flight)
        };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = m_renderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments    = attachments.data();
        fbInfo.width           = m_swapchainExtent.width;
        fbInfo.height          = m_swapchainExtent.height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer");
    }
    std::cout << "Framebuffers created: " << m_framebuffers.size() << std::endl;
}

void Renderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_graphicsFamily;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");

    std::cout << "Command pool created!" << std::endl;
}

void Renderer::createCommandBuffers() {
    m_commandBuffers.resize(m_framebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");

    std::cout << "Command buffers allocated: " << m_commandBuffers.size() << std::endl;
}

void Renderer::createSyncObjects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start signaled so first frame doesn't hang

    if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_imageAvailable) != VK_SUCCESS ||
        vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinished) != VK_SUCCESS ||
        vkCreateFence    (m_device, &fenceInfo, nullptr, &m_inFlight)     != VK_SUCCESS)
        throw std::runtime_error("Failed to create sync objects");

    std::cout << "Sync objects created!" << std::endl;
}
void Renderer::drawFrame() {
    vkWaitForFences(m_device, 1, &m_inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_inFlight);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                          m_imageAvailable, VK_NULL_HANDLE, &imageIndex);

    VkCommandBuffer cmd = m_commandBuffers[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // 1 — shadow pass
    drawShadowPass(cmd);

    // 2 — HDR geometry pass — renders into float framebuffer
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_hdrRenderPass;
    rpInfo.framebuffer       = m_hdrFramebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = m_swapchainExtent;
    rpInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
        VkBuffer     vertexBuffers[] = {m_vertexBuffer};
        VkDeviceSize offsets[]       = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
        drawSkybox(cmd);
    vkCmdEndRenderPass(cmd);

    // transition HDR image from COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL
    VkImageMemoryBarrier hdrBarrier{};
    hdrBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    hdrBarrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    hdrBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    hdrBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    hdrBarrier.image                           = m_hdrImage;
    hdrBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    hdrBarrier.subresourceRange.baseMipLevel   = 0;
    hdrBarrier.subresourceRange.levelCount     = 1;
    hdrBarrier.subresourceRange.baseArrayLayer = 0;
    hdrBarrier.subresourceRange.layerCount     = 1;
    hdrBarrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    hdrBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &hdrBarrier);

    // 3 — tonemap pass — reads HDR image, outputs LDR to swapchain
    drawTonemapPass(cmd, imageIndex);

    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &m_imageAvailable;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &m_renderFinished;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlight);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_renderFinished;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_swapchain;
    presentInfo.pImageIndices      = &imageIndex;

    vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
}

static std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open shader: " + path);
    size_t size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    if (vkCreateShaderModule(device, &info, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");
    return mod;
}

void Renderer::createGraphicsPipeline() {
    auto vertCode = readFile("shaders/triangle.vert.spv");
    auto fragCode = readFile("shaders/triangle.frag.spv");

    VkShaderModule vertModule = createShaderModule(m_device, vertCode);
    VkShaderModule fragModule = createShaderModule(m_device, fragCode);

    std::cout << "vertModule: " << vertModule << std::endl; std::cout << std::flush;
    std::cout << "fragModule: " << fragModule << std::endl; std::cout << std::flush;

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName  = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    auto binding    = Vertex::getBindingDescription();
    auto attributes = Vertex::getAttributeDescriptions();

    std::cout << "attribute count: " << attributes.size() << std::endl; std::cout << std::flush;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions    = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(m_swapchainExtent.width);
    viewport.height   = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable    = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments    = &blendAttachment;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &m_descriptorSetLayout;
    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    std::cout << "pipelineLayout: " << m_pipelineLayout << std::endl; std::cout << std::flush;
    std::cout << "renderPass: "     << m_renderPass     << std::endl; std::cout << std::flush;
    std::cout << "descriptorSetLayout: " << m_descriptorSetLayout << std::endl; std::cout << std::flush;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = stages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlend;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass = m_hdrRenderPass; // ← was m_renderPass
    pipelineInfo.subpass             = 0;

    VkResult result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1,
                                                &pipelineInfo, nullptr, &m_pipeline);
    std::cout << "vkCreateGraphicsPipelines result: " << result << std::endl; std::cout << std::flush;
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);

    std::cout << "Graphics pipeline created!" << std::endl;
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

void Renderer::createVertexBuffer(const std::vector<Vertex>& vertices) {
    VkDeviceSize size = sizeof(Vertex) * vertices.size();

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufInfo, nullptr, &m_vertexBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create vertex buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, m_vertexBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_vertexBufferMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate vertex buffer memory");

    vkBindBufferMemory(m_device, m_vertexBuffer, m_vertexBufferMemory, 0);

    void* data;
    vkMapMemory(m_device, m_vertexBufferMemory, 0, size, 0, &data);
    memcpy(data, vertices.data(), size);
    vkUnmapMemory(m_device, m_vertexBufferMemory);

    std::cout << "Vertex buffer created!" << std::endl;
}

void Renderer::createIndexBuffer(const std::vector<uint32_t>& indices) {
    VkDeviceSize size = sizeof(uint32_t) * indices.size();
    m_indexCount      = static_cast<uint32_t>(indices.size());

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufInfo, nullptr, &m_indexBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create index buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, m_indexBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_indexBufferMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate index buffer memory");

    vkBindBufferMemory(m_device, m_indexBuffer, m_indexBufferMemory, 0);

    void* data;
    vkMapMemory(m_device, m_indexBufferMemory, 0, size, 0, &data);
    memcpy(data, indices.data(), size);
    vkUnmapMemory(m_device, m_indexBufferMemory);

    std::cout << "Index buffer created!" << std::endl;
}

void Renderer::createUniformBuffer() {
    VkDeviceSize size = sizeof(UniformBufferObject);

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufInfo, nullptr, &m_uniformBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create uniform buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, m_uniformBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_uniformBufferMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate uniform buffer memory");

    vkBindBufferMemory(m_device, m_uniformBuffer, m_uniformBufferMemory, 0);
    vkMapMemory(m_device, m_uniformBufferMemory, 0, size, 0, &m_uniformMapped);
}

void Renderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 5;  // was 4, now 4 PBR + 1 shadow
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool");
}

void Renderer::createDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_descriptorSetLayout;
    

    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set");

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UniformBufferObject);

    auto makeImageInfo = [&](VkImageView view) {
        VkDescriptorImageInfo info{};
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView   = view;
        info.sampler     = m_pbrSampler;
        return info;
    };

    VkDescriptorImageInfo albedoInfo    = makeImageInfo(m_albedoImageView);
    VkDescriptorImageInfo normalInfo    = makeImageInfo(m_normalImageView);
    VkDescriptorImageInfo roughInfo     = makeImageInfo(m_roughnessImageView);
    VkDescriptorImageInfo metallicInfo  = makeImageInfo(m_metallicImageView);
    VkDescriptorImageInfo shadowInfo{};
    shadowInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shadowInfo.imageView   = m_shadowImageView;
    shadowInfo.sampler     = m_shadowSampler;

    std::array<VkWriteDescriptorSet, 6> writes{};

    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = m_descriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo     = &bufferInfo;

    auto makeSamplerWrite = [&](uint32_t binding, VkDescriptorImageInfo& imgInfo) {
        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = m_descriptorSet;
        w.dstBinding      = binding;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo      = &imgInfo;
        return w;
    };

    writes[1] = makeSamplerWrite(1, albedoInfo);
    writes[2] = makeSamplerWrite(2, normalInfo);
    writes[3] = makeSamplerWrite(3, roughInfo);
    writes[4] = makeSamplerWrite(4, metallicInfo);
    writes[5] = makeSamplerWrite(5, shadowInfo);

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    
}

void Renderer::updateUniformBuffer(const UniformBufferObject& ubo) {
    memcpy(m_uniformMapped, &ubo, sizeof(ubo));
}
VkCommandBuffer Renderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void Renderer::endSingleTimeCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

void Renderer::transitionImageLayout(VkImage image, VkFormat format,
                                     VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(cmd);
}

void Renderer::copyBufferToImage(VkBuffer buffer, VkImage image,
                                 uint32_t width, uint32_t height) {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(cmd);
}

void Renderer::createTextureImage(const std::string& path) {
    std::cout << "Loading texture from: " << path << std::endl; std::cout << std::flush;

    int width, height, channels;
    std::cout << "Calling stbi_load..." << std::endl; std::cout << std::flush;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    std::cout << "stbi_load returned: " << (void*)pixels << std::endl; std::cout << std::flush;

    if (!pixels)
        throw std::runtime_error("Failed to load texture: " + std::string(stbi_failure_reason()));

    std::cout << "Image: " << width << "x" << height << std::endl; std::cout << std::flush;

    VkDeviceSize imageSize = width * height * 4;
    std::cout << "imageSize: " << imageSize << std::endl; std::cout << std::flush;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = imageSize;
    bufInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    std::cout << "Creating staging buffer..." << std::endl; std::cout << std::flush;
    if (vkCreateBuffer(m_device, &bufInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create staging buffer");
    std::cout << "Staging buffer created" << std::endl; std::cout << std::flush;

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReqs);
    std::cout << "Got memory requirements" << std::endl; std::cout << std::flush;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::cout << "Allocating staging memory..." << std::endl; std::cout << std::flush;
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate staging memory");
    std::cout << "Staging memory allocated" << std::endl; std::cout << std::flush;

    if (vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS)
        throw std::runtime_error("Failed to bind staging buffer memory");
    std::cout << "Staging buffer bound" << std::endl; std::cout << std::flush;

    void* data;
    vkMapMemory(m_device, stagingMemory, 0, imageSize, 0, &data);
    std::cout << "Memory mapped" << std::endl; std::cout << std::flush;
    memcpy(data, pixels, imageSize);
    std::cout << "Pixels copied" << std::endl; std::cout << std::flush;
    vkUnmapMemory(m_device, stagingMemory);
    stbi_image_free(pixels);
    std::cout << "Staging buffer ready" << std::endl; std::cout << std::flush;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {(uint32_t)width, (uint32_t)height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    std::cout << "Creating VkImage..." << std::endl; std::cout << std::flush;
    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_textureImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image");
    std::cout << "VkImage created" << std::endl; std::cout << std::flush;

    vkGetImageMemoryRequirements(m_device, m_textureImage, &memReqs);
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    std::cout << "Allocating texture memory..." << std::endl; std::cout << std::flush;
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_textureMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate texture memory");
    std::cout << "Texture memory allocated" << std::endl; std::cout << std::flush;

    if (vkBindImageMemory(m_device, m_textureImage, m_textureMemory, 0) != VK_SUCCESS)
        throw std::runtime_error("Failed to bind texture image memory");
    std::cout << "Texture image bound" << std::endl; std::cout << std::flush;

    std::cout << "Transitioning layout to TRANSFER_DST..." << std::endl; std::cout << std::flush;
    transitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    std::cout << "Copying buffer to image..." << std::endl; std::cout << std::flush;
    copyBufferToImage(stagingBuffer, m_textureImage, width, height);
    std::cout << "Transitioning layout to SHADER_READ..." << std::endl; std::cout << std::flush;
    transitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    std::cout << "Layout transition done" << std::endl; std::cout << std::flush;

    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    std::cout << "Texture loaded: " << path << " (" << width << "x" << height << ")" << std::endl;
}

void Renderer::createTextureImageView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_textureImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_textureImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image view");
}

void Renderer::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_FALSE; // enable later
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler");
}
VkImageView Renderer::createImageView(VkImage image, VkFormat format) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VkImageView view;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view");
    return view;
}

void Renderer::createImageAndView(const std::string& path, bool srgb,
                                   VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("Failed to load texture: " + path);

    VkDeviceSize imageSize = width * height * 4;

    // staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = imageSize;
    bufInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(m_device, &bufInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);

    void* data;
    vkMapMemory(m_device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(m_device, stagingMemory);
    stbi_image_free(pixels);

    // use SRGB for albedo, UNORM for normal/roughness/metallic
    VkFormat format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {(uint32_t)width, (uint32_t)height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(m_device, &imageInfo, nullptr, &image);

    vkGetImageMemoryRequirements(m_device, image, &memReqs);
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(m_device, &allocInfo, nullptr, &memory);
    vkBindImageMemory(m_device, image, memory, 0);

    transitionImageLayout(image, format,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, image, width, height);
    transitionImageLayout(image, format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    view = createImageView(image, format);
    std::cout << "Loaded: " << path << " (" << width << "x" << height << ")" << std::endl;
}

void Renderer::loadPBRTextures() {
    createImageAndView(std::string(ASSETS_PATH) + "albedo.png",    true,
                       m_albedoImage,    m_albedoMemory,    m_albedoImageView);
    createImageAndView(std::string(ASSETS_PATH) + "normal.png",    false,
                       m_normalImage,    m_normalMemory,    m_normalImageView);
    createImageAndView(std::string(ASSETS_PATH) + "roughness.png", false,
                       m_roughnessImage, m_roughnessMemory, m_roughnessImageView);
    createImageAndView(std::string(ASSETS_PATH) + "metallic.png",  false,
                       m_metallicImage,  m_metallicMemory,  m_metallicImageView);
}
void Renderer::createPBRSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_pbrSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create PBR sampler");
}

void Renderer::createShadowResources() {
    createShadowRenderPass();

    // create depth image for shadow map
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_shadowImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, m_shadowImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_shadowMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate shadow memory");

    vkBindImageMemory(m_device, m_shadowImage, m_shadowMemory, 0);

    // image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_shadowImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_shadowImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow image view");

    // sampler — compare mode for sampler2DShadow in the shader
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable           = VK_TRUE;  // ← enables sampler2DShadow
    samplerInfo.compareOp               = VK_COMPARE_OP_LESS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_shadowSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow sampler");

    createShadowFramebuffer();
    createShadowDescriptors();
    createShadowPipeline();

    std::cout << "Shadow resources created!" << std::endl;
}

void Renderer::createShadowRenderPass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE; // keep depth for sampling
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 0; // depth only — no color output
    subpass.pDepthStencilAttachment = &depthRef;

    // two dependencies — transition in and out of shader read
    std::array<VkSubpassDependency, 2> deps{};

    deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass    = 0;
    deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    deps[1].srcSubpass    = 0;
    deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &depthAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(deps.size());
    rpInfo.pDependencies   = deps.data();

    if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_shadowRenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow render pass");
}

void Renderer::createShadowFramebuffer() {
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = m_shadowRenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments    = &m_shadowImageView;
    fbInfo.width           = SHADOW_MAP_SIZE;
    fbInfo.height          = SHADOW_MAP_SIZE;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_shadowFramebuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow framebuffer");
}

void Renderer::createShadowDescriptors() {
    // descriptor set layout — just one UBO for the light space matrix
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding        = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags     = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboBinding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
            &m_shadowDescriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow descriptor set layout");

    // pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_shadowDescriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow descriptor pool");

    // uniform buffer for light space matrix
    VkDeviceSize bufSize = sizeof(ShadowUBO);

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = bufSize;
    bufInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufInfo, nullptr, &m_shadowUniformBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow uniform buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, m_shadowUniformBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_shadowUniformMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate shadow uniform memory");

    vkBindBufferMemory(m_device, m_shadowUniformBuffer, m_shadowUniformMemory, 0);
    vkMapMemory(m_device, m_shadowUniformMemory, 0, bufSize, 0, &m_shadowUniformMapped);

    // allocate descriptor set
    VkDescriptorSetAllocateInfo dsAllocInfo{};
    dsAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAllocInfo.descriptorPool     = m_shadowDescriptorPool;
    dsAllocInfo.descriptorSetCount = 1;
    dsAllocInfo.pSetLayouts        = &m_shadowDescriptorSetLayout;

    if (vkAllocateDescriptorSets(m_device, &dsAllocInfo, &m_shadowDescriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate shadow descriptor set");

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_shadowUniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(ShadowUBO);

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = m_shadowDescriptorSet;
    write.dstBinding      = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo     = &bufferInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void Renderer::createShadowPipeline() {
    auto vertCode   = readFile("shaders/shadow.vert.spv");
    auto vertModule = createShaderModule(m_device, vertCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main";

    auto binding    = Vertex::getBindingDescription();
    auto attributes = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions    = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(SHADOW_MAP_SIZE);
    viewport.height   = static_cast<float>(SHADOW_MAP_SIZE);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode                = VK_CULL_MODE_FRONT_BIT; // cull front faces for shadow
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_TRUE;   // reduce shadow acne
    rasterizer.depthBiasConstantFactor = 1.25f;
    rasterizer.depthBiasSlopeFactor    = 1.75f;
    rasterizer.lineWidth               = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable   = VK_TRUE;
    depthStencil.depthWriteEnable  = VK_TRUE;
    depthStencil.depthCompareOp    = VK_COMPARE_OP_LESS;

    // no color attachments in shadow pass
    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 0;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &m_shadowDescriptorSetLayout;

    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_shadowPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 1; // vertex only
    pipelineInfo.pStages             = &vertStage;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlend;
    pipelineInfo.layout              = m_shadowPipelineLayout;
    pipelineInfo.renderPass          = m_shadowRenderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1,
            &pipelineInfo, nullptr, &m_shadowPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow pipeline");

    vkDestroyShaderModule(m_device, vertModule, nullptr);

    std::cout << "Shadow pipeline created!" << std::endl;
}

glm::mat4 Renderer::getLightSpaceMatrix() {
    glm::vec3 lightPos = glm::vec3(4.0f, 6.0f, 4.0f);
    glm::vec3 lightTarget = glm::vec3(0.0f);

    // orthographic projection covers the scene — tune these to your world size
    glm::mat4 lightProj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0.0f, 1.0f, 0.0f));

    // flip Y — Vulkan NDC has Y pointing down, GLM was designed for OpenGL
    lightProj[1][1] *= -1;

    return lightProj * lightView;
}

void Renderer::drawShadowPass(VkCommandBuffer cmd) {
    // update shadow UBO
    ShadowUBO shadowUBO{};
    shadowUBO.lightSpaceMatrix = getLightSpaceMatrix();
    memcpy(m_shadowUniformMapped, &shadowUBO, sizeof(shadowUBO));

    VkClearValue clearDepth{};
    clearDepth.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_shadowRenderPass;
    rpInfo.framebuffer       = m_shadowFramebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
    rpInfo.clearValueCount   = 1;
    rpInfo.pClearValues      = &clearDepth;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_shadowPipelineLayout, 0, 1, &m_shadowDescriptorSet, 0, nullptr);
        VkBuffer     vertexBuffers[] = {m_vertexBuffer};
        VkDeviceSize offsets[]       = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
    vkCmdEndRenderPass(cmd);
}
void Renderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding         = 0;
    uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    auto makeSamplerBinding = [](uint32_t binding) {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = binding;
        b.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount = 1;
        b.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        return b;
    };

    std::array<VkDescriptorSetLayoutBinding, 6> bindings = {
        uboBinding,
        makeSamplerBinding(1), // albedo
        makeSamplerBinding(2), // normal
        makeSamplerBinding(3), // roughness
        makeSamplerBinding(4), // metallic
        makeSamplerBinding(5), // shadow map ← new
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout");
}

void Renderer::createCubemap() {
    // face order: right, left, top, bottom, front, back
    std::array<std::string, 6> facePaths = {
        std::string(ASSETS_PATH) + "skybox_right.png",
        std::string(ASSETS_PATH) + "skybox_left.png",
        std::string(ASSETS_PATH) + "skybox_top.png",
        std::string(ASSETS_PATH) + "skybox_bottom.png",
        std::string(ASSETS_PATH) + "skybox_front.png",
        std::string(ASSETS_PATH) + "skybox_back.png",
    };

    int width, height, channels;
    // load first face to get dimensions
    stbi_uc* firstPixels = stbi_load(facePaths[0].c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!firstPixels)
        throw std::runtime_error("Failed to load cubemap face: " + facePaths[0]);
    stbi_image_free(firstPixels);

    VkDeviceSize faceSize  = width * height * 4;
    VkDeviceSize totalSize = faceSize * 6;

    // staging buffer for all 6 faces
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = totalSize;
    bufInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(m_device, &bufInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);

    // load each face into the staging buffer at the correct offset
    void* data;
    vkMapMemory(m_device, stagingMemory, 0, totalSize, 0, &data);
    for (int i = 0; i < 6; i++) {
        int w, h, c;
        stbi_uc* pixels = stbi_load(facePaths[i].c_str(), &w, &h, &c, STBI_rgb_alpha);
        if (!pixels)
            throw std::runtime_error("Failed to load cubemap face: " + facePaths[i]);
        memcpy((char*)data + faceSize * i, pixels, faceSize);
        stbi_image_free(pixels);
        std::cout << "Loaded cubemap face: " << facePaths[i] << std::endl;
    }
    vkUnmapMemory(m_device, stagingMemory);

    // create cubemap image — arrayLayers=6 + CUBE_COMPATIBLE flag
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {(uint32_t)width, (uint32_t)height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 6;  // one per face
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // ← required for cubemap

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_cubemapImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create cubemap image");

    vkGetImageMemoryRequirements(m_device, m_cubemapImage, &memReqs);
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(m_device, &allocInfo, nullptr, &m_cubemapMemory);
    vkBindImageMemory(m_device, m_cubemapImage, m_cubemapMemory, 0);

    // transition all 6 layers to transfer dst
    {
        VkCommandBuffer cmd = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_cubemapImage;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 6; // all faces
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(cmd);
    }

    // copy each face from staging buffer to its array layer
    {
        VkCommandBuffer cmd = beginSingleTimeCommands();
        for (uint32_t i = 0; i < 6; i++) {
            VkBufferImageCopy region{};
            region.bufferOffset      = faceSize * i;
            region.bufferRowLength   = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel       = 0;
            region.imageSubresource.baseArrayLayer = i; // ← each face goes to its layer
            region.imageSubresource.layerCount     = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {(uint32_t)width, (uint32_t)height, 1};
            vkCmdCopyBufferToImage(cmd, stagingBuffer, m_cubemapImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }
        endSingleTimeCommands(cmd);
    }

    // transition to shader read
    {
        VkCommandBuffer cmd = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_cubemapImage;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 6;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(cmd);
    }

    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    // cubemap image view — viewType must be CUBE
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_cubemapImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE; // ← key
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 6;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_cubemapImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create cubemap image view");

    // sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.borderColor  = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_cubemapSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create cubemap sampler");

    std::cout << "Cubemap loaded!" << std::endl;
}

void Renderer::createSkyboxDescriptors() {
    // layout — binding 0 = UBO, binding 1 = cubemap sampler
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();
    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
            &m_skyboxDescriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create skybox descriptor set layout");

    // pool
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = 1;
    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_skyboxDescriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create skybox descriptor pool");

    // allocate set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_skyboxDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_skyboxDescriptorSetLayout;
    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_skyboxDescriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate skybox descriptor set");

    // write UBO
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UniformBufferObject);

    // write cubemap
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = m_cubemapImageView;
    imageInfo.sampler     = m_cubemapSampler;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = m_skyboxDescriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo     = &bufferInfo;

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = m_skyboxDescriptorSet;
    writes[1].dstBinding      = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    std::cout << "Skybox descriptors created!" << std::endl;
}

void Renderer::createSkyboxPipeline() {
    auto vertCode   = readFile("shaders/skybox.vert.spv");
    auto fragCode   = readFile("shaders/skybox.frag.spv");
    auto vertModule = createShaderModule(m_device, vertCode);
    auto fragModule = createShaderModule(m_device, fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName  = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    // no vertex buffer — positions are hardcoded in the shader
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 0;
    vertexInput.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(m_swapchainExtent.width);
    viewport.height   = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_FRONT_BIT; // cull front — we're inside the cube
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // depth test ON but write OFF — skybox reads depth but never writes it
    // LESS_OR_EQUAL so skybox passes at z=1.0 (set by xyww trick in vert shader)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments    = &blendAttachment;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &m_skyboxDescriptorSetLayout;
    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_skyboxPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create skybox pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = stages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlend;
    pipelineInfo.layout              = m_skyboxPipelineLayout;
    pipelineInfo.renderPass = m_hdrRenderPass; // ← was m_renderPass
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1,
            &pipelineInfo, nullptr, &m_skyboxPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create skybox pipeline");

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);

    std::cout << "Skybox pipeline created!" << std::endl;
}

void Renderer::drawSkybox(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_skyboxPipelineLayout, 0, 1, &m_skyboxDescriptorSet, 0, nullptr);
    // 36 vertices hardcoded in shader — no vertex buffer bind needed
    vkCmdDraw(cmd, 36, 1, 0, 0);
}

void Renderer::createHDRRenderPass() {
    // HDR color attachment — float format holds values > 1.0
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = findDepthFormat();
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> deps{};
    deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass    = 0;
    deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                          | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                          | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    deps[1].srcSubpass    = 0;
    deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(deps.size());
    rpInfo.pDependencies   = deps.data();

    if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_hdrRenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create HDR render pass");
}

void Renderer::createHDRResources() {
    createHDRRenderPass();

    // HDR color image — float format
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {m_swapchainExtent.width, m_swapchainExtent.height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_hdrImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create HDR image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, m_hdrImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_hdrMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate HDR memory");

    vkBindImageMemory(m_device, m_hdrImage, m_hdrMemory, 0);

    // image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_hdrImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_hdrImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create HDR image view");

    // sampler for tonemapping pass to read from
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_hdrSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create HDR sampler");

    // framebuffer — HDR color + existing depth
    std::array<VkImageView, 2> attachments = {m_hdrImageView, m_depthImageView};

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = m_hdrRenderPass;
    fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbInfo.pAttachments    = attachments.data();
    fbInfo.width           = m_swapchainExtent.width;
    fbInfo.height          = m_swapchainExtent.height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_hdrFramebuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create HDR framebuffer");

    std::cout << "HDR resources created!" << std::endl;
}

void Renderer::createTonemapDescriptors() {
    // layout — just one sampler for the HDR image
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding         = 0;
    samplerBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &samplerBinding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
            &m_tonemapDescriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create tonemap descriptor set layout");

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_tonemapDescriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create tonemap descriptor pool");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_tonemapDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_tonemapDescriptorSetLayout;

    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_tonemapDescriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate tonemap descriptor set");

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = m_hdrImageView;
    imageInfo.sampler     = m_hdrSampler;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = m_tonemapDescriptorSet;
    write.dstBinding      = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    std::cout << "Tonemap descriptors created!" << std::endl;
}

void Renderer::createTonemapPipeline() {
    
    // tonemap render pass — renders directly to swapchain
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = m_swapchainFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &colorAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dep;
    

    if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_tonemapRenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create tonemap render pass");

    // rebuild swapchain framebuffers to use tonemap render pass
    for (auto fb : m_framebuffers)
        vkDestroyFramebuffer(m_device, fb, nullptr);
    m_framebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = m_tonemapRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &m_swapchainImageViews[i];
        fbInfo.width           = m_swapchainExtent.width;
        fbInfo.height          = m_swapchainExtent.height;
        fbInfo.layers          = 1;
        if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create tonemap framebuffer");
    }

    std::cout << "tonemap renderpass: " << m_tonemapRenderPass << std::endl;
    std::cout << "tonemap descriptorSetLayout: " << m_tonemapDescriptorSetLayout << std::endl;
    std::cout << "creating tonemap pipeline..." << std::endl;

    // pipeline
    auto vertCode   = readFile("shaders/tonemap.vert.spv");
    auto fragCode   = readFile("shaders/tonemap.frag.spv");
    auto vertModule = createShaderModule(m_device, vertCode);
    auto fragModule = createShaderModule(m_device, fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName  = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    // no vertex input
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 0;
    vertexInput.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(m_swapchainExtent.width);
    viewport.height   = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_NONE;
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments    = &blendAttachment;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_FALSE;
    depthStencil.depthWriteEnable      = VK_FALSE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    // push constant for exposure value
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof(float);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &m_tonemapDescriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushRange;


    
    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_tonemapPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create tonemap pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = stages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlend;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.layout              = m_tonemapPipelineLayout;
    pipelineInfo.renderPass          = m_tonemapRenderPass;
    pipelineInfo.subpass             = 0;
    

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1,
            &pipelineInfo, nullptr, &m_tonemapPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create tonemap pipeline");

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);

    std::cout << "Tonemap pipeline created!" << std::endl;
}

void Renderer::drawTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkClearValue clearValue{};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_tonemapRenderPass;
    rpInfo.framebuffer       = m_framebuffers[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = m_swapchainExtent;
    rpInfo.clearValueCount   = 1;
    rpInfo.pClearValues      = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tonemapPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_tonemapPipelineLayout, 0, 1, &m_tonemapDescriptorSet, 0, nullptr);
        // push exposure value
        vkCmdPushConstants(cmd, m_tonemapPipelineLayout,
            VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &m_exposure);
        // fullscreen triangle — 3 vertices, no vertex buffer
        vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}