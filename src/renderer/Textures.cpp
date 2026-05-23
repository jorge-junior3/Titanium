#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "Renderer.h"
#include <stdexcept>
#include <iostream>

#ifndef ASSETS_PATH
#define ASSETS_PATH "assets/"
#endif


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
    if (vkCreateImageView(m_vulkanContext->getDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS)
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
    vkCreateBuffer(m_vulkanContext->getDevice(), &bufInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_vulkanContext->getDevice(), stagingBuffer, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(m_vulkanContext->getDevice(), &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(m_vulkanContext->getDevice(), stagingBuffer, stagingMemory, 0);

    void* data;
    vkMapMemory(m_vulkanContext->getDevice(), stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(m_vulkanContext->getDevice(), stagingMemory);
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
    vkCreateImage(m_vulkanContext->getDevice(), &imageInfo, nullptr, &image);

    vkGetImageMemoryRequirements(m_vulkanContext->getDevice(), image, &memReqs);
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(m_vulkanContext->getDevice(), &allocInfo, nullptr, &memory);
    vkBindImageMemory(m_vulkanContext->getDevice(), image, memory, 0);

    transitionImageLayout(image, format,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, image, width, height);
    transitionImageLayout(image, format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_vulkanContext->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_vulkanContext->getDevice(), stagingMemory, nullptr);

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

    if (vkCreateSampler(m_vulkanContext->getDevice(), &samplerInfo, nullptr, &m_pbrSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create PBR sampler");
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
    if (vkCreateBuffer(m_vulkanContext->getDevice(), &bufInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create staging buffer");
    std::cout << "Staging buffer created" << std::endl; std::cout << std::flush;

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_vulkanContext->getDevice(), stagingBuffer, &memReqs);
    std::cout << "Got memory requirements" << std::endl; std::cout << std::flush;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::cout << "Allocating staging memory..." << std::endl; std::cout << std::flush;
    if (vkAllocateMemory(m_vulkanContext->getDevice(), &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate staging memory");
    std::cout << "Staging memory allocated" << std::endl; std::cout << std::flush;

    if (vkBindBufferMemory(m_vulkanContext->getDevice(), stagingBuffer, stagingMemory, 0) != VK_SUCCESS)
        throw std::runtime_error("Failed to bind staging buffer memory");
    std::cout << "Staging buffer bound" << std::endl; std::cout << std::flush;

    void* data;
    vkMapMemory(m_vulkanContext->getDevice(), stagingMemory, 0, imageSize, 0, &data);
    std::cout << "Memory mapped" << std::endl; std::cout << std::flush;
    memcpy(data, pixels, imageSize);
    std::cout << "Pixels copied" << std::endl; std::cout << std::flush;
    vkUnmapMemory(m_vulkanContext->getDevice(), stagingMemory);
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
    if (vkCreateImage(m_vulkanContext->getDevice(), &imageInfo, nullptr, &m_textureImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image");
    std::cout << "VkImage created" << std::endl; std::cout << std::flush;

    vkGetImageMemoryRequirements(m_vulkanContext->getDevice(), m_textureImage, &memReqs);
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    std::cout << "Allocating texture memory..." << std::endl; std::cout << std::flush;
    if (vkAllocateMemory(m_vulkanContext->getDevice(), &allocInfo, nullptr, &m_textureMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate texture memory");
    std::cout << "Texture memory allocated" << std::endl; std::cout << std::flush;

    if (vkBindImageMemory(m_vulkanContext->getDevice(), m_textureImage, m_textureMemory, 0) != VK_SUCCESS)
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

    vkDestroyBuffer(m_vulkanContext->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_vulkanContext->getDevice(), stagingMemory, nullptr);

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

    if (vkCreateImageView(m_vulkanContext->getDevice(), &viewInfo, nullptr, &m_textureImageView) != VK_SUCCESS)
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

    if (vkCreateSampler(m_vulkanContext->getDevice(), &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler");
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
    vkCreateBuffer(m_vulkanContext->getDevice(), &bufInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_vulkanContext->getDevice(), stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(m_vulkanContext->getDevice(), &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(m_vulkanContext->getDevice(), stagingBuffer, stagingMemory, 0);

    // load each face into the staging buffer at the correct offset
    void* data;
    vkMapMemory(m_vulkanContext->getDevice(), stagingMemory, 0, totalSize, 0, &data);
    for (int i = 0; i < 6; i++) {
        int w, h, c;
        stbi_uc* pixels = stbi_load(facePaths[i].c_str(), &w, &h, &c, STBI_rgb_alpha);
        if (!pixels)
            throw std::runtime_error("Failed to load cubemap face: " + facePaths[i]);
        memcpy((char*)data + faceSize * i, pixels, faceSize);
        stbi_image_free(pixels);
        std::cout << "Loaded cubemap face: " << facePaths[i] << std::endl;
    }
    vkUnmapMemory(m_vulkanContext->getDevice(), stagingMemory);

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

    if (vkCreateImage(m_vulkanContext->getDevice(), &imageInfo, nullptr, &m_cubemapImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create cubemap image");

    vkGetImageMemoryRequirements(m_vulkanContext->getDevice(), m_cubemapImage, &memReqs);
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(m_vulkanContext->getDevice(), &allocInfo, nullptr, &m_cubemapMemory);
    vkBindImageMemory(m_vulkanContext->getDevice(), m_cubemapImage, m_cubemapMemory, 0);

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

    vkDestroyBuffer(m_vulkanContext->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_vulkanContext->getDevice(), stagingMemory, nullptr);

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
    if (vkCreateImageView(m_vulkanContext->getDevice(), &viewInfo, nullptr, &m_cubemapImageView) != VK_SUCCESS)
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
    if (vkCreateSampler(m_vulkanContext->getDevice(), &samplerInfo, nullptr, &m_cubemapSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create cubemap sampler");

    std::cout << "Cubemap loaded!" << std::endl;
}

void Renderer::destroyPBRTextures() {
    vkDestroySampler(m_vulkanContext->getDevice(), m_pbrSampler, nullptr);
    auto destroyTex = [&](VkImage img, VkDeviceMemory mem, VkImageView view) {
        vkDestroyImageView(m_vulkanContext->getDevice(), view, nullptr);
        vkDestroyImage(m_vulkanContext->getDevice(), img, nullptr);
        vkFreeMemory(m_vulkanContext->getDevice(), mem, nullptr);
    };
    destroyTex(m_albedoImage,    m_albedoMemory,    m_albedoImageView);
    destroyTex(m_normalImage,    m_normalMemory,    m_normalImageView);
    destroyTex(m_roughnessImage, m_roughnessMemory, m_roughnessImageView);
    destroyTex(m_metallicImage,  m_metallicMemory,  m_metallicImageView);
}