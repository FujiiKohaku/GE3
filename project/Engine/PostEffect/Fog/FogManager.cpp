#include "Engine/PostEffect/Fog/FogManager.h"

#include "Engine/Winapp/WinApp.h"

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

    fogData_->time += 1.0f / 60.0f;
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

    fogData_->color = { 0.65f, 0.75f, 0.85f, 1.0f };

    fogData_->distance.start = 200.0f;
    fogData_->distance.end = 300.0f;
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
