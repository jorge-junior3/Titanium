#include "Renderer.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cmath>

static std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open shader: " + path);
    size_t size = file.tellg();
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), size);
    return buf;
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

// ============================================================
// Helpers to create HDR cubemap/2D images for IBL
// ============================================================

static void createIBLCubemap(VkDevice device, VkPhysicalDevice physDevice,
                              uint32_t size, uint32_t mipLevels, VkFormat format,
                              VkImage& image, VkDeviceMemory& memory,
                              auto findMemoryType)
{
    VkImageCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType     = VK_IMAGE_TYPE_2D;
    info.format        = format;
    info.extent        = {size, size, 1};
    info.mipLevels     = mipLevels;
    info.arrayLayers   = 6;
    info.samples       = VK_SAMPLE_COUNT_1_BIT;
    info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    info.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device, &info, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create IBL cubemap image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate IBL cubemap memory");

    vkBindImageMemory(device, image, memory, 0);
}

// ============================================================
// createIrradianceMap
// ============================================================

void Renderer::createIrradianceMap() {
    VkDevice device = m_vulkanContext->getDevice();
    constexpr uint32_t SIZE = 32;

    createIBLCubemap(device, m_vulkanContext->getPhysicalDevice(),
                     SIZE, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
                     m_irradianceImage, m_irradianceMemory,
                     [this](uint32_t f, VkMemoryPropertyFlags p){ return findMemoryType(f, p); });

    // image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_irradianceImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format                          = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.layerCount     = 6;
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_irradianceImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create irradiance image view");

    // storage view (per-face per-mip for imageStore)
    VkImageViewCreateInfo storageViewInfo = viewInfo;
    storageViewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_CUBE;
    storageViewInfo.subresourceRange.layerCount = 6;
    if (vkCreateImageView(device, &storageViewInfo, nullptr, &m_irradianceStorageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create irradiance storage view");

    // sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_irradianceSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create irradiance sampler");

    // transition to GENERAL for compute write
    {
        VkCommandBuffer cmd = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_irradianceImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
        barrier.srcAccessMask       = 0;
        barrier.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(cmd);
    }

    // descriptor set layout
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding        = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags     = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding        = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags     = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings    = bindings.data();
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_iblComputeDescriptorSetLayout);

    // descriptor pool
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = 10;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes.data();
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_iblComputeDescriptorPool);

    // allocate + write descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_iblComputeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_iblComputeDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &allocInfo, &m_irradianceDescriptorSet);

    VkDescriptorImageInfo envInfo{};
    envInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    envInfo.imageView   = m_cubemapImageView;
    envInfo.sampler     = m_cubemapSampler;

    VkDescriptorImageInfo outInfo{};
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outInfo.imageView   = m_irradianceStorageView;
    outInfo.sampler     = VK_NULL_HANDLE;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = m_irradianceDescriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo      = &envInfo;

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = m_irradianceDescriptorSet;
    writes[1].dstBinding      = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo      = &outInfo;

    vkUpdateDescriptorSets(device, 2, writes.data(), 0, nullptr);

    // pipeline
    auto code   = readFile("shaders/irradiance.comp.spv");
    auto shader = createShaderModule(device, code);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &m_iblComputeDescriptorSetLayout;

    VkPipelineLayout layout;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shader;
    pipelineInfo.stage.pName  = "main";
    pipelineInfo.layout = layout;

    VkPipeline pipeline;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    // dispatch
    VkCommandBuffer cmd = beginSingleTimeCommands();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        layout, 0, 1, &m_irradianceDescriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, SIZE / 8, SIZE / 8, 6);
    endSingleTimeCommands(cmd);

    // transition to shader read
    {
        VkCommandBuffer cmd2 = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_irradianceImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
        barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd2,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(cmd2);
    }

    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);

    std::cout << "Irradiance map computed!" << std::endl;
}

// ============================================================
// createPrefilteredMap
// ============================================================

void Renderer::createPrefilteredMap() {
    VkDevice device = m_vulkanContext->getDevice();
    constexpr uint32_t SIZE     = 128;
    constexpr uint32_t MIP_LEVELS = 5;

    createIBLCubemap(device, m_vulkanContext->getPhysicalDevice(),
                     SIZE, MIP_LEVELS, VK_FORMAT_R16G16B16A16_SFLOAT,
                     m_prefilteredImage, m_prefilteredMemory,
                     [this](uint32_t f, VkMemoryPropertyFlags p){ return findMemoryType(f, p); });

    // sampled view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_prefilteredImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format                          = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount     = MIP_LEVELS;
    viewInfo.subresourceRange.layerCount     = 6;
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_prefilteredImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create prefiltered image view");

    // sampler with mip support
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod       = 0.0f;
    samplerInfo.maxLod       = static_cast<float>(MIP_LEVELS);
    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_prefilteredSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create prefiltered sampler");

    // transition all mips to GENERAL
    {
        VkCommandBuffer cmd = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_prefilteredImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, MIP_LEVELS, 0, 6};
        barrier.srcAccessMask       = 0;
        barrier.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(cmd);
    }

    // pipeline layout with push constant for roughness + mip size
    struct PushConstants { float roughness; uint32_t mipLevel; uint32_t mipSize; };

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &m_iblComputeDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushRange;

    VkPipelineLayout layout;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);

    auto code   = readFile("shaders/prefilter.comp.spv");
    auto shader = createShaderModule(device, code);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shader;
    pipelineInfo.stage.pName  = "main";
    pipelineInfo.layout       = layout;

    VkPipeline pipeline;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    // dispatch one pass per mip level
    for (uint32_t mip = 0; mip < MIP_LEVELS; mip++) {
        uint32_t mipSize = static_cast<uint32_t>(SIZE * std::pow(0.5f, mip));

        // per-mip storage view
        VkImageViewCreateInfo mipViewInfo = viewInfo;
        mipViewInfo.subresourceRange.baseMipLevel = mip;
        mipViewInfo.subresourceRange.levelCount   = 1;
        mipViewInfo.subresourceRange.layerCount   = 6;
        VkImageView mipStorageView;
        vkCreateImageView(device, &mipViewInfo, nullptr, &mipStorageView);

        // descriptor set for this mip
        VkDescriptorSet mipDescSet;
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_iblComputeDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_iblComputeDescriptorSetLayout;
        vkAllocateDescriptorSets(device, &allocInfo, &mipDescSet);

        VkDescriptorImageInfo envInfo{};
        envInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        envInfo.imageView   = m_cubemapImageView;
        envInfo.sampler     = m_cubemapSampler;

        VkDescriptorImageInfo outInfo{};
        outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outInfo.imageView   = mipStorageView;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = mipDescSet;
        writes[0].dstBinding      = 0;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo      = &envInfo;
        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = mipDescSet;
        writes[1].dstBinding      = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo      = &outInfo;
        vkUpdateDescriptorSets(device, 2, writes.data(), 0, nullptr);

        PushConstants pc{};
        pc.roughness = static_cast<float>(mip) / static_cast<float>(MIP_LEVELS - 1);
        pc.mipLevel  = mip;
        pc.mipSize   = mipSize;

        VkCommandBuffer cmd = beginSingleTimeCommands();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            layout, 0, 1, &mipDescSet, 0, nullptr);
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(PushConstants), &pc);
        uint32_t groups = std::max(1u, mipSize / 8);
        vkCmdDispatch(cmd, groups, groups, 6);
        endSingleTimeCommands(cmd);

        vkDestroyImageView(device, mipStorageView, nullptr);
    }

    // transition to shader read
    {
        VkCommandBuffer cmd = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_prefilteredImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, MIP_LEVELS, 0, 6};
        barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(cmd);
    }

    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);

    std::cout << "Prefiltered env map computed! (" << MIP_LEVELS << " mips)" << std::endl;
}

// ============================================================
// createBRDFLut
// ============================================================

void Renderer::createBRDFLut() {
    VkDevice device = m_vulkanContext->getDevice();
    constexpr uint32_t SIZE = 512;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = VK_FORMAT_R16G16_SFLOAT;
    imageInfo.extent        = {SIZE, SIZE, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCreateImage(device, &imageInfo, nullptr, &m_brdfLutImage);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_brdfLutImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device, &allocInfo, nullptr, &m_brdfLutMemory);
    vkBindImageMemory(device, m_brdfLutImage, m_brdfLutMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_brdfLutImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R16G16_SFLOAT;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.layerCount     = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &m_brdfLutImageView);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(device, &samplerInfo, nullptr, &m_brdfLutSampler);

    // transition to GENERAL
    {
        VkCommandBuffer cmd = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_brdfLutImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(cmd);
    }

    // brdf descriptor set layout (just storage image)
    VkDescriptorSetLayoutBinding brdfBinding{};
    brdfBinding.binding        = 0;
    brdfBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    brdfBinding.descriptorCount = 1;
    brdfBinding.stageFlags     = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo brdfLayoutInfo{};
    brdfLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    brdfLayoutInfo.bindingCount = 1;
    brdfLayoutInfo.pBindings    = &brdfBinding;
    VkDescriptorSetLayout brdfDescLayout;
    vkCreateDescriptorSetLayout(device, &brdfLayoutInfo, nullptr, &brdfDescLayout);

    VkDescriptorSet brdfDescSet;
    VkDescriptorSetAllocateInfo allocDescInfo{};
    allocDescInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocDescInfo.descriptorPool     = m_iblComputeDescriptorPool;
    allocDescInfo.descriptorSetCount = 1;
    allocDescInfo.pSetLayouts        = &brdfDescLayout;
    vkAllocateDescriptorSets(device, &allocDescInfo, &brdfDescSet);

    VkDescriptorImageInfo brdfOutInfo{};
    brdfOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    brdfOutInfo.imageView   = m_brdfLutImageView;

    VkWriteDescriptorSet brdfWrite{};
    brdfWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    brdfWrite.dstSet          = brdfDescSet;
    brdfWrite.dstBinding      = 0;
    brdfWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    brdfWrite.descriptorCount = 1;
    brdfWrite.pImageInfo      = &brdfOutInfo;
    vkUpdateDescriptorSets(device, 1, &brdfWrite, 0, nullptr);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &brdfDescLayout;
    VkPipelineLayout brdfLayout;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &brdfLayout);

    auto code   = readFile("shaders/brdf.comp.spv");
    auto shader = createShaderModule(device, code);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shader;
    pipelineInfo.stage.pName  = "main";
    pipelineInfo.layout       = brdfLayout;
    VkPipeline brdfPipeline;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &brdfPipeline);

    VkCommandBuffer cmd = beginSingleTimeCommands();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, brdfPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        brdfLayout, 0, 1, &brdfDescSet, 0, nullptr);
    vkCmdDispatch(cmd, SIZE / 8, SIZE / 8, 1);
    endSingleTimeCommands(cmd);

    // transition to shader read
    {
        VkCommandBuffer cmd2 = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_brdfLutImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd2,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(cmd2);
    }

    vkDestroyPipeline(device, brdfPipeline, nullptr);
    vkDestroyPipelineLayout(device, brdfLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, brdfDescLayout, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);

    std::cout << "BRDF LUT computed!" << std::endl;
}

// ============================================================
// createIBL — called from initResources after createCubemap
// ============================================================

void Renderer::createIBL() {
    createIrradianceMap();
    createPrefilteredMap();
    createBRDFLut();
    std::cout << "IBL ready!" << std::endl;
}

// ============================================================
// destroyIBL
// ============================================================

void Renderer::destroyIBL() {
    VkDevice device = m_vulkanContext->getDevice();

    vkDestroySampler(device, m_irradianceSampler, nullptr);
    vkDestroyImageView(device, m_irradianceImageView, nullptr);
    vkDestroyImageView(device, m_irradianceStorageView, nullptr);
    vkDestroyImage(device, m_irradianceImage, nullptr);
    vkFreeMemory(device, m_irradianceMemory, nullptr);

    vkDestroySampler(device, m_prefilteredSampler, nullptr);
    vkDestroyImageView(device, m_prefilteredImageView, nullptr);
    vkDestroyImage(device, m_prefilteredImage, nullptr);
    vkFreeMemory(device, m_prefilteredMemory, nullptr);

    vkDestroySampler(device, m_brdfLutSampler, nullptr);
    vkDestroyImageView(device, m_brdfLutImageView, nullptr);
    vkDestroyImage(device, m_brdfLutImage, nullptr);
    vkFreeMemory(device, m_brdfLutMemory, nullptr);

    vkDestroyDescriptorPool(device, m_iblComputeDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, m_iblComputeDescriptorSetLayout, nullptr);
}