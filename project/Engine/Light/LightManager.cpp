#include "LightManager.h"
#include "../math/MathStruct.h"
#include <numbers>

std::unique_ptr<LightManager> LightManager::instance_ = nullptr;

LightManager::LightManager(ConstructorKey)
{
}

LightManager* LightManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<LightManager>(ConstructorKey());
    }

    return instance_.get();
}

void LightManager::Finalize()
{
    if (!instance_) {
        return;
    }

    if (instance_->lightResource_) {
        instance_->lightResource_->Unmap(0, nullptr);
        instance_->lightResource_.Reset();
    }

    if (instance_->pointLightResource_) {
        instance_->pointLightResource_->Unmap(0, nullptr);
        instance_->pointLightResource_.Reset();
    }

    if (instance_->spotLightResource_) {
        instance_->spotLightResource_->Unmap(0, nullptr);
        instance_->spotLightResource_.Reset();
    }

    if (instance_->ambientLightResource_) {
        instance_->ambientLightResource_->Unmap(0, nullptr);
        instance_->ambientLightResource_.Reset();
    }

    instance_->lightData_ = nullptr;
    instance_->pointLightData_ = nullptr;
    instance_->spotLightData_ = nullptr;
    instance_->ambientLightData_ = nullptr;
    instance_->dxCommon_ = nullptr;

    instance_.reset();
}

void LightManager::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;

    lightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    lightResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightData_));
    lightResource_->SetName(L"Object3d::DirectionalLightCB");
    lightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    lightData_->direction = Normalize(Vector3 { 0.0f, -1.0f, 0.0f });
    lightData_->intensity = 1.0f;

    ambientLightResource_ = dxCommon_->CreateBufferResource(sizeof(AmbientLight));
    ambientLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&ambientLightData_));
    ambientLightResource_->SetName(L"Object3d::AmbientLightCB");
    ambientLightData_->color = { 1.0f, 1.0f, 1.0f, 0.25f };

    pointLightResource_ = dxCommon_->CreateBufferResource(sizeof(PointLightCollection));
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));
    pointLightResource_->SetName(L"Object3d::PointLightCollectionCB");
    for (uint32_t lightIndex = 0; lightIndex < kMaxPointLights; ++lightIndex) {
        pointLightData_->lights[lightIndex] = {};
    }

    PointLight& defaultPointLight = pointLightData_->lights[0];
    defaultPointLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    defaultPointLight.position = { 0.0f, 2.0f, 0.0f };
    defaultPointLight.intensity = 1.0f;
    defaultPointLight.radius = 10.0f;
    defaultPointLight.decay = 1.0f;
    defaultPointLight.isActive = 1;
    pointLightData_->activeCount = 1;

    spotLightResource_ = dxCommon_->CreateBufferResource(sizeof(SpotLightCollection));
    spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));
    spotLightResource_->SetName(L"Object3d::SpotLightCollectionCB");
    for (uint32_t lightIndex = 0; lightIndex < kMaxSpotLights; ++lightIndex) {
        spotLightData_->lights[lightIndex] = {};
    }

    SpotLight& defaultSpotLight = spotLightData_->lights[0];
    defaultSpotLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    defaultSpotLight.position = { 2.0f, 1.25f, 0.0f };
    defaultSpotLight.distance = 7.0f;
    defaultSpotLight.direction = Normalize(Vector3 { -1.0f, -1.0f, 0.0f });
    defaultSpotLight.intensity = 4.0f;
    defaultSpotLight.decay = 2.0f;
    defaultSpotLight.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
    defaultSpotLight.isActive = 1;
    spotLightData_->activeCount = 1;
}

void LightManager::Update()
{
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

void LightManager::SetPointLight(const Vector4& color, const Vector3& pos, float intensity)
{
    PointLight& pointLight = pointLightData_->lights[0];
    pointLight.color = color;
    pointLight.position = pos;
    pointLight.intensity = intensity;
    pointLight.isActive = 1;
}

void LightManager::SetPointPosition(const Vector3& pos)
{
    pointLightData_->lights[0].position = pos;
}

void LightManager::SetPointIntensity(float intensity)
{
    pointLightData_->lights[0].intensity = intensity;
}

void LightManager::SetPointColor(const Vector4& color)
{
    pointLightData_->lights[0].color = color;
}

void LightManager::SetPointRadius(float radius)
{
    pointLightData_->lights[0].radius = radius;
}

void LightManager::SetPointDecay(float decay)
{
    pointLightData_->lights[0].decay = decay;
}

PointLightHandle LightManager::AddPointLight(
    const Vector4& color,
    const Vector3& position,
    float intensity,
    float radius,
    float decay)
{
    for (uint32_t lightIndex = 1; lightIndex < kMaxPointLights; ++lightIndex) {
        PointLight& pointLight = pointLightData_->lights[lightIndex];
        if (pointLight.isActive != 0) {
            continue;
        }

        pointLight.color = color;
        pointLight.position = position;
        pointLight.intensity = intensity;
        pointLight.radius = radius;
        pointLight.decay = decay;
        pointLight.isActive = 1;
        pointLightData_->activeCount++;
        return lightIndex;
    }

    return kInvalidPointLightHandle;
}

bool LightManager::UpdatePointLight(
    PointLightHandle handle,
    const Vector4& color,
    const Vector3& position,
    float intensity,
    float radius,
    float decay)
{
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    PointLight& pointLight = pointLightData_->lights[handle];
    pointLight.color = color;
    pointLight.position = position;
    pointLight.intensity = intensity;
    pointLight.radius = radius;
    pointLight.decay = decay;
    return true;
}

bool LightManager::SetPointLightPosition(PointLightHandle handle, const Vector3& position)
{
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle].position = position;
    return true;
}

bool LightManager::SetPointLightIntensity(PointLightHandle handle, float intensity)
{
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle].intensity = intensity;
    return true;
}

bool LightManager::SetPointLightColor(PointLightHandle handle, const Vector4& color)
{
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle].color = color;
    return true;
}

bool LightManager::SetPointLightRadius(PointLightHandle handle, float radius)
{
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle].radius = radius;
    return true;
}

bool LightManager::SetPointLightDecay(PointLightHandle handle, float decay)
{
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle].decay = decay;
    return true;
}

bool LightManager::RemovePointLight(PointLightHandle handle)
{
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle] = {};
    if (pointLightData_->activeCount > 0) {
        pointLightData_->activeCount--;
    }
    return true;
}

void LightManager::ClearDynamicPointLights()
{
    for (uint32_t lightIndex = 1; lightIndex < kMaxPointLights; ++lightIndex) {
        pointLightData_->lights[lightIndex] = {};
    }

    pointLightData_->activeCount = 0;
    if (pointLightData_->lights[0].isActive != 0) {
        pointLightData_->activeCount = 1;
    }
}

bool LightManager::IsValidDynamicPointLightHandle(PointLightHandle handle) const
{
    if (!pointLightData_) {
        return false;
    }
    if (handle == kInvalidPointLightHandle || handle == 0 || handle >= kMaxPointLights) {
        return false;
    }
    return pointLightData_->lights[handle].isActive != 0;
}

void LightManager::SetSpotLightColor(const Vector4& color)
{
    spotLightData_->lights[0].color = color;
}

void LightManager::SetSpotLightPosition(const Vector3& pos)
{
    spotLightData_->lights[0].position = pos;
}

void LightManager::SetSpotLightDirection(const Vector3& dir)
{
    spotLightData_->lights[0].direction = Normalize(dir);
}

void LightManager::SetSpotLightIntensity(float intensity)
{
    spotLightData_->lights[0].intensity = intensity;
}

void LightManager::SetSpotLightDistance(float distance)
{
    spotLightData_->lights[0].distance = distance;
}

void LightManager::SetSpotLightDecay(float decay)
{
    spotLightData_->lights[0].decay = decay;
}

void LightManager::SetSpotLightCosAngle(float cosAngle)
{
    spotLightData_->lights[0].cosAngle = cosAngle;
}

SpotLightHandle LightManager::AddSpotLight(
    const Vector4& color,
    const Vector3& position,
    const Vector3& direction,
    float intensity,
    float distance,
    float decay,
    float cosAngle)
{
    for (uint32_t lightIndex = 1; lightIndex < kMaxSpotLights; ++lightIndex) {
        SpotLight& spotLight = spotLightData_->lights[lightIndex];
        if (spotLight.isActive != 0) {
            continue;
        }

        spotLight.color = color;
        spotLight.position = position;
        spotLight.direction = Normalize(direction);
        spotLight.intensity = intensity;
        spotLight.distance = distance;
        spotLight.decay = decay;
        spotLight.cosAngle = cosAngle;
        spotLight.isActive = 1;
        spotLightData_->activeCount++;
        return lightIndex;
    }

    return kInvalidSpotLightHandle;
}

bool LightManager::UpdateSpotLight(
    SpotLightHandle handle,
    const Vector4& color,
    const Vector3& position,
    const Vector3& direction,
    float intensity,
    float distance,
    float decay,
    float cosAngle)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    SpotLight& spotLight = spotLightData_->lights[handle];
    spotLight.color = color;
    spotLight.position = position;
    spotLight.direction = Normalize(direction);
    spotLight.intensity = intensity;
    spotLight.distance = distance;
    spotLight.decay = decay;
    spotLight.cosAngle = cosAngle;
    return true;
}

bool LightManager::SetSpotLightPosition(SpotLightHandle handle, const Vector3& position)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle].position = position;
    return true;
}

bool LightManager::SetSpotLightDirection(SpotLightHandle handle, const Vector3& direction)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle].direction = Normalize(direction);
    return true;
}

bool LightManager::SetSpotLightIntensity(SpotLightHandle handle, float intensity)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle].intensity = intensity;
    return true;
}

bool LightManager::SetSpotLightColor(SpotLightHandle handle, const Vector4& color)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle].color = color;
    return true;
}

bool LightManager::SetSpotLightDistance(SpotLightHandle handle, float distance)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle].distance = distance;
    return true;
}

bool LightManager::SetSpotLightDecay(SpotLightHandle handle, float decay)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle].decay = decay;
    return true;
}

bool LightManager::SetSpotLightCosAngle(SpotLightHandle handle, float cosAngle)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle].cosAngle = cosAngle;
    return true;
}

bool LightManager::RemoveSpotLight(SpotLightHandle handle)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle] = {};
    if (spotLightData_->activeCount > 0) {
        spotLightData_->activeCount--;
    }
    return true;
}

void LightManager::ClearDynamicSpotLights()
{
    for (uint32_t lightIndex = 1; lightIndex < kMaxSpotLights; ++lightIndex) {
        spotLightData_->lights[lightIndex] = {};
    }

    spotLightData_->activeCount = 0;
    if (spotLightData_->lights[0].isActive != 0) {
        spotLightData_->activeCount = 1;
    }
}

bool LightManager::IsValidDynamicSpotLightHandle(SpotLightHandle handle) const
{
    if (!spotLightData_) {
        return false;
    }
    if (handle == kInvalidSpotLightHandle || handle == 0 || handle >= kMaxSpotLights) {
        return false;
    }
    return spotLightData_->lights[handle].isActive != 0;
}

void LightManager::SetAmbientColor(const Vector3& color)
{
    ambientLightData_->color.x = color.x;
    ambientLightData_->color.y = color.y;
    ambientLightData_->color.z = color.z;
}

Vector3 LightManager::GetAmbientColor() const
{
    return { ambientLightData_->color.x, ambientLightData_->color.y, ambientLightData_->color.z };
}

void LightManager::SetAmbientIntensity(float intensity)
{
    ambientLightData_->color.w = intensity;
}

float LightManager::GetAmbientIntensity() const
{
    return ambientLightData_->color.w;
}

void LightManager::Bind(ID3D12GraphicsCommandList* cmd)
{
    cmd->SetGraphicsRootConstantBufferView(3, lightResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(6, spotLightResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(7, ambientLightResource_->GetGPUVirtualAddress());
}
