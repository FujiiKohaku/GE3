#include "ModelPreviewApp.h"
#include "App/Game/Stage/GameplayVisualPreset.h"
#include "App/Game/Stage/StageCatalog.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/Logger/Logger.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Winapp/WinApp.h"
#include <stdexcept>

ModelPreviewApp::ModelPreviewApp() = default;
ModelPreviewApp::~ModelPreviewApp() = default;

void ModelPreviewApp::Initialize()
{
    WinApp::GetInstance()->initialize();
    SetWindowTextW(WinApp::GetInstance()->GetHwnd(), L"Kohaku Model Preview");
    dx_ = DirectXCommon::GetInstance();
    dx_->Initialize(WinApp::GetInstance());
    SrvManager::GetInstance()->Initialize(dx_);
    TextureManager::GetInstance()->Initialize(dx_, SrvManager::GetInstance());
    TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");
    ImGuiManager::GetInstance()->Initialize(WinApp::GetInstance(), dx_, SrvManager::GetInstance());
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    Object3dManager::GetInstance()->Initialize(dx_);
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    // Same unboosted FOV and clipping distances as GamePlayScene.
    camera_->SetFovY(0.45f);
    camera_->SetNearClip(0.1f);
    camera_->SetFarClip(1000.0f);
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    modelCommon_.Initialize(dx_);
    offscreen_ = std::make_unique<OffscreenRenderer>();
    offscreen_->Initialize();
    postEffects_ = std::make_unique<PostEffectManager>();
    postEffects_->Initialize(dx_);
    postEffects_->SetNormalTextureHandle(offscreen_->GetNormalSrvHandleGPU());
    StageCatalog* catalog = StageCatalog::GetInstance();
    if (!catalog->Load()) { throw std::runtime_error(catalog->GetLastError()); }
    const StageSettings* iceStage = catalog->Find("stage03");
    if (iceStage == nullptr) { throw std::runtime_error("Stage03 visual settings were not found"); }
    LevelDataLoader loader;
    iceLayout_ = loader.Load(iceStage->layoutFile);
    SkyBoxManager::GetInstance()->Initialize(dx_);
    skyBox_ = std::make_unique<SkyBox>();
    skyBox_->Initialize(dx_);
    TextureManager::GetInstance()->LoadTexture(iceStage->skybox);
    skyBox_->SetTexture(iceStage->skybox);
    GameplayVisualPreset::ApplyLighting("stage03");
    GameplayVisualPreset::ApplyAtmosphere("stage03");
    GameplayVisualPreset::ConfigurePostEffects(false);
    capture_.Initialize(dx_);
    TimeManager::GetInstance()->Initialize();
    shaderNames_ = { "Standard", "Unlit", "Toon", "Ice", "IceJellyfish",
        "StageIceSpire", "StageIceBoulder", "StageIceSlab", "StageIceArch",
        "StageIceIsland", "StageIceCrystal", "StageIceFloor",
        "ArchivePaper", "ArchiveLeather", "ArchiveBrass" };
    ScanModels();
    initialized_ = true;
    Logger::Log("ModelPreview initialized with Stage03 lighting, skybox, outline and fog; no game scenes/audio/particles");
}

void ModelPreviewApp::Finalize()
{
    if (!initialized_) { return; }
    capture_.StopGif();
    dx_->WaitForGPU();
    items_.clear();
    skyBox_.reset();
    postEffects_.reset();
    offscreen_.reset();
    camera_.reset();
    ImGuiManager::Finalize();
    SkyBoxManager::Finalize();
    Object3dManager::Finalize();
    LightManager::Finalize();
    TextureManager::GetInstance()->Finalize();
    SrvManager::GetInstance()->Finalize();
    initialized_ = false;
}
