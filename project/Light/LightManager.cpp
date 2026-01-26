#include "LightManager.h"
#include <numbers>
#include "../math//MathStruct.h"
LightManager* LightManager::instance = nullptr;

LightManager* LightManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new LightManager();
    }
    return instance;
}
void LightManager::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;

    // directional light default
    lightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    lightResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightData_));
    lightResource_->SetName(L"Object3d::DirectionalLightCB");
    lightData_->color = { 1, 1, 1, 1 };
    lightData_->direction = Normalize(Vector3 { 0.0f, -1.0f, 0.0f });
    lightData_->intensity = 1.0f;

    // point light default
    pointLightResource_ = dxCommon_->CreateBufferResource(sizeof(PointLight));
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));
    pointLightData_->color = { 1, 1, 1, 1 };
    pointLightData_->position = { 0, 2, 0 };
    pointLightData_->intensity = 1.0f;
    pointLightData_->radius = 10.0f;
    pointLightData_->decay = 1.0f;
    // spot light default
    spotLightResource_ = dxCommon_->CreateBufferResource(sizeof(SpotLight));
    spotLightResource_->Map(0, nullptr, (void**)&spotLightData_);
    spotLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    spotLightData_->position = { 2.0f, 1.25f, 0.0f };
    spotLightData_->distance = 7.0f;
    spotLightData_->direction = Normalize(Vector3 { -1.0f, -1.0f, 0.0f });
    spotLightData_->intensity = 4.0f;
    spotLightData_->decay = 2.0f;
    spotLightData_->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
    spotLightData_->cosFalloffStart = std::cos(std::numbers::pi_v<float> / 6.0f);
}

void LightManager::Update()
{
    // ここは後で必要になったら書く
}

void LightManager::Finalize()
{
    // GPU バッファのマップ解除
    if (lightResource_) {
        lightResource_->Unmap(0, nullptr);
        lightResource_.Reset(); // ComPtr を開放
    }

    delete instance;
    instance = nullptr;
}

void LightManager::SetDirectional(const Vector4& color, const Vector3& dir, float intensity)
{
    lightData_->color = color;
    lightData_->direction = Normalize(dir);
    lightData_->intensity = intensity;
}

void LightManager::SetDirection(const Vector3& dir)
{
    lightData_->direction = Normalize(dir);
}

void LightManager::SetIntensity(float intensity)
{
    lightData_->intensity = intensity;
}

void LightManager::SetPointLight(const Vector4& color,
    const Vector3& pos,
    float intensity)
{
    pointLightData_->color = color;
    pointLightData_->position = pos;
    pointLightData_->intensity = intensity;
}

void LightManager::SetPointPosition(const Vector3& pos)
{
    pointLightData_->position = pos;
}

void LightManager::SetPointIntensity(float intensity)
{
    pointLightData_->intensity = intensity;
}
void LightManager::SetPointRadius(float radius)
{
    pointLightData_->radius = radius;
}

void LightManager::SetPointDecay(float decay)
{
    pointLightData_->decay = decay;
}

// SpotLight設定
void LightManager::SetSpotLightColor(const Vector4& color)
{
    spotLightData_->color = color;
}

void LightManager::SetSpotLightPosition(const Vector3& pos)
{
    spotLightData_->position = pos;
}

void LightManager::SetSpotLightDirection(const Vector3& dir)
{
    spotLightData_->direction = Normalize(dir);
}

void LightManager::SetSpotLightIntensity(float intensity)
{
    spotLightData_->intensity = intensity;
}

void LightManager::SetSpotLightDistance(float distance)
{
    spotLightData_->distance = distance;
}

void LightManager::SetSpotLightDecay(float decay)
{
    spotLightData_->decay = decay;
}

void LightManager::SetSpotLightCosAngle(float cosAngle)
{
    spotLightData_->cosAngle = cosAngle;
}

void LightManager::SetSpotLightCosFalloffStart(float cosFalloffStart)
{
    spotLightData_->cosFalloffStart = cosFalloffStart;
}

void LightManager::Bind(ID3D12GraphicsCommandList* cmd)
{
    cmd->SetGraphicsRootConstantBufferView(3, lightResource_->GetGPUVirtualAddress());

    cmd->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());

    cmd->SetGraphicsRootConstantBufferView(6, spotLightResource_->GetGPUVirtualAddress());
}
