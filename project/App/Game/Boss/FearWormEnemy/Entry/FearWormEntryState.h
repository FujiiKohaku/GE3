#pragma once

#include "../State/FearWormState.h"
#include "JumOut/FearWormJumOutState.h"
#include "WaitForBody/FearWormWaitForBodyState.h"
#include "Engine/Math/MathStruct.h"
#include <cstddef>

// 登場全体の経過時間・内部状態と、各段階で共有する軌道計算を管理する。
class FearWormEntryState final : public FearWormState {
public:
    enum class EntryPhase { JumOut, WaitForBody };

    FearWormEntryState();
    ~FearWormEntryState() override;
    void Initialize();
    void Reset();
    void Enter(FearWormEnemy& boss) override;
    void Update(FearWormEnemy& boss) override;
    void Exit(FearWormEnemy& boss) override;
    void Draw();

    bool HasBreachedSurface() const { return hasPlayedSplash_; }
    void SetWaterHeight(float waterHeight) { waterHeight_ = waterHeight; }
    float GetWaterHeight() const { return waterHeight_; }
    EntryPhase GetPhase() const { return phase_; }
    // 登場軌道の終点で使う周回角。次の状態への受け渡しはEnemy本体が行う。
    float GetExitOrbitAngleRadians() const { return kExitOrbitAngleRadians; }

    // 頭部が登場軌道の終点に到着する時刻を過ぎたか確認する。
    bool IsHeadFlightFinished() const
    {
        return elapsedSeconds_ >= kEntryShadowDurationSeconds + kEntryFlightDurationSeconds;
    }

    static float CalculateEntryDurationSeconds(size_t segmentCount);
    static Vector3 CalculateEntryPosition(const Vector3& surfacePosition, const Vector3& battlePosition,
        float elapsedSeconds, size_t segmentIndex, float segmentSpacing);

private:
    friend class FearWormJumOutState;
    friend class FearWormWaitForBodyState;
    friend bool RunFearWormEntryTests();
    friend bool RunFearWormStateTests();

    static constexpr float kEntryShadowDurationSeconds = 2.0f;
    static constexpr float kEntryFlightDurationSeconds = 2.4f;
    static constexpr float kEntrySegmentDelaySeconds = 0.075f;
    static constexpr float kExitOrbitAngleRadians = 2.35f;
    // プレイヤーを基準にした、水面上の出現位置のオフセット。
    static constexpr float kEntrySurfaceOffsetX = -12.0f;
    static constexpr float kEntrySurfaceOffsetZ = 86.0f;
    static constexpr int kMaxPhaseUpdatesPerFrame = 2;

    void ChangePhase(EntryPhase phase, FearWormEnemy& boss);
    // 両段階で共通の登場時刻を使い、全身の位置とボスの代表位置を更新する。
    void UpdateSegmentPositions(FearWormEnemy& boss);
    Vector3 EntrySurfacePosition(const Vector3& playerPosition) const;

    FearWormJumOutState jumOutState_;
    FearWormWaitForBodyState waitForBodyState_;
    FearWormEntryPhase* currentPhase_ = nullptr;
    EntryPhase phase_ = EntryPhase::JumOut;

    float elapsedSeconds_ = 0.0f;
    float durationSeconds_ = 0.0f;
    float waterHeight_ = -30.0f;
    bool hasPlayedSplash_ = false;
};
