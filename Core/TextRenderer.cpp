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
// Author:  James Stanard
//

#include "TextRenderer.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "CompiledShaders/TextAntialiasFrag.h"
#include "CompiledShaders/TextShadowFrag.h"
#include "CompiledShaders/TextVert.h"
#include "DescriptorSet.h"
#include "FileUtility.h"
#include "GraphicsCommon.h"
#include "GraphicsCore.h"
#include "PipelineState.h"
#include "RenderPass.h"
#include "SystemTime.h"
#include "Texture.h"
#include "Utility.h"
#include <cstdio>
#include <map>
#include <memory>
#include <string>

#include "Fonts/consola24.h"
#ifdef _MSC_VER
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include <tinyutf8/tinyutf8.h>

#include "Math/Common.h"

using namespace Graphics;
// using namespace Math;
using namespace std;

namespace TextRenderer
{
class Font
{
public:
    Font()
    {
        m_NormalizeXCoord = 0.0f;
        m_NormalizeYCoord = 0.0f;
        m_FontLineSpacing = 0.0f;
        m_AntialiasRange = 0.0f;
        m_FontHeight = 0;
        m_BorderSize = 0;
        m_TextureWidth = 0;
        m_TextureHeight = 0;
    }

    ~Font()
    {
        m_Dictionary.clear();
        m_Texture.Destroy();
    }

    // Load SDF font
    void LoadFromBinary(const char *fontName, const uint8_t *pBinary, const size_t binarySize)
    {
        (fontName);

        // We should at least use this to assert that we have a complete file
        (binarySize);

        struct FontHeader
        {
            char FileDescriptor[8]; // "SDFFONT\0"
            uint8_t majorVersion;   // '1'
            uint8_t minorVersion;   // '0'
            uint16_t borderSize;    // Pixel empty space border width
            uint16_t textureWidth;  // Width of texture buffer
            uint16_t textureHeight; // Height of texture buffer
            uint16_t fontHeight;    // Font height in 12.4
            uint16_t advanceY;      // Line height in 12.4
            uint16_t numGlyphs;     // Glyph count in texture
            uint16_t searchDist;    // Range of search space 12.4
        };

        FontHeader *header = (FontHeader *)pBinary;
        m_NormalizeXCoord = 1.0f / (header->textureWidth * 16);
        m_NormalizeYCoord = 1.0f / (header->textureHeight * 16);
        m_FontHeight = header->fontHeight;
        m_FontLineSpacing = (float)header->advanceY / (float)header->fontHeight;
        m_BorderSize = header->borderSize * 16;
        m_AntialiasRange = (float)header->searchDist / header->fontHeight;
        uint16_t textureWidth = header->textureWidth;
        uint16_t textureHeight = header->textureHeight;
        uint16_t NumGlyphs = header->numGlyphs;

        const char16_t *charList = (char16_t *)(pBinary + sizeof(FontHeader));
        const Glyph *glyphData = (Glyph *)(charList + NumGlyphs);
        const void *texelData = glyphData + NumGlyphs;

        for (uint16_t i = 0; i < NumGlyphs; ++i)
            m_Dictionary[charList[i]] = glyphData[i];

        m_Texture.Create2D(textureWidth, textureWidth, textureHeight, vk::Format::eR8Snorm, texelData);

        DEBUGPRINT("Loaded SDF font:  %ls (ver. %d.%d)", fontName, header->majorVersion, header->minorVersion);
    }

    bool Load(const string &fileName)
    {
        Utility::ByteArray ba = Utility::ReadFileSync(fileName);

        if (ba->size() == 0)
        {
            ERROR("Cannot open file %ls", fileName.c_str());
            return false;
        }

        LoadFromBinary(fileName.c_str(), ba->data(), ba->size());

        return true;
    }

    // Each character has an XY start offset, a width, and they all share the same
    // height
    struct Glyph
    {
        uint16_t x, y, w;
        int16_t bearing;
        uint16_t advance;
    };

    const Glyph *GetGlyph(char32_t ch) const
    {
        auto it = m_Dictionary.find(ch);
        return it == m_Dictionary.end() ? nullptr : &it->second;
    }

    // Get the texel height of the font in 12.4 fixed point
    uint16_t GetHeight(void) const { return m_FontHeight; }

    // Get the size of the border in 12.4 fixed point
    uint16_t GetBorderSize(void) const { return m_BorderSize; }

    // Get the line advance height given a certain font size
    float GetVerticalSpacing(float size) const { return size * m_FontLineSpacing; }

    // Get the texture object
    const Texture &GetTexture(void) const { return m_Texture; }

    float GetXNormalizationFactor() const { return m_NormalizeXCoord; }
    float GetYNormalizationFactor() const { return m_NormalizeYCoord; }

    // Get the range in terms of height values centered on the midline that
    // represents a pixel in screen space (according to the specified font size.)
    // The pixel alpha should range from 0 to 1 over the height range 0.5 +/- 0.5
    // * aaRange.
    float GetAntialiasRange(float size) const { return glm::max(1.0f, size * m_AntialiasRange); }

private:
    float m_NormalizeXCoord;
    float m_NormalizeYCoord;
    float m_FontLineSpacing;
    float m_AntialiasRange;
    uint16_t m_FontHeight;
    uint16_t m_BorderSize;
    uint16_t m_TextureWidth;
    uint16_t m_TextureHeight;
    Texture m_Texture;
    map<char32_t, Glyph> m_Dictionary;
};

map<string, unique_ptr<Font>> LoadedFonts;

const Font *GetOrLoadFont(const string &filename)
{
    auto fontIter = LoadedFonts.find(filename);
    if (fontIter != LoadedFonts.end())
        return fontIter->second.get();

    Font *newFont = new Font();
    if (filename == "default")
        newFont->LoadFromBinary("default", g_pconsola24, sizeof(g_pconsola24));
    else
        newFont->Load("Fonts/" + filename + ".fnt");
    LoadedFonts[filename].reset(newFont);
    return newFont;
}

// RootSignature s_RootSignature;
GraphicsPSO s_TextPSO[2] = {{"Text Render: Text R8G8B8A8_UNORM PSO"},
                            {"Text Render: Text R11G11B10_FLOAT PSO"}}; // 0: R8G8B8A8_UNORM   1: R11G11B10_FLOAT
GraphicsPSO s_ShadowPSO[2] = {{"Text Render: Shadow R8G8B8A8_UNORM PSO"},
                              {"Text Render: Shadow R11G11B10_FLOAT PSO"}}; // 0: R8G8B8A8_UNORM   1: R11G11B10_FLOAT

DescriptorSet s_TextDS;
// RenderPass::RenderPassDesc s_TextRP;

// Framebuffer s_TextFb;
// Framebuffer s_ShadowFb;

} // namespace TextRenderer

void TextRenderer::Initialize(void)
{
    // s_TextDS.AddStaticSampler(0, SamplerLinearClamp);
    // vk::DescriptorSetLayoutBinding binding =
    //{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr };
    // s_TextDS.AddBinding(binding);
    s_TextDS.AddBindings(0, 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);

    s_TextDS.AddPushConstant(0, sizeof(TextContext::VertexShaderParams), vk::ShaderStageFlagBits::eVertex);
    s_TextDS.AddPushConstant(sizeof(TextContext::VertexShaderParams), sizeof(TextContext::FragShaderParams),
                             vk::ShaderStageFlagBits::eFragment);

    s_TextDS.Finalize();

    // from gamecore we know g_OverlayBuffer has been cleaned and may drawn something on
    // s_TextRP.colors = {{g_OverlayBuffer.GetFormat(), vk::AttachmentLoadOp::eLoad}};

    //    s_TextFb.Create(s_TextRP, g_OverlayBuffer);

    s_TextPSO[0].SetInputLayout(
        VertexInputBindingAttribute{// binding, stride, inputRate
                                    {0, sizeof(TextContext::TextVert), vk::VertexInputRate::eInstance},
                                    {// location, binding, format, offset
                                     {0, 0, vk::Format::eR32G32Sfloat, offsetof(TextContext::TextVert, X)},
                                     {1, 0, vk::Format::eR16G16B16A16Uint, offsetof(TextContext::TextVert, U)}}});
    s_TextPSO[0].SetPrimitiveTopologyType(vk::PrimitiveTopology::eTriangleStrip);
    s_TextPSO[0].SetRasterizerState(RasterizerTwoSided);
    s_TextPSO[0].SetBlendState(BlendPreMultiplied);
    s_TextPSO[0].SetDepthStencilState(DepthStateDisabled);
    s_TextPSO[0].SetPipelineLayout(s_TextDS.GetPipelineLayout());
    s_TextPSO[0].SetRenderPassFormat(g_OverlayBuffer.GetFormat());
    s_TextPSO[0].SetVertexShader(g_TextVert, sizeof(g_TextVert));
    s_TextPSO[0].SetFragmentShader(g_TextAntialiasFrag, sizeof(g_TextAntialiasFrag));
    s_TextPSO[0].Finalize();

    s_TextPSO[1] = s_TextPSO[0];
    // s_TextPSO[1].SetRenderTarget(&g_SceneColorBuffer);
    s_TextPSO[1].Finalize();

    s_ShadowPSO[0] = s_TextPSO[0];
    s_ShadowPSO[0].SetFragmentShader(g_TextShadowFrag, sizeof(g_TextShadowFrag));
    s_ShadowPSO[0].Finalize();

    s_ShadowPSO[1] = s_ShadowPSO[0];
    // s_ShadowPSO[1].SetRenderTarget(&g_SceneColorBuffer);
    s_ShadowPSO[1].Finalize();
}

void TextRenderer::Shutdown(void)
{
    //    s_ShadowPSO[0].Destroy();
    //    s_ShadowPSO[1].Destroy();
    //    s_TextPSO[0].Destroy();
    //    s_TextPSO[1].Destroy();
    //    s_TextRP.Destroy();
    s_TextDS.Destroy();
    //    s_TextFb.Destroy();

    LoadedFonts.clear();
}

// void TextRenderer::Resize(size_t Width, size_t Height)
//{
//     s_TextFb.Destroy();
//     s_TextFb.Create(s_TextRP, g_OverlayBuffer);
// }

TextContext::TextContext(GraphicsContext &CmdContext, float ViewWidth, float ViewHeight) : m_Context(CmdContext)
{
    m_HDR = false;
    m_CurrentFont = nullptr;
    m_ViewWidth = ViewWidth;
    m_ViewHeight = ViewHeight;

    // Transform from text view space to clip space.
    const float vpX = 0.0f;
    const float vpY = 0.0f;
    const float twoDivW = 2.0f / ViewWidth; // -1 to 1 totals 2, normalize the coord
    const float twoDivH = 2.0f / ViewHeight;
    m_VSParams.ViewportTransform =
        // x scale, y scale(flip y), center offset x, center offset y
        glm::vec4(twoDivW, -twoDivH, -vpX * twoDivW - 1.0f, vpY * twoDivH + 1.0f);

    // The font texture dimensions are still unknown
    m_VSParams.NormalizeX = 1.0f;
    m_VSParams.NormalizeY = 1.0f;

    ResetSettings();
}

void TextContext::ResetSettings(void)
{
    m_EnableShadow = true;
    ResetCursor(0.0f, 0.0f);
    m_ShadowOffsetX = 0.05f;
    m_ShadowOffsetY = 0.05f;
    m_FSParams.ShadowHardness = 0.5f;
    m_FSParams.ShadowOpacity = 1.0f;
    m_FSParams.TextColor = Color(1.0f, 1.0f, 1.0f, 1.0f);

    m_VSConstantBufferIsStale = true;
    m_FSConstantBufferIsStale = true;
    m_TextureIsStale = true;

    SetFont("default", 24.0f);
}

void TextContext::SetLeftMargin(float x) { m_LeftMargin = x; }
void TextContext::SetCursorX(float x) { m_TextPosX = x; }
void TextContext::SetCursorY(float y) { m_TextPosY = y; }
void TextContext::NewLine(void)
{
    m_TextPosX = m_LeftMargin;
    m_TextPosY += m_LineHeight;
}
float TextContext::GetLeftMargin(void) { return m_LeftMargin; }
float TextContext::GetCursorX(void) { return m_TextPosX; }
float TextContext::GetCursorY(void) { return m_TextPosY; }

void TextContext::ResetCursor(float x, float y)
{
    m_LeftMargin = x;
    m_TextPosX = x;
    m_TextPosY = y;
}

void TextContext::EnableDropShadow(bool enable)
{
    if (m_EnableShadow == enable)
        return;

    m_EnableShadow = enable;

    m_Context.BindPipeline(m_EnableShadow ? TextRenderer::s_ShadowPSO[m_HDR] : TextRenderer::s_TextPSO[m_HDR]);
}

void TextContext::SetShadowOffset(float xPercent, float yPercent)
{
    m_ShadowOffsetX = xPercent;
    m_ShadowOffsetY = yPercent;
    m_FSParams.ShadowOffsetX = m_CurrentFont->GetHeight() * m_ShadowOffsetX * m_VSParams.NormalizeX;
    m_FSParams.ShadowOffsetY = m_CurrentFont->GetHeight() * m_ShadowOffsetY * m_VSParams.NormalizeY;
    m_FSConstantBufferIsStale = true;
}

void TextContext::SetShadowParams(float opacity, float width)
{
    m_FSParams.ShadowHardness = 1.0f / width;
    m_FSParams.ShadowOpacity = opacity;
    m_FSConstantBufferIsStale = true;
}

void TextContext::SetColor(Color c)
{
    m_FSParams.TextColor = c;
    m_FSConstantBufferIsStale = true;
}

float TextContext::GetVerticalSpacing(void) { return m_LineHeight; }

void TextContext::Begin(bool EnableHDR)
{
    ResetSettings();

    m_HDR = EnableHDR;

    m_Context.BeginRenderPass(g_OverlayBuffer);

    m_Context.BindPipeline(TextRenderer::s_ShadowPSO[m_HDR]);

    m_Context.SetDescriptorSet(TextRenderer::s_TextDS);
    // m_Context.SetPrimitiveTopology(vk::PrimitiveTopology::eTriangleStrip);
    // m_Context.BindDesriptorSet(TextRenderer::s_TextDS[g_CurrentBuffer],
    // TextRenderer::s_TextDS.GetPipelineLayout());
}

void TextContext::SetFont(const string &fontName, float size)
{
    // If that font is already set or doesn't exist, return.
    const TextRenderer::Font *NextFont = TextRenderer::GetOrLoadFont(fontName);
    if (NextFont == m_CurrentFont || NextFont == nullptr)
    {
        if (size > 0.0f)
            SetTextSize(size);

        return;
    }

    m_CurrentFont = NextFont;

    // Check to see if a new size was specified
    if (size > 0.0f)
        m_VSParams.TextSize = size;

    // Update constants directly tied to the font or the font size
    m_LineHeight = NextFont->GetVerticalSpacing(m_VSParams.TextSize);
    m_VSParams.NormalizeX = m_CurrentFont->GetXNormalizationFactor();
    m_VSParams.NormalizeY = m_CurrentFont->GetYNormalizationFactor();
    m_VSParams.Scale = m_VSParams.TextSize / m_CurrentFont->GetHeight();
    m_VSParams.DstBorder = m_CurrentFont->GetBorderSize() * m_VSParams.Scale;
    m_VSParams.SrcBorder = m_CurrentFont->GetBorderSize();
    m_FSParams.ShadowOffsetX = m_CurrentFont->GetHeight() * m_ShadowOffsetX * m_VSParams.NormalizeX;
    m_FSParams.ShadowOffsetY = m_CurrentFont->GetHeight() * m_ShadowOffsetY * m_VSParams.NormalizeY;
    m_FSParams.HeightRange = m_CurrentFont->GetAntialiasRange(m_VSParams.TextSize);
    m_VSConstantBufferIsStale = true;
    m_FSConstantBufferIsStale = true;
    m_TextureIsStale = true;
}

void TextContext::SetTextSize(float size)
{
    if (m_VSParams.TextSize == size)
        return;

    m_VSParams.TextSize = size;
    m_VSConstantBufferIsStale = true;

    if (m_CurrentFont != nullptr)
    {
        m_FSParams.HeightRange = m_CurrentFont->GetAntialiasRange(m_VSParams.TextSize);
        m_VSParams.Scale = m_VSParams.TextSize / m_CurrentFont->GetHeight();
        m_VSParams.DstBorder = m_CurrentFont->GetBorderSize() * m_VSParams.Scale;
        m_FSConstantBufferIsStale = true;
        m_LineHeight = m_CurrentFont->GetVerticalSpacing(size);
    }
    else
        m_LineHeight = 0.0f;
}

void TextContext::SetViewSize(float ViewWidth, float ViewHeight)
{
    m_ViewWidth = ViewWidth;
    m_ViewHeight = ViewHeight;

    const float vpX = 0.0f;
    const float vpY = 0.0f;
    const float twoDivW = 2.0f / ViewWidth;
    const float twoDivH = 2.0f / ViewHeight;

    // Essentially transform from screen coordinates to to clip space with W = 1.
    m_VSParams.ViewportTransform = glm::vec4(twoDivW, -twoDivH, -vpX * twoDivW - 1.0f, vpY * twoDivH + 1.0f);
    m_VSConstantBufferIsStale = true;
}

void TextContext::End(void)
{
    m_VSConstantBufferIsStale = true;
    m_FSConstantBufferIsStale = true;
    m_TextureIsStale = true;

    m_Context.EndRenderPass();
}

void TextContext::SetRenderState(void)
{
    WARN_ONCE_IF(nullptr == m_CurrentFont, "Attempted to draw text without a font");

    if (m_VSConstantBufferIsStale)
    {
        m_Context.PushConstants(vk::ShaderStageFlagBits::eVertex, 0, m_VSParams);
        m_VSConstantBufferIsStale = false;
    }

    if (m_FSConstantBufferIsStale)
    {
        m_Context.PushConstants(vk::ShaderStageFlagBits::eFragment, sizeof(m_VSParams), m_FSParams);
        m_FSConstantBufferIsStale = false;
    }

    if (m_TextureIsStale)
    {
        //        m_Context.SetDynamicDescriptors(2, 0, 1,
        //        &m_CurrentFont->GetTexture().GetSRV());
        //        m_Context.SetTexture(0, m_CurrentFont->GetTexture().GetGLTexture());
        // TextRenderer::s_TextDS.UpdateImageSampler(g_CurrentBuffer, 0,
        //    m_CurrentFont->GetTexture(), SamplerLinearClamp);
        // m_Context.BeginUpdateDescriptorSet();
        m_Context.UpdateImageSampler(0, m_CurrentFont->GetTexture(), SamplerLinearClamp);
        // m_Context.EndUpdateAndBindDescriptorSet();

        m_TextureIsStale = false;
    }
}

// These are made with templates to handle char and wchar_t simultaneously.
std::vector<TextContext::TextVert> TextContext::FillVertexBuffer(const std::string &str)
{
    unsigned int charsDrawn = 0;

    const float UVtoPixel = m_VSParams.Scale;

    float curX = m_TextPosX;
    float curY = m_TextPosY;

    const uint16_t texelHeight = m_CurrentFont->GetHeight();

    // const char* iter = str;
    tiny_utf8::string u8str(str);

    // reserve enough size for chars
    std::vector<TextVert> ret;
    ret.reserve(u8str.length());

    for (char32_t wc : u8str)
    {
        // wchar_t wc = (stride == 2 ? *(wchar_t*)iter : *iter);
        // iter += stride;

        // Terminate on null character (this really shouldn't happen with string or
        // wstring)
        if (wc == U'\0')
            break;

        // Handle newlines by inserting a carriage return and line feed
        if (wc == U'\n')
        {
            curX = m_LeftMargin;
            curY += m_LineHeight;
            continue;
        }

        const TextRenderer::Font::Glyph *gi = m_CurrentFont->GetGlyph(wc);

        // Ignore missing characters
        if (nullptr == gi)
            continue;

        TextVert vert;

        vert.X = curX + (float)gi->bearing * UVtoPixel;
        vert.Y = curY;
        vert.U = gi->x;
        vert.V = gi->y;
        vert.W = gi->w;
        vert.H = texelHeight;
        ret.push_back(vert);

        // Advance the cursor position
        curX += (float)gi->advance * UVtoPixel;
        ++charsDrawn;
    }

    m_TextPosX = curX;
    m_TextPosY = curY;

    return ret;
}

// void TextContext::DrawString(const std::wstring& str)
//{
//     SetRenderState();
//
//     void* stackMem = _malloca((str.size() + 1) * 16);
//     TextVert* vbPtr = Math::AlignUp((TextVert*)stackMem, 16);
//     UINT primCount = FillVertexBuffer(vbPtr, (char*)str.c_str(), 2,
//     str.size());
//
//     if (primCount > 0)
//     {
//         m_Context.SetDynamicVB(0, primCount, sizeof(TextVert), vbPtr);
//         m_Context.DrawInstanced(4, primCount);
//     }
//
//     _freea(stackMem);
// }

void TextContext::DrawString(const std::string &str)
{
    SetRenderState();

    std::vector<TextVert> vb = FillVertexBuffer(str);
    size_t primCount = vb.size();

    if (primCount > 0)
    {
        m_Context.BindDynamicVertexBuffer(0, sizeof(TextVert) * primCount, vb.data());
        //        m_Context.SetDynamicVB(0, primCount, sizeof(TextVert), vbPtr);
        m_Context.DrawInstanced(4, primCount, 0, 0);
    }
}

// void TextContext::DrawFormattedString(const wchar_t* format, ...)
//{
//     wchar_t buffer[256];
//     va_list ap;
//     va_start(ap, format);
//     vswprintf(buffer, 256, format, ap);
//     va_end(ap);
//     DrawString(wstring(buffer));
// }

void TextContext::DrawFormattedString(const char *format, ...)
{
    char buffer[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, 256, format, ap);
    va_end(ap);
    DrawString(string(buffer));
}
