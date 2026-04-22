#include "Renderer.h"
#include <stdexcept>
#include <iostream>

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
    m_commandBuffers.resize(m_swapchainImages.size()); // ← was m_framebuffers.size()
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

void Renderer::destroySwapchain() {
    for (auto view : m_swapchainImageViews)
        vkDestroyImageView(m_device, view, nullptr);
    m_swapchainImageViews.clear();
    
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    
    if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}