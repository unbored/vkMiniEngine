#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

enum TexFilterFlags : unsigned long
{
    TexFilterDefault = 0,

    // Wrap vs. Mirror vs. Clamp filtering options
    TexFilterWrapU = 0x1,
    TexFilterWrapV = 0x2,
    TexFilterWrapW = 0x4,
    TexFilterWrap = (TexFilterWrapU | TexFilterWrapV | TexFilterWrapW),
    TexFilterMirrorU = 0x10,
    TexFilterMirrorV = 0x20,
    TexFilterMirrorW = 0x40,
    TexFilterMirror = (TexFilterMirrorU | TexFilterMirrorV | TexFilterMirrorW),

    // Resize color and alpha channel independently
    TexFilterSeperateAlpha = 0x100,

    // Enable *2 - 1 conversion cases for unorm<->float and positive-only float formats
    TexFilterFloatX2Bias = 0x200,

    // When converting RGB to R, defaults to using grayscale. These flags indicate copying a specific channel instead
    // When converting RGB to RG, defaults to copying RED | GREEN. These flags control which channels are selected
    // instead.
    TexFilterRgbCopyRed = 0x1000,
    TexFilterRgbCopyGreen = 0x2000,
    TexFilterRgbCopyBlue = 0x4000,

    // Filtering mode to use for any required image resizing
    TexFilterNearest = 0x100000,
    TexFilterLinear = 0x200000,
    TexFilterCubic = 0x300000,
    TexFilterBox = 0x400000,
    TexFilterFant = 0x400000,
    TexFilterTriangle = 0x500000,

    // sRGB <-> RGB for use in conversion operations
    // if the input format type is IsSRGB(), then SRGB_IN is on by default
    // if the output format type is IsSRGB(), then SRGB_OUT is on by default
    TexFilterSrgbIn = 0x1000000,
    TexFilterSrgbOut = 0x2000000,
    TexFilterSrgb = (TexFilterSrgbIn | TexFilterSrgbOut),
};

constexpr unsigned long TexFilterModeMask = 0xF00000;
constexpr unsigned long TexFilterSrgbMask = 0xF000000;

bool GenerateMipMaps(const uint8_t *image, int width, int height, vk::Format format, TexFilterFlags filter,
                     size_t levels, std::vector<std::unique_ptr<uint8_t[]>> &mipChain);