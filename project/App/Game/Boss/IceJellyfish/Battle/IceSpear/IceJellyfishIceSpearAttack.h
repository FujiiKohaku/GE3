#pragma once

#include "../Common/IceJellyfishAttackMotion.h"
#include <array>
#include <cstddef>
#include <cstdint>

class IceJellyfish;

class IceJellyfishIceSpearAttack final {
public:
    void Reset();
    float GetPoseWeight() const { return iceSpearPoseWeight_; }
    Vector3 ApplyTentacleDirection(const IceJellyfish& boss, const IceJellyfishAttackMotionContext& context,
        const Vector3& direction) const;
    bool TryApplyTentacleColor(const IceJellyfish& boss, const IceJellyfishAttackMotionContext& context,
        const Vector4& baseColor, Vector4& color) const;
    bool IsEmitter(const IceJellyfish& boss, size_t tentacle, size_t segment) const;
    bool IsTentacleSelected(size_t tentacle) const;
    Vector3 GetAimTarget(size_t tentacle) const;
    void Start(IceJellyfish& boss);
    void Update(IceJellyfish& boss, float deltaTimeSeconds);
    void EmitPendingVolleys(IceJellyfish& boss);

private:
    friend bool RunJellyfishAttackTests();
    friend bool RunIceJellyfishStateTests();
    struct PendingVolley {
        int32_t volley;
        int32_t tentacle;
        Vector3 target;
    };
    void FireVolley(IceJellyfish& boss, const PendingVolley& shot);
    std::array<PendingVolley, 3> pendingVolleys_ {};
    size_t pendingVolleyCount_ = 0;
    Vector3 lockedTarget_ {};
    std::array<int32_t, 3> iceSpearTentacles_ { -1, -1, -1 };
    int32_t iceSpearVolley_ = -1;
    float iceSpearTimer_ = 0.0f;
    float iceSpearPoseWeight_ = 0.0f;
};
