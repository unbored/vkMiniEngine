#include "MeshConvert.h"
#include "ModelLoader.h"
#include "TextureConvert.h"
#include "UniformBuffers.h"
#include "glTF.h"
#include <Math/Common.h>
#include <Utility.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

using namespace Renderer;

static inline glm::vec3 SafeNormalize(glm::vec3 x)
{
    float lenSq = LengthSquare(x);
    return lenSq < 1e-10f ? CreateXUnitVector() : x * glm::inversesqrt(lenSq);
}

void Renderer::CompileMesh(std::vector<Mesh *> &meshList, std::vector<uint8_t> &bufferMemory, glTF::Mesh &srcMesh,
                           uint32_t matrixIdx, const glm::mat4 &localToObject, BoundingSphere &boundingSphere,
                           AxisAlignedBox &boundingBox)
{
    // We still have a lot of work to do.  Now that we know about all of the primitives in this mesh
    // and have standardized their vertex buffer streams, we must set out to identify which primitives
    // have the same vertex format and material.  These can share a PSO and Vertex/Index buffer views.
    // There may be more than one draw call per group due to 16-bit indices.

    size_t totalVertexSize = 0;
    size_t totalDepthVertexSize = 0;
    size_t totalIndexSize = 0;

    BoundingSphere sphereOS(kZero);
    AxisAlignedBox bboxOS(kZero);

    std::vector<Primitive> primitives(srcMesh.primitives.size());
    for (uint32_t i = 0; i < primitives.size(); ++i)
    {
        OptimizeMesh(primitives[i], srcMesh.primitives[i], localToObject);
        sphereOS = sphereOS.Union(primitives[i].m_BoundsOS);
        bboxOS.AddBoundingBox(primitives[i].m_BBoxOS);
    }

    boundingSphere = sphereOS;
    boundingBox = bboxOS;

    std::map<uint32_t, std::vector<Primitive *>> renderMeshes;
    for (auto &prim : primitives)
    {
        uint32_t hash = prim.hash;
        renderMeshes[hash].push_back(&prim);
        totalVertexSize += prim.VB->size();
        totalDepthVertexSize += prim.DepthVB->size();
        totalIndexSize += Math::AlignUp(prim.IB->size(), 4);
    }

    uint32_t totalBufferSize = (uint32_t)(totalVertexSize + totalDepthVertexSize + totalIndexSize);

    Utility::ByteArray stagingBuffer;
    stagingBuffer.reset(new std::vector<uint8_t>(totalBufferSize));
    uint8_t *uploadMem = stagingBuffer->data();

    uint32_t curVBOffset = 0;
    uint32_t curDepthVBOffset = (uint32_t)totalVertexSize;
    uint32_t curIBOffset = curDepthVBOffset + (uint32_t)totalDepthVertexSize;

    for (auto &iter : renderMeshes)
    {
        size_t numDraws = iter.second.size();
        Mesh *mesh = (Mesh *)malloc(sizeof(Mesh) + sizeof(Mesh::Draw) * (numDraws - 1));
        size_t vbSize = 0;
        size_t vbDepthSize = 0;
        size_t ibSize = 0;

        // Compute local space bounding sphere for all submeshes
        BoundingSphere collectiveSphere(kZero);

        for (auto &draw : iter.second)
        {
            vbSize += draw->VB->size();
            vbDepthSize += draw->DepthVB->size();
            ibSize += draw->IB->size();
            collectiveSphere = collectiveSphere.Union(draw->m_BoundsLS);
        }

        mesh->bounds[0] = collectiveSphere.GetCenter().x;
        mesh->bounds[1] = collectiveSphere.GetCenter().y;
        mesh->bounds[2] = collectiveSphere.GetCenter().z;
        mesh->bounds[3] = collectiveSphere.GetRadius();
        mesh->vbOffset = (uint32_t)bufferMemory.size() + curVBOffset;
        mesh->vbSize = (uint32_t)vbSize;
        mesh->vbDepthOffset = (uint32_t)bufferMemory.size() + curDepthVBOffset;
        mesh->vbDepthSize = (uint32_t)vbDepthSize;
        mesh->ibOffset = (uint32_t)bufferMemory.size() + curIBOffset;
        mesh->ibSize = (uint32_t)ibSize;
        mesh->vbStride = (uint8_t)iter.second[0]->vertexStride;
        mesh->ibFormat = uint8_t(iter.second[0]->index32 ? vk::IndexType::eUint32 : vk::IndexType::eUint16);
        mesh->meshUB = (uint16_t)matrixIdx;
        mesh->materialUB = iter.second[0]->materialIdx;
        mesh->psoFlags = iter.second[0]->psoFlags;
        mesh->pso = 0xFFFF;
        if (srcMesh.skin >= 0)
        {
            mesh->numJoints = 0xFFFF;
            mesh->startJoint = (uint16_t)srcMesh.skin;
        }
        else
        {
            mesh->numJoints = 0;
            mesh->startJoint = 0xFFFF;
        }

        mesh->numDraws = (uint16_t)numDraws;

        uint32_t drawIdx = 0;
        uint32_t curVertOffset = 0;
        uint32_t curIndexOffset = 0;
        for (auto &draw : iter.second)
        {
            Mesh::Draw &d = mesh->draw[drawIdx++];
            d.primCount = draw->primCount;
            d.baseVertex = curVertOffset;
            d.startIndex = curIndexOffset;
            std::memcpy(uploadMem + curVBOffset + curVertOffset, draw->VB->data(), draw->VB->size());
            curVertOffset += (uint32_t)draw->VB->size() / draw->vertexStride;
            std::memcpy(uploadMem + curDepthVBOffset, draw->DepthVB->data(), draw->DepthVB->size());
            std::memcpy(uploadMem + curIBOffset + curIndexOffset, draw->IB->data(), draw->IB->size());
            curIndexOffset += (uint32_t)draw->IB->size() >> (draw->index32 + 1);
        }

        curVBOffset += (uint32_t)vbSize;
        curDepthVBOffset += (uint32_t)vbDepthSize;
        curIBOffset += (uint32_t)Math::AlignUp(ibSize, 4);
        curIndexOffset = Math::AlignUp(curIndexOffset, 4);

        meshList.push_back(mesh);
    }

    bufferMemory.insert(bufferMemory.end(), stagingBuffer->begin(), stagingBuffer->end());
}

static uint32_t WalkGraph(std::vector<GraphNode> &sceneGraph, BoundingSphere &modelBSphere, AxisAlignedBox &modelBBox,
                          std::vector<Mesh *> &meshList, std::vector<uint8_t> &bufferMemory,
                          const std::vector<glTF::Node *> &siblings, uint32_t curPos, const glm::mat4 &xform)
{
    size_t numSiblings = siblings.size();

    for (size_t i = 0; i < numSiblings; ++i)
    {
        glTF::Node *curNode = siblings[i];
        GraphNode &thisGraphNode = sceneGraph[curPos];
        thisGraphNode.hasChildren = 0;
        thisGraphNode.hasSibling = 0;
        thisGraphNode.matrixIdx = curPos;
        thisGraphNode.skeletonRoot = curNode->skeletonRoot;
        curNode->linearIdx = curPos;

        // They might not be used, but we have space to hold the neutral values which could be
        // useful when updating the matrix via animation.
        std::memcpy((float *)&thisGraphNode.scale, curNode->scale, sizeof(curNode->scale));
        std::memcpy((float *)&thisGraphNode.rotation, curNode->rotation, sizeof(curNode->rotation));

        if (curNode->hasMatrix)
        {
            std::memcpy((float *)&thisGraphNode.xform, curNode->matrix, sizeof(curNode->matrix));
        }
        else
        {
            glm::mat4 scale = glm::scale(glm::mat4(1.0), thisGraphNode.scale);
            thisGraphNode.xform = glm::mat4(thisGraphNode.rotation) * scale;
            thisGraphNode.xform[3] =
                glm::vec4(curNode->translation[0], curNode->translation[1], curNode->translation[2], 1.0);
        }

        const glm::mat4 LocalXform = xform * thisGraphNode.xform;

        if (!curNode->pointsToCamera && curNode->mesh != nullptr)
        {
            BoundingSphere sphereOS;
            AxisAlignedBox boxOS;
            CompileMesh(meshList, bufferMemory, *curNode->mesh, curPos, LocalXform, sphereOS, boxOS);
            modelBSphere = modelBSphere.Union(sphereOS);
            modelBBox.AddBoundingBox(boxOS);
        }

        uint32_t nextPos = curPos + 1;

        if (curNode->children.size() > 0)
        {
            thisGraphNode.hasChildren = 1;
            nextPos = WalkGraph(sceneGraph, modelBSphere, modelBBox, meshList, bufferMemory, curNode->children, nextPos,
                                LocalXform);
        }

        // Are there more siblings?
        if (i + 1 < numSiblings)
        {
            thisGraphNode.hasSibling = 1;
        }

        curPos = nextPos;
    }

    return curPos;
}

inline void CompileTexture(const std::string &basePath, const std::string &fileName, uint8_t flags)
{
    CompileTextureOnDemand(basePath + fileName, flags);
}

inline void SetTextureOptions(std::map<std::string, uint8_t> &optionsMap, glTF::Texture *texture, uint8_t options)
{
    if (texture && texture->source && optionsMap.find(texture->source->path) == optionsMap.end())
        optionsMap[texture->source->path] = options;
}

void BuildMaterials(ModelData &model, const glTF::Asset &asset)
{
    static_assert((alignof(MaterialConstants) & 255) == 0, "CBVs need 256 uint8_t alignment");

    // Replace texture filename extensions with "KTX" in the string table
    model.m_TextureNames.resize(asset.m_images.size());
    for (size_t i = 0; i < asset.m_images.size(); ++i)
        model.m_TextureNames[i] = asset.m_images[i].path;

    std::map<std::string, uint8_t> textureOptions;

    const uint32_t numMaterials = (uint32_t)asset.m_materials.size();

    model.m_MaterialConstants.resize(numMaterials);
    model.m_MaterialTextures.resize(numMaterials);

    for (uint32_t i = 0; i < numMaterials; ++i)
    {
        const glTF::Material &srcMat = asset.m_materials[i];

        MaterialConstantData &material = model.m_MaterialConstants[i];
        material.baseColorFactor[0] = srcMat.baseColorFactor[0];
        material.baseColorFactor[1] = srcMat.baseColorFactor[1];
        material.baseColorFactor[2] = srcMat.baseColorFactor[2];
        material.baseColorFactor[3] = srcMat.baseColorFactor[3];
        material.emissiveFactor[0] = srcMat.emissiveFactor[0];
        material.emissiveFactor[1] = srcMat.emissiveFactor[1];
        material.emissiveFactor[2] = srcMat.emissiveFactor[2];
        material.normalTextureScale = srcMat.normalTextureScale;
        material.metallicFactor = srcMat.metallicFactor;
        material.roughnessFactor = srcMat.roughnessFactor;
        material.flags = srcMat.flags;

        MaterialTextureData &dstMat = model.m_MaterialTextures[i];
        dstMat.addressModes = 0;

        for (uint32_t ti = 0; ti < kNumTextures; ++ti)
        {
            dstMat.stringIdx[ti] = 0xFFFF;

            if (srcMat.textures[ti] != nullptr)
            {
                if (srcMat.textures[ti]->source != nullptr)
                {
                    dstMat.stringIdx[ti] = uint16_t(srcMat.textures[ti]->source - asset.m_images.data());
                }

                if (srcMat.textures[ti]->sampler != nullptr)
                {
                    dstMat.addressModes |= (uint32_t)srcMat.textures[ti]->sampler->wrapS << (ti * 4);
                    dstMat.addressModes |= (uint32_t)srcMat.textures[ti]->sampler->wrapT << (ti * 4 + 2);
                }
                else
                {
                    dstMat.addressModes |= 0x5 << (ti * 4);
                }
            }
            else
            {
                dstMat.addressModes |= 0x5 << (ti * 4);
            }
        }

        SetTextureOptions(textureOptions, srcMat.textures[kBaseColor],
                          TextureOptions(true, srcMat.alphaBlend | srcMat.alphaTest));
        SetTextureOptions(textureOptions, srcMat.textures[kMetallicRoughness], TextureOptions(false));
        SetTextureOptions(textureOptions, srcMat.textures[kOcclusion], TextureOptions(false));
        SetTextureOptions(textureOptions, srcMat.textures[kEmissive], TextureOptions(true));
        SetTextureOptions(textureOptions, srcMat.textures[kNormal], TextureOptions(false));
    }

    model.m_TextureOptions.clear();
    for (auto name : model.m_TextureNames)
    {
        auto iter = textureOptions.find(name);
        if (iter != textureOptions.end())
        {
            model.m_TextureOptions.push_back(iter->second);
            CompileTextureOnDemand(asset.m_basePath + iter->first, iter->second);
        }
        else
            model.m_TextureOptions.push_back(0xFF);
    }
    ASSERT(model.m_TextureOptions.size() == model.m_TextureNames.size());
}

void BuildAnimations(ModelData &model, const glTF::Asset &asset)
{
    size_t numAnimations = asset.m_animations.size();
    if (numAnimations == 0)
        return;

    model.m_Animations.resize(numAnimations);
    uint32_t animIdx = 0;

    for (const glTF::Animation &anim : asset.m_animations)
    {
        AnimationSet &animSet = model.m_Animations[animIdx++];
        animSet.duration = 0.0f;
        animSet.firstCurve = (uint32_t)model.m_AnimationCurves.size();
        animSet.numCurves = (uint32_t)anim.m_channels.size();

        for (size_t i = 0; i < animSet.numCurves; ++i)
        {
            const glTF::AnimChannel &channel = anim.m_channels[i];
            const glTF::AnimSampler &sampler = *channel.m_sampler;

            ASSERT(channel.m_target->linearIdx >= 0);

            AnimationCurve curve;
            curve.targetNode = channel.m_target->linearIdx;
            curve.targetPath = channel.m_path;
            curve.interpolation = sampler.m_interpolation;
            curve.keyFrameOffset = model.m_AnimationKeyFrameData.size();
            curve.keyFrameFormat = std::min<uint32_t>(sampler.m_output->componentType, AnimationCurve::kFloat);
            curve.numSegments = sampler.m_output->count - 1.0f;

            // In glTF, stride==0 means "packed tightly"
            if (sampler.m_output->stride == 0)
            {
                uint32_t numComponents = sampler.m_output->type + 1;
                uint32_t uint8_tsPerComponent = sampler.m_output->componentType / 2 + 1;
                curve.keyFrameStride = numComponents * uint8_tsPerComponent / 4;
            }
            else
            {
                ASSERT(sampler.m_output->stride <= 16 && sampler.m_output->stride % 4 == 0);
                curve.keyFrameStride = sampler.m_output->stride / 4;
            }

            // Determine start and stop time stamps
            const float *timeStamps = (float *)sampler.m_input->dataPtr;
            curve.startTime = timeStamps[0];

            const float endTime = timeStamps[sampler.m_output->count - 1];
            curve.rangeScale = curve.numSegments / (endTime - curve.startTime);

            animSet.duration = std::max<float>(animSet.duration, endTime);

            // Append this curve data
            model.m_AnimationKeyFrameData.insert(model.m_AnimationKeyFrameData.end(), sampler.m_output->dataPtr,
                                                 sampler.m_output->dataPtr +
                                                     sampler.m_output->count * curve.keyFrameStride * 4);

            model.m_AnimationCurves.push_back(curve);
        }
    }
}

void BuildSkins(ModelData &model, const glTF::Asset &asset)
{
    size_t numSkins = asset.m_skins.size();
    if (numSkins == 0)
        return;

    std::vector<std::pair<uint16_t, uint16_t>> skinMap;
    skinMap.reserve(asset.m_skins.size());

    for (const glTF::Skin &skin : asset.m_skins)
    {
        // Record offset and joint count
        uint16_t numJoints = (uint16_t)skin.joints.size();
        uint16_t curOffset = (uint16_t)model.m_JointIndices.size();
        skinMap.push_back(std::make_pair(curOffset, numJoints));

        // Append remapped joint indices
        for (glTF::Node *joint : skin.joints)
        {
            ASSERT(joint->linearIdx >= 0, "Skin joint not present in node hierarchy");
            model.m_JointIndices.push_back((uint16_t)joint->linearIdx);
        }

        // Append IBMs
        glm::mat4 *IBMstart = (glm::mat4 *)skin.inverseBindMatrices->dataPtr;
        glm::mat4 *IBMend = IBMstart + skin.inverseBindMatrices->count;
        ASSERT(skin.inverseBindMatrices->count == numJoints);
        model.m_JointIBMs.insert(model.m_JointIBMs.end(), IBMstart, IBMend);
    }

    // Assign skinned meshes the proper joint offset and count
    for (Mesh *mesh : model.m_Meshes)
    {
        if (mesh->numJoints != 0)
        {
            std::pair<uint16_t, uint16_t> offsetAndCount = skinMap[mesh->startJoint];
            mesh->startJoint = offsetAndCount.first;
            mesh->numJoints = offsetAndCount.second;
        }
    }
}

bool Renderer::BuildModel(ModelData &model, const glTF::Asset &asset, int sceneIdx)
{
    BuildMaterials(model, asset);

    // Generate scene graph and meshes
    model.m_SceneGraph.resize(asset.m_nodes.size());
    const glTF::Scene *scene = sceneIdx < 0 ? asset.m_scene : &asset.m_scenes[sceneIdx];
    if (scene == nullptr)
        return false;

    // Aggregate all of the vertex and index buffers in this unified buffer
    std::vector<uint8_t> &bufferMemory = model.m_GeometryData;

    model.m_BoundingSphere = BoundingSphere(kZero);
    model.m_BoundingBox = AxisAlignedBox(kZero);
    uint32_t numNodes = WalkGraph(model.m_SceneGraph, model.m_BoundingSphere, model.m_BoundingBox, model.m_Meshes,
                                  bufferMemory, scene->nodes, 0, glm::mat4(kIdentity));
    model.m_SceneGraph.resize(numNodes);

    BuildAnimations(model, asset);
    BuildSkins(model, asset);

    return true;
}