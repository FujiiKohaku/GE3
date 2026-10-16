#pragma once
// Opt-in GPU verification fixture: KohakuEngine.exe --verify-object-fog
#include "App/Scene/Common/BaseScene.h"
#include "App/Scene/Common/SceneManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/PostEffect/Volumetric/VolumetricLightRenderer.h"
#include "Engine/SuperResolution/DlssSuperResolution.h"
#include "Engine/Screenshot/ScreenshotManager.h"
#include "Engine/Logger/Logger.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/Light/LightManager.h"
#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"
#include <cassert>
#include <d3d12sdklayers.h>

class ObjectFogVerificationScene final : public BaseScene {
public:
    explicit ObjectFogVerificationScene(Renderer* renderer) : renderer_(renderer) {}
    void Initialize() override {
        if (SUCCEEDED(DirectXCommon::GetInstance()->GetDevice()->QueryInterface(IID_PPV_ARGS(&infoQueue_)))) {
            infoQueue_->RegisterMessageCallback([](D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity,
                D3D12_MESSAGE_ID, LPCSTR message, void*) {
                if (severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
                    Logger::Error(std::string("ObjectFogVerification D3D12: ") + message);
                    Logger::Flush();
                }
            }, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &callbackId_);
        }
        Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
        camera_ = std::make_unique<Camera>();
        camera_->Initialize();
        camera_->SetFarClip(2000.0f);
        camera_->LookAt({ 0, 0, 0 }, { 0, 0, 1 });
        camera_->Update();
        Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
        ShowCursor(TRUE);
        ClipCursor(nullptr);
        renderer_->GetDlssSuperResolution()->SetEnabled(false);
        renderer_->SetTemporalAntiAliasing(false, false);
        renderer_->SetLowResolutionRendering(false);
        auto* scene = SceneManager::GetInstance();
        scene->ClearPostEffects();
        scene->AddPostEffect(PostEffectType::Fog, PostEffectStage::BeforeParticle);
        scene->SetSceneDistanceFog({ 0, 100, 1, 1 });
        scene->SetSceneFogColor({ 0.1f, 0.18f, 0.26f, 1 });
        auto* volume = scene->GetVolumetricLightRenderer();
        volume->SetEnabled(false);
        LightManager::GetInstance()->SetAtmosphere(false, 0, 0, 0);
        cube_ = ModelManager::GetInstance()->CreateCube("resources/Textures/white.png");
        sphere_ = ModelManager::GetInstance()->Load("Enemy/Bullet/LowPolySphere/lowPolySphere.obj");
        background_ = MakeObject(cube_, { 0, 0, 120 }, { 100, 100, 2 }, { 0.25f, 0.35f, 0.45f, 1 });
        occluder_ = MakeObject(cube_, { 0, -7, 30 }, { 3, 3, 1 }, { 1, 1, 0.2f, 1 });
        for (int index = 0; index < 2; ++index) {
            auto bullet = std::make_unique<NormalEnemyBullet>();
            assert(bullet->IsFogEnabled());
            bullet->SetFogEnabled(index == 0);
            bullet->Initialize(sphere_);
            assert(bullet->GetObject3d()->IsFogEnabled() == (index == 0));
            bullet->SetTranslate({ index == 0 ? -10.0f : 10.0f, 6, 40 });
            bullet->SetScale({ 4, 4, 4 });
            bullets_.push_back(std::move(bullet));
        }
        auto hidden = std::make_unique<NormalEnemyBullet>();
        hidden->SetFogEnabled(false);
        hidden->Initialize(sphere_);
        hidden->SetTranslate({ 0, -14, 60 }); // Same projected center as the occluder.
        hidden->SetScale({ 2, 2, 2 });
        bullets_.push_back(std::move(hidden));
        near_ = MakeObject(sphere_, { 0, 0, 36 }, { 4, 4, 4 }, { 1, 0.15f, 0.05f, 0.6f });
        far_ = MakeObject(sphere_, { 0, 0, 44 }, { 5, 5, 5 }, { 0.05f, 0.6f, 1, 0.6f });
        near_->SetTransparent(true); near_->SetFogEnabled(false);
        far_->SetTransparent(true); // Default fog ON; order must not depend on fog state.
        opaqueOn_ = MakeObject(cube_, { -10, -8, 40 }, { 6, 6, 6 }, { 0.1f, 0.8f, 0.15f, 1 });
        opaqueOff_ = MakeObject(cube_, { 10, -8, 40 }, { 6, 6, 6 }, { 0.1f, 0.8f, 0.15f, 1 });
        assert(opaqueOff_->IsFogEnabled());
        opaqueOff_->SetFogEnabled(false);
        opaqueHidden_ = MakeObject(cube_, { 0, -14, 60 }, { 4, 4, 4 }, { 1, 0, 0, 1 });
        opaqueHidden_->SetFogEnabled(false);
        label_ = std::make_unique<Text>();
        label_->Initialize("resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf");
        label_->SetPosition({ 30, 35 }); label_->SetFontSize(22);
        label_->SetText("LEFT: FOG ON | RIGHT: FOG OFF | GREEN: OPAQUE | CENTER: ALPHA ORDER / OCCLUSION");
        label_->Update();
        Logger::Log("ObjectFogVerification phase=distance native");
    }
    void Update() override {
        ++frame_;
        if (frame_ == 30 || frame_ == 90 || frame_ == 150 || frame_ == 210 || frame_ == 270 || frame_ == 330 || frame_ == 390)
            ScreenshotManager::GetInstance()->RequestCapture(true, false);
        if (frame_ == 60) {
            auto* volume = SceneManager::GetInstance()->GetVolumetricLightRenderer();
            volume->SetEnabled(true); volume->SetLocalFogEnabled(true);
            volume->SetHeightFog(100, 0.02f, 0.1f); volume->SetNoiseParameters(0.1f, 0, {});
            volume->SetFogColor({ 0.1f, 0.18f, 0.26f }); volume->SetLightIntensity(0);
            LightManager::GetInstance()->SetAtmosphere(true, 0.01f, 1.0f, 0.1f);
            Logger::Log("ObjectFogVerification phase=distance+volume native");
        }
        if (frame_ == 120) {
            reverse_ = true;
            Logger::Log("ObjectFogVerification phase=reversed submissions");
        }
        if (frame_ == 180) {
            renderer_->SetTemporalAntiAliasing(true, false);
            renderer_->SetLowResolutionRendering(true, 960, 540);
            Logger::Log("ObjectFogVerification phase=TAA 960x540");
        }
        if (frame_ == 240) {
            renderer_->SetLowResolutionRendering(false);
            renderer_->SetTemporalAntiAliasing(false, false);
            renderer_->GetDlssSuperResolution()->SetEnabled(true);
            Logger::Log("ObjectFogVerification phase=DLAA requested");
        }
        if (frame_ == 295) Logger::Log("ObjectFogVerification DLAA=" + renderer_->GetDlssSuperResolution()->GetStatus());
        if (frame_ == 300) {
            opaqueOff_->SetFogEnabled(true); opaqueHidden_->SetFogEnabled(true);
            label_->SetText("ALL OPAQUE: FOG ON | MASK RESET CHECK"); label_->Update();
            Logger::Log("ObjectFogVerification phase=all opaque flags ON, fog still ON");
        }
        if (frame_ == 360) {
            SceneManager::GetInstance()->ClearPostEffects();
            SceneManager::GetInstance()->GetVolumetricLightRenderer()->SetEnabled(false);
            LightManager::GetInstance()->SetAtmosphere(false, 0, 0, 0);
            label_->SetText("ALL FOG: OFF | OPAQUE FLAGS: ON"); label_->Update();
            Logger::Log("ObjectFogVerification phase=global fog OFF, all opaque flags ON");
        }
        if (frame_ == 420) { Logger::Log("ObjectFogVerification complete"); PostQuitMessage(0); }
    }
    void Draw3D() override {
        Object3dManager::GetInstance()->PreDraw();
        background_->Draw();
        if (reverse_) occluder_->Draw();
        opaqueHidden_->Draw(); opaqueOn_->Draw(); opaqueOff_->Draw();
        for (const auto& bullet : bullets_) bullet->Draw();
        if (reverse_) { far_->Draw(); near_->Draw(); }
        else { near_->Draw(); far_->Draw(); }
        // Deliberately submit opaque geometry last; queued bullets must still be occluded.
        if (!reverse_) occluder_->Draw();
    }
    void Draw2D() override { TextRenderer::GetInstance()->PreDraw(); label_->Draw(); }
    void DrawParticle() override {}
    void DrawImGui() override {}
    void Finalize() override {
        if (infoQueue_) infoQueue_->UnregisterMessageCallback(callbackId_);
        bullets_.clear(); near_.reset(); far_.reset(); background_.reset(); occluder_.reset();
        opaqueOn_.reset(); opaqueOff_.reset(); opaqueHidden_.reset();
        Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    }
private:
    std::unique_ptr<Object3d> MakeObject(Model* model, Vector3 position, Vector3 scale, Vector4 color) {
        auto object = std::make_unique<Object3d>(); object->Initialize(Object3dManager::GetInstance());
        object->SetModel(model); object->SetTranslate(position); object->SetScale(scale);
        object->SetColor(color); object->SetEnableLighting(false); object->Update(); return object;
    }
    Renderer* renderer_;
    std::unique_ptr<Camera> camera_;
    Model* cube_ = nullptr;
    Model* sphere_ = nullptr;
    std::unique_ptr<Object3d> background_, occluder_, near_, far_;
    std::unique_ptr<Object3d> opaqueOn_, opaqueOff_, opaqueHidden_;
    std::vector<std::unique_ptr<NormalEnemyBullet>> bullets_;
    std::unique_ptr<Text> label_;
    int frame_ = 0;
    bool reverse_ = false;
    Microsoft::WRL::ComPtr<ID3D12InfoQueue1> infoQueue_;
    DWORD callbackId_ = 0;
};
