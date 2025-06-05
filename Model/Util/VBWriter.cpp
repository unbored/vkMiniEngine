#include "VulkanMesh.h"

#include <Utility.h>
#include <glm/gtc/packing.hpp>
#include <map>

constexpr size_t c_MaxBinding = 32;
constexpr size_t c_MaxStride = 2048;

class VBWriter::Impl
{
public:
    Impl() noexcept : mStrides{}, mBuffers{}, mVerts{}, mDefaultStrides{}, mTempSize(0) {}

    bool Initialize(const std::vector<vk::VertexInputAttributeDescription> &desc);
    bool AddStream(void *vb, size_t nVerts, size_t binding, size_t stride);
    bool Write(glm::vec4 *buffer, uint32_t location, size_t count) const;

    void Release() noexcept
    {
        mInputDesc.clear();
        mLocations.clear();
        memset(mStrides, 0, sizeof(mStrides));
        memset(mBuffers, 0, sizeof(mBuffers));
        memset(mVerts, 0, sizeof(mVerts));
        memset(mDefaultStrides, 0, sizeof(mDefaultStrides));
        mTempBuffer.reset();
    }

    glm::vec4 *GetTemporaryBuffer(size_t count) const noexcept
    {
        // alloc enough space for temp buffer
        if (!mTempBuffer || mTempSize < count)
        {
            mTempSize = count;

            for (size_t j = 0; j < c_MaxBinding; ++j)
            {
                if (mVerts[j] > mTempSize)
                    mTempSize = mVerts[j];
            }

            auto temp = std::make_unique<glm::vec4[]>(mTempSize);
            if (!temp)
            {
                mTempSize = 0;
            }
            mTempBuffer.swap(temp);
        }
        return mTempBuffer.get();
    }

private:
    // location versus index
    using LocationMap = std::map<uint32_t, uint32_t>;

    std::vector<vk::VertexInputAttributeDescription> mInputDesc;
    LocationMap mLocations;
    uint32_t mStrides[c_MaxBinding];
    const void *mBuffers[c_MaxBinding];
    size_t mVerts[c_MaxBinding];
    uint32_t mDefaultStrides[c_MaxBinding];
    mutable size_t mTempSize;
    mutable std::unique_ptr<glm::vec4[]> mTempBuffer;
};

bool VBWriter::Impl::Initialize(const std::vector<vk::VertexInputAttributeDescription> &desc)
{
    Release();
    uint32_t offsets[c_MaxBinding] = {};

    // TODO: Check for desc valid

    ComputeInputLayout(desc, offsets, mDefaultStrides);

    for (size_t i = 0; i < desc.size(); ++i)
    {
        // TODO: Instance data not supported yet in DXMesh

        mInputDesc.push_back(desc[i]);
        mInputDesc[i].offset = offsets[i];
        mLocations[desc[i].location] = i;
    }

    return true;
}

bool VBWriter::Impl::AddStream(void *vb, size_t nVerts, size_t binding, size_t stride)
{
    if (!vb || nVerts == 0)
        return false;
    if (nVerts >= UINT32_MAX)
        return false;
    if (binding >= c_MaxBinding)
        return false;
    if (stride >= c_MaxStride)
        return false;

    mStrides[binding] = (stride > 0) ? stride : mDefaultStrides[binding];
    mBuffers[binding] = vb;
    mVerts[binding] = nVerts;

    return true;
}

//-------------------------------------------------------------------------------------
#define STORE_VERTS(type)                                                                                              \
    for (size_t icount = 0; icount < count; ++icount)                                                                  \
    {                                                                                                                  \
        if ((ptr + sizeof(type)) > eptr)                                                                               \
            return false;                                                                                              \
        *(type *)ptr = *buffer++;                                                                                      \
        ptr += stride;                                                                                                 \
    }                                                                                                                  \
    break;

#define STORE_PACK(type, func)                                                                                         \
    for (size_t icount = 0; icount < count; ++icount)                                                                  \
    {                                                                                                                  \
        if ((ptr + sizeof(type)) > eptr)                                                                               \
            return false;                                                                                              \
        type data = func(*buffer++);                                                                                   \
        *(type *)ptr = data;                                                                                           \
        ptr += stride;                                                                                                 \
    }                                                                                                                  \
    break;

#define LOAD_UNPACKF(type, func)                                                                                       \
    for (size_t icount = 0; icount < count; ++icount)                                                                  \
    {                                                                                                                  \
        if ((ptr + sizeof(type)) > eptr)                                                                               \
            return false;                                                                                              \
        glm::vec4 v = *buffer++;                                                                                       \
        type data = func(v.x);                                                                                         \
        *(type *)ptr = data;                                                                                           \
        ptr += stride;                                                                                                 \
    }                                                                                                                  \
    break;

bool VBWriter::Impl::Write(glm::vec4 *buffer, uint32_t location, size_t count) const
{
    if (!buffer || count == 0)
        return false;

    auto iter = mLocations.find(location);
    if (iter == mLocations.end())
    {
        return false;
    }

    const uint32_t binding = mInputDesc[iter->second].binding;

    auto vb = (uint8_t *)mBuffers[binding];
    if (!vb)
    {
        return false;
    }
    if (count > mVerts[binding])
    {
        return false;
    }
    const uint32_t stride = mStrides[binding];
    if (stride == 0)
    {
        return false;
    }

    const uint8_t *eptr = vb + stride * mVerts[binding];
    const uint8_t *ptr = vb + mInputDesc[iter->second].offset; // offset should be 0

    // copy data from ptr(mBuffers) to buffer until exceeds eptr
    switch (mInputDesc[iter->second].format)
    {
    case vk::Format::eR32G32B32A32Sfloat:
        STORE_VERTS(glm::vec4)
    case vk::Format::eR32G32B32A32Sint:
        STORE_VERTS(glm::ivec4)
    case vk::Format::eR32G32B32A32Uint:
        STORE_VERTS(glm::uvec4)
    case vk::Format::eR32G32B32Sfloat:
        STORE_VERTS(glm::vec3)
    case vk::Format::eR32G32B32Sint:
        STORE_VERTS(glm::ivec3)
    case vk::Format::eR32G32B32Uint:
        STORE_VERTS(glm::uvec3)
    case vk::Format::eR16G16B16A16Sfloat:
        STORE_PACK(glm::uint64, glm::packHalf4x16)
    case vk::Format::eR16G16B16A16Sint:
        STORE_VERTS(glm::i16vec4)
    case vk::Format::eR16G16B16A16Snorm:
        STORE_PACK(glm::uint64, glm::packSnorm4x16)
    case vk::Format::eR16G16B16A16Uint:
        STORE_VERTS(glm::u16vec4)
    case vk::Format::eR16G16B16A16Unorm:
        STORE_PACK(glm::uint64, glm::packUnorm4x16)
    case vk::Format::eR32G32Sfloat:
        STORE_VERTS(glm::vec2)
    case vk::Format::eR32G32Sint:
        STORE_VERTS(glm::ivec2)
    case vk::Format::eR32G32Uint:
        STORE_VERTS(glm::uvec2)
    case vk::Format::eA2B10G10R10UintPack32:
        STORE_PACK(glm::uint32, glm::packU3x10_1x2)
    case vk::Format::eA2B10G10R10UnormPack32:
        STORE_PACK(glm::uint32, glm::packUnorm3x10_1x2)
    case vk::Format::eA2B10G10R10SnormPack32:
        STORE_PACK(glm::uint32, glm::packSnorm3x10_1x2)
    case vk::Format::eB10G11R11UfloatPack32:
        STORE_PACK(glm::uint32, glm::packF2x11_1x10)
    case vk::Format::eR8G8B8A8Sint:
        STORE_VERTS(glm::i8vec4)
    case vk::Format::eR8G8B8A8Snorm:
        STORE_PACK(glm::uint, glm::packSnorm4x8)
    case vk::Format::eR8G8B8A8Uint:
        STORE_VERTS(glm::u8vec4)
    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eB8G8R8A8Unorm: // TODO: swizzle
        STORE_PACK(glm::uint, glm::packUnorm4x8)
    case vk::Format::eR16G16Sfloat:
        STORE_PACK(glm::uint, glm::packHalf2x16)
    case vk::Format::eR16G16Sint:
        STORE_VERTS(glm::i16vec2)
    case vk::Format::eR16G16Snorm:
        STORE_PACK(glm::uint, glm::packSnorm2x16)
    case vk::Format::eR16G16Uint:
        STORE_VERTS(glm::u16vec2)
    case vk::Format::eR16G16Unorm:
        STORE_PACK(glm::uint, glm::packUnorm2x16)
    case vk::Format::eR32Sfloat:
        STORE_VERTS(glm::vec1)
    case vk::Format::eR32Sint:
        STORE_VERTS(glm::ivec1)
    case vk::Format::eR32Uint:
        STORE_VERTS(glm::uvec1)
    case vk::Format::eR8G8Sint:
        STORE_VERTS(glm::i8vec2)
    case vk::Format::eR8G8Snorm:
        STORE_PACK(glm::uint16, glm::packSnorm2x8)
    case vk::Format::eR8G8Uint:
        STORE_VERTS(glm::u8vec2)
    case vk::Format::eR8G8Unorm:
        STORE_PACK(glm::uint16, glm::packUnorm2x8)
    case vk::Format::eR16Sfloat:
        LOAD_UNPACKF(glm::uint16, glm::packHalf1x16)
    case vk::Format::eR16Sint:
        STORE_VERTS(glm::i16vec1)
    case vk::Format::eR16Snorm:
        LOAD_UNPACKF(glm::uint16, glm::packSnorm1x16)
    case vk::Format::eR16Uint:
        STORE_VERTS(glm::u16vec1)
    case vk::Format::eR16Unorm:
        LOAD_UNPACKF(glm::uint16, glm::packUnorm1x16)
    case vk::Format::eR8Sint:
        STORE_VERTS(glm::i8vec1)
    case vk::Format::eR8Snorm:
        LOAD_UNPACKF(glm::uint8, glm::packSnorm1x8)
    case vk::Format::eR8Uint:
        STORE_VERTS(glm::u8vec1)
    case vk::Format::eR8Unorm:
        LOAD_UNPACKF(glm::uint8, glm::packUnorm1x8)
    case vk::Format::eB5G6R5UnormPack16:
        STORE_PACK(glm::uint16, glm::packUnorm1x5_1x6_1x5)
    case vk::Format::eB5G5R5A1UnormPack16:
        STORE_PACK(glm::uint16, glm::packUnorm3x5_1x1)
    case vk::Format::eA4B4G4R4UnormPack16:
        STORE_PACK(glm::uint16, glm::packUnorm4x4)
    default:
        Utility::Printf("Writer Format unsupport: %d\n", mInputDesc[iter->second].format);
        return false;
    }

    return true;
}

VBWriter::VBWriter() noexcept(false) : pImpl(std::make_unique<Impl>()) {}

VBWriter::VBWriter(VBWriter &&) noexcept = default;
VBWriter &VBWriter::operator=(VBWriter &&) noexcept = default;
VBWriter::~VBWriter() = default;

bool VBWriter::Initialize(const std::vector<vk::VertexInputAttributeDescription> &desc)
{
    return pImpl->Initialize(desc);
}

bool VBWriter::AddStream(void *vb, size_t nVerts, size_t binding, size_t stride)
{
    return pImpl->AddStream(vb, nVerts, binding, stride);
}

bool VBWriter::Write(float *buffer, uint32_t location, size_t count) const
{
    auto temp = pImpl->GetTemporaryBuffer(count);
    if (!temp)
    {
        return false;
    }

    glm::vec4 *dptr = temp;
    const float *sptr = buffer;
    for (size_t i = 0; i < count; ++i)
    {
        const glm::vec4 v = glm::vec4(*sptr++, 0.0, 0.0, 0.0);
        *dptr++ = v;
    }

    return pImpl->Write(temp, location, count);
}

bool VBWriter::Write(glm::vec2 *buffer, uint32_t location, size_t count) const
{
    auto temp = pImpl->GetTemporaryBuffer(count);
    if (!temp)
    {
        return false;
    }

    glm::vec4 *dptr = temp;
    const glm::vec2 *sptr = buffer;
    for (size_t i = 0; i < count; ++i)
    {
        const glm::vec4 v = glm::vec4(*sptr++, 0.0, 0.0);
        *dptr++ = v;
    }

    return pImpl->Write(temp, location, count);
}

bool VBWriter::Write(glm::vec3 *buffer, uint32_t location, size_t count) const
{
    auto temp = pImpl->GetTemporaryBuffer(count);
    if (!temp)
    {
        return false;
    }

    glm::vec4 *dptr = temp;
    const glm::vec3 *sptr = buffer;
    for (size_t i = 0; i < count; ++i)
    {
        const glm::vec4 v = glm::vec4(*sptr++, 0.0);
        *dptr++ = v;
    }

    return pImpl->Write(temp, location, count);
}

bool VBWriter::Write(glm::vec4 *buffer, uint32_t location, size_t count) const
{
    return pImpl->Write(buffer, location, count);
}

void VBWriter::Release() noexcept { pImpl->Release(); }