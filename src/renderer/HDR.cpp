#include "Renderer.h"
#include <stdexcept>
#include <iostream>
#include <fstream>

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

void Renderer::createTonemapDescriptors() {
    VkDevice dev = device();
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

    if (vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr,
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

    if (vkCreateDescriptorPool(dev, &poolInfo, nullptr, &m_tonemapDescriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create tonemap descriptor pool");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_tonemapDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_tonemapDescriptorSetLayout;

    if (vkAllocateDescriptorSets(dev, &allocInfo, &m_tonemapDescriptorSet) != VK_SUCCESS)
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

    vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
    std::cout << "Tonemap descriptors created!" << std::endl;
}

void Renderer::createTonemapPipeline() {
    VkDevice dev = device();
    VkFormat swapchainFmt = swapchainFormat();
    const auto& swapchainImageViews = this->swapchainImageViews();
    VkExtent2D extent = swapchainExtent();
    // tonemap render pass — renders directly to swapchain
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = swapchainFmt;
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
    

    if (vkCreateRenderPass(dev, &rpInfo, nullptr, &m_tonemapRenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create tonemap render pass");

    // rebuild swapchain framebuffers to use tonemap render pass
    for (auto fb : m_framebuffers)
        vkDestroyFramebuffer(dev, fb, nullptr);
    m_framebuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = m_tonemapRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &swapchainImageViews[i];
        fbInfo.width           = extent.width;
        fbInfo.height          = extent.height;
        fbInfo.layers          = 1;
        if (vkCreateFramebuffer(dev, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create tonemap framebuffer");
    }

    std::cout << "tonemap renderpass: " << m_tonemapRenderPass << std::endl;
    std::cout << "tonemap descriptorSetLayout: " << m_tonemapDescriptorSetLayout << std::endl;
    std::cout << "creating tonemap pipeline..." << std::endl;

    // pipeline
    auto vertCode   = readFile("shaders/tonemap.vert.spv");
    auto fragCode   = readFile("shaders/tonemap.frag.spv");
    auto vertModule = createShaderModule(dev, vertCode);
    auto fragModule = createShaderModule(dev, fragCode);

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
    viewport.width    = static_cast<float>(extent.width);
    viewport.height   = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

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


    
    if (vkCreatePipelineLayout(dev, &layoutInfo, nullptr, &m_tonemapPipelineLayout) != VK_SUCCESS)
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
    

    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1,
            &pipelineInfo, nullptr, &m_tonemapPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create tonemap pipeline");

    vkDestroyShaderModule(dev, vertModule, nullptr);
    vkDestroyShaderModule(dev, fragModule, nullptr);

    std::cout << "Tonemap pipeline created!" << std::endl;
}

void Renderer::drawTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex) {
    if (imageIndex >= m_framebuffers.size() || m_framebuffers[imageIndex] == VK_NULL_HANDLE)
        throw std::runtime_error("Invalid framebuffer for tonemap pass");
    if (m_tonemapPipeline == VK_NULL_HANDLE || m_tonemapPipelineLayout == VK_NULL_HANDLE)
        throw std::runtime_error("Tonemap pipeline is not initialized");

    VkExtent2D extent = swapchainExtent();
    VkClearValue clearValue{};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_tonemapRenderPass;
    rpInfo.framebuffer       = m_framebuffers[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = extent;
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

void Renderer::drawHDRPass(VkCommandBuffer cmd) {
    VkExtent2D extent = swapchainExtent();
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_hdrRenderPass;
    rpInfo.framebuffer       = m_hdrFramebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = extent;
    rpInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        if (m_indexCount > 0 && m_vertexBuffer != VK_NULL_HANDLE && m_indexBuffer != VK_NULL_HANDLE) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
            VkBuffer     vertexBuffers[] = {m_vertexBuffer};
            VkDeviceSize offsets[]       = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
        }
        drawSkybox(cmd);
    vkCmdEndRenderPass(cmd);

    VkImageMemoryBarrier hdrBarrier{};
    hdrBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    hdrBarrier.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    hdrBarrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hdrBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hdrBarrier.image               = m_hdrImage;
    hdrBarrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    hdrBarrier.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    hdrBarrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &hdrBarrier);
}

void Renderer::destroyHDRResources() {
    VkDevice dev = device();
    if (m_tonemapPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, m_tonemapPipeline, nullptr);
        m_tonemapPipeline = VK_NULL_HANDLE;
    }
    if (m_tonemapPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, m_tonemapPipelineLayout, nullptr);
        m_tonemapPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_tonemapRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, m_tonemapRenderPass, nullptr);
        m_tonemapRenderPass = VK_NULL_HANDLE;
    }
    if (m_hdrRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, m_hdrRenderPass, nullptr);
        m_hdrRenderPass = VK_NULL_HANDLE;
    }
    if (m_tonemapDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(dev, m_tonemapDescriptorPool, nullptr);
        m_tonemapDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_tonemapDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(dev, m_tonemapDescriptorSetLayout, nullptr);
        m_tonemapDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_hdrSampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, m_hdrSampler, nullptr);
        m_hdrSampler = VK_NULL_HANDLE;
    }
    if (m_hdrImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, m_hdrImageView, nullptr);
        m_hdrImageView = VK_NULL_HANDLE;
    }
    if (m_hdrImage != VK_NULL_HANDLE) {
        vkDestroyImage(dev, m_hdrImage, nullptr);
        m_hdrImage = VK_NULL_HANDLE;
    }
    if (m_hdrMemory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, m_hdrMemory, nullptr);
        m_hdrMemory = VK_NULL_HANDLE;
    }
    if (m_hdrFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(dev, m_hdrFramebuffer, nullptr);
        m_hdrFramebuffer = VK_NULL_HANDLE;
    }
}