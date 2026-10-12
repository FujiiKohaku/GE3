#include <cstring>
#include "Engine/PostEffect/Fog/FogManager.h"

#include "Engine/Winapp/WinApp.h"
#include "Engine/Time/TimeManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void FogManager::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateConstantBuffer();
    SetDefaultData();
}

void FogManager::Update()
{
    if (fogData_ == nullptr) {
        return;
    }

    if (fogData_->distance.end <= fogData_->distance.start) {
        fogData_->distance.end = fogData_->distance.start + 0.01f;
    }

    if (fogData_->distance.start < 0.0f) {
        fogData_->distance.start = 0.0f;
    }

    if (fogData_->distance.curve < 0.01f) {
        fogData_->distance.curve = 0.01f;
    }

    if (fogData_->distance.density < 0.0f) {
        fogData_->distance.density = 0.0f;
    }

    if (fogData_->distance.density > 1.0f) {
        fogData_->distance.density = 1.0f;
    }

    if (fogData_->nearClip < 0.001f) {
        fogData_->nearClip = 0.001f;
    }

    if (fogData_->farClip <= fogData_->nearClip) {
        fogData_->farClip = fogData_->nearClip + 0.001f;
    }

    if (fogData_->fovY < 0.001f) {
        fogData_->fovY = 0.001f;
    }

    if (fogData_->aspectRatio < 0.001f) {
        fogData_->aspectRatio = 0.001f;
    }

    fogData_->time += TimeManager::GetInstance()->GetDeltaTime();
}

void FogManager::DrawImGui()
{
#ifdef USE_IMGUI
    if (fogData_ == nullptr) {
        return;
    }

    ImGui::Begin("Fog");

    bool isEnabled = false;
    if (fogData_->isEnabled != 0) {
        isEnabled = true;
    }

    if (ImGui::Checkbox("Enable Fog", &isEnabled)) {
        if (isEnabled) {
            fogData_->isEnabled = 1;
        } else {
            fogData_->isEnabled = 0;
        }
    }

    bool distanceEnabled = false;
    if (fogData_->distanceEnabled != 0) {
        distanceEnabled = true;
    }

    if (ImGui::Checkbox("Distance Fog", &distanceEnabled)) {
        if (distanceEnabled) {
            fogData_->distanceEnabled = 1;
        } else {
            fogData_->distanceEnabled = 0;
        }
    }

    ImGui::ColorEdit3("Fog Color", &fogData_->color.x);
    ImGui::DragFloat("Fog Start", &fogData_->distance.start, 0.1f, 0.0f, 3000.0f);
    ImGui::DragFloat("Fog End", &fogData_->distance.end, 0.1f, 0.01f, 3000.0f);
    ImGui::DragFloat("Fog Curve", &fogData_->distance.curve, 0.01f, 0.01f, 8.0f);
    ImGui::DragFloat("Fog Density", &fogData_->distance.density, 0.01f, 0.0f, 1.0f);

    ImGui::End();
#endif
}

void FogManager::SetCameraInfo(float nearClip, float farClip, float fovY, float aspectRatio)
{
    if (fogData_ == nullptr) {
        return;
    }

    if (nearClip < 0.001f) {
        nearClip = 0.001f;
    }

    if (farClip <= nearClip) {
        farClip = nearClip + 0.001f;
    }

    if (fovY < 0.001f) {
        fovY = 0.001f;
    }

    if (aspectRatio < 0.001f) {
        aspectRatio = 0.001f;
    }

    if (!std::isfinite(nearClip) || !std::isfinite(farClip) || !std::isfinite(fovY) ||
        !std::isfinite(aspectRatio) || fovY >= 3.14159265f) return;
    fogData_->nearClip = nearClip;
    fogData_->farClip = farClip;
    fogData_->fovY = fovY;
    fogData_->aspectRatio = aspectRatio;
}

const FogData& FogManager::GetFogData() const
{
    return *fogData_;
}

D3D12_GPU_VIRTUAL_ADDRESS FogManager::GetConstantBufferView() const
{
    if (constantBuffer_ == nullptr) {
        return 0;
    }

    return constantBuffer_->GetGPUVirtualAddress();
}

void FogManager::CreateConstantBuffer()
{
    constantBuffer_ = dxCommon_->CreateBufferResource(sizeof(FogData));
    constantBuffer_->SetName(L"FogManager::FogDataCB");
    constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&fogData_));
}

void FogManager::SetDefaultData()
{
    if (fogData_ == nullptr) {
        return;
    }

    fogData_->color = { 0.58f, 0.80f, 0.96f, 1.0f };

    fogData_->distance.start = 380.0f;
    fogData_->distance.end = 720.0f;
    fogData_->distance.density = 1.0f;
    fogData_->distance.curve = 1.2f;

    fogData_->height.startHeight = 0.0f;
    fogData_->height.endHeight = 100.0f;
    fogData_->height.density = 0.0f;
    fogData_->height.padding = 0.0f;

    fogData_->exponential.density = 0.0f;
    fogData_->exponential.falloff = 1.0f;
    fogData_->exponential.padding[0] = 0.0f;
    fogData_->exponential.padding[1] = 0.0f;

    fogData_->noise.scale = 1.0f;
    fogData_->noise.speed = 0.0f;
    fogData_->noise.strength = 0.0f;
    fogData_->noise.padding = 0.0f;

    fogData_->volumetric.scattering = 0.0f;
    fogData_->volumetric.stepLength = 1.0f;
    fogData_->volumetric.maxDistance = 1000.0f;
    fogData_->volumetric.padding = 0.0f;

    fogData_->nearClip = 0.1f;
    fogData_->farClip = 1000.0f;
    fogData_->fovY = 0.45f;
    fogData_->aspectRatio = static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight);

    fogData_->isEnabled = 1;
    fogData_->distanceEnabled = 1;
    fogData_->heightEnabled = 0;
    fogData_->exponentialEnabled = 0;
    fogData_->noiseEnabled = 0;
    fogData_->volumetricEnabled = 0;
    fogData_->time = 0.0f;
    fogData_->padding = 0.0f;
}


bool FogManager::IsValidParameter(const FogData& value)
{
    return (ValueValidation::IsFinite(value.color)) &&
        (ValueValidation::IsFinite(value.nearClip)) &&
        (ValueValidation::IsFinite(value.farClip)) &&
        (ValueValidation::IsFinite(value.fovY)) &&
        (ValueValidation::IsFinite(value.aspectRatio)) &&
        (ValueValidation::IsFinite(value.time)) &&
        (ValueValidation::IsFinite(value.padding)) &&
        (std::isfinite(value.distance.start)) &&
        (std::isfinite(value.distance.end)) &&
        (std::isfinite(value.distance.density)) &&
        (std::isfinite(value.distance.curve)) &&
        (std::isfinite(value.height.startHeight)) &&
        (std::isfinite(value.height.endHeight)) &&
        (std::isfinite(value.height.density)) &&
        (std::isfinite(value.exponential.density)) &&
        (std::isfinite(value.exponential.falloff)) &&
        (std::isfinite(value.noise.scale)) &&
        (std::isfinite(value.noise.speed)) &&
        (std::isfinite(value.noise.strength)) &&
        (std::isfinite(value.volumetric.scattering)) &&
        (std::isfinite(value.volumetric.stepLength)) &&
        (std::isfinite(value.volumetric.maxDistance)) &&
        (value.distance.start >= 0) &&
        (value.distance.end > value.distance.start) &&
        (value.distance.density >= 0) &&
        (value.distance.density <= 1) &&
        (value.distance.curve > 0) &&
        (value.height.density >= 0) &&
        (value.exponential.density >= 0) &&
        (value.exponential.falloff >= 0) &&
        (value.noise.scale > 0) &&
        (value.noise.strength >= 0) &&
        (value.volumetric.scattering >= 0) &&
        (value.volumetric.stepLength > 0) &&
        (value.volumetric.maxDistance > 0) &&
        (value.nearClip > 0) &&
        (value.farClip > value.nearClip) &&
        (value.fovY > 0) &&
        (value.fovY < 3.14159265f) &&
        (value.aspectRatio > 0) &&
        (value.isEnabled == 0 || value.isEnabled == 1) &&
        (value.distanceEnabled == 0 || value.distanceEnabled == 1) &&
        (value.heightEnabled == 0 || value.heightEnabled == 1) &&
        (value.exponentialEnabled == 0 || value.exponentialEnabled == 1) &&
        (value.noiseEnabled == 0 || value.noiseEnabled == 1) &&
        (value.volumetricEnabled == 0 || value.volumetricEnabled == 1);
}
bool FogManager::SetFogData(const FogData& value)
{
    if (!fogData_ || !IsValidParameter(value)) return false;
    *fogData_ = value;
    return true;
}
SettingsEdit<FogData> FogManager::BeginFogEdit()
{
    const FogData snapshot = fogData_ ? *fogData_ : FogData{};
    return SettingsEdit<FogData>(snapshot, [this](const FogData& original, const FogData& edited) {
        if (!fogData_) return false;
        auto value = *fogData_;
        if (std::memcmp(&original.color, &edited.color, sizeof(value.color)) != 0) value.color = edited.color;
        if (std::memcmp(&original.distance, &edited.distance, sizeof(value.distance)) != 0) value.distance = edited.distance;
        if (std::memcmp(&original.height, &edited.height, sizeof(value.height)) != 0) value.height = edited.height;
        if (std::memcmp(&original.exponential, &edited.exponential, sizeof(value.exponential)) != 0) value.exponential = edited.exponential;
        if (std::memcmp(&original.noise, &edited.noise, sizeof(value.noise)) != 0) value.noise = edited.noise;
        if (std::memcmp(&original.volumetric, &edited.volumetric, sizeof(value.volumetric)) != 0) value.volumetric = edited.volumetric;
        if (std::memcmp(&original.nearClip, &edited.nearClip, sizeof(value.nearClip)) != 0) value.nearClip = edited.nearClip;
        if (std::memcmp(&original.farClip, &edited.farClip, sizeof(value.farClip)) != 0) value.farClip = edited.farClip;
        if (std::memcmp(&original.fovY, &edited.fovY, sizeof(value.fovY)) != 0) value.fovY = edited.fovY;
        if (std::memcmp(&original.aspectRatio, &edited.aspectRatio, sizeof(value.aspectRatio)) != 0) value.aspectRatio = edited.aspectRatio;
        if (std::memcmp(&original.isEnabled, &edited.isEnabled, sizeof(value.isEnabled)) != 0) value.isEnabled = edited.isEnabled;
        if (std::memcmp(&original.distanceEnabled, &edited.distanceEnabled, sizeof(value.distanceEnabled)) != 0) value.distanceEnabled = edited.distanceEnabled;
        if (std::memcmp(&original.heightEnabled, &edited.heightEnabled, sizeof(value.heightEnabled)) != 0) value.heightEnabled = edited.heightEnabled;
        if (std::memcmp(&original.exponentialEnabled, &edited.exponentialEnabled, sizeof(value.exponentialEnabled)) != 0) value.exponentialEnabled = edited.exponentialEnabled;
        if (std::memcmp(&original.noiseEnabled, &edited.noiseEnabled, sizeof(value.noiseEnabled)) != 0) value.noiseEnabled = edited.noiseEnabled;
        if (std::memcmp(&original.volumetricEnabled, &edited.volumetricEnabled, sizeof(value.volumetricEnabled)) != 0) value.volumetricEnabled = edited.volumetricEnabled;
        if (std::memcmp(&original.time, &edited.time, sizeof(value.time)) != 0) value.time = edited.time;
        if (std::memcmp(&original.padding, &edited.padding, sizeof(value.padding)) != 0) value.padding = edited.padding;
        return SetFogData(value);
    });
}
