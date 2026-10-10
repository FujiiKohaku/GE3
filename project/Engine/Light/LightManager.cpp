#include "LightManager.h"
#include "Engine/3D/Object3dRootParameter.h"
#include "../math/MathStruct.h"
#include <cassert>
#include <numbers>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "Engine/Camera/Camera.h"
#include "Engine/Math/MatrixMath.h"

namespace {
// ライトの保守的な投影範囲だけを走査する。全クラスタ×全ライトの判定を避ける。
void AssignLightToClusters(AmbientLight& ambient, const Vector3& position, float radius,
    float tangentX, float tangentY, uint32_t lightIndex, bool isSpot)
{
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) { return; }
    const float nearClip = ambient.clusterSettings.x;
    const float farClip = ambient.clusterSettings.y;
    if (position.z + radius < nearClip || position.z - radius > farClip) { return; }
    const float minZ = (std::max)(nearClip, position.z - radius);
    const float maxZ = (std::min)(farClip, position.z + radius);
    int minX = 0; int maxX = 11; int minY = 0; int maxY = 7;
    if (position.z - radius > nearClip) {
        const float left = position.x - radius;
        const float right = position.x + radius;
        const float bottom = position.y - radius;
        const float top = position.y + radius;
        const float screenLeft = (std::min)(left / minZ, left / maxZ) / tangentX;
        const float screenRight = (std::max)(right / minZ, right / maxZ) / tangentX;
        const float screenBottom = (std::min)(bottom / minZ, bottom / maxZ) / tangentY;
        const float screenTop = (std::max)(top / minZ, top / maxZ) / tangentY;
        if (screenLeft > 1.0f || screenRight < -1.0f || screenBottom > 1.0f || screenTop < -1.0f) { return; }
        // 境界の小さな余白で浮動小数点誤差を吸収する。
        minX = std::clamp(static_cast<int>(std::floor((std::clamp(screenLeft, -1.0f, 1.0f) + 1.0f) * 6.0f - 0.0001f)), 0, 11);
        maxX = std::clamp(static_cast<int>(std::floor((std::clamp(screenRight, -1.0f, 1.0f) + 1.0f) * 6.0f + 0.0001f)), 0, 11);
        minY = std::clamp(static_cast<int>(std::floor((1.0f - std::clamp(screenTop, -1.0f, 1.0f)) * 4.0f - 0.0001f)), 0, 7);
        maxY = std::clamp(static_cast<int>(std::floor((1.0f - std::clamp(screenBottom, -1.0f, 1.0f)) * 4.0f + 0.0001f)), 0, 7);
    }
    const int minSlice = std::clamp(static_cast<int>(std::floor(std::log(minZ / nearClip) * ambient.clusterSettings.z - 0.0001f)), 0, 15);
    const int maxSlice = std::clamp(static_cast<int>(std::floor(std::log(maxZ / nearClip) * ambient.clusterSettings.z + 0.0001f)), 0, 15);
    uint32_t maskIndex = 0;
    if (isSpot) { maskIndex = 1; }
    const uint32_t lightMask = 1u << lightIndex;
    for (int slice = minSlice; slice <= maxSlice; ++slice) {
        for (int tileY = minY; tileY <= maxY; ++tileY) {
            for (int tileX = minX; tileX <= maxX; ++tileX) {
                ambient.clusterMasks[(slice * 8 + tileY) * 12 + tileX][maskIndex] |= lightMask;
            }
        }
    }
}

constexpr uint32_t kLightIndexMask = 0xffu;
constexpr uint32_t kLightGenerationMask = 0x00ffffffu;
bool IsFiniteLightVector(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
bool IsValidLightColor(const Vector4& color) {
    return std::isfinite(color.x) && std::isfinite(color.y) && std::isfinite(color.z) && std::isfinite(color.w) &&
        color.x >= 0.0f && color.y >= 0.0f && color.z >= 0.0f && color.x <= 64.0f && color.y <= 64.0f && color.z <= 64.0f;
}
bool IsValidLightNumber(float value) { return std::isfinite(value) && value >= 0.0f; }
bool IsValidLightDirection(const Vector3& direction) {
    const float lengthSquared = Dot(direction, direction);
    return IsFiniteLightVector(direction) && std::isfinite(lengthSquared) && lengthSquared > 0.000001f;
}
bool IsValidPointSettings(const Vector4& color, const Vector3& position, float intensity, float radius, float decay) {
    return IsValidLightColor(color) && IsFiniteLightVector(position) && (IsValidLightNumber(intensity) && intensity <= 64.0f) &&
        IsValidLightNumber(radius) && radius > 0.1f && radius <= 10000.0f && IsValidLightNumber(decay) && decay <= 16.0f;
}
bool IsValidSpotSettings(const Vector4& color, const Vector3& position, const Vector3& direction,
    float intensity, float distance, float decay, float cosAngle) {
    return IsValidPointSettings(color, position, intensity, distance, decay) && IsValidLightDirection(direction) &&
        std::isfinite(cosAngle) && cosAngle > 0.0f && cosAngle < 1.0f;
}
}

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
    if (dxCommon_ != nullptr) {
        return;
    }

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
    *ambientLightData_ = {};
    ambientLightData_->componentSettings = {1.0f, 1.0f, 1.0f, 0.0f};
    ambientLightData_->environmentSettings = { 0.12f, 0.20f, 0.0f, 1.0f };
    ambientLightData_->atmosphereSettings = { 1.0f, 0.00035f, 0.45f, 0.65f };
    ambientLightData_->color = { 1.0f, 1.0f, 1.0f, 0.25f };
    ambientLightData_->skyColor = { 0.78f, 0.90f, 1.10f, 1.0f };
    ambientLightData_->groundColor = { 0.42f, 0.38f, 0.34f, 1.0f };

    pointLightResource_ = dxCommon_->CreateBufferResource(sizeof(PointLightCollection));
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));
    pointLightResource_->SetName(L"Object3d::PointLightCollectionCB");
    for (uint32_t lightIndex = 0; lightIndex < kMaxPointLights; ++lightIndex) {
        pointLightData_->lights[lightIndex] = {};
        pointShadowEnabled_[lightIndex] = false;
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
        spotShadowEnabled_[lightIndex] = false;
        spotVolumetricEnabled_[lightIndex] = false;
    }

    SpotLight& defaultSpotLight = spotLightData_->lights[0];
    defaultSpotLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    defaultSpotLight.position = { 2.0f, 1.25f, 0.0f };
    defaultSpotLight.distance = 7.0f;
    defaultSpotLight.direction = Normalize(Vector3 { -1.0f, -1.0f, 0.0f });
    defaultSpotLight.intensity = 4.0f;
    defaultSpotLight.decay = 2.0f;
    defaultSpotLight.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
    defaultSpotLight.cosFalloffStart = 1.0f;
    defaultSpotLight.isActive = 1;
    spotLightData_->activeCount = 1;
}

void LightManager::Update()
{
}

void LightManager::SetDirectional(const Vector4& color, const Vector3& dir, float intensity)
{
    if (!(IsInitialized() && IsValidLightColor(color) && IsValidLightDirection(dir) && (IsValidLightNumber(intensity) && intensity <= 64.0f))) { return; }
    lightData_->color = color;
    lightData_->direction = Normalize(dir);
    lightData_->intensity = intensity;
}

void LightManager::SetDirection(const Vector3& dir)
{
    if (!(IsInitialized() && IsValidLightDirection(dir))) { return; }
    lightData_->direction = Normalize(dir);
}

void LightManager::SetIntensity(float intensity)
{
    if (!(IsInitialized() && (IsValidLightNumber(intensity) && intensity <= 64.0f))) { return; }
    lightData_->intensity = intensity;
}

void LightManager::SetPointLight(const Vector4& color, const Vector3& pos, float intensity)
{
    if (!(IsInitialized() && IsValidLightColor(color) && IsFiniteLightVector(pos) && (IsValidLightNumber(intensity) && intensity <= 64.0f))) { return; }
    PointLight& pointLight = pointLightData_->lights[0];
    pointLight.color = color;
    pointLight.position = pos;
    pointLight.intensity = intensity;
    pointLight.isActive = 1;
}

void LightManager::SetPointPosition(const Vector3& pos)
{
    if (!(IsInitialized() && IsFiniteLightVector(pos))) { return; }
    pointLightData_->lights[0].position = pos;
}

void LightManager::SetPointIntensity(float intensity)
{
    if (!(IsInitialized() && (IsValidLightNumber(intensity) && intensity <= 64.0f))) { return; }
    pointLightData_->lights[0].intensity = intensity;
}

void LightManager::SetPointColor(const Vector4& color)
{
    if (!(IsInitialized() && IsValidLightColor(color))) { return; }
    pointLightData_->lights[0].color = color;
}

void LightManager::SetPointRadius(float radius)
{
    if (!(IsInitialized() && IsValidLightNumber(radius) && radius > 0.1f && radius <= 10000.0f)) { return; }
    pointLightData_->lights[0].radius = radius;
}

void LightManager::SetPointDecay(float decay)
{
    if (!(IsInitialized() && IsValidLightNumber(decay) && decay <= 16.0f)) { return; }
    pointLightData_->lights[0].decay = decay;
}

PointLightHandle LightManager::AddPointLight(
    const Vector4& color,
    const Vector3& position,
    float intensity,
    float radius,
    float decay)
{
    if (!(IsInitialized() && IsValidPointSettings(color, position, intensity, radius, decay))) { return kInvalidPointLightHandle; }
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
        pointGenerations_[lightIndex] = (pointGenerations_[lightIndex] + 1) & kLightGenerationMask;
        if (pointGenerations_[lightIndex] == 0) { pointGenerations_[lightIndex] = 1; }
        return (pointGenerations_[lightIndex] << 8) | lightIndex;
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
    if (!(IsInitialized() && IsValidPointSettings(color, position, intensity, radius, decay))) { return false; }
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    PointLight& pointLight = pointLightData_->lights[handle & kLightIndexMask];
    pointLight.color = color;
    pointLight.position = position;
    pointLight.intensity = intensity;
    pointLight.radius = radius;
    pointLight.decay = decay;
    return true;
}

bool LightManager::SetPointLightPosition(PointLightHandle handle, const Vector3& position)
{
    if (!(IsInitialized() && IsFiniteLightVector(position))) { return false; }
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle & kLightIndexMask].position = position;
    return true;
}

bool LightManager::SetPointLightIntensity(PointLightHandle handle, float intensity)
{
    if (!(IsInitialized() && (IsValidLightNumber(intensity) && intensity <= 64.0f))) { return false; }
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle & kLightIndexMask].intensity = intensity;
    return true;
}

bool LightManager::SetPointLightColor(PointLightHandle handle, const Vector4& color)
{
    if (!(IsInitialized() && IsValidLightColor(color))) { return false; }
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle & kLightIndexMask].color = color;
    return true;
}

bool LightManager::SetPointLightRadius(PointLightHandle handle, float radius)
{
    if (!(IsInitialized() && IsValidLightNumber(radius) && radius > 0.1f && radius <= 10000.0f)) { return false; }
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle & kLightIndexMask].radius = radius;
    return true;
}

bool LightManager::SetPointLightDecay(PointLightHandle handle, float decay)
{
    if (!(IsInitialized() && IsValidLightNumber(decay) && decay <= 16.0f)) { return false; }
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle & kLightIndexMask].decay = decay;
    return true;
}

bool LightManager::RemovePointLight(PointLightHandle handle)
{
    if (!IsValidDynamicPointLightHandle(handle)) {
        return false;
    }

    pointLightData_->lights[handle & kLightIndexMask] = {};
    pointShadowEnabled_[handle & kLightIndexMask] = false;
    if (pointLightData_->activeCount > 0) {
        pointLightData_->activeCount--;
    }
    return true;
}

void LightManager::ClearDynamicPointLights()
{
    if (!IsInitialized()) { return; }
    for (uint32_t lightIndex = 1; lightIndex < kMaxPointLights; ++lightIndex) {
        pointLightData_->lights[lightIndex] = {};
        pointShadowEnabled_[lightIndex] = false;
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
    if (handle == kInvalidPointLightHandle || handle == 0 || (handle & kLightIndexMask) >= kMaxPointLights) {
        return false;
    }
    return pointLightData_->lights[handle & kLightIndexMask].isActive != 0 && pointGenerations_[handle & kLightIndexMask] == (handle >> 8);
}

void LightManager::SetSpotLightColor(const Vector4& color)
{
    if (!(IsInitialized() && IsValidLightColor(color))) { return; }
    spotLightData_->lights[0].color = color;
}

void LightManager::SetSpotLightPosition(const Vector3& pos)
{
    if (!(IsInitialized() && IsFiniteLightVector(pos))) { return; }
    spotLightData_->lights[0].position = pos;
}

void LightManager::SetSpotLightDirection(const Vector3& dir)
{
    if (!(IsInitialized() && IsValidLightDirection(dir))) { return; }
    spotLightData_->lights[0].direction = Normalize(dir);
}

void LightManager::SetSpotLightIntensity(float intensity)
{
    if (!(IsInitialized() && (IsValidLightNumber(intensity) && intensity <= 64.0f))) { return; }
    spotLightData_->lights[0].intensity = intensity;
}

void LightManager::SetSpotLightDistance(float distance)
{
    if (!(IsInitialized() && IsValidLightNumber(distance) && distance > 0.1f && distance <= 10000.0f)) { return; }
    spotLightData_->lights[0].distance = distance;
}

void LightManager::SetSpotLightDecay(float decay)
{
    if (!(IsInitialized() && IsValidLightNumber(decay) && decay <= 16.0f)) { return; }
    spotLightData_->lights[0].decay = decay;
}

void LightManager::SetSpotLightCosAngle(float cosAngle)
{
    if (!(IsInitialized() && std::isfinite(cosAngle) && cosAngle > 0.0f && cosAngle < 1.0f)) { return; }
    spotLightData_->lights[0].cosAngle = cosAngle;
}

void LightManager::SetSpotLightCosFalloffStart(float cosFalloffStart)
{
    if (!IsInitialized() || !std::isfinite(cosFalloffStart) || cosFalloffStart <= spotLightData_->lights[0].cosAngle || cosFalloffStart > 1.0f) { return; }
    spotLightData_->lights[0].cosFalloffStart = cosFalloffStart;
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
    if (!(IsInitialized() && IsValidSpotSettings(color, position, direction, intensity, distance, decay, cosAngle))) { return kInvalidSpotLightHandle; }
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
        spotLight.cosFalloffStart = 1.0f;
        spotLight.isActive = 1;
        spotLightData_->activeCount++;
        spotGenerations_[lightIndex] = (spotGenerations_[lightIndex] + 1) & kLightGenerationMask;
        if (spotGenerations_[lightIndex] == 0) { spotGenerations_[lightIndex] = 1; }
        return (spotGenerations_[lightIndex] << 8) | lightIndex;
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
    if (!(IsInitialized() && IsValidSpotSettings(color, position, direction, intensity, distance, decay, cosAngle))) { return false; }
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    SpotLight& spotLight = spotLightData_->lights[handle & kLightIndexMask];
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
    if (!(IsInitialized() && IsFiniteLightVector(position))) { return false; }
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle & kLightIndexMask].position = position;
    return true;
}

bool LightManager::SetSpotLightDirection(SpotLightHandle handle, const Vector3& direction)
{
    if (!(IsInitialized() && IsValidLightDirection(direction))) { return false; }
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle & kLightIndexMask].direction = Normalize(direction);
    return true;
}

bool LightManager::SetSpotLightIntensity(SpotLightHandle handle, float intensity)
{
    if (!(IsInitialized() && (IsValidLightNumber(intensity) && intensity <= 64.0f))) { return false; }
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle & kLightIndexMask].intensity = intensity;
    return true;
}

bool LightManager::SetSpotLightColor(SpotLightHandle handle, const Vector4& color)
{
    if (!(IsInitialized() && IsValidLightColor(color))) { return false; }
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle & kLightIndexMask].color = color;
    return true;
}

bool LightManager::SetSpotLightDistance(SpotLightHandle handle, float distance)
{
    if (!(IsInitialized() && IsValidLightNumber(distance) && distance > 0.1f && distance <= 10000.0f)) { return false; }
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle & kLightIndexMask].distance = distance;
    return true;
}

bool LightManager::SetSpotLightDecay(SpotLightHandle handle, float decay)
{
    if (!(IsInitialized() && IsValidLightNumber(decay) && decay <= 16.0f)) { return false; }
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle & kLightIndexMask].decay = decay;
    return true;
}

bool LightManager::SetSpotLightCosAngle(SpotLightHandle handle, float cosAngle)
{
    if (!(IsInitialized() && std::isfinite(cosAngle) && cosAngle > 0.0f && cosAngle < 1.0f)) { return false; }
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle & kLightIndexMask].cosAngle = cosAngle;
    return true;
}

bool LightManager::RemoveSpotLight(SpotLightHandle handle)
{
    if (!IsValidDynamicSpotLightHandle(handle)) {
        return false;
    }

    spotLightData_->lights[handle & kLightIndexMask] = {};
    spotShadowEnabled_[handle & kLightIndexMask] = false;
    spotVolumetricEnabled_[handle & kLightIndexMask] = false;
    if (spotLightData_->activeCount > 0) {
        spotLightData_->activeCount--;
    }
    return true;
}

void LightManager::ClearDynamicSpotLights()
{
    if (!IsInitialized()) { return; }
    for (uint32_t lightIndex = 1; lightIndex < kMaxSpotLights; ++lightIndex) {
        spotLightData_->lights[lightIndex] = {};
        spotShadowEnabled_[lightIndex] = false;
        spotVolumetricEnabled_[lightIndex] = false;
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
    if (handle == kInvalidSpotLightHandle || handle == 0 || (handle & kLightIndexMask) >= kMaxSpotLights) {
        return false;
    }
    return spotLightData_->lights[handle & kLightIndexMask].isActive != 0 && spotGenerations_[handle & kLightIndexMask] == (handle >> 8);
}

void LightManager::SetAmbientColor(const Vector3& color)
{
    if (!IsInitialized() || !IsFiniteLightVector(color) || color.x < 0.0f || color.y < 0.0f || color.z < 0.0f || color.x > 4.0f || color.y > 4.0f || color.z > 4.0f) { return; }
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
    if (!IsInitialized() || !IsValidLightNumber(intensity) || intensity > 4.0f) { return; }
    ambientLightData_->color.w = intensity;
}

float LightManager::GetAmbientIntensity() const
{
    return ambientLightData_->color.w;
}

void LightManager::Bind(ID3D12GraphicsCommandList* cmd)
{
    const bool initialized = IsInitialized() && lightResource_ && pointLightResource_ &&
        spotLightResource_ && ambientLightResource_;
    assert(initialized && "LightManager::Bind called before Initialize");
    if (!initialized || cmd == nullptr) {
        return;
    }

    cmd->SetGraphicsRootConstantBufferView(
        RootParameterIndex(Object3dRootParameter::DirectionalLight),
        lightResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(
        RootParameterIndex(Object3dRootParameter::PointLights),
        pointLightResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(
        RootParameterIndex(Object3dRootParameter::SpotLights),
        spotLightResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(
        RootParameterIndex(Object3dRootParameter::AmbientLight),
        ambientLightResource_->GetGPUVirtualAddress());
}

void LightManager::SetHemisphereColors(const Vector3& sky, const Vector3& ground)
{
    ambientLightData_->skyColor = { sky.x, sky.y, sky.z, 1.0f };
    ambientLightData_->groundColor = { ground.x, ground.y, ground.z, 1.0f };
}

bool LightManager::SetPointLightShadowEnabled(PointLightHandle handle, bool isEnabled) {
    if (!IsInitialized()) { return false; }
    if (handle != 0 && !IsValidDynamicPointLightHandle(handle)) { return false; }
    pointShadowEnabled_[handle & kLightIndexMask] = isEnabled;
    return true;
}
bool LightManager::SetSpotLightShadowEnabled(SpotLightHandle handle, bool isEnabled) {
    if (!IsInitialized()) { return false; }
    if (handle != 0 && !IsValidDynamicSpotLightHandle(handle)) { return false; }
    spotShadowEnabled_[handle & kLightIndexMask] = isEnabled;
    return true;
}
void LightManager::ApplyLightingPreset(const LightingPreset& preset)
{
    SetDirectional(preset.color, preset.direction, preset.intensity);
    SetAmbientColor({ preset.ambient.x, preset.ambient.y, preset.ambient.z });
    SetAmbientIntensity(preset.ambient.w);
    SetHemisphereColors(preset.skyColor, preset.groundColor);
    SetPointRadius(preset.pointRadius);
    SetPointDecay(preset.pointDecay);
    SetPointLight(preset.pointColor, preset.pointPosition, preset.pointIntensity);
    SetSpotLightIntensity(preset.spotIntensity);
}
bool LightManager::SetSpotLightVolumetricEnabled(SpotLightHandle handle, bool isEnabled) {
    if (!IsInitialized()) { return false; }
    if (handle != 0 && !IsValidDynamicSpotLightHandle(handle)) { return false; }
    spotVolumetricEnabled_[handle & kLightIndexMask] = isEnabled;
    return true;
}
bool LightManager::IsSpotLightVolumetricEnabled(uint32_t lightIndex) const {
    return lightIndex < kMaxSpotLights && spotVolumetricEnabled_[lightIndex];
}
bool LightManager::IsPointLightShadowEnabled(uint32_t lightIndex) const {
    return lightIndex < kMaxPointLights && pointShadowEnabled_[lightIndex];
}
bool LightManager::IsSpotLightShadowEnabled(uint32_t lightIndex) const {
    return lightIndex < kMaxSpotLights && spotShadowEnabled_[lightIndex];
}
PointLight LightManager::GetPointLight(uint32_t lightIndex) const {
    if (!pointLightData_ || lightIndex >= kMaxPointLights) { return {}; }
    return pointLightData_->lights[lightIndex];
}
SpotLight LightManager::GetSpotLight(uint32_t lightIndex) const {
    if (!spotLightData_ || lightIndex >= kMaxSpotLights) { return {}; }
    return spotLightData_->lights[lightIndex];
}
bool LightManager::SetSpotLightCosFalloffStart(SpotLightHandle handle, float cosFalloffStart) {
    if (!IsValidDynamicSpotLightHandle(handle) || !std::isfinite(cosFalloffStart)) { return false; }
    SpotLight& light = spotLightData_->lights[handle & kLightIndexMask];
    if (cosFalloffStart <= light.cosAngle || cosFalloffStart > 1.0f) { return false; }
    light.cosFalloffStart = cosFalloffStart;
    return true;
}

void LightManager::SetClusteredLightingEnabled(bool isEnabled)
{
    isClusteredLightingEnabled_ = isEnabled;
    if (IsInitialized()) { ambientLightData_->clusterSettings.w = 0.0f; }
}

bool LightManager::SetEnvironmentLighting(float diffuseStrength, float specularStrength)
{
    if (!IsInitialized() || !std::isfinite(diffuseStrength) || !std::isfinite(specularStrength) ||
        diffuseStrength < 0.0f || diffuseStrength > 2.0f || specularStrength < 0.0f || specularStrength > 2.0f) { return false; }
    ambientLightData_->environmentSettings.x = diffuseStrength;
    ambientLightData_->environmentSettings.y = specularStrength;
    return true;
}

bool LightManager::SetSkyLighting(bool isEnabled, float strength)
{
    if (!IsInitialized() || !std::isfinite(strength) || strength < 0.0f || strength > 2.0f) { return false; }
    float enabled = 0.0f;
    if (isEnabled) { enabled = 1.0f; }
    ambientLightData_->environmentSettings.z = enabled;
    ambientLightData_->environmentSettings.w = strength;
    return true;
}

bool LightManager::SetLightingComponents(float directStrength, float indirectStrength, float iceAmbientMultiplier, uint32_t viewMode)
{
    if (!IsInitialized() || !std::isfinite(directStrength) || !std::isfinite(indirectStrength) || !std::isfinite(iceAmbientMultiplier) ||
        directStrength < 0 || directStrength > 2 || indirectStrength < 0 || indirectStrength > 2 ||
        iceAmbientMultiplier < 0 || iceAmbientMultiplier > 2 || viewMode > 2) { return false; }
    ambientLightData_->componentSettings = {directStrength, indirectStrength, iceAmbientMultiplier, static_cast<float>(viewMode)};
    return true;
}

bool LightManager::SetAtmosphere(bool isEnabled, float density, float strength, float anisotropy)
{
    if (!IsInitialized() || !std::isfinite(density) || !std::isfinite(strength) || !std::isfinite(anisotropy) ||
        density < 0.0f || density > 0.01f || strength < 0.0f || strength > 2.0f || anisotropy < 0.0f || anisotropy > 0.9f) { return false; }
    float enabled = 0.0f;
    if (isEnabled) { enabled = 1.0f; }
    ambientLightData_->atmosphereSettings = { enabled, density, strength, anisotropy };
    return true;
}

void LightManager::UpdateClusters(const Camera* camera)
{
    if (!IsInitialized()) { return; }
    ambientLightData_->clusterSettings.w = 0.0f;
    if (!isClusteredLightingEnabled_ || camera == nullptr) { return; }
    const float nearClip = camera->GetNearClip();
    const float farClip = camera->GetFarClip();
    const float tangentY = std::tan(camera->GetFovY() * 0.5f);
    const float tangentX = tangentY * camera->GetAspectRatio();
    if (!std::isfinite(nearClip) || !std::isfinite(farClip) || nearClip <= 0.0f || farClip <= nearClip ||
        !std::isfinite(tangentX) || !std::isfinite(tangentY) || tangentX <= 0.0f || tangentY <= 0.0f) { return; }
    const Vector3 scale = camera->GetScale();
    const float minimumScale = (std::min)((std::min)(std::abs(scale.x), std::abs(scale.y)), std::abs(scale.z));
    if (!IsFiniteLightVector(scale) || minimumScale < 0.000001f) { return; }
    const float radiusScale = 1.0f / minimumScale;
    const Matrix4x4 view = camera->GetViewMatrix();
    const Matrix4x4 projection = camera->GetProjectionMatrix();
    for (uint32_t row = 0; row < 4; ++row) {
        for (uint32_t column = 0; column < 4; ++column) {
            if (!std::isfinite(view.m[row][column]) || !std::isfinite(projection.m[row][column])) { return; }
        }
    }
    ambientLightData_->clusterView = view;
    ambientLightData_->clusterProjection = projection;
    const float logarithmicRange = std::log(farClip / nearClip);
    if (!std::isfinite(logarithmicRange) || logarithmicRange <= 0.000001f) { return; }
    ambientLightData_->clusterSettings = { nearClip, farClip, 16.0f / logarithmicRange, 1.0f };
    // Uploadメモリを読みながらビットを更新するとCPUが待たされるため、通常RAMで組み立てて一括転送する。
    AmbientLight clusterData {};
    clusterData.clusterSettings = { nearClip, farClip, 16.0f / logarithmicRange, 1.0f };
    PointLightCollection pointLights {};
    SpotLightCollection spotLights {};
    std::memcpy(&pointLights, pointLightData_, sizeof(pointLights));
    std::memcpy(&spotLights, spotLightData_, sizeof(spotLights));
    for (uint32_t index = 0; index < kMaxPointLights; ++index) {
        const PointLight& light = pointLights.lights[index];
        if (light.isActive == 0 || light.intensity <= 0.0f) { continue; }
        AssignLightToClusters(clusterData, MatrixMath::Transform(light.position, view), light.radius * radiusScale,
            tangentX, tangentY, index, false);
    }
    // スポットは到達距離の球で保守的に判定し、円錐外はPSで除外する。
    for (uint32_t index = 0; index < kMaxSpotLights; ++index) {
        const SpotLight& light = spotLights.lights[index];
        if (light.isActive == 0 || light.intensity <= 0.0f) { continue; }
        AssignLightToClusters(clusterData, MatrixMath::Transform(light.position, view), light.distance * radiusScale,
            tangentX, tangentY, index, true);
    }
    std::memcpy(ambientLightData_->clusterMasks, clusterData.clusterMasks, sizeof(clusterData.clusterMasks));
}
