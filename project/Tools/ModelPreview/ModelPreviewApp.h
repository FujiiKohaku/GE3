#pragma once
#include "PreviewCapture.h"
#include "PreviewItem.h"
#include "Engine/3D/ModelCommon.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Camera;
class OffscreenRenderer;
class PostEffectManager;
class SkyBox;

class ModelPreviewApp {
public:
    ModelPreviewApp();
    ~ModelPreviewApp();
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
