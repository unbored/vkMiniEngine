//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author(s):  James Stanard
//             Chuck Walbourn (ATG)
//
// This code depends on DirectXTex
//
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_format_traits.hpp>

#include "IndexOptimizePostTransform.h"
#include "MeshConvert.h"
#include "Model.h"
#include "TextureConvert.h"
#include "Util/VulkanMesh.h"
#include "glTF.h"

using namespace glTF;
using namespace Math;

static vk::Format JointIndexFormat(const Accessor &accessor)
{
    switch (accessor.componentType)
    {
    case Accessor::kUnsignedByte:
        return vk::Format::eR8G8B8A8Uint;
    case Accessor::kUnsignedShort:
        return vk::Format::eR16G16B16A16Uint;
    default:
        ASSERT("Invalid joint index format");
        return vk::Format::eUndefined;
    }
}

static vk::Format AccessorFormat(const Accessor &accessor)
{
    switch (accessor.componentType)
    {
    case Accessor::kUnsignedByte:
        switch (accessor.type)
        {
        case Accessor::kScalar:
            return vk::Format::eR8Unorm;
        case Accessor::kVec2:
            return vk::Format::eR8G8Unorm;
        default:
            return vk::Format::eR8G8B8A8Unorm;
        }
    case Accessor::kUnsignedShort:
        switch (accessor.type)
        {
        case Accessor::kScalar:
            return vk::Format::eR16Unorm;
        case Accessor::kVec2:
            return vk::Format::eR16G16Unorm;
        default:
            return vk::Format::eR16G16B16A16Unorm;
        }
    case Accessor::kFloat:
        switch (accessor.type)
        {
        case Accessor::kScalar:
            return vk::Format::eR32Sfloat;
        case Accessor::kVec2:
            return vk::Format::eR32G32Sfloat;
        case Accessor::kVec3:
            return vk::Format::eR32G32B32Sfloat;
        default:
            return vk::Format::eR32G32B32A32Sfloat;
        }
    default:
        ASSERT("Invalid accessor format");
        return vk::Format::eUndefined;
    }
}

void OptimizeMesh(Renderer::Primitive &outPrim, const glTF::Primitive &inPrim, const glm::mat4 &localToObject)
{
    ASSERT(inPrim.attributes[0] != nullptr, "Must have POSITION");
    uint32_t vertexCount = inPrim.attributes[0]->count;

    void *indices = nullptr;
    uint32_t indexCount;
    bool b32BitIndices;
    uint32_t maxIndex = inPrim.maxIndex;

    if (inPrim.indices == nullptr)
    {
        // always process triangle
        ASSERT(inPrim.mode == 4, "Impossible primitive topology when lacking indices");

        // no indice data in inPrim. Generate new ones
        indexCount = vertexCount * 3;
        maxIndex = indexCount - 1;
        if (indexCount > 0xFFFF)
        {
            b32BitIndices = true;
            outPrim.IB = std::make_shared<std::vector<uint8_t>>(4 * indexCount);
            indices = outPrim.IB->data();
            uint32_t *tmp = (uint32_t *)indices;
            for (uint32_t i = 0; i < indexCount; ++i)
                tmp[i] = i;
        }
        else
        {
            b32BitIndices = false;
            outPrim.IB = std::make_shared<std::vector<uint8_t>>(2 * indexCount);
            indices = outPrim.IB->data();
            uint16_t *tmp = (uint16_t *)indices;
            for (uint16_t i = 0; i < indexCount; ++i)
                tmp[i] = i;
        }
    }
    else
    {
        // Only process triangles here
        switch (inPrim.mode)
        {
        default:
        case 0: // POINT LIST
        case 1: // LINE LIST
        case 2: // LINE LOOP
        case 3: // LINE STRIP
            Utility::Printf("Found unsupported primitive topology\n");
            return;
        case 4: // TRIANGLE LIST
            break;
        case 5: // TODO: Convert TRIANGLE STRIP
        case 6: // TODO: Convert TRIANGLE FAN
            Utility::Printf("Found an index buffer that needs to be converted to a triangle list\n");
            return;
        }

        // read indice data from inPrim
        indices = inPrim.indices->dataPtr;
        indexCount = inPrim.indices->count;
        if (maxIndex == 0)
        {
            // missing maxIndex info, find the max index
            if (inPrim.indices->componentType == Accessor::kUnsignedInt)
            {
                uint32_t *ib = (uint32_t *)inPrim.indices->dataPtr;
                for (uint32_t k = 0; k < indexCount; ++k)
                    maxIndex = std::max(ib[k], maxIndex);
            }
            else
            {
                uint16_t *ib = (uint16_t *)inPrim.indices->dataPtr;
                for (uint32_t k = 0; k < indexCount; ++k)
                    maxIndex = std::max<uint32_t>(ib[k], maxIndex);
            }
        }

        // optimize face indice and save to outPrim, made sure indice are float type
        b32BitIndices = maxIndex > 0xFFFF;
        uint32_t indexSize = b32BitIndices ? 4 : 2;
        outPrim.IB = std::make_shared<std::vector<uint8_t>>(indexSize * indexCount);
        if (b32BitIndices)
        {
            ASSERT(inPrim.indices->componentType == Accessor::kUnsignedInt);
            OptimizeFaces((uint32_t *)inPrim.indices->dataPtr, inPrim.indices->count, (uint32_t *)outPrim.IB->data(),
                          64);
        }
        else if (inPrim.indices->componentType == Accessor::kUnsignedShort)
        {
            OptimizeFaces((uint16_t *)inPrim.indices->dataPtr, inPrim.indices->count, (uint16_t *)outPrim.IB->data(),
                          64);
        }
        else
        {
            OptimizeFaces((uint32_t *)inPrim.indices->dataPtr, inPrim.indices->count, (uint16_t *)outPrim.IB->data(),
                          64);
        }
        indices = outPrim.IB->data();
    }

    ASSERT(maxIndex > 0);

    const bool HasNormals = inPrim.attributes[glTF::Primitive::kNormal] != nullptr;
    const bool HasTangents = inPrim.attributes[glTF::Primitive::kTangent] != nullptr;
    const bool HasUV0 = inPrim.attributes[glTF::Primitive::kTexcoord0] != nullptr;
    const bool HasUV1 = inPrim.attributes[glTF::Primitive::kTexcoord1] != nullptr;
    const bool HasJoints = inPrim.attributes[glTF::Primitive::kJoints0] != nullptr;
    const bool HasWeights = inPrim.attributes[glTF::Primitive::kWeights0] != nullptr;
    const bool HasSkin = HasJoints && HasWeights;

    std::vector<vk::VertexInputAttributeDescription> InputElements;
    // here location is just an id for reader to seek proper data
    // while binding indicates all data are alone
    // location, binding, format, offset
    InputElements.push_back({glTF::Primitive::kPosition, glTF::Primitive::kPosition, vk::Format::eR32G32B32Sfloat});
    if (HasNormals)
    {
        InputElements.push_back({glTF::Primitive::kNormal, glTF::Primitive::kNormal, vk::Format::eR32G32B32Sfloat});
    }
    if (HasTangents)
    {
        InputElements.push_back(
            {glTF::Primitive::kTangent, glTF::Primitive::kTangent, vk::Format::eR32G32B32A32Sfloat});
    }
    if (HasUV0)
    {
        InputElements.push_back({glTF::Primitive::kTexcoord0, glTF::Primitive::kTexcoord0,
                                 AccessorFormat(*inPrim.attributes[glTF::Primitive::kTexcoord0])});
    }
    if (HasUV1)
    {
        InputElements.push_back({glTF::Primitive::kTexcoord1, glTF::Primitive::kTexcoord1,
                                 AccessorFormat(*inPrim.attributes[glTF::Primitive::kTexcoord1])});
    }
    if (HasSkin)
    {
        InputElements.push_back({glTF::Primitive::kJoints0, glTF::Primitive::kJoints0,
                                 JointIndexFormat(*inPrim.attributes[glTF::Primitive::kJoints0])});
        InputElements.push_back({glTF::Primitive::kWeights0, glTF::Primitive::kWeights0,
                                 AccessorFormat(*inPrim.attributes[glTF::Primitive::kWeights0])});
    }

    VBReader vbr;
    vbr.Initialize(InputElements);

    for (uint32_t i = 0; i < Primitive::kNumAttribs; ++i)
    {
        Accessor *attrib = inPrim.attributes[i];
        if (attrib)
            vbr.AddStream(attrib->dataPtr, vertexCount, i, attrib->stride);
    }

    const glTF::Material &material = *inPrim.material;

    std::unique_ptr<glm::vec3[]> position;
    std::unique_ptr<glm::vec3[]> normal;
    std::unique_ptr<glm::vec4[]> tangent;
    std::unique_ptr<glm::vec2[]> texcoord0;
    std::unique_ptr<glm::vec2[]> texcoord1;
    std::unique_ptr<glm::vec4[]> joints;
    std::unique_ptr<glm::vec4[]> weights;
    position.reset(new glm::vec3[vertexCount]);
    normal.reset(new glm::vec3[vertexCount]);

    ASSERT_SUCCEEDED(vbr.Read(position.get(), glTF::Primitive::kPosition, vertexCount));
    {
        // Local space bounds
        glm::vec3 sphereCenterLS =
            (glm::vec3(*(glm::vec3 *)inPrim.minPos) + glm::vec3(*(glm::vec3 *)inPrim.maxPos)) * 0.5f;
        float maxRadiusLSSq(kZero);

        // Object space bounds
        // (This would be expressed better with an AffineTransform * glm::vec3)
        glm::vec3 sphereCenterOS = glm::vec3(localToObject * glm::vec4(sphereCenterLS, 1.0));
        float maxRadiusOSSq(kZero);

        outPrim.m_BBoxLS = AxisAlignedBox(kZero);
        outPrim.m_BBoxOS = AxisAlignedBox(kZero);

        // calculate Sphere radius and bounding box size
        for (uint32_t v = 0; v < vertexCount /*maxIndex*/; ++v)
        {
            glm::vec3 positionLS = glm::vec3(position[v]);
            maxRadiusLSSq = glm::max(maxRadiusLSSq, LengthSquare(sphereCenterLS - positionLS));

            outPrim.m_BBoxLS.AddPoint(positionLS);

            glm::vec3 positionOS = glm::vec3(localToObject * glm::vec4(positionLS, 1.0));
            maxRadiusOSSq = glm::max(maxRadiusOSSq, LengthSquare(sphereCenterOS - positionOS));

            outPrim.m_BBoxOS.AddPoint(positionOS);
        }

        outPrim.m_BoundsLS = Math::BoundingSphere(sphereCenterLS, glm::sqrt(maxRadiusLSSq));
        outPrim.m_BoundsOS = Math::BoundingSphere(sphereCenterOS, glm::sqrt(maxRadiusOSSq));
        ASSERT(outPrim.m_BoundsOS.GetRadius() > 0.0f);
    }

    if (HasNormals)
    {
        ASSERT_SUCCEEDED(vbr.Read(normal.get(), glTF::Primitive::kNormal, vertexCount));
    }
    else
    {
        const size_t faceCount = indexCount / 3;

        if (b32BitIndices)
            ComputeNormals((const uint32_t *)indices, faceCount, position.get(), vertexCount, normal.get());
        else
            ComputeNormals((const uint16_t *)indices, faceCount, position.get(), vertexCount, normal.get());
    }

    if (HasUV0)
    {
        texcoord0.reset(new glm::vec2[vertexCount]);
        ASSERT_SUCCEEDED(vbr.Read(texcoord0.get(), glTF::Primitive::kTexcoord0, vertexCount));
    }

    if (HasUV1)
    {
        texcoord1.reset(new glm::vec2[vertexCount]);
        ASSERT_SUCCEEDED(vbr.Read(texcoord1.get(), glTF::Primitive::kTexcoord1, vertexCount));
    }

    if (HasTangents)
    {
        tangent.reset(new glm::vec4[vertexCount]);
        ASSERT_SUCCEEDED(vbr.Read(tangent.get(), glTF::Primitive::kTangent, vertexCount));
    }
    else
    {
        ASSERT(maxIndex < vertexCount);
        ASSERT(indexCount % 3 == 0);

        bool hr = true;

        if (HasUV0 && material.normalUV == 0)
        {
            tangent.reset(new glm::vec4[vertexCount]);
            if (b32BitIndices)
            {
                hr = ComputeTangentFrame((uint32_t *)indices, indexCount / 3, position.get(), normal.get(),
                                         texcoord0.get(), vertexCount, tangent.get());
            }
            else
            {
                hr = ComputeTangentFrame((uint16_t *)indices, indexCount / 3, position.get(), normal.get(),
                                         texcoord0.get(), vertexCount, tangent.get());
            }
        }
        else if (HasUV1 && material.normalUV == 1)
        {
            tangent.reset(new glm::vec4[vertexCount]);
            if (b32BitIndices)
            {
                hr = ComputeTangentFrame((uint32_t *)indices, indexCount / 3, position.get(), normal.get(),
                                         texcoord1.get(), vertexCount, tangent.get());
            }
            else
            {
                hr = ComputeTangentFrame((uint16_t *)indices, indexCount / 3, position.get(), normal.get(),
                                         texcoord1.get(), vertexCount, tangent.get());
            }
        }

        ASSERT_SUCCEEDED(hr, "Error generating a tangent frame");
    }

    if (HasSkin)
    {
        joints.reset(new glm::vec4[vertexCount]);
        weights.reset(new glm::vec4[vertexCount]);
        ASSERT_SUCCEEDED(vbr.Read(joints.get(), glTF::Primitive::kJoints0, vertexCount));
        ASSERT_SUCCEEDED(vbr.Read(weights.get(), glTF::Primitive::kWeights0, vertexCount));
    }

    // Use VBWriter to generate a new, interleaved and compressed vertex buffer
    std::vector<vk::VertexInputAttributeDescription> OutputElements;

    outPrim.psoFlags = PSOFlags::kHasPosition | PSOFlags::kHasNormal;
    uint32_t inputOffset = 0;
    OutputElements.push_back({0, 0, vk::Format::eR32G32B32Sfloat, inputOffset}); // position
    inputOffset += vk::blockSize(vk::Format::eR32G32B32Sfloat);
    OutputElements.push_back({1, 0, vk::Format::eA2B10G10R10SnormPack32, inputOffset}); // normal
    inputOffset += vk::blockSize(vk::Format::eA2B10G10R10SnormPack32);
    if (tangent.get())
    {
        OutputElements.push_back({2, 0, vk::Format::eA2B10G10R10SnormPack32, inputOffset}); // tangent
        inputOffset += vk::blockSize(vk::Format::eA2B10G10R10SnormPack32);
        outPrim.psoFlags |= PSOFlags::kHasTangent;
    }
    if (texcoord0.get())
    {
        OutputElements.push_back({3, 0, vk::Format::eR16G16Sfloat, inputOffset}); // texcoord
        inputOffset += vk::blockSize(vk::Format::eR16G16Sfloat);
        outPrim.psoFlags |= PSOFlags::kHasUV0;
    }
    if (texcoord1.get())
    {
        OutputElements.push_back({4, 0, vk::Format::eR16G16Sfloat, inputOffset}); // texcoord
        inputOffset += vk::blockSize(vk::Format::eR16G16Sfloat);
        outPrim.psoFlags |= PSOFlags::kHasUV1;
    }
    if (HasSkin)
    {
        OutputElements.push_back({5, 0, vk::Format::eR16G16B16A16Uint, inputOffset});
        inputOffset += vk::blockSize(vk::Format::eR16G16B16A16Uint);
        OutputElements.push_back({6, 0, vk::Format::eR16G16B16A16Unorm, inputOffset});
        inputOffset += vk::blockSize(vk::Format::eR16G16B16A16Unorm);
        outPrim.psoFlags |= PSOFlags::kHasSkin;
    }
    if (material.alphaBlend)
        outPrim.psoFlags |= PSOFlags::kAlphaBlend;
    if (material.alphaTest)
        outPrim.psoFlags |= PSOFlags::kAlphaTest;
    if (material.twoSided)
        outPrim.psoFlags |= PSOFlags::kTwoSided;

    std::vector<vk::VertexInputAttributeDescription> layout = OutputElements;

    VBWriter vbw;
    vbw.Initialize(layout);

    uint32_t offsets[10];
    uint32_t strides[32];
    ComputeInputLayout(layout, offsets, strides);
    uint32_t stride = strides[0];

    outPrim.VB = std::make_shared<std::vector<uint8_t>>(stride * vertexCount);
    ASSERT_SUCCEEDED(vbw.AddStream(outPrim.VB->data(), vertexCount, 0, stride));

    vbw.Write(position.get(), 0, vertexCount);
    vbw.Write(normal.get(), 1, vertexCount); // TODO: why here x2bias?
    if (tangent.get())
        vbw.Write(tangent.get(), 2, vertexCount); // TODO: why here x2bias?
    if (texcoord0.get())
        vbw.Write(texcoord0.get(), 3, vertexCount);
    if (texcoord1.get())
        vbw.Write(texcoord1.get(), 4, vertexCount);
    if (HasSkin)
    {
        vbw.Write(joints.get(), 5, vertexCount);
        vbw.Write(weights.get(), 6, vertexCount);
    }

    // Now write a VB for positions only (or positions and UV when alpha testing)
    uint32_t depthStride = 12;
    std::vector<vk::VertexInputAttributeDescription> DepthElements;
    inputOffset = 0;
    DepthElements.push_back({0, 0, vk::Format::eR32G32B32Sfloat, inputOffset}); // position
    inputOffset += vk::blockSize(vk::Format::eR32G32B32Sfloat);
    if (material.alphaTest)
    {
        depthStride += 4;
        DepthElements.push_back({1, 0, vk::Format::eR16G16Sfloat, inputOffset}); // texcoord
        inputOffset += vk::blockSize(vk::Format::eR16G16Sfloat);
    }
    if (HasSkin)
    {
        depthStride += 16;
        DepthElements.push_back({2, 0, vk::Format::eR16G16B16A16Uint, inputOffset});
        inputOffset += vk::blockSize(vk::Format::eR16G16B16A16Uint);
        DepthElements.push_back({3, 0, vk::Format::eR16G16B16A16Unorm, inputOffset});
        inputOffset += vk::blockSize(vk::Format::eR16G16B16A16Unorm);
    }

    VBWriter dvbw;
    dvbw.Initialize(DepthElements);

    outPrim.DepthVB = std::make_shared<std::vector<uint8_t>>(depthStride * vertexCount);
    ASSERT_SUCCEEDED(dvbw.AddStream(outPrim.DepthVB->data(), vertexCount, 0, depthStride));

    dvbw.Write(position.get(), 0, vertexCount);
    if (material.alphaTest)
    {
        dvbw.Write(material.baseColorUV ? texcoord1.get() : texcoord0.get(), 1, vertexCount);
    }
    if (HasSkin)
    {
        dvbw.Write(joints.get(), 2, vertexCount);
        dvbw.Write(weights.get(), 3, vertexCount);
    }

    ASSERT(material.index < 0x8000, "Only 15-bit material indices allowed");

    outPrim.vertexStride = (uint16_t)stride;
    outPrim.index32 = b32BitIndices ? 1 : 0;
    outPrim.materialIdx = material.index;

    outPrim.primCount = indexCount;

    // TODO:  Generate optimized depth-only streams
}
