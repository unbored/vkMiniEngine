#include "SamplerManager.h"

#include "GraphicsCore.h"
#include "Hash.h"
#include <algorithm>
#include <float.h>
#include <map>

namespace
{
std::map<size_t, vk::Sampler> s_SamplerMap;
}

SamplerDesc::SamplerDesc()
{
    magFilter = vk::Filter::eLinear;
    minFilter = vk::Filter::eLinear;
    addressModeU = vk::SamplerAddressMode::eRepeat;
    addressModeV = vk::SamplerAddressMode::eRepeat;
    addressModeW = vk::SamplerAddressMode::eRepeat;
    anisotropyEnable = VK_TRUE;
    maxAnisotropy = 16;
    borderColor = vk::BorderColor::eFloatOpaqueWhite;
    unnormalizedCoordinates = VK_FALSE;
    compareEnable = VK_FALSE;
    compareOp = vk::CompareOp::eAlways;
    mipmapMode = vk::SamplerMipmapMode::eLinear;
    mipLodBias = 0.0f;
    minLod = 0.0f;
    maxLod = FLT_MAX;
}

void SamplerDesc::SetAddressMode(vk::SamplerAddressMode Mode)
{
    addressModeU = Mode;
    addressModeV = Mode;
    addressModeW = Mode;
}

vk::Sampler SamplerDesc::CreateSampler()
{
    size_t hash = Utility::HashState(this);
    auto iter = s_SamplerMap.find(hash);
    if (iter != s_SamplerMap.end())
    {
        return iter->second;
    }

    auto sampler = Graphics::g_Device.createSampler(*this);
    s_SamplerMap[hash] = sampler;
    return sampler;
}

void SamplerDesc::DestroyAll()
{
    for (auto &i : s_SamplerMap)
    {
        Graphics::g_Device.destroySampler(i.second);
    }
    s_SamplerMap.clear();
}