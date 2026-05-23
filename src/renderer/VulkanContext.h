#pragma once
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vector>
#include <string>

class VulkanContext {
public:
    VulkanContext(SDL_Window* window);
    ~VulkanContext();

    void init();
    void cleanup();

    VkInstance getInstance() const { return m_instance; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkDevice getDevice() const { return m_device; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkSurfaceKHR getSurface() const { return m_surface; }
    VkSwapchainKHR getSwapchain() const { return m_swapchain; }
    VkFormat getSwapchainFormat() const { return m_swapchainFormat; }
    VkExtent2D getSwapchainExtent() const { return m_swapchainExtent; }
    const std::vector<VkImage>& getSwapchainImages() const { return m_swapchainImages; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return m_swapchainImageViews; }
    VkCommandPool getCommandPool() const { return m_commandPool; }
    const std::vector<VkCommandBuffer>& getCommandBuffers() const { return m_commandBuffers; }
    VkSemaphore getImageAvailableSemaphore() const { return m_imageAvailable; }
    VkSemaphore getRenderFinishedSemaphore() const { return m_renderFinished; }
    VkFence getInFlightFence() const { return m_inFlight; }

    uint32_t getGraphicsFamily() const { return m_graphicsFamily; }

    // Utility functions
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSurface();
    void createSwapchain();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void destroySwapchain();

    VkInstance       m_instance       = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice         m_device         = VK_NULL_HANDLE;
    VkQueue          m_graphicsQueue  = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface        = VK_NULL_HANDLE;
    VkSwapchainKHR   m_swapchain      = VK_NULL_HANDLE;
    VkFormat         m_swapchainFormat;
    VkExtent2D       m_swapchainExtent;

    std::vector<VkImage>         m_swapchainImages;
    std::vector<VkImageView>     m_swapchainImageViews;
    std::vector<VkCommandBuffer> m_commandBuffers;

    VkCommandPool m_commandPool    = VK_NULL_HANDLE;
    VkSemaphore   m_imageAvailable = VK_NULL_HANDLE;
    VkSemaphore   m_renderFinished = VK_NULL_HANDLE;
    VkFence       m_inFlight       = VK_NULL_HANDLE;

    uint32_t    m_graphicsFamily = 0;
    SDL_Window* m_window         = nullptr;
};