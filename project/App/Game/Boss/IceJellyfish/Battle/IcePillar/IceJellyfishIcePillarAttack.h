#pragma once

#include "../Common/IceJellyfishAttackMotion.h"
#include <array>
#include <cstddef>
#include <cstdint>

class IceJellyfish;

class IceJellyfishIcePillarAttack final {
public:
    void Reset();
    static constexpr size_t kModelCount = 6;
    bool HasModels() const { return icePillarModelSpawnedForWave_; }
    const std::array<Vector3, 2>& GetPositions() const { return icePillarPositions_; }
    const std::array<Vector3, kModelCount>& GetModelPositions() const { return icePillarModelPositions_; }
    Vector3 ApplyTentacleDirection(const IceJellyfish& boss, const IceJellyfishAttackMotionContext& context,
        const Vector3& direction) const;
    Vector4 ApplyTentacleColor(const IceJellyfishAttackMotionContext& context, const Vector4& baseColor) const;
    void Start(IceJellyfish& boss);
    void Update(IceJellyfish& boss, float deltaTimeSeconds);
    bool IsTentacleSelected(size_t tentacle) const;
    float GetCharge() const;
    float GetModelScaleWeight() const;

private:
    friend bool RunJellyfishAttackTests();
    friend bool RunIceJellyfishStateTests();
    void PrepareWarning(IceJellyfish& boss);
    void DamagePlayerOnce(IceJellyfish& boss, int32_t waveIndex, int damage);
    int32_t icePillarWave_ = -1;
    int32_t icePillarSafeLane_ = 1;
    float icePillarTimer_ = 0.0f;
    std::array<Vector3, 2> icePillarPositions_ {};
    std::array<Vector3, kModelCount> icePillarModelPositions_ {};
    float icePillarModelTimer_ = 0.0f;
    bool icePillarModelSpawnedForWave_ = false;
    std::array<bool, 3> hasHitPlayerByWave_ {};
};
