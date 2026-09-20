#include "render/VulkanRenderer.h"

PipelineInfo VulkanRenderer::CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout, const char* vertFile, const char* fragFile,
    const char* tessCtrlFile, const char* tessEvalFile, const char* geomFile) {

    PipelineInfo graphicsPipeInfo;

    std::vector<char> vertShaderCode;
    std::vector<char> fragShaderCode;
    std::vector<char> tessCtrlCode;
    std::vector<char> tessEvalCode;
    std::vector<char> geomCode;
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;
    VkShaderModule tessCtrlModule = VK_NULL_HANDLE;
    VkShaderModule tessEvalModule = VK_NULL_HANDLE;
    VkShaderModule geomModule = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{}; 
    VkPipelineShaderStageCreateInfo tessCtrlStageInfo{};
    VkPipelineShaderStageCreateInfo tessEvalStageInfo{};
    VkPipelineShaderStageCreateInfo geomStageInfo{};

    if (vertFile != nullptr && fragFile != nullptr){
        vertShaderCode = readFile(vertFile);
        fragShaderCode = readFile(fragFile);
        vertShaderModule = createShaderModule(vertShaderCode);
        fragShaderModule = createShaderModule(fragShaderCode);
    }

    if(tessCtrlFile != nullptr && tessEvalFile != nullptr){
        tessCtrlCode = readFile(tessCtrlFile);
        tessEvalCode = readFile(tessEvalFile);
        tessCtrlModule = createShaderModule(tessCtrlCode);
        tessEvalModule = createShaderModule(tessEvalCode); 
    }

    if(geomFile != nullptr){
        geomCode = readFile(geomFile);
        geomModule = createShaderModule(geomCode);
       
    }

    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    tessCtrlStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    tessCtrlStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    tessCtrlStageInfo.module = tessCtrlModule;
    tessCtrlStageInfo.pName = "main";

    tessEvalStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    tessEvalStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    tessEvalStageInfo.module = tessEvalModule;
    tessEvalStageInfo.pName = "main";

    geomStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    geomStageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    geomStageInfo.module = geomModule;
    geomStageInfo.pName = "main";


    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    if(vertShaderModule != VK_NULL_HANDLE) shaderStages.push_back(vertShaderStageInfo);
    if(tessCtrlModule != VK_NULL_HANDLE) shaderStages.push_back(tessCtrlStageInfo);
    if(tessEvalModule != VK_NULL_HANDLE) shaderStages.push_back(tessEvalStageInfo);
    if(geomModule != VK_NULL_HANDLE) shaderStages.push_back(geomStageInfo);
    if(fragShaderModule != VK_NULL_HANDLE) shaderStages.push_back(fragShaderStageInfo);


    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.offset = 0;
    range.size = sizeof(ModelMatrixPushConst);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &range;
    //pipelineLayoutInfo.flags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;


    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &graphicsPipeInfo.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

     

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = graphicsPipeInfo.pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeInfo.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    
    if (fragShaderModule) vkDestroyShaderModule(device, fragShaderModule, nullptr);
    if (fragShaderModule) vkDestroyShaderModule(device, vertShaderModule, nullptr);
    if (tessCtrlModule) vkDestroyShaderModule(device, tessCtrlModule, nullptr);
    if (tessEvalModule) vkDestroyShaderModule(device, tessEvalModule, nullptr);
    if (geomModule) vkDestroyShaderModule(device, geomModule, nullptr);

    return graphicsPipeInfo;
}

void VulkanRenderer::DestroyPipeline(PipelineInfo pipelineInfo){
    vkDestroyPipelineLayout(device, pipelineInfo.pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipelineInfo.pipeline, nullptr);
}

std::vector<char> VulkanRenderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}
