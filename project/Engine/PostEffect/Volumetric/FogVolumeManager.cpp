#include "FogVolumeManager.h"
#include <algorithm>
#include <cmath>

namespace {
bool IsFiniteVector(const Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
}

void FogVolumeManager::SetEnabled(bool isEnabled)
{
    if (parameters_.isEnabled != isEnabled) { ++resetRevision_; }
    parameters_.isEnabled = isEnabled;
}

bool FogVolumeManager::SetVolume(uint32_t volumeIndex, const FogVolumeSettings& settings) {
    if (volumeIndex >= kMaxFogVolumes || !IsFiniteVector(settings.center) || !IsFiniteVector(settings.halfExtents) ||
        !std::isfinite(settings.radius) || !std::isfinite(settings.density) || !std::isfinite(settings.edgeSoftness)) { return false; }
    if (settings.shape != FogVolumeShape::Sphere && settings.shape != FogVolumeShape::Box) { return false; }
    if (settings.radius <= 0.0f || settings.radius > 10000.0f || settings.halfExtents.x <= 0.0f ||
        settings.halfExtents.y <= 0.0f || settings.halfExtents.z <= 0.0f || settings.halfExtents.x > 10000.0f ||
        settings.halfExtents.y > 10000.0f || settings.halfExtents.z > 10000.0f || settings.density < 0.0f ||
        settings.density > 0.05f || settings.edgeSoftness <= 0.0f) { return false; }
    float minimumExtent = settings.radius;
    if (settings.shape == FogVolumeShape::Box) {
        minimumExtent = (std::min)({ settings.halfExtents.x, settings.halfExtents.y, settings.halfExtents.z });
    }
    if (settings.edgeSoftness > minimumExtent) { return false; }
    fogVolumes_[volumeIndex] = settings;
    return true;
}
void FogVolumeManager::ClearVolumes() { fogVolumes_ = {}; }
bool FogVolumeManager::ApplyPreset(const FogPreset& preset) {
    const LocalFogParameters previousParameters = parameters_;
    const auto previousVolumes = fogVolumes_;
    const LocalFogParameters& settings = preset.parameters;
    bool isValid = SetColor(settings.color) &&
        SetHeightFog(settings.baseHeight, settings.heightDensity, settings.heightFalloff) &&
        SetNoiseParameters(settings.noiseScale, settings.noiseStrength, settings.noiseVelocity);
    for (uint32_t volumeIndex = 0; isValid && volumeIndex < kMaxFogVolumes; ++volumeIndex) {
        isValid = SetVolume(volumeIndex, preset.volumes[volumeIndex]);
    }
    if (!isValid) {
        parameters_ = previousParameters;
        fogVolumes_ = previousVolumes;
        return false;
    }
    parameters_.isEnabled = settings.isEnabled;
    noiseOffset_ = {};
    ++resetRevision_;
    return true;
}
void FogVolumeManager::Reset() {
    ClearVolumes();
    parameters_ = {};
    noiseOffset_ = {};
    ++resetRevision_;
}
bool FogVolumeManager::SetHeightFog(float baseHeight, float density, float heightFalloff) {
    if (!std::isfinite(baseHeight) || !std::isfinite(density) || !std::isfinite(heightFalloff) ||
        density < 0.0f || density > 0.05f || heightFalloff <= 0.0f || heightFalloff > 10.0f) { return false; }
    parameters_.baseHeight = baseHeight;
    parameters_.heightDensity = density;
    parameters_.heightFalloff = heightFalloff;
    return true;
}
bool FogVolumeManager::SetNoiseParameters(float scale, float strength, const Vector3& velocity) {
    if (!std::isfinite(scale) || !std::isfinite(strength) || !IsFiniteVector(velocity) || scale <= 0.0f ||
        scale > 10.0f || strength < 0.0f || strength > 1.0f || std::abs(velocity.x) > 100.0f ||
        std::abs(velocity.y) > 100.0f || std::abs(velocity.z) > 100.0f) { return false; }
    parameters_.noiseScale = scale;
    parameters_.noiseStrength = strength;
    parameters_.noiseVelocity = velocity;
    return true;
}
bool FogVolumeManager::SetColor(const Vector3& color) {
    if (!IsFiniteVector(color) || color.x < 0.0f || color.y < 0.0f || color.z < 0.0f ||
        color.x > 4.0f || color.y > 4.0f || color.z > 4.0f) { return false; }
    parameters_.color = color;
    return true;
}

void FogVolumeManager::UpdateNoise(float deltaTimeSeconds)
{
    if (!std::isfinite(deltaTimeSeconds)) { return; }
    const float boundedDeltaSeconds = std::clamp(deltaTimeSeconds, 0.0f, 0.1f);
    // 既存のノイズ速度と折り返し周期を維持する。
    noiseOffset_.x = std::fmod(noiseOffset_.x + parameters_.noiseVelocity.x * parameters_.noiseScale * boundedDeltaSeconds, 256.0f);
    noiseOffset_.y = std::fmod(noiseOffset_.y + parameters_.noiseVelocity.y * parameters_.noiseScale * boundedDeltaSeconds, 256.0f);
    noiseOffset_.z = std::fmod(noiseOffset_.z + parameters_.noiseVelocity.z * parameters_.noiseScale * boundedDeltaSeconds, 256.0f);
}
