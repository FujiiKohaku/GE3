#include "PreviewCapture.h"
#include "App/Game/Stage/GameplayVisualPreset.h"
#include "App/Game/Stage/StageCatalog.h"
#include "Engine/3D/Model.h"
#include "Engine/3D/ModelCommon.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include "Engine/Logger/Logger.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Winapp/WinApp.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <commdlg.h>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace {
constexpr float kTau = std::numbers::pi_v<float> * 2.0f;
constexpr const char* kDefaultVertexShader = "resources/Shaders/Object3D/Object3d.VS.hlsl";

struct PreviewItem {
    std::unique_ptr<Model> model;
    std::unique_ptr<Object3d> object;
    std::string name;
    Vector3 center {};
    float radius = 1.0f;
    Vector3 position {};
    Vector3 rotation {};
    Vector3 axisScale { 1.0f, 1.0f, 1.0f };
    bool stageTransform = false;
    float scale = 1.0f;
    Vector4 color { 1.0f, 1.0f, 1.0f, 1.0f };
    std::string shader = "Standard";
    bool transparent = false;
    bool environment = false;
    float reflection = 0.12f;
    float shininess = 32.0f;
};

class ModelPreviewApp {
public:
    void Initialize();
    void Update();
    void Draw();
    void Finalize();
    void Load(const std::filesystem::path& path, bool append);
    PreviewCapture& Capture() { return capture_; }
    const std::string& Error() const { return error_; }

private:
    void Controls();
    void Fit();
    void ApplyMaterial(PreviewItem& item);
    void Import(const std::filesystem::path& path, PreviewItem& item);
    void ChooseFile();
    void ScanModels();
    Matrix4x4 ItemTransform(const PreviewItem& item) const;
    Vector3 ItemCenter(const PreviewItem& item) const;
    DirectXCommon* dx_ = nullptr;
    std::unique_ptr<Camera> camera_;
    ModelCommon modelCommon_;
    std::unique_ptr<OffscreenRenderer> offscreen_;
    std::unique_ptr<PostEffectManager> postEffects_;
    std::unique_ptr<SkyBox> skyBox_;
    LevelData iceLayout_;
    bool gameLook_ = true;
    PreviewCapture capture_;
    std::vector<std::unique_ptr<PreviewItem>> items_;
    std::vector<std::string> modelPaths_;
    std::vector<std::string> shaderNames_;
    int selected_ = 0;
    char filter_[128] {};
    bool append_ = false;
    bool showControls_ = true;
    bool animate_ = true;
    float shaderTime_ = 0.0f;
    float orbitYaw_ = 0.0f;
    float orbitPitch_ = 0.22f;
    float distance_ = 10.0f;
    float sceneRadius_ = 1.0f;
    Vector3 target_ {};
    Vector4 background_ { 0.028f, 0.037f, 0.055f, 1.0f };
    Vector3 lightDirection_ { 0.6f, -0.8f, 0.5f };
    float lightIntensity_ = 0.65f;
    float ambient_ = 0.22f;
    int gifFps_ = 15;
    int gifSeconds_ = 4;
    bool turntable_ = true;
    float recordingYaw_ = 0.0f;
    bool wasRecording_ = false;
    std::string error_;
    bool initialized_ = false;
};

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

void ModelPreviewApp::ScanModels()
{
    modelPaths_.clear();
    for (const auto& entry : std::filesystem::recursive_directory_iterator("resources/Models")) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string extension = entry.path().extension().string();
        for (char& character : extension) {
            if (character >= 'A' && character <= 'Z') {
                character += 'a' - 'A';
            }
        }
        if (extension == ".obj" || extension == ".gltf" || extension == ".glb" ||
            extension == ".fbx" || extension == ".dae" || extension == ".ply") {
            modelPaths_.push_back(entry.path().generic_string());
        }
    }
    std::sort(modelPaths_.begin(), modelPaths_.end());
}

void ModelPreviewApp::Import(const std::filesystem::path& path, PreviewItem& item)
{
    Assimp::Importer importer;
    // Bake the entire scene hierarchy, so a multi-mesh model is previewed as a whole.
    const aiScene* scene = importer.ReadFile(path.string(), aiProcess_Triangulate |
        aiProcess_GenSmoothNormals | aiProcess_PreTransformVertices |
        aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
    if (scene == nullptr || !scene->HasMeshes()) {
        throw std::runtime_error(std::string("Model import failed: ") + importer.GetErrorString());
    }
    ModelData data {};
    data.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    data.rootNode.transform.scale = { 1.0f, 1.0f, 1.0f };
    data.rootNode.transform.rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
    data.rootNode.name = "PreviewRoot";
    data.materials.resize((std::max)(scene->mNumMaterials, 1u));
    for (unsigned int index = 0; index < data.materials.size(); ++index) {
        auto& material = data.materials[index];
        material.textureFilePath = "resources/Textures/white.png";
        if (index >= scene->mNumMaterials) {
            continue;
        }
        aiString textureName;
        auto* source = scene->mMaterials[index];
        aiTextureType type = aiTextureType_BASE_COLOR;
        if (source->GetTextureCount(type) == 0) {
            type = aiTextureType_DIFFUSE;
        }
        if (source->GetTexture(type, 0, &textureName) != AI_SUCCESS) {
            continue;
        }
        const aiTexture* embedded = scene->GetEmbeddedTexture(textureName.C_Str());
        if (embedded != nullptr) {
            std::string key = "embedded://preview/" + path.generic_string() + "/" + textureName.C_Str();
            if (embedded->mHeight == 0) {
                TextureManager::GetInstance()->LoadTextureFromMemory(key,
                    reinterpret_cast<const std::uint8_t*>(embedded->pcData), embedded->mWidth);
            } else {
                TextureManager::GetInstance()->LoadTextureFromBGRA(key,
                    reinterpret_cast<const std::uint8_t*>(embedded->pcData),
                    embedded->mWidth, embedded->mHeight);
            }
            material.textureFilePath = key;
        } else {
            auto texturePath = (path.parent_path() / textureName.C_Str()).lexically_normal();
            if (std::filesystem::is_regular_file(texturePath)) {
                material.textureFilePath = texturePath.string();
            }
        }
    }
    float infinity = std::numeric_limits<float>::infinity();
    Vector3 minimum { infinity, infinity, infinity };
    Vector3 maximum { -infinity, -infinity, -infinity };
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const auto* mesh = scene->mMeshes[meshIndex];
        if (mesh->mNumVertices == 0 || !(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) {
            continue;
        }
        MeshPrimitive primitive {};
        primitive.mode = PrimitiveMode::Triangles;
        primitive.materialIndex = mesh->mMaterialIndex;
        for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
            auto position = mesh->mVertices[vertexIndex];
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                throw std::runtime_error("Model contains non-finite vertex positions");
            }
            VertexData vertex {};
            vertex.position = { -position.x, position.y, position.z, 1.0f };
            if (mesh->HasNormals()) {
                const auto& normal = mesh->mNormals[vertexIndex];
                vertex.normal = { -normal.x, normal.y, normal.z };
            } else {
                vertex.normal = { 0.0f, 1.0f, 0.0f };
            }
            if (mesh->HasTextureCoords(0)) {
                const auto& uv = mesh->mTextureCoords[0][vertexIndex];
                vertex.texcoord = { uv.x, uv.y };
            }
            primitive.vertices.push_back(vertex);
            minimum.x = (std::min)(minimum.x, vertex.position.x);
            minimum.y = (std::min)(minimum.y, vertex.position.y);
            minimum.z = (std::min)(minimum.z, vertex.position.z);
            maximum.x = (std::max)(maximum.x, vertex.position.x);
            maximum.y = (std::max)(maximum.y, vertex.position.y);
            maximum.z = (std::max)(maximum.z, vertex.position.z);
        }
        for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const auto& face = mesh->mFaces[faceIndex];
            if (face.mNumIndices == 3) {
                primitive.indices.insert(primitive.indices.end(), face.mIndices, face.mIndices + 3);
            }
        }
        if (!primitive.indices.empty()) {
            data.primitives.push_back(std::move(primitive));
        }
    }
    if (data.primitives.empty()) {
        throw std::runtime_error("This model has no triangle meshes");
    }
    item.center = (minimum + maximum) * 0.5f;
    Vector3 extent = maximum - minimum;
    item.radius = (std::max)(0.01f, std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z) * 0.5f);
    item.model = std::make_unique<Model>();
    item.model->Initialize(&modelCommon_, data);
}

void ModelPreviewApp::ApplyMaterial(PreviewItem& item)
{
    item.object->SetMaterial("resources/Shaders/Object3D/" + item.shader);
    item.object->SetVertexShaderPath(kDefaultVertexShader);
    if (item.shader == "IceJellyfish") {
        item.object->SetVertexShaderPath("resources/Shaders/Object3D/IceJellyfish/Render.VS.hlsl");
    }
    item.object->SetColor(item.color);
    item.object->SetTransparent(item.transparent);
    item.object->SetEnableEnvironmentMap(item.environment);
    item.object->SetEnvironmentMapStrength(item.reflection);
    item.object->GetMaterial()->shininess = item.shininess;
}

void ModelPreviewApp::Load(const std::filesystem::path& path, bool append)
{
    try {
        if (capture_.IsRecording()) {
            throw std::runtime_error("Stop GIF recording before changing models");
        }
        dx_->WaitForGPU();
        auto item = std::make_unique<PreviewItem>();
        item->name = path.filename().string();
        Import(std::filesystem::absolute(path), *item);
        item->object = std::make_unique<Object3d>();
        item->object->Initialize(Object3dManager::GetInstance());
        item->object->SetModel(item->model.get());
        item->object->SetCamera(camera_.get());
        std::string name = path.filename().string();
        if (path.generic_string().find("Environment/Ice/") != std::string::npos || name == "IceSpike.obj") {
            std::string gameModelPath = "Environment/Ice/" + name;
            if (path.parent_path().filename() != "Ice" && name == "IceSpike.obj") {
                gameModelPath = name;
            }
            item->shader = std::filesystem::path(
                GameplayVisualPreset::IceMaterialFolder(gameModelPath)).filename().string();
            item->color = { 0.82f, 0.94f, 1.0f, 1.0f };
            item->shininess = GameplayVisualPreset::kIceShininess;
            item->environment = false;
            item->reflection = 0.0f;
            // Match the scale and placement used by the game.
            for (const auto& placement : iceLayout_.objects) {
                if (placement.fileName == gameModelPath && !placement.disabled) {
                    item->position = placement.translation;
                    item->rotation = placement.rotation;
                    item->axisScale = placement.scale;
                    item->stageTransform = true;
                    break;
                }
            }
        } else if (name.find("IceJellyfishBell") != std::string::npos ||
            name.find("IceJellyfishSegment") != std::string::npos ||
            name.find("IceJellyfishTip") != std::string::npos) {
            item->shader = "IceJellyfish";
            item->color = { 0.65f, 0.86f, 1.0f, 0.95f };
            item->shininess = 96.0f;
            item->environment = true;
            item->reflection = 0.10f;
            item->transparent = true;
        }
        ApplyMaterial(*item);
        // Validate/compile the pipeline before replacing the current model.
        Object3dManager::GetInstance()->BindPipeline(item->object->GetPixelShaderPath(),
            item->transparent, false, item->object->GetVertexShaderPath());
        if (!append) {
            items_.clear();
        }
        items_.push_back(std::move(item));
        selected_ = static_cast<int>(items_.size()) - 1;
        Fit();
        error_.clear();
        Logger::Log("Preview model loaded: " + path.generic_string());
    } catch (const std::exception& error) {
        error_ = error.what();
        Logger::Error(error_);
    }
}

Matrix4x4 ModelPreviewApp::ItemTransform(const PreviewItem& item) const
{
    Matrix4x4 transform = MatrixMath::MakeAffineMatrix(item.axisScale * item.scale,
        item.rotation, item.position);
    if (item.stageTransform) { return transform; }
    Matrix4x4 recenter = MatrixMath::MakeTranslateMatrix(item.center * -1.0f);
    return MatrixMath::Multiply(recenter, transform);
}

Vector3 ModelPreviewApp::ItemCenter(const PreviewItem& item) const
{
    return MatrixMath::Transform(item.center, ItemTransform(item));
}

void ModelPreviewApp::Fit()
{
    target_ = {};
    sceneRadius_ = 1.0f;
    for (const auto& item : items_) { target_ = target_ + ItemCenter(*item); }
    if (!items_.empty()) { target_ = target_ * (1.0f / static_cast<float>(items_.size())); }
    for (const auto& item : items_) {
        Vector3 delta = ItemCenter(*item) - target_;
        float offset = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        float largestScale = (std::max)({ std::abs(item->axisScale.x),
            std::abs(item->axisScale.y), std::abs(item->axisScale.z) });
        sceneRadius_ = (std::max)(sceneRadius_, item->radius * item->scale * largestScale + offset);
    }
    // Fit projected vertices instead of a max-scale sphere. The latter places
    // wide, shallow ice islands unnecessarily far into the game's distance fog.
    float tangentY = std::tan(camera_->GetFovY() * 0.5f);
    float tangentX = tangentY * camera_->GetAspectRatio();
    Vector3 right { std::cos(orbitYaw_), 0.0f, std::sin(orbitYaw_) };
    Vector3 up { -std::sin(orbitYaw_) * std::sin(orbitPitch_), std::cos(orbitPitch_),
        std::cos(orbitYaw_) * std::sin(orbitPitch_) };
    Vector3 back { std::sin(orbitYaw_) * std::cos(orbitPitch_), std::sin(orbitPitch_),
        -std::cos(orbitYaw_) * std::cos(orbitPitch_) };
    distance_ = 1.0f;
    for (const auto& item : items_) {
        Matrix4x4 world = ItemTransform(*item);
        for (const auto& primitive : item->model->GetModelData().primitives) {
            for (const auto& vertex : primitive.vertices) {
                Vector3 local { vertex.position.x, vertex.position.y, vertex.position.z };
                Vector3 delta = MatrixMath::Transform(local, world) - target_;
                float x = delta.x * right.x + delta.y * right.y + delta.z * right.z;
                float y = delta.x * up.x + delta.y * up.y + delta.z * up.z;
                float z = delta.x * back.x + delta.y * back.y + delta.z * back.z;
                distance_ = (std::max)(distance_, std::abs(x) / tangentX + z);
                distance_ = (std::max)(distance_, std::abs(y) / tangentY + z);
            }
        }
    }
    distance_ = distance_ * 1.10f + sceneRadius_ * 0.05f;
}

void ModelPreviewApp::ChooseFile()
{
    wchar_t file[32768] {};
    OPENFILENAMEW dialog {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = WinApp::GetInstance()->GetHwnd();
    dialog.lpstrFilter = L"3D Models\0*.obj;*.gltf;*.glb;*.fbx;*.dae;*.ply\0All files\0*.*\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = _countof(file);
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        Load(file, append_);
    }
}

void ModelPreviewApp::Controls()
{
    if (!showControls_) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310, 690), ImGuiCond_FirstUseEver);
    ImGui::Begin("Model Preview");
    ImGui::TextWrapped("Drag RMB: orbit | MMB: pan | Wheel: zoom | F: fit | Tab: hide UI | F12: PNG");
    ImGui::TextWrapped("Stage03 game lighting, sky, outlines and fog. Static model poses; no gameplay particles.");
    if (ImGui::Button("Save PNG")) { capture_.RequestPng(); }
    ImGui::SameLine();
    if (capture_.IsRecording()) {
        if (ImGui::Button("Stop GIF")) { capture_.StopGif(); }
        ImGui::Text("Frame %d / %d", capture_.FrameIndex(), capture_.FrameCount());
    } else {
        ImGui::BeginDisabled(items_.empty());
        if (ImGui::Button("Record GIF")) {
            recordingYaw_ = orbitYaw_;
            capture_.StartGif(gifFps_, gifFps_ * gifSeconds_);
        }
        ImGui::EndDisabled();
    }
    ImGui::TextWrapped("%s", capture_.Status().c_str());
    ImGui::Separator();
    ImGui::BeginDisabled(capture_.IsRecording());
    ImGui::Checkbox("Game rendering (Stage03)", &gameLook_);
    if (ImGui::Button("Open model...")) { ChooseFile(); }
    ImGui::SameLine();
    ImGui::Checkbox("Add", &append_);
    ImGui::InputTextWithHint("##filter", "Filter project models", filter_, sizeof(filter_));
    if (ImGui::BeginListBox("##models", ImVec2(-1, 145))) {
        for (const auto& path : modelPaths_) {
            if (filter_[0] != '\0' && path.find(filter_) == std::string::npos) {
                continue;
            }
            std::string label = path.substr(std::string("resources/Models/").size());
            if (ImGui::Selectable(label.c_str())) { Load(path, append_); }
        }
        ImGui::EndListBox();
    }
    if (ImGui::Button("Rescan")) { ScanModels(); }
    ImGui::SameLine();
    if (ImGui::Button("Fit view")) { Fit(); }
    if (!items_.empty()) {
        if (ImGui::BeginCombo("Object", items_[selected_]->name.c_str())) {
            for (int index = 0; index < static_cast<int>(items_.size()); ++index) {
                ImGui::PushID(index);
                if (ImGui::Selectable(items_[index]->name.c_str(), selected_ == index)) { selected_ = index; }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        auto& item = *items_[selected_];
        if (ImGui::BeginCombo("Material / PS", item.shader.c_str())) {
            for (const auto& shader : shaderNames_) {
                if (ImGui::Selectable(shader.c_str(), shader == item.shader)) { item.shader = shader; }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Reload shaders")) {
            try {
                Object3dManager::GetInstance()->ReloadMaterialPipelines();
                error_.clear();
            } catch (const std::exception& error) { error_ = error.what(); }
        }
        ImGui::ColorEdit4("Tint", &item.color.x);
        ImGui::Checkbox("Transparent", &item.transparent);
        ImGui::Checkbox("Environment reflection", &item.environment);
        ImGui::SliderFloat("Reflection", &item.reflection, 0.0f, 0.5f);
        ImGui::SliderFloat("Shininess", &item.shininess, 0.0f, 128.0f);
        ImGui::DragFloat3("Position", &item.position.x, sceneRadius_ * 0.01f);
        ImGui::DragFloat3("Rotation (rad)", &item.rotation.x, 0.01f);
        ImGui::DragFloat("Scale", &item.scale, 0.01f, 0.01f, 100.0f);
        if (item.stageTransform) {
            ImGui::Text("Stage scale: %.1f, %.1f, %.1f", item.axisScale.x, item.axisScale.y, item.axisScale.z);
        }
        ApplyMaterial(item);
        if (ImGui::Button("Remove object")) {
            dx_->WaitForGPU();
            items_.erase(items_.begin() + selected_);
            selected_ = 0;
            Fit();
        }
    }
    ImGui::BeginDisabled(gameLook_);
    ImGui::ColorEdit3("Background", &background_.x);
    ImGui::SliderFloat3("Light direction", &lightDirection_.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Light intensity", &lightIntensity_, 0.0f, 2.0f);
    ImGui::SliderFloat("Ambient", &ambient_, 0.0f, 1.0f);
    ImGui::EndDisabled();
    ImGui::TextWrapped("Ice/Archive presets use fixed shader lighting; light controls affect Standard/Toon.");
    ImGui::Checkbox("Animate shader", &animate_);
    ImGui::SliderInt("GIF fps", &gifFps_, 5, 30);
    ImGui::SliderInt("GIF seconds", &gifSeconds_, 1, 10);
    ImGui::Checkbox("One full orbit", &turntable_);
    ImGui::EndDisabled();
    ImGui::TextWrapped("PNG: 1280x720 | GIF: 640x360, 256 colors");
    if (!error_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.3f, 1), "%s", error_.c_str());
    }
    ImGui::End();
}

void ModelPreviewApp::Update()
{
    TimeManager::GetInstance()->Update();
    ImGuiManager::GetInstance()->Begin();
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) { showControls_ = !showControls_; }
        if (ImGui::IsKeyPressed(ImGuiKey_F12)) { capture_.RequestPng(); }
        if (ImGui::IsKeyPressed(ImGuiKey_F) && !capture_.IsRecording()) { Fit(); }
    }
    if (!io.WantCaptureMouse && !capture_.IsRecording()) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            orbitYaw_ -= io.MouseDelta.x * 0.008f;
            orbitPitch_ = std::clamp(orbitPitch_ + io.MouseDelta.y * 0.008f, -1.45f, 1.45f);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            float factor = distance_ * 0.001f;
            target_.x -= io.MouseDelta.x * std::cos(orbitYaw_) * factor;
            target_.z -= io.MouseDelta.x * std::sin(orbitYaw_) * factor;
            target_.y += io.MouseDelta.y * factor;
        }
        distance_ *= std::exp(-io.MouseWheel * 0.12f);
        distance_ = std::clamp(distance_, sceneRadius_ * 0.03f, sceneRadius_ * 80.0f);
    }
    Controls();
    if (wasRecording_ && !capture_.IsRecording()) { orbitYaw_ = recordingYaw_; }
    wasRecording_ = capture_.IsRecording();
    float delta = TimeManager::GetInstance()->GetDeltaTime();
    if (capture_.IsRecording()) {
        delta = capture_.FrameSeconds();
        if (turntable_) {
            orbitYaw_ = recordingYaw_ + kTau * capture_.FrameIndex() / capture_.FrameCount();
        }
    }
    if (animate_) { shaderTime_ += delta; }
    Vector3 eye {
        target_.x + std::sin(orbitYaw_) * std::cos(orbitPitch_) * distance_,
        target_.y + std::sin(orbitPitch_) * distance_,
        target_.z - std::cos(orbitYaw_) * std::cos(orbitPitch_) * distance_
    };
    camera_->LookAt(eye, target_);
    camera_->Update();
    Vector3 direction = lightDirection_;
    if (direction.x * direction.x + direction.y * direction.y + direction.z * direction.z < 0.0001f) {
        direction = { 0.0f, -1.0f, 0.0f };
    }
    if (gameLook_) {
        GameplayVisualPreset::ApplyLighting("stage03");
        GameplayVisualPreset::ConfigurePostEffects(false);
    } else {
        LightManager::GetInstance()->SetDirectional({ 1, 1, 1, 1 }, direction, lightIntensity_);
        LightManager::GetInstance()->SetAmbientColor({ 1, 1, 1 });
        LightManager::GetInstance()->SetAmbientIntensity(ambient_);
        SceneManager::GetInstance()->ClearPostEffects();
    }
    LightManager::GetInstance()->Update();
    for (auto& item : items_) {
        float partType = 0.0f;
        if (item->name.find("Segment") != std::string::npos || item->name.find("Tip") != std::string::npos) {
            partType = 1.0f;
        }
        item->object->SetVertexShaderParameters({ shaderTime_, 1.0f, 0.0f, partType });
        item->object->SetCustomWorldMatrix(ItemTransform(*item));
        item->object->Update();
    }
    skyBox_->Update(camera_.get());
    postEffects_->Update(camera_.get());
    ImGuiManager::GetInstance()->End();
}

void ModelPreviewApp::Draw()
{
    TextureManager::GetInstance()->FlushUploads();
    SrvManager::GetInstance()->PreDraw();
    auto* commands = dx_->GetCommandList();
    postEffects_->PreDrawDepth();
    Vector4 clearColor = background_;
    if (gameLook_) { clearColor = SceneManager::GetInstance()->GetSceneClearColor(); }
    offscreen_->SetClearColor(clearColor);
    offscreen_->PreDraw(postEffects_->GetDepthDSVHandle());
    if (gameLook_) {
        SkyBoxManager::GetInstance()->PreDraw();
        skyBox_->Draw(commands);
    }
    Object3dManager::GetInstance()->PreDraw();
    try {
        for (const auto& item : items_) {
            if (!item->transparent) { item->object->Draw(); }
        }
        // Small preview scenes: select the farthest remaining transparent object without lambdas.
        std::vector<bool> drawn(items_.size(), false);
        for (std::size_t pass = 0; pass < items_.size(); ++pass) {
            int farthest = -1;
            float farthestDistance = -1.0f;
            for (std::size_t index = 0; index < items_.size(); ++index) {
                if (drawn[index] || !items_[index]->transparent) { continue; }
                Vector3 offset = ItemCenter(*items_[index]) - camera_->GetTranslate();
                float distance = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
                if (distance > farthestDistance) {
                    farthestDistance = distance;
                    farthest = static_cast<int>(index);
                }
            }
            if (farthest < 0) { break; }
            items_[farthest]->object->Draw();
            drawn[farthest] = true;
        }
    } catch (const std::exception& error) { error_ = error.what(); }
    postEffects_->PostDrawDepth();
    offscreen_->PostDraw();
    dx_->PreDraw();
    postEffects_->SetBoostRadialBlurParameters(false);
    // Use the exact game's composition order, with an empty particle layer.
    postEffects_->PrepareSceneForParticleDraw(SceneManager::GetInstance(), offscreen_->GetSrvHandleGPU());
    postEffects_->PrepareDepthForParticleDraw();
    postEffects_->BeginParticleDraw();
    postEffects_->EndParticleDraw();
    postEffects_->PostDrawDepth();
    postEffects_->ApplyAfterParticleDraw(SceneManager::GetInstance());
    capture_.Prepare();
    ImGuiManager::GetInstance()->Draw();
    dx_->PostDraw();
    capture_.Complete();
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

void FindResources()
{
    wchar_t executable[32768] {};
    GetModuleFileNameW(nullptr, executable, _countof(executable));
    std::filesystem::path parent = std::filesystem::path(executable).parent_path();
    const std::filesystem::path executableDirectory = parent;
    // Prefer the repository over old resource copies left beside game builds.
    for (int depth = 0; depth < 6; ++depth) {
        if (std::filesystem::is_directory(parent / "project/resources/Models")) {
            std::filesystem::current_path(parent / "project");
            return;
        }
        parent = parent.parent_path();
    }
    if (std::filesystem::is_directory("resources/Models")) { return; }
    if (std::filesystem::is_directory(executableDirectory / "resources/Models")) {
        std::filesystem::current_path(executableDirectory);
        return;
    }
    throw std::runtime_error("Could not find resources/Models beside the app or in the repository");
}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
    ModelPreviewApp app;
    int exitCode = 0;
    try {
        FindResources();
        Logger::Initialize();
        app.Initialize();
        // Hidden deterministic export verifies real DX12 drawing and both encoders without launching the game.
        bool smokeTest = std::string(commandLine).find("--smoke-test") != std::string::npos;
        if (smokeTest) {
            app.Load("resources/Models/Environment/Ice/ice_boulder.obj", false);
            if (!app.Error().empty()) { throw std::runtime_error(app.Error()); }
            app.Capture().RequestPng();
            app.Capture().StartGif(15, 8);
            for (int frame = 0; frame < 8; ++frame) {
                app.Update();
                app.Draw();
            }
            if (!app.Error().empty() || app.Capture().Status().find("Saved:") != 0) {
                throw std::runtime_error("Preview smoke capture failed: " + app.Capture().Status());
            }
            const char* smokeModels[] = {
                "resources/Models/Environment/Ice/IceSpike.obj",
                "resources/Models/Environment/Ice/ice_slab.obj",
                "resources/Models/Environment/Ice/ice_arch.obj",
                "resources/Models/Environment/Ice/ice_island.obj",
                "resources/Models/Environment/Ice/crystal.obj",
                "resources/Models/AnimatedCube.gltf",
                "resources/Models/Boss/IceJellyfish/IceJellyfishBell.obj"
            };
            for (const char* model : smokeModels) {
                app.Load(model, false);
                if (!app.Error().empty()) { throw std::runtime_error(app.Error()); }
                app.Capture().RequestPng();
                app.Update();
                app.Draw();
                if (!app.Error().empty() || app.Capture().Status().find("Saved:") != 0) {
                    throw std::runtime_error("Model preview capture failed: " + std::string(model));
                }
            }
            Object3dManager::GetInstance()->ReloadMaterialPipelines();
            app.Load("resources/Models/Environment/Ice/ice_boulder.obj", true);
            app.Update();
            app.Draw();
            if (!app.Error().empty()) { throw std::runtime_error(app.Error()); }
            app.Load("resources/Models/missing-preview-test.obj", false);
            if (app.Error().empty()) { throw std::runtime_error("Missing model was accepted"); }
            app.Load("resources/Models/Environment/Ice/ice_boulder.obj", false);
            if (!app.Error().empty()) { throw std::runtime_error(app.Error()); }
            Logger::Log("ModelPreview smoke test passed: 8 models, PNG + GIF, multiple objects, shader reload, invalid-file recovery");
        } else {
            WinApp::GetInstance()->Show();
            app.Load("resources/Models/Environment/Ice/ice_boulder.obj", false);
            while (!WinApp::GetInstance()->ProcessMessage()) {
                app.Update();
                app.Draw();
            }
        }
        app.Finalize();
    } catch (const std::exception& error) {
        Logger::Error(error.what());
        OutputDebugStringA(error.what());
        exitCode = 1;
    }
    Logger::Finalize();
    WinApp::FinalizeInstance();
    return exitCode;
}
