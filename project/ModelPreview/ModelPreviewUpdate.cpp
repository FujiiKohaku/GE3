#include "ModelPreviewApp.h"
#include "App/Game/Stage/GameplayVisualPreset.h"
#include "App/Scene/Common/SceneManager.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/Camera/Camera.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr float kTau = std::numbers::pi_v<float> * 2.0f;
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
