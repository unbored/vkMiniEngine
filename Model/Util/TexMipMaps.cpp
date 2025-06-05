#include "VulkanTex.h"

#include <glm/glm.hpp>
#include <glm/gtc/color_space.hpp>
#include <glm/gtc/packing.hpp>

constexpr bool ispow2(size_t x) { return ((x != 0) && !(x & (x - 1))); }

size_t CountMips(size_t width, size_t height)
{
    size_t mipLevles = 1;
    while (width > 1 || height > 1)
    {
        if (height > 1)
        {
            height >>= 1;
        }
        if (width > 1)
        {
            width >>= 1;
        }
        ++mipLevles;
    }
    return mipLevles;
}

bool CalculateMipLevels(size_t width, size_t height, size_t &mipLevels)
{
    if (mipLevels > 1)
    {
        const size_t maxMips = CountMips(width, height);
        if (mipLevels > maxMips)
        {
            return false;
        }
    }
    else if (mipLevels == 0)
    {
        mipLevels = CountMips(width, height);
    }
    else
    {
        mipLevels = 1;
    }
    return true;
}

bool LoadScanline(glm::vec4 *pDest, size_t count, const uint8_t *pSrc, size_t size, vk::Format format)
{
    // currently only support r8g8b8a8unorm
    switch (format)
    {
    case vk::Format::eR8G8B8A8Srgb:
    case vk::Format::eR8G8B8A8Unorm:
        if (size >= sizeof(glm::uint))
        {
            glm::vec4 *dPtr = pDest;
            glm::vec4 *ePtr = pDest + count;
            glm::uint *sPtr = (glm::uint *)pSrc;
            for (size_t i = 0; i < size; i += sizeof(glm::uint))
            {
                if (dPtr >= ePtr)
                    break;
                *dPtr++ = glm::unpackUnorm4x8(*sPtr++);
            }
            return true;
        }
        return false;
    default:
        return false;
    }
}

bool LoadScanlineLinear(glm::vec4 *pDest, size_t count, const uint8_t *pSrc, size_t size, vk::Format format,
                        TexFilterFlags flags)
{
    if (!pDest || !pSrc || !count || !size)
    {
        return false;
    }
    switch (format)
    {
    case vk::Format::eR8G8B8A8Srgb:
        flags = TexFilterFlags(flags | TexFilterSrgb);
        break;
    case vk::Format::eR8G8B8A8Unorm:
        break;
    default:
        flags = TexFilterFlags(flags & (~TexFilterSrgb));
        break;
    }

    if (!LoadScanline(pDest, count, pSrc, size, format))
    {
        return false;
    }
    if (flags & TexFilterSrgbIn)
    {
        glm::vec4 *ptr = pDest;
        for (size_t i = 0; i < count; ++i, ++ptr)
        {
            *ptr = glm::convertSRGBToLinear(*ptr);
        }
    }
    return true;
}
bool StoreScanline(const uint8_t *pDest, size_t size, vk::Format format, glm::vec4 *pSrc, size_t count)
{
    // currently only support r8g8b8a8unorm
    switch (format)
    {
    case vk::Format::eR8G8B8A8Srgb:
    case vk::Format::eR8G8B8A8Unorm:
        if (size >= sizeof(glm::uint))
        {
            glm::vec4 *sPtr = pSrc;
            glm::vec4 *ePtr = pSrc + count;
            glm::uint *dPtr = (glm::uint *)pDest;
            for (size_t i = 0; i < size; i += sizeof(glm::uint))
            {
                if (sPtr >= ePtr)
                    break;
                *dPtr++ = glm::packUnorm4x8(*sPtr++);
            }
            return true;
        }
        return false;
    default:
        return false;
    }
}
bool StoreScanlineLinear(const uint8_t *pDest, size_t size, vk::Format format, glm::vec4 *pSrc, size_t count,
                         TexFilterFlags flags)
{
    if (!pDest || !pSrc || !count || !size)
    {
        return false;
    }
    switch (format)
    {
    case vk::Format::eR8G8B8A8Srgb:
        flags = TexFilterFlags(flags | TexFilterSrgb);
        break;
    case vk::Format::eR8G8B8A8Unorm:
        break;
    default:
        flags = TexFilterFlags(flags & (~TexFilterSrgb));
        break;
    }
    if (flags & TexFilterSrgbOut)
    {
        glm::vec4 *ptr = pSrc;
        for (size_t i = 0; i < count; ++i, ++ptr)
        {
            *ptr = glm::convertLinearToSRGB(*ptr);
        }
    }
    return StoreScanline(pDest, size, format, pSrc, count);
}

bool Generate2DMipsBoxFilter(int texWidth, int texHeight, vk::Format format, size_t levels, TexFilterFlags filter,
                             std::vector<std::unique_ptr<uint8_t[]>> &mipChain)
{
    if (!ispow2(texWidth) || !ispow2(texHeight))
    {
        return false;
    }
    auto scanline = std::make_unique<glm::vec4[]>(texWidth * 3);
    if (!scanline)
    {
        return false;
    }

    size_t width = texWidth;
    size_t height = texHeight;

    // temporary data
    glm::vec4 *target = scanline.get();
    glm::vec4 *urow0 = target + width;
    glm::vec4 *urow1 = target + width * 2;
    const glm::vec4 *urow2 = urow0 + 1;
    const glm::vec4 *urow3 = urow1 + 1;

    // Resize base image to each target mip level
    for (size_t level = 1; level < levels; ++level)
    {
        if (height <= 1)
        {
            urow1 = urow0;
        }
        if (width <= 1)
        {
            urow2 = urow0;
            urow3 = urow1;
        }
        const uint8_t *pSrc = mipChain[level - 1].get();
        const uint8_t *pDest = mipChain[level].get();

        if (!pSrc || !pDest)
        {
            return false;
        }

        const size_t srcRowPitch = width * 4;

        const size_t nwidth = width > 1 ? width >> 1 : 1;
        const size_t nheight = height > 1 ? height >> 1 : 1;

        const size_t destRowPitch = nwidth * 4;

        for (size_t y = 0; y < nheight; ++y)
        {
            if (!LoadScanlineLinear(urow0, width, pSrc, srcRowPitch, format, filter))
            {
                return false;
            }
            pSrc += srcRowPitch;

            if (urow0 != urow1)
            {
                if (!LoadScanlineLinear(urow1, width, pSrc, srcRowPitch, format, filter))
                {
                    return false;
                }
                pSrc += srcRowPitch;
            }

            for (size_t x = 0; x < nwidth; ++x)
            {
                const size_t x2 = x << 1; // next level is half size, to address current level x should be double
                target[x] = (urow0[x2] + urow1[x2] + urow2[x2] + urow3[x2]) / glm::vec4(4);
            }
            if (!StoreScanlineLinear(pDest, destRowPitch, format, target, nwidth, filter))
                return false;
            pDest += destRowPitch;
        }
        if (height > 1)
            height >>= 1;

        if (width > 1)
            width >>= 1;
    }
    return true;
}

//-------------------------------------------------------------------------------------
// Linear filtering helpers
//-------------------------------------------------------------------------------------

struct LinearFilter
{
    size_t u0;
    float weight0;
    size_t u1;
    float weight1;
};

inline void CreateLinearFilter(size_t source, size_t dest, bool wrap, LinearFilter *lf) noexcept
{
    assert(source > 0);
    assert(dest > 0);
    assert(lf != nullptr);

    const float scale = float(source) / float(dest);

    // Mirror is the same case as clamp for linear

    for (size_t u = 0; u < dest; ++u)
    {
        const float srcB = (float(u) + 0.5f) * scale + 0.5f;

        int64_t isrcB = int64_t(srcB);
        int64_t isrcA = isrcB - 1;

        const float weight = 1.0f + float(isrcB) - srcB;

        if (isrcA < 0)
        {
            isrcA = (wrap) ? (int64_t(source) - 1) : 0;
        }

        if (size_t(isrcB) >= source)
        {
            isrcB = (wrap) ? 0 : (int64_t(source) - 1);
        }

        auto &entry = lf[u];
        entry.u0 = size_t(isrcA);
        entry.weight0 = weight;

        entry.u1 = size_t(isrcB);
        entry.weight1 = 1.0f - weight;
    }
}

bool Generate2DMipsLinearFilter(int texWidth, int texHeight, vk::Format format, size_t levels, TexFilterFlags filter,
                                std::vector<std::unique_ptr<uint8_t[]>> &mipChain)
{
    size_t width = texWidth;
    size_t height = texHeight;

    auto scanline = std::make_unique<glm::vec4[]>(texWidth * 3);
    if (!scanline)
    {
        return false;
    }

    std::unique_ptr<LinearFilter[]> lf(new (std::nothrow) LinearFilter[width + height]);
    if (!lf)
        return false;

    LinearFilter *lfX = lf.get();
    LinearFilter *lfY = lf.get() + width;

    // temporary data
    glm::vec4 *target = scanline.get();
    glm::vec4 *row0 = target + width;
    glm::vec4 *row1 = target + width * 2;

    // Resize base image to each target mip level
    for (size_t level = 1; level < levels; ++level)
    {
        const uint8_t *pSrc = mipChain[level - 1].get();
        const uint8_t *pDest = mipChain[level].get();

        if (!pSrc || !pDest)
        {
            return false;
        }

        const size_t srcRowPitch = width * 4;

        const size_t nwidth = width > 1 ? width >> 1 : 1;
        CreateLinearFilter(width, nwidth, (filter & TexFilterWrapU) != 0, lfX);
        const size_t nheight = height > 1 ? height >> 1 : 1;
        CreateLinearFilter(height, nheight, (filter & TexFilterWrapV) != 0, lfY);

        const size_t destRowPitch = nwidth * 4;

        size_t u0 = size_t(-1);
        size_t u1 = size_t(-1);

        for (size_t y = 0; y < nheight; ++y)
        {
            auto const &toY = lfY[y];

            if (toY.u0 != u0)
            {
                if (toY.u0 != u1)
                {
                    u0 = toY.u0;

                    if (!LoadScanlineLinear(row0, width, pSrc + (srcRowPitch * u0), srcRowPitch, format, filter))
                        return false;
                }
                else
                {
                    u0 = u1;
                    u1 = size_t(-1);

                    std::swap(row0, row1);
                }
            }

            if (toY.u1 != u1)
            {
                u1 = toY.u1;

                if (!LoadScanlineLinear(row1, width, pSrc + (srcRowPitch * u1), srcRowPitch, format, filter))
                    return false;
            }

            for (size_t x = 0; x < nwidth; ++x)
            {
                auto const &toX = lfX[x];

                // BILINEAR_INTERPOLATE(target[x], toX, toY, row0, row1)
                target[x] = (row0[toX.u0] * toX.weight0 + row0[toX.u1] * toX.weight1) * toY.weight0 +
                            (row1[toX.u0] * toX.weight0 + row1[toX.u1] * toX.weight1) * toY.weight1;
            }

            if (!StoreScanlineLinear(pDest, destRowPitch, format, target, nwidth, filter))
                return false;
            pDest += destRowPitch;
        }

        if (height > 1)
            height >>= 1;

        if (width > 1)
            width >>= 1;
    }

    return true;
}

bool GenerateMipMaps(const uint8_t *image, int width, int height, vk::Format format, TexFilterFlags filter,
                     size_t levels, std::vector<std::unique_ptr<uint8_t[]>> &mipChain)
{
    if (!image || format == vk::Format::eUndefined)
    {
        return false;
    }
    if (!CalculateMipLevels(width, height, levels))
    {
        return false;
    }
    if (levels <= 1)
    {
        return false;
    }

    unsigned long filter_select = (filter & TexFilterModeMask);
    if (!filter_select)
    {
        filter_select = (ispow2(width) && ispow2(height)) ? TexFilterBox : TexFilterLinear;
    }

    // alloc memory for levels
    int w = width, h = height;
    mipChain.resize(levels);
    for (size_t i = 0; i < levels; ++i)
    {
        mipChain[i].reset(new uint8_t[w * h * 4]);
        memset(mipChain[i].get(), 0, w * h * 4);

        if (w > 1)
        {
            w >>= 1;
        }
        if (h > 1)
        {
            h >>= 1;
        }
    }

    // copy the original pixels to level 0
    memcpy(mipChain[0].get(), image, width * height * 4);

    bool ret = false;
    switch (filter_select)
    {
    case TexFilterBox:
        ret = Generate2DMipsBoxFilter(width, height, format, levels, filter, mipChain);
        if (!ret)
        {
            mipChain.clear();
        }
        return ret;
    case TexFilterNearest:
        return false;
    case TexFilterLinear:
        ret = Generate2DMipsLinearFilter(width, height, format, levels, filter, mipChain);
        if (!ret)
        {
            mipChain.clear();
        }
        return ret;
    default:
        return false;
    }
}