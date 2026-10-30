#include "EffectEditorApp.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Serialization/JsonFile.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Renderer/SceneRenderResolution.h"
#include "App/Scene/Common/SceneManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
void AppendQuad(MeshPrimitive& primitive, float x0, float z0, float x1, float z1, float height)
{
    const uint32_t offset = static_cast<uint32_t>(primitive.vertices.size());
    const Vector4 positions[] = { { x0, height, z0, 1 }, { x0, height, z1, 1 }, { x1, height, z0, 1 }, { x1, height, z1, 1 } };
    for (const auto& position : positions) {
        VertexData vertex {};
        vertex.position = position; vertex.normal = { 0, 1, 0 };
        primitive.vertices.push_back(vertex);
    }
    const uint32_t indices[] = { 0, 1, 2, 1, 3, 2 };
    for (uint32_t index : indices) { primitive.indices.push_back(offset + index); }
}

ModelData PlaneData(bool isGrid)
{
    ModelData data {};
    data.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    data.rootNode.transform.scale = { 1, 1, 1 };
    data.rootNode.transform.rotate = { 0, 0, 0, 1 };
    data.rootNode.name = "EditorFloor";
    data.materials.resize(1);
    data.materials[0].textureFilePath = "resources/Textures/white.png";
    MeshPrimitive primitive {};
    primitive.mode = PrimitiveMode::Triangles;
    if (isGrid) {
        for (int index = -20; index <= 20; ++index) {
            const float coordinate = static_cast<float>(index);
            AppendQuad(primitive, coordinate - 0.006f, -20, coordinate + 0.006f, 20, 0.003f);
            AppendQuad(primitive, -20, coordinate - 0.006f, 20, coordinate + 0.006f, 0.003f);
        }
    } else { AppendQuad(primitive, -20, -20, 20, 20, 0); }
    data.primitives.push_back(std::move(primitive));
    return data;
}

float LinearChannel(float channel)
{
    if (channel <= 0.04045f) { return channel / 12.92f; }
    return std::pow((channel + 0.055f) / 1.055f, 2.4f);
}
}

void EditorHistory::Push(const nlohmann::json& value)
{
    if (undo.size() >= 100) { undo.erase(undo.begin()); }
    undo.push_back(value); redo.clear();
}
bool EditorHistory::Undo(nlohmann::json& value)
{
    if (undo.empty()) { return false; }
    redo.push_back(value); value = undo.back(); undo.pop_back(); return true;
}
bool EditorHistory::Redo(nlohmann::json& value)
{
    if (redo.empty()) { return false; }
    undo.push_back(value); value = redo.back(); redo.pop_back(); return true;
}

EffectEditorApp::EffectEditorApp() = default;
EffectEditorApp::~EffectEditorApp() = default;

void EffectEditorApp::Initialize()
{
    WinApp::GetInstance()->initialize();
    SetWindowTextW(WinApp::GetInstance()->GetHwnd(), L"Kohaku Effect Editor — エフェクト編集");
    // Windowed mode keeps the engine's fixed size; F11 switches through the engine's display API.
    LONG_PTR style = GetWindowLongPtrW(WinApp::GetInstance()->GetHwnd(), GWL_STYLE);
    SetWindowLongPtrW(WinApp::GetInstance()->GetHwnd(), GWL_STYLE, style & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX);
    dx_ = DirectXCommon::GetInstance();
    dx_->Initialize(WinApp::GetInstance());
    SrvManager::GetInstance()->Initialize(dx_);
    TextureManager::GetInstance()->Initialize(dx_, SrvManager::GetInstance());
    TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");
    ImGuiManager::GetInstance()->Initialize(WinApp::GetInstance(), dx_, SrvManager::GetInstance());
    ConfigureStyle();
    Object3dManager::GetInstance()->Initialize(dx_);
    camera_.Initialize(); camera_.SetFovY(0.55f); camera_.SetNearClip(0.1f); camera_.SetFarClip(500);
    camera_.SetAspectRatio(16.0f / 9.0f);
    Object3dManager::GetInstance()->SetDefaultCamera(&camera_);
    modelCommon_.Initialize(dx_);
    CreateGeometry();
    offscreen_ = std::make_unique<OffscreenRenderer>(); offscreen_->Initialize();
    post_ = std::make_unique<PostEffectManager>(); post_->Initialize(dx_);
    post_->SetNormalTextureHandle(offscreen_->GetNormalSrvHandleGPU());
    post_->SetIndirectTextureHandle(offscreen_->GetIndirectSrvHandleGPU());
    preview_ = std::make_unique<PreviewTarget>(); preview_->Initialize(dx_);
    auto* effects = EffectManager::GetInstance();
    effects->Initialize(dx_, SrvManager::GetInstance(), &camera_, false);
    effects->SetFogConstantBufferView(post_->GetFogConstantBufferView());
    LoadPost(postPath_);
    ScanEffects();
    if (entries_.empty()) { throw std::runtime_error("パーティクルのデータがありません"); }
    std::string initialId = entries_.front().id;
    for (const auto& entry : entries_) { if (entry.id == "HinokamiFlame") { initialId = entry.id; } }
    SelectEffect(initialId);
    for (const auto& entry : std::filesystem::recursive_directory_iterator("resources/Models")) {
        if (entry.is_regular_file() && entry.path().extension() == ".obj") { modelPaths_.push_back(entry.path()); }
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator("resources/Effects")) {
        if (texturePaths_.size() >= 30) { break; }
        if (entry.is_regular_file() && entry.path().extension() == ".png") { texturePaths_.push_back(entry.path().generic_string()); }
    }
    for (size_t index = 0; index < (std::min)(texturePaths_.size(), size_t(4)); ++index) {
        try { TextureManager::GetInstance()->LoadTexture(texturePaths_[index]); } catch (const std::exception&) { }
    }
    TimeManager::GetInstance()->Initialize();
    isInitialized_ = true;
    SetStatus("準備完了。右ドラッグで回転、ホイールでズームできます");
}

void EffectEditorApp::ConfigureStyle()
{
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    const char* fontPath = "C:/Windows/Fonts/meiryo.ttc";
    if (!std::filesystem::is_regular_file(fontPath)) { fontPath = "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf"; }
    io.FontDefault = io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.FontSizeBase = 18;
    style.WindowRounding = 0; style.ChildRounding = 3; style.FrameRounding = 3; style.GrabRounding = 3;
    style.WindowPadding = { 10, 8 }; style.FramePadding = { 6, 4 }; style.ItemSpacing = { 6, 5 };
    style.Colors[ImGuiCol_WindowBg] = { 0.075f, 0.085f, 0.10f, 1 };
    style.Colors[ImGuiCol_ChildBg] = { 0.085f, 0.095f, 0.11f, 1 };
    style.Colors[ImGuiCol_FrameBg] = { 0.045f, 0.055f, 0.07f, 1 };
    style.Colors[ImGuiCol_Button] = { 0.16f, 0.19f, 0.23f, 1 };
    style.Colors[ImGuiCol_ButtonHovered] = { 0.14f, 0.34f, 0.55f, 1 };
    style.Colors[ImGuiCol_Header] = { 0.08f, 0.30f, 0.55f, 1 };
    style.Colors[ImGuiCol_HeaderHovered] = { 0.12f, 0.39f, 0.67f, 1 };
    style.Colors[ImGuiCol_ButtonActive] = style.Colors[ImGuiCol_Header];
    style.Colors[ImGuiCol_CheckMark] = { 0.25f, 0.62f, 1, 1 };
    style.Colors[ImGuiCol_SliderGrab] = style.Colors[ImGuiCol_CheckMark];
    style.Colors[ImGuiCol_Border] = { 0.20f, 0.23f, 0.28f, 1 };
    // The engine's UI render target is sRGB; feed linear colors to avoid washed-out panels.
    for (auto& color : style.Colors) {
        color.x = LinearChannel(color.x); color.y = LinearChannel(color.y); color.z = LinearChannel(color.z);
    }
}

void EffectEditorApp::CreateGeometry()
{
    floorModel_ = std::make_unique<Model>(); floorModel_->Initialize(&modelCommon_, PlaneData(false));
    gridModel_ = std::make_unique<Model>(); gridModel_->Initialize(&modelCommon_, PlaneData(true));
    floor_ = std::make_unique<Object3d>(); floor_->Initialize(Object3dManager::GetInstance());
    floor_->SetModel(floorModel_.get()); floor_->SetCamera(&camera_);
    floor_->SetMaterial("resources/Shaders/Object3D/Standard"); floor_->SetColor({ 0.025f, 0.03f, 0.04f, 1 });
    grid_ = std::make_unique<Object3d>(); grid_->Initialize(Object3dManager::GetInstance());
    grid_->SetModel(gridModel_.get()); grid_->SetCamera(&camera_);
    grid_->SetMaterial("resources/Shaders/Object3D/Unlit"); grid_->SetColor({ 0.05f, 0.06f, 0.08f, 1 });
}

void EffectEditorApp::LoadModel(const std::filesystem::path& path)
{
    auto model = std::make_unique<Model>();
    model->Initialize(&modelCommon_, path.parent_path().generic_string(), path.filename().string());
    auto object = std::make_unique<Object3d>(); object->Initialize(Object3dManager::GetInstance());
    object->SetModel(model.get()); object->SetCamera(&camera_); object->SetMaterial("resources/Shaders/Object3D/Standard");
    const float radius = (std::max)(model->GetBoundsRadius(), 0.01f);
    object->SetScale({ 1.5f / radius, 1.5f / radius, 1.5f / radius }); object->SetTranslate({ 2.5f, 1, 0 });
    sample_.reset(); sampleModel_ = std::move(model); sample_ = std::move(object); isModelVisible_ = true;
}

void EffectEditorApp::Update()
{
    float timeScale = 0;
    if (isPlaying_) { timeScale = speed_; }
    TimeManager::GetInstance()->SetTimeScale(timeScale);
    TimeManager::GetInstance()->Update();
    ImGuiManager::GetInstance()->Begin();
    auto& io = ImGui::GetIO();
    // WinApp renders fullscreen at 1080p even when the monitor has a different physical size.
    if (io.DisplaySize.x > 0 && io.DisplaySize.y > 0) {
        io.DisplayFramebufferScale = { WinApp::GetInstance()->GetRenderWidth() / io.DisplaySize.x,
            WinApp::GetInstance()->GetRenderHeight() / io.DisplaySize.y };
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) { shouldToggleFullscreen_ = true; }
    DrawUi();
    if (isViewportHovered_ && !io.WantTextInput) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            yaw_ -= io.MouseDelta.x * 0.008f;
            pitch_ = std::clamp(pitch_ + io.MouseDelta.y * 0.008f, -1.4f, 1.4f);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            const float scale = distance_ * 0.0015f;
            target_.x -= io.MouseDelta.x * std::cos(yaw_) * scale;
            target_.z -= io.MouseDelta.x * std::sin(yaw_) * scale;
            target_.y += io.MouseDelta.y * scale;
        }
        distance_ = std::clamp(distance_ * std::exp(-io.MouseWheel * 0.12f), 0.5f, 100.0f);
        if (ImGui::IsKeyPressed(ImGuiKey_F)) { target_ = { 0, 0.7f, 0 }; distance_ = 5; }
    }
    const Vector3 eye { target_.x + std::sin(yaw_) * std::cos(pitch_) * distance_,
        target_.y + std::sin(pitch_) * distance_, target_.z - std::cos(yaw_) * std::cos(pitch_) * distance_ };
    camera_.LookAt(eye, target_); camera_.Update();
    auto* lights = LightManager::GetInstance();
    lights->SetDirectional({ 1, 0.97f, 0.92f, 1 }, { -0.4f, -1, 0.3f }, 0.9f);
    lights->SetAmbientColor({ 0.65f, 0.72f, 0.85f }); lights->SetAmbientIntensity(0.30f); lights->Update();
    floor_->Update(); grid_->Update();
    if (sample_) { sample_->Update(); }
    elapsedSeconds_ += TimeManager::GetInstance()->GetDeltaTime();
    Vector3 emitter { 0, 0.15f, 0 };
    if (motion_ == 1) { emitter = { std::sin(elapsedSeconds_) * motionRadius_, 0.5f, std::cos(elapsedSeconds_) * motionRadius_ }; }
    if (motion_ == 2) { emitter = { std::sin(elapsedSeconds_) * motionRadius_, 0.5f, 0 }; }
    EffectManager::GetInstance()->SetEffectPosition(effectHandle_, emitter);
    EffectManager::GetInstance()->UpdatePerView();
    // Skipping simulation when paused avoids emitting a first particle batch on a zero-time frame.
    if (isPlaying_) { EffectManager::GetInstance()->Update(); }
    else { EffectManager::GetInstance()->InitializePendingEffects(); }
    post_->Update(&camera_);
    ImGuiManager::GetInstance()->End();
}

void EffectEditorApp::ResizePreviewTargets()
{
    auto* window = WinApp::GetInstance();
    SceneRenderResolution::SetSize(window->GetRenderWidth(), window->GetRenderHeight());
    offscreen_->ResizeSceneTargets();
    post_->ResizeSceneTargets();
    post_->ResizeDisplayTargets();
    post_->SetNormalTextureHandle(offscreen_->GetNormalSrvHandleGPU());
    post_->SetIndirectTextureHandle(offscreen_->GetIndirectSrvHandleGPU());
    preview_->Resize();
    camera_.SetAspectRatio(static_cast<float>(preview_->GetWidth()) / preview_->GetHeight());
}

bool EffectEditorApp::ToggleFullscreen()
{
    if (!dx_->IsCommandListPristine()) { throw std::runtime_error("表示の切り替えは描画終了後に実行してください"); }
    auto* window = WinApp::GetInstance();
    const bool wasFullscreen = window->IsBorderlessFullscreen();
    dx_->WaitForGPU();
    if (!window->SetBorderlessFullscreen(!wasFullscreen)) { SetStatus("全画面の切り替えに失敗しました", true); return false; }
    lastBackBuffer_ = nullptr;
    try {
        if (!dx_->ResizeDisplayTargets()) { throw std::runtime_error("画面の描画先を更新できませんでした"); }
        ResizePreviewTargets();
    } catch (const std::exception& exception) {
        window->SetBorderlessFullscreen(wasFullscreen);
        if (!dx_->ResizeDisplayTargets()) { throw std::runtime_error("元の描画サイズを復元できませんでした"); }
        ResizePreviewTargets();
        SetStatus(std::string("表示の切り替えに失敗しました：") + exception.what(), true);
        return false;
    }
    if (window->IsBorderlessFullscreen()) { SetStatus("全画面表示（F11でウィンドウ表示に戻ります）"); }
    else { SetStatus("ウィンドウ表示に戻しました"); }
    return true;
}

void EffectEditorApp::Draw()
{
    TextureManager::GetInstance()->FlushUploads();
    SrvManager::GetInstance()->PreDraw();
    post_->PreDrawDepth(); offscreen_->SetClearColor(background_);
    offscreen_->PreDraw(post_->GetDepthDSVHandle());
    Object3dManager::GetInstance()->PreDraw();
    if (isFloorVisible_) { floor_->Draw(); }
    if (isGridVisible_) { grid_->Draw(); }
    if (isModelVisible_ && sample_) { sample_->Draw(); }
    post_->PostDrawDepth(); offscreen_->PostDraw();
    dx_->PreDraw();
    post_->PrepareSceneForParticleDraw(SceneManager::GetInstance(), offscreen_->GetSrvHandleGPU());
    post_->PrepareDepthForParticleDraw(); post_->BeginParticleDraw();
    EffectManager::GetInstance()->PreDraw(); EffectManager::GetInstance()->Draw();
    post_->EndParticleDraw(); post_->PostDrawDepth();
    preview_->Begin();
    post_->SetOutputTarget(preview_->GetRtv(), preview_->GetWidth(), preview_->GetHeight());
    post_->ApplyAfterParticleDraw(SceneManager::GetInstance());
    post_->ClearOutputTarget(); preview_->End();
    dx_->SetBackBufferRenderTarget(dx_->GetDSVHandle());
    ImGuiManager::GetInstance()->Draw();
    lastBackBuffer_ = dx_->GetCurrentBackBuffer();
    dx_->PostDraw();
    ProcessPending();
    ProcessLiveChanges(GetTickCount64());
}

void EffectEditorApp::Finalize()
{
    if (!isInitialized_) { return; }
    dx_->WaitForGPU();
    EffectManager::GetInstance()->StopAllEffects(); EffectManager::Finalize();
    sample_.reset(); sampleModel_.reset(); grid_.reset(); floor_.reset(); gridModel_.reset(); floorModel_.reset();
    post_.reset(); offscreen_.reset(); preview_.reset();
    ImGuiManager::Finalize(); Object3dManager::Finalize(); LightManager::Finalize();
    TextureManager::GetInstance()->Finalize(); SrvManager::GetInstance()->Finalize();
    DirectXCommon::Finalize();
    isInitialized_ = false;
}

bool EffectEditorApp::IsParticleDirty() const { return particleDraft_ != particleSaved_; }
bool EffectEditorApp::IsPostDirty() const { return postDraft_ != postSaved_; }
void EffectEditorApp::SetStatus(const std::string& text, bool isError) { status_ = text; isStatusError_ = isError; }
