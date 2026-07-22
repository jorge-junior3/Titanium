#include "Renderer.h"
#include "editor/Gizmo.h"
#include <stdexcept>
#include <fstream>
#include <iostream>

static std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Cannot open: " + path);
    size_t size = file.tellg();
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), size);
    return buf;
}

static VkShaderModule makeShader(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    if (vkCreateShaderModule(device, &info, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("Failed to create gizmo shader module");
    return mod;
}

void Renderer::createGizmoPipeline() {
    VkDevice device = m_vulkanContext->getDevice();

    auto vertCode = readFile("shaders/gizmo.vert.spv");
    auto fragCode = readFile("shaders/gizmo.frag.spv");
    auto vertMod  = makeShader(device, vertCode);
    auto fragMod  = makeShader(device, fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "main";

    auto binding    = GizmoVertex::getBindingDescription();
    auto attributes = GizmoVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions    = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkExtent2D extent = m_vulkanContext->getSwapchainExtent();
    VkViewport viewport{0, 0, (float)extent.width, (float)extent.height, 0, 1};
    VkRect2D   scissor{{0,0}, extent};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_NONE; // show both sides
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // no depth test — gizmo always on top
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments    = &blend;

    // push constant: mvp matrix
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushRange;
    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_gizmoPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create gizmo pipeline layout");

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
    pipelineInfo.layout              = m_gizmoPipelineLayout;
    pipelineInfo.renderPass          = m_hdrRenderPass; // draw into HDR pass
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                  &pipelineInfo, nullptr, &m_gizmoPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create gizmo pipeline");

    vkDestroyShaderModule(device, vertMod, nullptr);
    vkDestroyShaderModule(device, fragMod, nullptr);

    std::cout << "Gizmo pipeline created!" << std::endl;
}

void Renderer::updateGizmoBuffers(const std::vector<GizmoVertex>& vertices) {
    VkDevice device = m_vulkanContext->getDevice();

    // destroy old buffer
    if (m_gizmoVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_gizmoVertexBuffer, nullptr);
        vkFreeMemory(device, m_gizmoVertexMemory, nullptr);
        m_gizmoVertexBuffer = VK_NULL_HANDLE;
        m_gizmoVertexMemory = VK_NULL_HANDLE;
    }

    if (vertices.empty()) { m_gizmoVertexCount = 0; return; }

    VkDeviceSize size = sizeof(GizmoVertex) * vertices.size();
    m_gizmoVertexCount = static_cast<uint32_t>(vertices.size());

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult createResult = vkCreateBuffer(device, &bufInfo, nullptr, &m_gizmoVertexBuffer);
    if (createResult != VK_SUCCESS)
        throw std::runtime_error("Failed to create gizmo vertex buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, m_gizmoVertexBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkResult allocResult = vkAllocateMemory(device, &allocInfo, nullptr, &m_gizmoVertexMemory);
    if (allocResult != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate gizmo vertex memory");
    if (vkBindBufferMemory(device, m_gizmoVertexBuffer, m_gizmoVertexMemory, 0) != VK_SUCCESS)
        throw std::runtime_error("Failed to bind gizmo vertex memory");

    void* data;
    if (vkMapMemory(device, m_gizmoVertexMemory, 0, size, 0, &data) != VK_SUCCESS)
        throw std::runtime_error("Failed to map gizmo vertex memory");
    memcpy(data, vertices.data(), size);
    vkUnmapMemory(device, m_gizmoVertexMemory);
}

void Renderer::drawGizmo(VkCommandBuffer cmd, const glm::mat4& view,
                          const glm::mat4& proj, const glm::vec3& nodePos,
                          int hoveredAxis) {
    if (m_gizmoPipeline == VK_NULL_HANDLE ||
        m_gizmoPipelineLayout == VK_NULL_HANDLE ||
        m_gizmoVertexBuffer == VK_NULL_HANDLE ||
        m_gizmoVertexCount == 0) return;

    // model matrix places gizmo at node position, scaled to be constant screen size
    // scale by distance to camera so it stays the same apparent size
    glm::mat4 model = glm::translate(glm::mat4(1.0f), nodePos);
    glm::mat4 mvp   = proj * view * model;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gizmoPipeline);
    vkCmdPushConstants(cmd, m_gizmoPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

    VkBuffer     buffers[] = {m_gizmoVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdDraw(cmd, m_gizmoVertexCount, 1, 0, 0);
}

void Renderer::destroyGizmoResources() {
    VkDevice device = m_vulkanContext->getDevice();
    if (m_gizmoVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_gizmoVertexBuffer, nullptr);
        vkFreeMemory(device, m_gizmoVertexMemory, nullptr);
    }
    if (m_gizmoPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device, m_gizmoPipeline, nullptr);
    if (m_gizmoPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, m_gizmoPipelineLayout, nullptr);
}