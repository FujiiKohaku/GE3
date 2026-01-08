#include "LightManager.h"
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

    lightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    lightResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightData_));
    lightResource_->SetName(L"Object3d::DirectionalLightCB");
    // デフォルト
    lightData_->color = { 1, 1, 1, 1 };
    lightData_->direction = Normalize({ 0, -1, 0 });
    lightData_->intensity = 1.0f;

    // 点光源の初期化
    pointLightResource_ = dxCommon_->CreateBufferResource(sizeof(PointLight));

    pointLightResource_->Map(0, nullptr,reinterpret_cast<void**>(&pointLightData_));

    pointLightData_->color = { 1, 1, 1, 1 };
    pointLightData_->position = { 0, 2, 0 };
    pointLightData_->intensity = 1.0f;

    pointLightData_->radius = 10.0f; 
    pointLightData_->decay = 1.0f; 
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



void LightManager::Bind(ID3D12GraphicsCommandList* cmd)
{
    cmd->SetGraphicsRootConstantBufferView(
        3, lightResource_->GetGPUVirtualAddress());

    cmd->SetGraphicsRootConstantBufferView(
        5, pointLightResource_->GetGPUVirtualAddress());
}
