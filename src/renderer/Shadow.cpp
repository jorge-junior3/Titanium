#include "Renderer.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

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

void Renderer::destroyShadowResources() {
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
}