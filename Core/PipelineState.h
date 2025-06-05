#pragma once

#include "GraphicsCommon.h"
#include "RenderPass.h"
#include <vulkan/vulkan.hpp>

class PSO
{
public:
    PSO(const char *name) : m_Name(name) {}

    vk::Pipeline GetPipeline() const { return m_PSO; }

    void SetPipelineLayout(const vk::PipelineLayout &layout);

    static void DestroyAll();

protected:
    // std::vector<vk::PipelineShaderStageCreateInfo> m_ShaderStageInfos;
    const char *m_Name;

    vk::PipelineLayout m_Layout;
    vk::Pipeline m_PSO;
};

using VertexInputBindingAttribute =
    std::pair<vk::VertexInputBindingDescription, std::vector<vk::VertexInputAttributeDescription>>;
class GraphicsPSO : public PSO
{
public:
    GraphicsPSO(const char *Name = "Unnamed Graphics PSO");

    void SetVertexShader(const unsigned int *binary, size_t size);
    void SetGeometryShader(const unsigned int *binary, size_t size);
    void SetFragmentShader(const unsigned int *binary, size_t size);
    // void SetInputLayout(const vk::VertexInputBindingDescription &binding,
    //                     const std::vector<vk::VertexInputAttributeDescription> &attributes);
    void SetInputLayout(const vk::ArrayProxy<VertexInputBindingAttribute> &attributes);
    void SetPrimitiveTopologyType(vk::PrimitiveTopology topology);
    void SetRasterizerState(const RasterizationState &rasterizer);
    void SetDepthStencilState(const DepthStencilState &depthStencil);
    void SetBlendState(const ColorBlendState &blend);
    void SetRenderPassFormat(const vk::ArrayProxy<vk::Format> &colors, vk::Format depth = vk::Format::eUndefined);
    void Finalize();
    //    void Destroy();

private:
    vk::ShaderModule m_VertShader;
    vk::ShaderModule m_FragShader;
    vk::ShaderModule m_GeomShader;
    std::pair<const unsigned int *, size_t> m_VertShaderSource;
    std::pair<const unsigned int *, size_t> m_GeomShaderSource;
    std::pair<const unsigned int *, size_t> m_FragShaderSource;
    std::vector<vk::VertexInputBindingDescription> m_BindingDescriptions;
    std::vector<vk::VertexInputAttributeDescription> m_AttributeDescriptions;
    vk::RenderPass m_RenderPass;
    vk::PipelineInputAssemblyStateCreateInfo m_AssemblyInfo;
    RasterizationState m_RasterizerState;
    ColorBlendState m_BlendState;
    DepthStencilState m_DepthStencilState;
};

class ComputePSO : public PSO
{
public:
    ComputePSO(const char *Name = "Unnamed Compute PSO");

    void SetComputeShader(const unsigned int *binary, size_t size);

    void Finalize();

private:
    std::pair<const unsigned int *, size_t> m_CompShaderSource;
    vk::ShaderModule m_CompShader;
};
