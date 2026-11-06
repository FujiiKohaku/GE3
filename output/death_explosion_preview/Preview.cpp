#include "App/Scene/Application/Game.h"
#include "App/Scene/Common/BaseScene.h"
#include "App/Scene/Common/SceneManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Capture/GifRecordingManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Screenshot/ScreenshotManager.h"
#include "Engine/Time/TimeManager.h"
#include <array>
#include <cstdio>

class DeathExplosionPreviewScene final : public BaseScene {
public:
    void Initialize() override
    {
        camera_.Initialize();
        camera_.LookAt({ 0.0f, 0.0f, -30.0f }, {});
        camera_.Update();
        Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
        Object3dManager::GetInstance()->SetDefaultCamera(&camera_);
        SceneManager::GetInstance()->ClearPostEffects();
        SceneManager::GetInstance()->SetSceneClearColor({ 0.045f, 0.04f, 0.035f, 1.0f });
        EffectManager* effects = EffectManager::GetInstance();
        if (!effects->IsInitialized()) {
            effects->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), &camera_, false);
        }
        effects->SetCamera(&camera_);
        for (const char* effectName : kEffectNames) {
            const std::string directory = std::string("resources/Effects/") + effectName;
            effects->RegisterEffect({ effectName, directory, directory + "/Effect.json",
                directory + "/Emit.CS.hlsl", directory + "/Update.CS.hlsl" });
        }
    }
    void Finalize() override { EffectManager::GetInstance()->StopAllEffects(); }
    void Update() override
    {
        const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
        elapsedSeconds_ += deltaTimeSeconds;
        if (!isRecording_ && elapsedSeconds_ >= 0.4f) {
            GifRecordingManager::GetInstance()->ToggleRecording();
            isRecording_ = true;
        }
        if (elapsedSeconds_ >= nextBurstSeconds_) {
            for (const char* effectName : kEffectNames) {
                const EffectHandle handle = EffectManager::GetInstance()->PlayEffect(effectName, {});
                if (handle == kInvalidEffectHandle) { throw std::runtime_error("Preview effect failed to start"); }
            }
            burstElapsedSeconds_ = 0.0f;
            nextBurstSeconds_ += 1.4f;
        }
        EffectManager::GetInstance()->Update();
        burstElapsedSeconds_ += deltaTimeSeconds;
        if (captureIndex_ < kCaptureTimesSeconds.size() && elapsedSeconds_ < 1.9f
            && burstElapsedSeconds_ >= kCaptureTimesSeconds[captureIndex_] && elapsedSeconds_ >= 0.8f) {
            ScreenshotManager::GetInstance()->RequestCapture(true, false);
            ++captureIndex_;
        }
    }
    void Draw2D() override {}
    void Draw3D() override {}
    void DrawImGui() override {}
    void DrawParticle() override
    {
        EffectManager::GetInstance()->PreDraw();
        EffectManager::GetInstance()->Draw();
    }
    bool IsFinished() const { return elapsedSeconds_ >= 6.5f; }
private:
    inline static constexpr std::array<const char*, 4> kEffectNames = {
        "ExplosionSmoke", "ExplosionFlame", "ExplosionSparks", "ExplosionFlash" };
    inline static constexpr std::array<float, 5> kCaptureTimesSeconds = { 0.06f, 0.14f, 0.26f, 0.44f, 0.65f };
    Camera camera_;
    float elapsedSeconds_ = 0.0f;
    float burstElapsedSeconds_ = -1.0f;
    float nextBurstSeconds_ = 0.8f;
    size_t captureIndex_ = 0;
    bool isRecording_ = false;
};

int main()
{
    Logger::Initialize();
    Game game;
    game.Initialize();
    auto scene = std::make_unique<DeathExplosionPreviewScene>();
    DeathExplosionPreviewScene* preview = scene.get();
    SceneManager::GetInstance()->SetNextScene(std::move(scene));
    for (int frame = 0; frame < 1600; ++frame) {
        if (WinApp::GetInstance()->ProcessMessage()) { break; }
        game.Update();
        game.Draw();
        if (preview->IsFinished()) { break; }
    }
    game.Finalize();
    Logger::Log("Death explosion preview PASS");
    Logger::Finalize();
    WinApp::FinalizeInstance();
    return 0;
}
