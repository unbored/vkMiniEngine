#pragma once

#include "Model.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace glTF
{
class Asset;
struct Mesh;
} // namespace glTF

namespace Renderer
{
using namespace Math;

// Unaligned mirror of MaterialConstants
struct MaterialConstantData
{
    float baseColorFactor[4]; // default=[1,1,1,1]
    float emissiveFactor[3];  // default=[0,0,0]
    float normalTextureScale; // default=1
    float metallicFactor;     // default=1
    float roughnessFactor;    // default=1
    uint32_t flags;
};

// Used at load time to construct descriptor tables
struct MaterialTextureData
{
    uint16_t stringIdx[kNumTextures];
    uint32_t addressModes;
};

// All of the information that needs to be written to a .mini data file
struct ModelData
{
    BoundingSphere m_BoundingSphere;
    AxisAlignedBox m_BoundingBox;
    std::vector<uint8_t> m_GeometryData;
    std::vector<uint8_t> m_AnimationKeyFrameData;
    std::vector<AnimationCurve> m_AnimationCurves;
    std::vector<AnimationSet> m_Animations;
    std::vector<uint16_t> m_JointIndices;
    std::vector<glm::mat4> m_JointIBMs;
    std::vector<MaterialTextureData> m_MaterialTextures;
    std::vector<MaterialConstantData> m_MaterialConstants;
    std::vector<Mesh *> m_Meshes;
    std::vector<GraphNode> m_SceneGraph;
    std::vector<std::string> m_TextureNames;
    std::vector<uint8_t> m_TextureOptions;
};

void CompileMesh(std::vector<Mesh *> &meshList, std::vector<uint8_t> &bufferMemory, glTF::Mesh &srcMesh,
                 uint32_t matrixIdx, const glm::mat4 &localToObject, Math::BoundingSphere &boundingSphere,
                 Math::AxisAlignedBox &boundingBox);

bool BuildModel(ModelData &model, const glTF::Asset &asset, int sceneIdx = -1);

std::shared_ptr<Model> LoadModel(const std::string &filePath, bool forceRebuild = false);

} // namespace Renderer