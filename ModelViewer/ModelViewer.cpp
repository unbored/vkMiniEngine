#include <utility>
#include <vulkan/vulkan.hpp>

#include <BufferManager.h>
#include <Camera.h>
#include <CameraController.h>
#include <CommandContext.h>
#include <Display.h>
#include <EngineTuning.h>
#include <GameCore.h>
#include <GameInput.h>
#include <GpuBuffer.h>
#include <GraphicsCore.h>
#include <ModelLoader.h>
#include <Renderer.h>
#include <ShadowCamera.h>
#include <TextureManager.h>
#include <UniformBuffers.h>
#include <Util/CommandLineArg.h>
#include <filesystem>
#include <memory>
#include <regex>

using namespace GameCore;
using namespace Graphics;
using namespace Math;
using namespace std;

using Renderer::MeshSorter;

class ModelViewer : public GameCore::IGameApp
{
public:
    ModelViewer(void) {}

    virtual void Startup(void) override;
    virtual void Cleanup(void) override;

    virtual void Update(float deltaT) override;
    virtual void RenderScene(void) override;

private:
    Camera m_Camera;
    unique_ptr<CameraController> m_CameraController;

    vk::Viewport m_MainViewport;
    vk::Rect2D m_MainScissor;

    ModelInstance m_ModelInst;
    ShadowCamera m_SunShadowCamera;
};

CREATE_APPLICATION(ModelViewer)

ExpVar g_SunLightIntensity("Viewer/Lighting/Sun Light Intensity", 4.0f, 0.0f, 16.0f, 0.1f);
NumVar g_SunOrientation("Viewer/Lighting/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f);
NumVar g_SunInclination("Viewer/Lighting/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f);

void ChangeIBLSet(EngineVar::ActionType);
void ChangeIBLBias(EngineVar::ActionType);

DynamicEnumVar g_IBLSet("Viewer/Lighting/Environment", ChangeIBLSet);
std::vector<std::pair<TextureRef, TextureRef>> g_IBLTextures;
NumVar g_IBLBias("Viewer/Lighting/Gloss Reduction", 2.0f, 0.0f, 10.0f, 1.0f, ChangeIBLBias);

void ChangeIBLSet(EngineVar::ActionType)
{
    int setIdx = g_IBLSet - 1;
    if (setIdx < 0)
    {
        Renderer::SetIBLTextures(nullptr, nullptr);
    }
    else
    {
        auto texturePair = g_IBLTextures[setIdx];
        Renderer::SetIBLTextures(texturePair.first, texturePair.second);
    }
}

void ChangeIBLBias(EngineVar::ActionType) { Renderer::SetIBLBias(g_IBLBias); }

void LoadIBLTextures()
{
    namespace fs = std::filesystem;

    Utility::Printf("Loading IBL environment maps\n");

    g_IBLSet.AddEnum("None");

    fs::path dir("Textures");
    // make sure dir is a directory
    if (!fs::exists(dir) || !fs::is_directory(dir))
    {
        return;
    }
    fs::directory_iterator iter(dir);
    std::regex fileSuffix(".*_diffuseIBL.ktx");
    for (auto &f : iter)
    {
        auto path = f.path();
        auto name = path.filename();
        if (std::regex_match(name.string(), fileSuffix))
        {
            std::string diffuseFile = name.string();
            std::string baseFile = diffuseFile.substr(0, diffuseFile.rfind("_diffuseIBL.ktx"));
            std::string specularFile = baseFile + "_specularIBL.ktx";
            TextureRef diffuseTex = TextureManager::LoadKTXFromFile("Textures/" + diffuseFile);
            if (diffuseTex.IsValid())
            {
                TextureRef specularTex = TextureManager::LoadKTXFromFile("Textures/" + specularFile);
                if (specularTex.IsValid())
                {
                    g_IBLSet.AddEnum(baseFile);
                    g_IBLTextures.push_back(std::make_pair(diffuseTex, specularTex));
                }
            }
        }
    }
    Utility::Printf("Found %u IBL environment map sets\n", g_IBLTextures.size());

    if (g_IBLTextures.size() > 0)
    {
        g_IBLSet.Increment();
    }
}

void ModelViewer::Startup(void)
{
    // MotionBlur::Enable = true;
    // TemporalEffects::EnableTAA = true;
    // FXAA::Enable = false;
    // PostEffects::EnableHDR = true;
    // PostEffects::EnableAdaptation = true;
    // SSAO::Enable = true;

    Renderer::Initialize();

    LoadIBLTextures();

    std::string gltfFileName;

    if (!CommandLineArgs::GetString("model", gltfFileName))
    {
        Utility::Printf("Usage: <exe> -model <file>\n");
    }

    m_ModelInst = Renderer::LoadModel(gltfFileName, true);
    // TODO: loop Animations
    m_ModelInst.Resize(10.0f);

    m_Camera.SetZRange(1.0f, 10000.0f);
    // if (gltfFileName.size() == 0)
    //     m_CameraController.reset(new FlyingFPSCamera(m_Camera, Vector3(kYUnitVector)));
    // else
    m_CameraController.reset(new OrbitCamera(m_Camera, m_ModelInst.GetBoundingSphere(), CreateYUnitVector()));
}

void ModelViewer::Cleanup(void)
{
    m_ModelInst = nullptr;

    g_IBLTextures.clear();
    Renderer::Shutdown();
}

namespace Graphics
{
extern EnumVar DebugZoom;
}

void ModelViewer::Update(float deltaT)
{
    if (GameInput::IsFirstPressed(GameInput::kLShoulder))
        DebugZoom.Decrement();
    else if (GameInput::IsFirstPressed(GameInput::kRShoulder))
        DebugZoom.Increment();

    m_CameraController->Update(deltaT);

    GraphicsContext &gfxContext = GraphicsContext::Begin("Scene Update");
    m_ModelInst.Update(gfxContext, deltaT);

    gfxContext.Finish();

    m_MainViewport.width = (float)g_SceneColorBuffer.GetWidth();
    m_MainViewport.height = (float)g_SceneColorBuffer.GetHeight();
    m_MainViewport.minDepth = 0.0f;
    m_MainViewport.maxDepth = 1.0f;

    m_MainScissor.setOffset({0, 0});
    m_MainScissor.setExtent({g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight()});
}

void ModelViewer::RenderScene(void)
{
    GraphicsContext &gfxContext = GraphicsContext::Begin("Scene Render");

    // Update global constants
    float costheta = glm::cos(g_SunOrientation);
    float sintheta = glm::sin(g_SunOrientation);
    float cosphi = glm::cos(g_SunInclination * glm::half_pi<float>()); // 0 means water level, 1 means upstraight
    float sinphi = glm::sin(g_SunInclination * glm::half_pi<float>());

    glm::vec3 SunDirection = glm::normalize(glm::vec3(costheta * cosphi, sinphi, sintheta * cosphi));
    glm::vec3 ShadowBounds = glm::vec3(m_ModelInst.GetRadius());
    // m_SunShadowCamera.UpdateMatrix(-SunDirection, m_ModelInst.GetCenter(), ShadowBounds,
    m_SunShadowCamera.UpdateMatrix(-SunDirection, glm::vec3(0, -500.0f, 0), glm::vec3(5000, 3000, 3000),
                                   (uint32_t)g_ShadowBuffer.GetWidth(), (uint32_t)g_ShadowBuffer.GetHeight(), 16);

    GlobalConstants globals;
    globals.ViewProjMatrix = m_Camera.GetViewProjMatrix();
    globals.SunShadowMatrix = m_SunShadowCamera.GetShadowMatrix();
    globals.CameraPos = glm::vec4(m_Camera.GetPosition(), 0.0);
    globals.SunDirection = glm::vec4(SunDirection, 0.0);
    globals.SunIntensity = glm::vec3(g_SunLightIntensity); // w will be ignored anyway

    // gfxContext.ClearColor(g_SceneColorBuffer);
    gfxContext.ClearDepth(g_SceneDepthBuffer);

    MeshSorter sorter(MeshSorter::kDefault);
    sorter.SetCamera(m_Camera);
    sorter.SetViewport(m_MainViewport);
    sorter.SetScissor(m_MainScissor);
    sorter.SetDepthBuffer(g_SceneDepthBuffer);
    sorter.AddColorBuffer(g_SceneColorBuffer);

    m_ModelInst.Render(sorter);

    sorter.Sort();

    {
        // ScopedTimer _prof(L"Depth Pre-Pass", gfxContext);
        sorter.RenderMeshes(MeshSorter::kZPass, gfxContext, globals);
    }

    MeshSorter shadowSorter(MeshSorter::kShadows);
    shadowSorter.SetCamera(m_SunShadowCamera);
    shadowSorter.SetDepthBuffer(g_ShadowBuffer);

    m_ModelInst.Render(shadowSorter);

    shadowSorter.Sort();
    shadowSorter.RenderMeshes(MeshSorter::kZPass, gfxContext, globals);

    gfxContext.ClearColor(g_SceneColorBuffer);

    gfxContext.TransitionImageLayout(g_SSAOFullScreen, vk::ImageLayout::eShaderReadOnlyOptimal);
    gfxContext.TransitionImageLayout(g_ShadowBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
    gfxContext.TransitionImageLayout(g_SceneDepthBuffer, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    // gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());
    gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

    sorter.RenderMeshes(MeshSorter::kOpaque, gfxContext, globals);

    Renderer::DrawSkybox(gfxContext, m_Camera, m_MainViewport, m_MainScissor);

    sorter.RenderMeshes(MeshSorter::kTransparent, gfxContext, globals);

    // Renderer::Render(gfxContext, globals);

    gfxContext.Finish();
}
