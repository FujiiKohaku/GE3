#pragma once
#include "Engine/Math/MathStruct.h"
#include <array>
#include <cstddef>

class IceJellyfish;

// 共通の揺れと追従を保持し、Battleの攻撃姿勢をモデルへ反映する。
class IceJellyfishPoseController final {
public:
    static constexpr size_t kTentacleCount = 6;
    static constexpr size_t kSegmentsPerTentacle = 7;
    struct CollisionTransforms {
        Matrix4x4 core;
        std::array<std::array<Matrix4x4, kSegmentsPerTentacle>, kTentacleCount> tentacles;
    };

    static Vector4 GetBellAnimationParameters(float time);
    void Reset();
    void Update(IceJellyfish& boss, float deltaTimeSeconds, CollisionTransforms& collisionTransforms);

private:
    void UpdateIcePillarModelTransforms(IceJellyfish& boss);
    std::array<float, kTentacleCount> tentacleWavePhase_ {};
    std::array<float, kTentacleCount> tentacleWaveSpeed_ {};
    std::array<float, kTentacleCount> tentacleWaveAmplitude_ {};
    std::array<std::array<Vector3, kSegmentsPerTentacle>, kTentacleCount> softDirections_ {};
    float softPoseTime_ = 0.0f;
    bool softPoseInitialized_ = false;

};
