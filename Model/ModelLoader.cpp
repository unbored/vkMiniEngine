#include "ModelLoader.h"
#include "Animation.h"
#include "CommandContext.h"
#include "GpuBuffer.h"
#include "Model.h"
#include "TextureConvert.h"
#include "TextureManager.h"
#include "glTF.h"
#include <GraphicsCommon.h>
#include <SamplerManager.h>
#include <Utility.h>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <unordered_map>
#include <vector>

using namespace Renderer;
using namespace Graphics;

vk::Sampler GetSampler(uint32_t addressModes)
{
    SamplerDesc desc;
    desc.addressModeU = vk::SamplerAddressMode(addressModes & 0x03);
    desc.addressModeV = vk::SamplerAddressMode(addressModes >> 2);
    return desc.CreateSampler();
}

void LoadMaterials(Model &model, const std::vector<MaterialTextureData> &materialTextures,
                   const std::vector<std::string> &textureNames, const std::vector<uint8_t> &textureOptions,
                   const std::string &basePath)
{
    static_assert((alignof(MaterialConstants) & 255) == 0, "CBVs need 256 byte alignment");

    // Load textures
    const uint32_t numTextures = (uint32_t)textureNames.size();
    model.textures.resize(numTextures);
    for (size_t ti = 0; ti < numTextures; ++ti)
    {
        std::string originalFile = basePath + textureNames[ti];
        CompileTextureOnDemand(originalFile, textureOptions[ti]);

        std::string ktxFile = Utility::RemoveExtension(originalFile) + ".ktx";
        TextureRef ref = TextureManager::LoadKTXFromFile(ktxFile);
        model.textures[ti] = ref;
    }

    // Generate descriptor tables and record offsets for each material
    const uint32_t numMaterials = (uint32_t)materialTextures.size();
    std::vector<uint32_t> tableOffsets(numMaterials);

    for (uint32_t matIdx = 0; matIdx < numMaterials; ++matIdx)
    {
        const MaterialTextureData &srcMat = materialTextures[matIdx];

        uint32_t DestCount = kNumTextures;
        std::vector<vk::DescriptorImageInfo> DefaultTextures = {
            {SamplerLinearClamp, GetDefaultTexture(kWhiteOpaque2D), vk::ImageLayout::eShaderReadOnlyOptimal},
            {SamplerLinearClamp, GetDefaultTexture(kWhiteOpaque2D), vk::ImageLayout::eShaderReadOnlyOptimal},
            {SamplerLinearClamp, GetDefaultTexture(kWhiteOpaque2D), vk::ImageLayout::eShaderReadOnlyOptimal},
            {SamplerLinearClamp, GetDefaultTexture(kBlackTransparent2D), vk::ImageLayout::eShaderReadOnlyOptimal},
            {SamplerLinearClamp, GetDefaultTexture(kDefaultNormalMap), vk::ImageLayout::eShaderReadOnlyOptimal},
        };

        // unlike DX12, since texture and sampler are combined, we always create a new combination
        uint32_t addressModes = srcMat.addressModes;
        std::vector<vk::DescriptorImageInfo> SourceTextures(kNumTextures);
        for (int i = 0; i < kNumTextures; ++i)
        {
            SourceTextures[i].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            if (srcMat.stringIdx[i] == 0xffff)
            {
                SourceTextures[i] = DefaultTextures[i];
            }
            else
            {
                SourceTextures[i].imageView = vk::ImageView(*model.textures[srcMat.stringIdx[i]].Get());
            }
            SourceTextures[i].sampler = GetSampler(addressModes & 0xF);
            addressModes >>= 4;
        }
        tableOffsets[matIdx] = Renderer::m_TextureHeap.size();
        Renderer::m_TextureHeap.push_back(SourceTextures);
    }

    // Update table offsets for each mesh
    uint8_t *meshPtr = model.m_MeshData.get();
    for (uint32_t i = 0; i < model.m_NumMeshes; ++i)
    {
        Mesh &mesh = *(Mesh *)meshPtr;
        uint32_t offset = tableOffsets[mesh.materialUB];
        mesh.imageTable = offset;
        mesh.pso = Renderer::GetPSO(mesh.psoFlags);
        meshPtr += sizeof(Mesh) + (mesh.numDraws - 1) * sizeof(Mesh::Draw);
    }
}

std::shared_ptr<Model> Renderer::LoadModel(const std::string &filePath, bool forceRebuild)
{
    namespace fs = std::filesystem;

    const std::string fileName = Utility::RemoveBasePath(filePath);

    fs::path sourceFile(filePath);
    bool sourceFileMissing = !fs::exists(sourceFile) || fs::is_directory(sourceFile);

    if (sourceFileMissing)
    {
        Utility::Printf("Error: Could not find %s\n", fileName.c_str());
        return nullptr;
    }

    ModelData modelData;
    const std::string fileExt = Utility::ToLower(Utility::GetFileExtension(filePath));
    if (fileExt == "gltf" || fileExt == "glb")
    {
        // read a gltf file
        glTF::Asset asset(filePath);
        if (!BuildModel(modelData, asset))
        {
            return nullptr;
        }
    }
    else
    {
        Utility::Printf("Unsupported model file extension: %s\n", fileExt.c_str());
        return nullptr;
    }

    std::string basePath = Utility::GetBasePath(filePath);

    std::shared_ptr<Model> model(new Model);

    model->m_NumNodes = modelData.m_SceneGraph.size();
    model->m_SceneGraph.reset(new GraphNode[model->m_NumNodes]);
    model->m_NumMeshes = modelData.m_Meshes.size();
    size_t meshDataSize = 0;
    for (auto &mesh : modelData.m_Meshes)
    {
        meshDataSize += sizeof(Mesh) + (mesh->numDraws - 1) * sizeof(Mesh::Draw);
    }
    model->m_MeshData.reset(new uint8_t[meshDataSize]);

    if (!modelData.m_GeometryData.empty())
    {
        StagingBuffer data;
        data.Create(modelData.m_GeometryData.size());
        memcpy(data.Map(), modelData.m_GeometryData.data(), modelData.m_GeometryData.size());
        data.Unmap();
        model->m_DataBuffer.Create(modelData.m_GeometryData.size());
        CommandContext::InitializeBuffer(model->m_DataBuffer, data, 0);
        data.Destroy();
    }

    memcpy(model->m_SceneGraph.get(), modelData.m_SceneGraph.data(), model->m_NumNodes * sizeof(GraphNode));

    char *meshPtr = (char *)model->m_MeshData.get();
    for (auto &mesh : modelData.m_Meshes)
    {
        size_t meshSize = sizeof(Mesh) + (mesh->numDraws - 1) * sizeof(Mesh::Draw);
        memcpy(meshPtr, mesh, meshSize);
        meshPtr += meshSize;
    }

    if (!modelData.m_MaterialConstants.empty())
    {
        StagingBuffer materialConstants;
        materialConstants.Create(modelData.m_MaterialConstants.size() * sizeof(MaterialConstants));
        MaterialConstants *materialUB = (MaterialConstants *)materialConstants.Map();
        for (auto &m : modelData.m_MaterialConstants)
        {
            memcpy(materialUB, &m, sizeof(MaterialConstantData));
            materialUB++;
        }
        materialConstants.Unmap();
        model->m_MaterialConstants.Create(modelData.m_MaterialConstants.size() * sizeof(MaterialConstants));
        CommandContext::InitializeBuffer(model->m_MaterialConstants, materialConstants, 0);
        materialConstants.Destroy();
    }

    // Read material texture and sampler properties so we can load the material
    std::vector<MaterialTextureData> materialTextures = modelData.m_MaterialTextures;

    std::vector<std::string> textureNames = modelData.m_TextureNames;

    std::vector<uint8_t> textureOptions = modelData.m_TextureOptions;

    LoadMaterials(*model, modelData.m_MaterialTextures, modelData.m_TextureNames, modelData.m_TextureOptions, basePath);

    model->m_BoundingSphere = modelData.m_BoundingSphere;
    model->m_BoundingBox = modelData.m_BoundingBox;

    model->m_NumAnimations = modelData.m_Animations.size();

    if (!modelData.m_Animations.empty())
    {
        assert(!modelData.m_AnimationKeyFrameData.empty() && !modelData.m_AnimationCurves.empty());
        model->m_KeyFrameData.reset(new uint8_t[modelData.m_AnimationKeyFrameData.size()]);
        memcpy(model->m_KeyFrameData.get(), modelData.m_AnimationKeyFrameData.data(),
               modelData.m_AnimationKeyFrameData.size());
        model->m_CurveData.reset(new AnimationCurve[modelData.m_AnimationCurves.size()]);
        memcpy(model->m_CurveData.get(), modelData.m_AnimationCurves.data(),
               modelData.m_AnimationCurves.size() * sizeof(AnimationCurve));
        model->m_Animations.reset(new AnimationSet[modelData.m_Animations.size()]);
        memcpy(model->m_Animations.get(), modelData.m_Animations.data(),
               modelData.m_Animations.size() * sizeof(AnimationSet));
    }

    model->m_NumJoints = modelData.m_JointIndices.size();

    if (!modelData.m_JointIndices.empty())
    {
        model->m_JointIndices.reset(new uint16_t[modelData.m_JointIndices.size()]);
        memcpy(model->m_JointIndices.get(), modelData.m_JointIndices.data(),
               modelData.m_JointIndices.size() * sizeof(uint16_t));
        model->m_JointIBMs.reset(new glm::mat4[modelData.m_JointIBMs.size()]);
        memcpy(model->m_JointIBMs.get(), modelData.m_JointIBMs.data(),
               modelData.m_JointIBMs.size() * sizeof(glm::mat4));
    }

    return model;
}