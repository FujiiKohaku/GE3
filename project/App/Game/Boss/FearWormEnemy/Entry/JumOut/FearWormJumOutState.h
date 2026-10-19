#pragma once

#include "../State/FearWormEntryPhase.h"
#include "Engine/Math/MathStruct.h"
#include <memory>

class Object3d;

// 水面の予兆から飛び出し移動、水しぶきまでを担当する。
class FearWormJumOutState final : public FearWormEntryPhase {
public:
    FearWormJumOutState();
    ~FearWormJumOutState() override;
    void Initialize();
    void Enter(FearWormEntryState& entry, FearWormEnemy& boss) override;
    void Update(FearWormEntryState& entry, FearWormEnemy& boss) override;
    void PlaySplashIfNeeded(FearWormEntryState& entry, FearWormEnemy& boss);
    void UpdateSurface(FearWormEntryState& entry, FearWormEnemy& boss);
    void Draw();

private:
    static constexpr float kInitialOrbitAngle = 2.35f;
    static constexpr float kSplashHeightOffset = 1.0f;
    static constexpr float kSurfaceRotationRadians = 1.5707963268f;
    static constexpr Vector3 kSurfaceScale = { 42.0f, 68.0f, 1.0f };
    static constexpr float kSurfaceHeightOffset = 2.5f;
    static constexpr float kSurfaceOffsetZ = -14.0f;

    std::unique_ptr<Object3d> surface_;
};
