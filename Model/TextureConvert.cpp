#include "TextureConvert.h"

#include <vulkan/vulkan.hpp>

#include "Util/VulkanTex.h"
#include <Utility.h>
#include <cstddef>
#include <filesystem>
#include <ktx.h>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void CompileTextureOnDemand(const std::string &originalFile, uint32_t flags)
{
    namespace fs = std::filesystem;

    std::string ktxFile = Utility::RemoveExtension(originalFile) + ".ktx";

    fs::path ktxFilePath(ktxFile);
    fs::path srcFilePath(originalFile);

    bool srcFileMissing = !fs::exists(srcFilePath);
    bool ktxFileMissing = !fs::exists(ktxFilePath);

    if (srcFileMissing && ktxFileMissing)
    {
        Utility::Printf("Texture %s is missing.\n", Utility::RemoveBasePath(originalFile).c_str());
        return;
    }

    // If we can find the source texture and the ktx file is older, reconvert.
    if (ktxFileMissing || !srcFileMissing && fs::last_write_time(ktxFilePath) < fs::last_write_time(srcFilePath))
    {
        Utility::Printf("KTX texture %s missing or older than source.  Rebuilding.\n",
                        Utility::RemoveBasePath(originalFile).c_str());
        ConvertToKTX(originalFile, flags);
    }
}

bool ConvertToKTX(const std::string &filePath, uint32_t Flags)
{
    auto GetFlag = [](uint32_t Flags, TexConversionFlags f) { return (Flags & f) != 0; };
    bool bInterpretAsSRGB = GetFlag(Flags, kSRGB);
    bool bPreserveAlpha = GetFlag(Flags, kPreserveAlpha);
    bool bContainsNormals = GetFlag(Flags, kNormalMap);
    bool bBumpMap = GetFlag(Flags, kBumpToNormal);
    bool bBlockCompress = GetFlag(Flags, kDefaultBC);
    bool bUseBestBC = GetFlag(Flags, kQualityBC);
    bool bFlipImage = GetFlag(Flags, kFlipVertical);

    // Can't be both
    ASSERT(!bInterpretAsSRGB || !bContainsNormals);
    ASSERT(!bPreserveAlpha || !bContainsNormals);

    Utility::Printf("Converting file \"%s\" to KTX.\n", filePath.c_str());

    // Get extension as utf8 (ascii)
    std::string ext = Utility::ToLower(Utility::GetFileExtension(filePath));

    int width, height, channel;
    stbi_uc *pixels = nullptr;

    bool isKTX = false;
    bool isHDR = false;

    if (ext == "ktx")
    {
        Utility::Printf("Ignoring existing ktx \"%s\".\n", filePath.c_str());
        return false;
    }
    else if (ext == "exr")
    {
        Utility::Printf("OpenEXR not supported for this build of the content exporter\n");
        return false;
    }

    bool imageValid = stbi_info(filePath.c_str(), &width, &height, &channel);
    if (!imageValid)
    {
        Utility::Printf("Image not valid \"%s\".\n", filePath.c_str());
    }

    // set flip flag in advance
    if (bFlipImage)
    {
        stbi_set_flip_vertically_on_load(true);
    }

    // load image, always 4 channels
    isHDR = stbi_is_hdr(filePath.c_str());
    if (isHDR)
    {
        Utility::Printf("HDR format not supported yet\n");
        return false;
    }
    else
    {
        pixels = stbi_load(filePath.c_str(), &width, &height, &channel, 4);
    }

    if (!pixels)
    {
        Utility::Printf("Could not load texture \"%s\".\n", filePath.c_str());
        return false;
    }

    // TODO: check for texture dimension sizes

    vk::Format tformat;

    tformat = bInterpretAsSRGB ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    if (bBumpMap)
    {
        Utility::Printf("Currently not support bump map yet \"%s\".\n", filePath.c_str());
        return false;
    }

    std::vector<std::unique_ptr<uint8_t[]>> mipChain;

    if (!GenerateMipMaps(pixels, width, height, tformat, TexFilterDefault, 0, mipChain))
    {
        Utility::Printf("Failing generating mimaps for \"%s\".\n", filePath.c_str());
    }

    // create ktx file with mipmap
    ktxTexture1 *texture;
    ktxTextureCreateInfo createInfo;
    KTX_error_code result;
    ktx_size_t srcSize;

    createInfo.glInternalformat =
        (tformat == vk::Format::eR8G8B8A8Srgb) ? 0x8C43 : 0x8058; // GL_SRGB8_ALPHA8 : GL_RGBA8
    createInfo.vkFormat = (ktx_uint32_t)tformat;
    createInfo.baseWidth = width;
    createInfo.baseHeight = height;
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    // Note: it is not necessary to provide a full mipmap pyramid.
    // createInfo.numLevels = log2(createInfo.baseWidth) + 1;
    createInfo.numLevels = mipChain.size();
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE; // generate mipmaps

    result = ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);

    // TODO: process HDR files?
    int w = width, h = height;
    for (size_t i = 0; i < mipChain.size(); ++i)
    {
        result = ktxTexture_SetImageFromMemory(ktxTexture(texture), i, 0, 0, mipChain[i].get(), w * h * 4);
        if (result != KTX_SUCCESS)
        {
            Utility::Printf("Set KTX Image \"%s\" failed.\n", filePath.c_str());
        }
        if (w > 1)
        {
            w >>= 1;
        }
        if (h > 1)
        {
            h >>= 1;
        }
    }

    // ktxBasisParams params = {0};
    // params.structSize = sizeof(params);
    //// For BasisLZ/ETC1S
    // params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
    //// For UASTC
    // params.uastc = KTX_TRUE;

    //  Set other BasisLZ/ETC1S or UASTC params to change default quality settings.
    // result = ktxTexture2_CompressBasisEx(texture, &params);

    // Rename file extension to ktx
    const std::string dest = Utility::RemoveExtension(filePath) + ".ktx";

    char writer[100];
    snprintf(writer, sizeof(writer), "MiniEngine version 1.0");
    ktxHashList_AddKVPair(&texture->kvDataHead, KTX_WRITER_KEY, (ktx_uint32_t)strlen(writer) + 1, writer);
    ktxTexture_WriteToNamedFile(ktxTexture(texture), dest.c_str());
    ktxTexture_Destroy(ktxTexture(texture));

    stbi_image_free(pixels);

    return true;
}