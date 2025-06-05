#include "Texture.h"
#include "CommandContext.h"
#include "GpuBuffer.h"
#include "GraphicsCore.h"
#include <ktxvulkan.h>
#include <vulkan/vulkan_enums.hpp>

using namespace Graphics;

void Texture::Create2D(uint32_t RowLength, uint32_t Width, uint32_t Height, vk::Format Format, const void *InitData)
{
    Destroy();

    // standard 2d image
    m_Extent.width = Width;
    m_Extent.height = Height;
    m_Extent.depth = 1;
    m_Format = Format;
    m_MipLevel = 1;
    m_NumLayers = 1;

    m_Layout = vk::ImageLayout::eUndefined;

    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = m_Extent;
    imageInfo.mipLevels = m_MipLevel;
    imageInfo.arrayLayers = m_NumLayers;
    imageInfo.format = Format;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = m_Layout;
    imageInfo.usage = m_ImageUsage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.samples = vk::SampleCountFlagBits::e1;

    vma::AllocationCreateInfo allocInfo;
    allocInfo.usage = vma::MemoryUsage::eGpuOnly;
    std::tie(m_Image, m_Allocation) = g_Allocator.createImage(imageInfo, allocInfo);

    m_SubresourceRange.aspectMask = m_AspectMask;
    m_SubresourceRange.baseMipLevel = 0;
    m_SubresourceRange.levelCount = m_MipLevel;
    m_SubresourceRange.baseArrayLayer = 0;
    m_SubresourceRange.layerCount = m_NumLayers;

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.setImage(m_Image);
    viewInfo.setViewType(vk::ImageViewType::e2D);
    viewInfo.setFormat(Format);
    viewInfo.setComponents(vk::ComponentMapping());
    viewInfo.subresourceRange = m_SubresourceRange;
    m_ImageView = g_Device.createImageView(viewInfo);

    // upload data
    size_t pixelSize = RowLength * Height;
    StagingBuffer stagingBuffer;
    stagingBuffer.Create(pixelSize);
    void *data = stagingBuffer.Map();
    memcpy(data, InitData, pixelSize);
    stagingBuffer.Unmap();

    // for the case of stride greater than width,
    // we should create multiple regions
    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(Height);
    for (int i = 0; i < Height; ++i)
    {
        vk::BufferImageCopy region;
        region.bufferOffset = RowLength * i;
        region.bufferRowLength = Width;
        region.bufferImageHeight = 1;
        region.imageSubresource.aspectMask = m_SubresourceRange.aspectMask;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = m_SubresourceRange.baseArrayLayer;
        region.imageSubresource.layerCount = m_SubresourceRange.layerCount;
        region.imageOffset = vk::Offset3D{0, i, 0};
        region.imageExtent = vk::Extent3D{Width, 1, 1};

        regions.push_back(region);
    }

    CommandContext::InitializeImage(*this, stagingBuffer, regions);

    stagingBuffer.Destroy();
}

void Texture::CreateCube(uint32_t RowLength, uint32_t Width, uint32_t Height, vk::Format Format, const void *InitData)
{
    Destroy();

    m_Extent.width = Width;
    m_Extent.height = Height;
    m_Extent.depth = 1;
    m_Format = Format;
    m_MipLevel = 1;
    m_NumLayers = 6;

    m_Layout = vk::ImageLayout::eUndefined;

    vk::ImageCreateInfo imageInfo;
    imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = m_Extent;
    imageInfo.mipLevels = m_MipLevel;
    imageInfo.arrayLayers = m_NumLayers;
    imageInfo.format = Format;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = m_Layout;
    imageInfo.usage = m_ImageUsage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.samples = vk::SampleCountFlagBits::e1;

    vma::AllocationCreateInfo allocInfo;
    allocInfo.usage = vma::MemoryUsage::eGpuOnly;
    std::tie(m_Image, m_Allocation) = g_Allocator.createImage(imageInfo, allocInfo);

    m_SubresourceRange.aspectMask = m_AspectMask;
    m_SubresourceRange.baseMipLevel = 0;
    m_SubresourceRange.levelCount = m_MipLevel;
    m_SubresourceRange.baseArrayLayer = 0;
    m_SubresourceRange.layerCount = m_NumLayers;

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.setImage(m_Image);
    viewInfo.setViewType(vk::ImageViewType::eCube);
    viewInfo.setFormat(Format);
    viewInfo.setComponents(vk::ComponentMapping());
    viewInfo.subresourceRange = m_SubresourceRange;
    m_ImageView = g_Device.createImageView(viewInfo);

    // upload data
    size_t pixelSize = RowLength * Height * 6;
    StagingBuffer stagingBuffer;
    stagingBuffer.Create(pixelSize);
    void *data = stagingBuffer.Map();
    memcpy(data, InitData, pixelSize);
    stagingBuffer.Unmap();

    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(Height * 6);
    for (int face = 0; face < 6; ++face)
    {
        // for the case of stride greater than width,
        // we should create multiple regions
        for (int i = 0; i < Height; ++i)
        {
            vk::BufferImageCopy region;
            region.bufferOffset = RowLength * Height * face + RowLength * i;
            region.bufferRowLength = Width;
            region.bufferImageHeight = 1;
            region.imageSubresource.aspectMask = m_SubresourceRange.aspectMask;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = vk::Offset3D{0, i, 0};
            region.imageExtent = vk::Extent3D{Width, 1, 1};

            regions.push_back(region);
        }
    }
    CommandContext::InitializeImage(*this, stagingBuffer, regions);

    stagingBuffer.Destroy();
}
//===================For KTX===================

/*
 * Pad nbytes to next multiple of n
 */
#define _KTX_PADN(n, nbytes) (ktx_uint32_t)(n * ceilf((float)(nbytes) / n))
/*
 * Calculate bytes of of padding needed to reach next multiple of n.
 */
/* Equivalent to (n * ceil(nbytes / n)) - nbytes */
#define _KTX_PADN_LEN(n, nbytes) (ktx_uint32_t)((n * ceilf((float)(nbytes) / n)) - (nbytes))

/*
 * Pad nbytes to KTX_GL_UNPACK_ALIGNMENT
 */
#define _KTX_PAD_UNPACK_ALIGN(nbytes) _KTX_PADN(KTX_GL_UNPACK_ALIGNMENT, nbytes)
/*
 * Calculate bytes of of padding needed to reach KTX_GL_UNPACK_ALIGNMENT.
 */
#define _KTX_PAD_UNPACK_ALIGN_LEN(nbytes) _KTX_PADN_LEN(KTX_GL_UNPACK_ALIGNMENT, nbytes)

// Recursive function to return the greatest common divisor of a and b.
uint32_t gcd(uint32_t a, uint32_t b)
{
    if (a == 0)
        return b;
    return gcd(b % a, a);
}
// Function to return the least common multiple of a & 4.
uint32_t lcm4(uint32_t a)
{
    if (!(a & 0x03))
        return a; // a is a multiple of 4.
    return (a * 4) / gcd(a, 4);
}

typedef struct user_cbdata_optimal
{
    VkBufferImageCopy *region; // Specify destination region in final image.
    VkDeviceSize offset;       // Offset of current level in staging buffer
    ktx_uint32_t numFaces;
    ktx_uint32_t numLayers;
    // The following are used only by optimalTilingPadCallback
    ktx_uint8_t *dest; // Pointer to mapped staging buffer.
    ktx_uint32_t elementSize;
    ktx_uint32_t numDimensions;
#if defined(_DEBUG)
    VkBufferImageCopy *regionsArrayEnd;
#endif
} user_cbdata_optimal;

//======================================================================
//  ReadImages callbacks
//======================================================================

/**
 * @internal
 * @~English
 * @brief Callback for optimally tiled textures with no source row padding.
 *
 * Images must be preloaded into the staging buffer. Each iteration, i.e.
 * the value of @p faceLodSize must be for a complete mip level, regardless of
 * texture type. This should be used only with @c ktx_Texture_IterateLevels.
 *
 * Sets up a region to copy the data from the staging buffer to the final
 * image.
 *
 * @note @p pixels is not used.
 *
 * @copydetails PFNKTXITERCB
 */
static KTX_error_code optimalTilingCallback(int miplevel, int face, int width, int height, int depth,
                                            ktx_uint64_t faceLodSize, void *pixels, void *userdata)
{
    user_cbdata_optimal *ud = (user_cbdata_optimal *)userdata;

    // Set up copy to destination region in final image
#if defined(_DEBUG)
    assert(ud->region < ud->regionsArrayEnd);
#endif
    ud->region->bufferOffset = ud->offset;
    ud->offset += faceLodSize;
    // These 2 are expressed in texels.
    ud->region->bufferRowLength = 0;
    ud->region->bufferImageHeight = 0;
    ud->region->imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ud->region->imageSubresource.mipLevel = miplevel;
    ud->region->imageSubresource.baseArrayLayer = face;
    ud->region->imageSubresource.layerCount = ud->numLayers * ud->numFaces;
    ud->region->imageOffset.x = 0;
    ud->region->imageOffset.y = 0;
    ud->region->imageOffset.z = 0;
    ud->region->imageExtent.width = width;
    ud->region->imageExtent.height = height;
    ud->region->imageExtent.depth = depth;

    ud->region += 1;

    return KTX_SUCCESS;
}

/**
 * @internal
 * @~English
 * @brief Callback for optimally tiled textures with possible source row
 *        padding.
 *
 * Copies data to the staging buffer removing row padding, if necessary.
 * Increments the offset for destination of the next copy increasing it to an
 * appropriate common multiple of the element size and 4 to comply with Vulkan
 * valid usage. Finally sets up a region to copy the face/lod from the staging
 * buffer to the final image.
 *
 * This longer method is needed because row padding is different between
 * KTX (pad to 4) and Vulkan (none). Also region->bufferOffset, i.e. the start
 * of each image, has to be a multiple of 4 and also a multiple of the
 * element size.
 *
 * This should be used with @c ktx_Texture_IterateFaceLevels or
 * @c ktx_Texture_IterateLoadLevelFaces. Face-level iteration has been
 * selected to minimize the buffering needed between reading the file and
 * copying the data into the staging buffer. Obviously when
 * @c ktx_Texture_IterateFaceLevels is being used, this is a moot point.
 *
 * @copydetails PFNKTXITERCB
 */
KTX_error_code optimalTilingPadCallback(int miplevel, int face, int width, int height, int depth,
                                        ktx_uint64_t faceLodSize, void *pixels, void *userdata)
{
    user_cbdata_optimal *ud = (user_cbdata_optimal *)userdata;
    ktx_uint32_t rowPitch = width * ud->elementSize;

    // Set bufferOffset in destination region in final image
#if defined(_DEBUG)
    assert(ud->region < ud->regionsArrayEnd);
#endif
    ud->region->bufferOffset = ud->offset;

    // Copy data into staging buffer
    if (_KTX_PAD_UNPACK_ALIGN_LEN(rowPitch) == 0)
    {
        // No padding. Can copy in bulk.
        memcpy(ud->dest + ud->offset, pixels, faceLodSize);
        ud->offset += faceLodSize;
    }
    else
    {
        // Must remove padding. Copy a row at a time.
        ktx_uint32_t image, imageIterations;
        ktx_int32_t row;
        ktx_uint32_t paddedRowPitch;

        if (ud->numDimensions == 3)
            imageIterations = depth;
        else if (ud->numLayers > 1)
            imageIterations = ud->numLayers * ud->numFaces;
        else
            imageIterations = 1;
        rowPitch = paddedRowPitch = width * ud->elementSize;
        paddedRowPitch = _KTX_PAD_UNPACK_ALIGN(paddedRowPitch);
        for (image = 0; image < imageIterations; image++)
        {
            for (row = 0; row < height; row++)
            {
                memcpy(ud->dest + ud->offset, pixels, rowPitch);
                ud->offset += rowPitch;
                pixels = (ktx_uint8_t *)pixels + paddedRowPitch;
            }
        }
    }

    // Round to needed multiples for next region, if necessary.
    if (ud->offset % ud->elementSize != 0 || ud->offset % 4 != 0)
    {
        // Only elementSizes of 1,2 and 3 will bring us here.
        assert(ud->elementSize < 4 && ud->elementSize > 0);
        ktx_uint32_t lcm = lcm4(ud->elementSize);
        ud->offset = _KTX_PADN(lcm, ud->offset);
    }
    // These 2 are expressed in texels; not suitable for dealing with padding.
    ud->region->bufferRowLength = 0;
    ud->region->bufferImageHeight = 0;
    ud->region->imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ud->region->imageSubresource.mipLevel = miplevel;
    ud->region->imageSubresource.baseArrayLayer = face;
    ud->region->imageSubresource.layerCount = ud->numLayers * ud->numFaces;
    ud->region->imageOffset.x = 0;
    ud->region->imageOffset.y = 0;
    ud->region->imageOffset.z = 0;
    ud->region->imageExtent.width = width;
    ud->region->imageExtent.height = height;
    ud->region->imageExtent.depth = depth;

    ud->region += 1;

    return KTX_SUCCESS;
}

bool Texture::CreateKTXFromMemory(const uint8_t *ktxData, size_t ktxDataSize, bool forceSRGB)
{
    ktxTexture *kTex;
    ktxResult result = ktxTexture_CreateFromMemory(ktxData, ktxDataSize, KTX_TEXTURE_CREATE_NO_FLAGS, &kTex);
    if (result != KTX_SUCCESS)
    {
        return false;
    }

    // Following are rewrite from ktx vulkan loader
    vk::ImageCreateInfo imageInfo;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = m_ImageUsage;
    vk::ImageViewCreateInfo viewInfo;

    uint32_t numImageLayers = kTex->numLayers;
    if (kTex->isCubemap)
    {
        numImageLayers *= 6;
        imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    }

    assert(kTex->numDimensions >= 1 && kTex->numDimensions <= 3);
    switch (kTex->numDimensions)
    {
    case 1:
        imageInfo.imageType = vk::ImageType::e1D;
        viewInfo.viewType = kTex->isArray ? vk::ImageViewType::e1DArray : vk::ImageViewType::e1D;
        break;
    case 2:
        [[fallthrough]];
    default:
        imageInfo.imageType = vk::ImageType::e2D;
        if (kTex->isCubemap)
        {
            viewInfo.viewType = kTex->isArray ? vk::ImageViewType::eCubeArray : vk::ImageViewType::eCube;
        }
        else
        {
            viewInfo.viewType = kTex->isArray ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
        }
        break;
    case 3:
        imageInfo.imageType = vk::ImageType::e3D;
        assert(!kTex->isArray);
        viewInfo.viewType = vk::ImageViewType::e3D;
        break;
    }

    vk::Format vkFormat = (vk::Format)ktxTexture_GetVkFormat(kTex);
    if (vkFormat == vk::Format::eUndefined)
    {
        return false;
    }

    if (kTex->generateMipmaps)
    {
        imageInfo.usage |= vk::ImageUsageFlagBits::eTransferSrc;
    }

    auto properties = g_PhysicalDevice.getImageFormatProperties(vkFormat, imageInfo.imageType, imageInfo.tiling,
                                                                imageInfo.usage, imageInfo.flags);
    if (kTex->numLayers > properties.maxArrayLayers)
    {
        return false;
    }

    vk::Filter blitFilter;
    uint32_t numImageLevels = 1;
    if (kTex->generateMipmaps)
    {
        uint32_t max_dim;
        auto formatProperties = g_PhysicalDevice.getFormatProperties(vkFormat);
        auto neededFeatures = vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eBlitSrc;
        if ((formatProperties.optimalTilingFeatures & neededFeatures) != neededFeatures)
        {
            return false;
        }
        if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)
        {
            blitFilter = vk::Filter::eLinear;
        }
        else
        {
            blitFilter = vk::Filter::eNearest;
        }
        max_dim = std::max(std::max(kTex->baseWidth, kTex->baseHeight), kTex->baseDepth);
        numImageLevels = std::floor(std::log2(max_dim)) + 1;
    }
    else
    {
        numImageLevels = kTex->numLevels;
    }
    if (numImageLevels > properties.maxMipLevels)
    {
        return false;
    }

    uint32_t elementSize = ktxTexture_GetElementSize(kTex);
    bool canUseFasterPath;
    if (kTex->classId == ktxTexture2_c)
    {
        canUseFasterPath = true;
    }
    else
    {
        ktx_uint32_t actualRowPitch = ktxTexture_GetRowPitch(kTex, 0);
        ktx_uint32_t tightRowPitch = elementSize * kTex->baseWidth;
        // If the texture's images do not have any row padding, we can use a
        // faster path. Only uncompressed textures might have padding.
        //
        // The first test in the if will match compressed textures, because
        // they all have a block size that is a multiple of 4, as well as
        // a class of uncompressed textures that will never need padding.
        //
        // The second test matches textures whose level 0 has no padding. Any
        // texture whose block size is not a multiple of 4 will need padding
        // at some miplevel even if level 0 does not. So, if more than 1 level
        // exists, we must use the slower path.
        //
        // Note all elementSizes > 4 Will be a multiple of 4, so only
        // elementSizes of 1, 2 & 3 are a concern here.
        if (elementSize % 4 == 0 /* There'll be no padding at any level. */
                                 /* There is no padding at level 0 and no other levels. */
            || (kTex->numLevels == 1 && actualRowPitch == tightRowPitch))
        {
            canUseFasterPath = true;
        }
        else
        {
            canUseFasterPath = false;
        }
    }

    m_Extent.width = kTex->baseWidth;
    m_Extent.height = kTex->baseHeight;
    m_Extent.depth = kTex->baseDepth;

    m_Layout = vk::ImageLayout::eUndefined;
    m_Format = vkFormat;
    m_MipLevel = numImageLevels;
    m_NumLayers = numImageLayers;

    // calc copy region first
    size_t textureSize = ktxTexture_GetDataSizeUncompressed(kTex);
    uint32_t numCopyRegions;
    if (canUseFasterPath)
    {
        /*
         * Because all array layers and faces are the same size they can
         * be copied in a single operation so there'll be 1 copy per mip
         * level.
         */
        numCopyRegions = kTex->numLevels;
    }
    else
    {
        /*
         * Have to copy all images individually into the staging
         * buffer so we can place them at correct multiples of
         * elementSize and 4 and also need a copy region per image
         * in case they end up with padding between them.
         */
        numCopyRegions = kTex->isArray ? kTex->numLevels : kTex->numLevels * kTex->numFaces;
        /*
         * Add extra space to allow for possible padding described
         * above. A bit ad-hoc but it's only a small amount of
         * memory.
         */
        textureSize += numCopyRegions * elementSize * 4;
    }

    // create staging buffer
    StagingBuffer stagingBuffer;
    stagingBuffer.Create(textureSize);
    uint8_t *mappedData = (uint8_t *)stagingBuffer.Map();

    std::vector<vk::BufferImageCopy> copyRegions(numCopyRegions);
    user_cbdata_optimal cbData;
    cbData.offset = 0;
    cbData.region = (VkBufferImageCopy *)copyRegions.data();
    cbData.numFaces = kTex->numFaces;
    cbData.numLayers = kTex->numLayers;
    cbData.dest = mappedData;
    cbData.elementSize = elementSize;
    cbData.numDimensions = kTex->numDimensions;
#if defined(_DEBUG)
    cbData.regionsArrayEnd = cbData.region + numCopyRegions;
#endif

    if (canUseFasterPath)
    {
        // Bulk load the data to the staging buffer and iterate
        // over levels.
        if (kTex->pData)
        {
            // Image data has already been loaded. Copy to staging
            // buffer.
            memcpy(mappedData, kTex->pData, kTex->dataSize);
        }
        else
        {
            /* Load the image data directly into the staging buffer. */
            /* The strange cast quiets an Xcode warning when building
             * for the Generic iOS Device where size_t is 32-bit even
             * when building for arm64. */
            auto allocInfo = stagingBuffer.GetAllocationInfo();
            result = ktxTexture_LoadImageData(kTex, mappedData, allocInfo.size);
            if (result != KTX_SUCCESS)
            {
                return false;
            }
        }
        // Iterate over mip levels to set up the copy regions.
        result = ktxTexture_IterateLevels(kTex, optimalTilingCallback, &cbData);
    }
    else
    {
        // Iterate over face-levels with callback that copies the
        // face-levels to Vulkan-valid offsets in the staging buffer while
        // removing padding. Using face-levels minimizes pre-staging-buffer
        // buffering, in the event the data is not already loaded.
        if (kTex->pData)
        {
            result = ktxTexture_IterateLevelFaces(kTex, optimalTilingPadCallback, &cbData);
        }
        else
        {
            result = ktxTexture_IterateLoadLevelFaces(kTex, optimalTilingPadCallback, &cbData);
            // XXX Check for possible errors.
        }
    }
    // remember to ummap staging buffer
    stagingBuffer.Unmap();

    imageInfo.extent = m_Extent;
    imageInfo.mipLevels = m_MipLevel;
    imageInfo.arrayLayers = m_NumLayers;
    imageInfo.format = m_Format;
    imageInfo.initialLayout = m_Layout;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.samples = vk::SampleCountFlagBits::e1;

    vma::AllocationCreateInfo allocInfo;
    allocInfo.usage = vma::MemoryUsage::eGpuOnly;
    std::tie(m_Image, m_Allocation) = g_Allocator.createImage(imageInfo, allocInfo);

    m_SubresourceRange.aspectMask = m_AspectMask;
    m_SubresourceRange.baseMipLevel = 0;
    m_SubresourceRange.levelCount = m_MipLevel;
    m_SubresourceRange.baseArrayLayer = 0;
    m_SubresourceRange.layerCount = m_NumLayers;

    viewInfo.setImage(m_Image);
    viewInfo.setFormat(m_Format);
    viewInfo.setComponents(vk::ComponentMapping());
    viewInfo.subresourceRange = m_SubresourceRange;
    m_ImageView = g_Device.createImageView(viewInfo);

    CommandContext::InitializeImage(*this, stagingBuffer, copyRegions);

    stagingBuffer.Destroy();

    return true;
}
