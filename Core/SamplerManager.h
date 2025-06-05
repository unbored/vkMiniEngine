#pragma once

#include <vulkan/vulkan.hpp>

#include "Color.h"

class SamplerDesc : public vk::SamplerCreateInfo
{
public:
    SamplerDesc();

    void SetAddressMode(vk::SamplerAddressMode Mode);

    vk::Sampler CreateSampler();

    static void DestroyAll();
};