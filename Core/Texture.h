#pragma once

#include "ImageView.h"

class Texture : public ImageView
{
public:
    Texture()
        : ImageView(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                    vk::ImageAspectFlagBits::eColor)
    {
    }
    void Create2D(uint32_t RowLength, uint32_t Width, uint32_t Height, vk::Format Format, const void *InitData);

    void CreateCube(uint32_t RowLength, uint32_t Width, uint32_t Height, vk::Format Format, const void *InitData);

    bool CreateKTXFromMemory(const uint8_t *ktxData, size_t ktxDataSize, bool forceSRGB);
};
