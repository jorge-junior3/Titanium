#include "Renderer.h"
#include <stdexcept>
#include <iostream>

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

void Renderer::destroyDescriptors() {
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
}