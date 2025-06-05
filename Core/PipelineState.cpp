#include "PipelineState.h"
#include "Display.h"
#include "GraphicsCore.h"
#include "Hash.h"
#include "Utility.h"
#include <map>

static std::map<const unsigned int *, vk::ShaderModule> s_ShaderMap;
static std::map<size_t, vk::Pipeline> s_GraphicsPSOHashMap;
static std::map<size_t, vk::Pipeline> s_ComputePSOHashMap;

void PSO::DestroyAll()
{
    for (auto &i : s_GraphicsPSOHashMap)
    {
        Graphics::g_Device.destroyPipeline(i.second);
    }
    s_GraphicsPSOHashMap.clear();

    for (auto &i : s_ComputePSOHashMap)
    {
        Graphics::g_Device.destroyPipeline(i.second);
    }
    s_ComputePSOHashMap.clear();

    for (auto &i : s_ShaderMap)
    {
        Graphics::g_Device.destroyShaderModule(i.second);
    }
    s_ShaderMap.clear();
}

void PSO::SetPipelineLayout(const vk::PipelineLayout &layout) { m_Layout = layout; }

GraphicsPSO::GraphicsPSO(const char *Name) : PSO(Name) {}

void GraphicsPSO::SetVertexShader(const unsigned int *binary, size_t size)
{
    m_VertShaderSource.first = binary;
    m_VertShaderSource.second = size;
}

void GraphicsPSO::SetGeometryShader(const unsigned int *binary, size_t size)
{
    m_GeomShaderSource.first = binary;
    m_GeomShaderSource.second = size;
}

void GraphicsPSO::SetFragmentShader(const unsigned int *binary, size_t size)
{
    m_FragShaderSource.first = binary;
    m_FragShaderSource.second = size;
}

// void GraphicsPSO::SetInputLayout(const vk::VertexInputBindingDescription &binding,
//                                  const std::vector<vk::VertexInputAttributeDescription> &attributes)
//{
//     m_BindingDescriptions.push_back(binding);
//     m_AttributeDescriptions.insert(m_AttributeDescriptions.end(), attributes.begin(), attributes.end());
// }

void GraphicsPSO::SetInputLayout(const vk::ArrayProxy<VertexInputBindingAttribute> &attributes)
{
    m_BindingDescriptions.clear();
    m_AttributeDescriptions.clear();
    for (auto &i : attributes)
    {
        m_BindingDescriptions.push_back(i.first);
        m_AttributeDescriptions.insert(m_AttributeDescriptions.end(), i.second.begin(), i.second.end());
    }
}

void GraphicsPSO::SetRasterizerState(const RasterizationState &rasterizer) { m_RasterizerState = rasterizer; }

void GraphicsPSO::SetDepthStencilState(const DepthStencilState &depthStencil) { m_DepthStencilState = depthStencil; }

void GraphicsPSO::SetBlendState(const ColorBlendState &blend)
{
    m_BlendState = blend;
    //    m_BlendInfo.createInfo.attachmentCount = 1;
    //    m_BlendInfo.createInfo.pAttachments = &m_BlendInfo.attachmentState;
}

void GraphicsPSO::SetRenderPassFormat(const vk::ArrayProxy<vk::Format> &colors, vk::Format depth)
{
    m_RenderPass = RenderPass::GetRenderPass(colors, depth);
}

void GraphicsPSO::SetPrimitiveTopologyType(vk::PrimitiveTopology topology)
{
    m_AssemblyInfo.setTopology(topology);
    m_AssemblyInfo.setPrimitiveRestartEnable(VK_FALSE);
}

void GraphicsPSO::Finalize()
{
    size_t hash = 2166136261U;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStageInfos;
    if (m_VertShaderSource.first && m_VertShaderSource.second > 0)
    {
        // find existed shader
        auto iter = s_ShaderMap.find(m_VertShaderSource.first);
        if (iter != s_ShaderMap.end())
        {
            m_VertShader = iter->second;
        }
        else
        {
            // create a new one. remember to add to shader map
            vk::ShaderModuleCreateInfo moduleInfo;
            moduleInfo.pCode = m_VertShaderSource.first;
            moduleInfo.codeSize = m_VertShaderSource.second;
            m_VertShader = Graphics::g_Device.createShaderModule(moduleInfo);
            s_ShaderMap[m_VertShaderSource.first] = m_VertShader;
        }

        vk::PipelineShaderStageCreateInfo stageInfo;
        stageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
        stageInfo.setModule(m_VertShader);
        stageInfo.setPName("main");
        shaderStageInfos.push_back(stageInfo);

        hash = Utility::HashState(&m_VertShaderSource.first, 1, hash);
    }
    if (m_GeomShaderSource.first && m_GeomShaderSource.second > 0)
    {
        // find existed shader
        auto iter = s_ShaderMap.find(m_GeomShaderSource.first);
        if (iter != s_ShaderMap.end())
        {
            m_GeomShader = iter->second;
        }
        else
        {
            // create a new one. remember to add to shader map
            vk::ShaderModuleCreateInfo moduleInfo;
            moduleInfo.pCode = m_GeomShaderSource.first;
            moduleInfo.codeSize = m_GeomShaderSource.second;
            m_GeomShader = Graphics::g_Device.createShaderModule(moduleInfo);
            s_ShaderMap[m_GeomShaderSource.first] = m_GeomShader;
        }

        vk::PipelineShaderStageCreateInfo stageInfo;
        stageInfo.setStage(vk::ShaderStageFlagBits::eGeometry);
        stageInfo.setModule(m_GeomShader);
        stageInfo.setPName("main");
        shaderStageInfos.push_back(stageInfo);

        hash = Utility::HashState(&m_GeomShaderSource.first, 1, hash);
    }
    if (m_FragShaderSource.first && m_FragShaderSource.second > 0)
    {
        // find existed shader
        auto iter = s_ShaderMap.find(m_FragShaderSource.first);
        if (iter != s_ShaderMap.end())
        {
            m_FragShader = iter->second;
        }
        else
        {
            vk::ShaderModuleCreateInfo moduleInfo;
            moduleInfo.pCode = m_FragShaderSource.first;
            moduleInfo.codeSize = m_FragShaderSource.second;
            m_FragShader = Graphics::g_Device.createShaderModule(moduleInfo);
            s_ShaderMap[m_FragShaderSource.first] = m_FragShader;
        }
        vk::PipelineShaderStageCreateInfo stageInfo;
        stageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
        stageInfo.setModule(m_FragShader);
        stageInfo.setPName("main");
        shaderStageInfos.push_back(stageInfo);

        hash = Utility::HashState(&m_FragShaderSource.first, 1, hash);
    }
    ASSERT(shaderStageInfos.size() != 0);

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.setVertexBindingDescriptionCount(m_BindingDescriptions.size());
    vertexInputInfo.setVertexBindingDescriptions(m_BindingDescriptions);
    vertexInputInfo.setVertexAttributeDescriptionCount(m_AttributeDescriptions.size());
    vertexInputInfo.setVertexAttributeDescriptions(m_AttributeDescriptions);

    hash = Utility::HashState(m_BindingDescriptions.data(), m_BindingDescriptions.size(), hash);
    hash = Utility::HashState(m_AttributeDescriptions.data(), m_AttributeDescriptions.size(), hash);

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = Graphics::s_Swapchain.GetExtent().width;
    viewport.height = Graphics::s_Swapchain.GetExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vk::Rect2D scissor;
    scissor.setOffset({0, 0});
    scissor.setExtent(Graphics::s_Swapchain.GetExtent());
    vk::PipelineViewportStateCreateInfo viewportInfo;
    viewportInfo.setViewportCount(1);
    viewportInfo.setPViewports(&viewport);
    viewportInfo.setScissorCount(1);
    viewportInfo.setPScissors(&scissor);

    vk::PipelineMultisampleStateCreateInfo multisampleInfo;
    multisampleInfo.setSampleShadingEnable(VK_FALSE);
    multisampleInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
        vk::DynamicState::eDepthBias,
    };
    vk::PipelineDynamicStateCreateInfo dynamicInfo;
    dynamicInfo.setDynamicStateCount(dynamicStates.size());
    dynamicInfo.setDynamicStates(dynamicStates);

    vk::PipelineColorBlendStateCreateInfo blendInfo;
    blendInfo.attachmentCount = 1;
    blendInfo.pAttachments = &m_BlendState;

    vk::GraphicsPipelineCreateInfo psoInfo;
    psoInfo.setStageCount(shaderStageInfos.size());
    psoInfo.setStages(shaderStageInfos);
    psoInfo.setPVertexInputState(&vertexInputInfo);
    psoInfo.setPInputAssemblyState(&m_AssemblyInfo);
    psoInfo.setPViewportState(&viewportInfo);
    psoInfo.setPRasterizationState(&m_RasterizerState);
    psoInfo.setPMultisampleState(&multisampleInfo);
    psoInfo.setPColorBlendState(&blendInfo);
    psoInfo.setPDepthStencilState(&m_DepthStencilState);
    psoInfo.setPDynamicState(&dynamicInfo);
    psoInfo.setLayout(m_Layout);
    psoInfo.setRenderPass(m_RenderPass);
    psoInfo.setSubpass(0);

    hash = Utility::HashState(&m_AssemblyInfo, 1, hash);
    hash = Utility::HashState(&m_RasterizerState, 1, hash);
    hash = Utility::HashState(&m_BlendState, 1, hash);
    hash = Utility::HashState(&m_DepthStencilState, 1, hash);
    VkPipelineLayout layout = VkPipelineLayout(m_Layout);
    hash = Utility::HashState(&layout, 1, hash);
    VkRenderPass rp = VkRenderPass(m_RenderPass);
    hash = Utility::HashState(&rp, 1, hash);

    auto iter = s_GraphicsPSOHashMap.find(hash);
    if (iter != s_GraphicsPSOHashMap.end())
    {
        m_PSO = iter->second;
        return;
    }

    m_PSO = Graphics::g_Device.createGraphicsPipeline(nullptr, psoInfo).value;

    s_GraphicsPSOHashMap[hash] = m_PSO;
}

// void GraphicsPSO::Destroy()
//{
//     if (m_PSO)
//     {
//         Graphics::g_Device.destroyPipeline(m_PSO);
//         m_PSO = nullptr;
//     }
//     if (m_VertShader)
//     {
//         Graphics::g_Device.destroyShaderModule(m_VertShader);
//         m_VertShader = nullptr;
//     }
//     if (m_FragShader)
//     {
//         Graphics::g_Device.destroyShaderModule(m_FragShader);
//         m_FragShader = nullptr;
//     }
// }
ComputePSO::ComputePSO(const char *Name) : PSO(Name) {}

void ComputePSO::SetComputeShader(const unsigned int *binary, size_t size)
{
    m_CompShaderSource.first = binary;
    m_CompShaderSource.second = size;
}

void ComputePSO::Finalize()
{
    size_t hash = 2166136261U;

    vk::PipelineShaderStageCreateInfo stageInfo;
    if (m_CompShaderSource.first && m_CompShaderSource.second > 0)
    {
        // find existed shader
        auto iter = s_ShaderMap.find(m_CompShaderSource.first);
        if (iter != s_ShaderMap.end())
        {
            m_CompShader = iter->second;
        }
        else
        {
            // create a new one. remember to add to shader map
            vk::ShaderModuleCreateInfo moduleInfo;
            moduleInfo.pCode = m_CompShaderSource.first;
            moduleInfo.codeSize = m_CompShaderSource.second;
            m_CompShader = Graphics::g_Device.createShaderModule(moduleInfo);
            s_ShaderMap[m_CompShaderSource.first] = m_CompShader;
        }

        stageInfo.setStage(vk::ShaderStageFlagBits::eCompute);
        stageInfo.setModule(m_CompShader);
        stageInfo.setPName("main");

        hash = Utility::HashState(&m_CompShaderSource.first, 1, hash);
    };

    ASSERT(stageInfo.pName != nullptr);

    VkPipelineLayout layout = VkPipelineLayout(m_Layout);
    hash = Utility::HashState(&layout, 1, hash);

    auto iter = s_ComputePSOHashMap.find(hash);
    if (iter != s_ComputePSOHashMap.end())
    {
        m_PSO = iter->second;
        return;
    }

    vk::ComputePipelineCreateInfo psoInfo;
    psoInfo.setLayout(m_Layout);
    psoInfo.setStage(stageInfo);
    m_PSO = Graphics::g_Device.createComputePipeline(nullptr, psoInfo).value;

    s_ComputePSOHashMap[hash] = m_PSO;
}
