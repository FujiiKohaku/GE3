#pragma once

#include "Engine/math/MathStruct.h"
#include <array>
#include <cstdint>

enum class FogVolumeShape { Sphere, Box };

struct FogVolumeSettings {
    bool isEnabled = false;
    FogVolumeShape shape = FogVolumeShape::Sphere;
    Vector3 center {};
    Vector3 halfExtents { 20.0f, 10.0f, 20.0f };
    float radius = 20.0f;
    float density = 0.01f;
    float edgeSoftness = 2.0f;
};

struct LocalFogParameters {
    bool isEnabled = false;
    Vector3 color { 0.58f, 0.80f, 0.96f };
    float baseHeight = 0.0f;
    float heightDensity = 0.0f;
    float heightFalloff = 0.1f;
    float noiseScale = 0.05f;
    float noiseStrength = 0.0f;
    Vector3 noiseVelocity {};
};

struct FogPreset {
    LocalFogParameters parameters;
    std::array<FogVolumeSettings, 8> volumes {};
};

// 空間に置く霧とその動きを管理する。GPUリソースや描画処理は持たない。
class FogVolumeManager {
public:
    static constexpr uint32_t kMaxFogVolumes = 8;
    void SetEnabled(bool isEnabled);
    bool ApplyPreset(const FogPreset& preset);
    bool SetVolume(uint32_t volumeIndex, const FogVolumeSettings& settings);
    const std::array<FogVolumeSettings, kMaxFogVolumes>& GetVolumes() const { return fogVolumes_; }
    const LocalFogParameters& GetParameters() const { return parameters_; }
    void ClearVolumes();
    void Reset();
    bool SetHeightFog(float baseHeight, float density, float heightFalloff);
    bool SetNoiseParameters(float scale, float strength, const Vector3& velocity);
    bool SetColor(const Vector3& color);
    void UpdateNoise(float deltaTimeSeconds);
    const Vector3& GetNoiseOffset() const { return noiseOffset_; }
    uint64_t GetResetRevision() const { return resetRevision_; }

private:
    LocalFogParameters parameters_;
    std::array<FogVolumeSettings, kMaxFogVolumes> fogVolumes_ {};
    Vector3 noiseOffset_ {};
    uint64_t resetRevision_ = 0;
};
