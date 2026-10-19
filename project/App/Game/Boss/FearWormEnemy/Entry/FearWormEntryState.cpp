#include "FearWormEntryState.h"
#include "../FearWormEnemy.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

FearWormEntryState::FearWormEntryState() = default;
FearWormEntryState::~FearWormEntryState() = default;

void FearWormEntryState::Reset()
{
    elapsedSeconds_ = 0.0f;
    durationSeconds_ = 0.0f;
    hasPlayedSplash_ = false;
    phase_ = EntryPhase::JumOut;
    currentPhase_ = nullptr;
}

void FearWormEntryState::Initialize()
{
    jumOutState_.Initialize();
}

void FearWormEntryState::Draw()
{
    jumOutState_.Draw();
}

void FearWormEntryState::Enter(FearWormEnemy& boss)
{
    Reset();
    durationSeconds_ = CalculateEntryDurationSeconds(boss.segments_.size());
    ChangePhase(EntryPhase::JumOut, boss);
}

void FearWormEntryState::Update(FearWormEnemy& boss)
{
    if (boss.player_ == nullptr) {
        boss.ChangeState(FearWormEnemy::BossState::Wait);
        return;
    }

    elapsedSeconds_ += TimeManager::GetInstance()->GetDeltaTime();
    // 低フレームレートで複数段階を越えた場合も、そのフレーム内で追いつく。
    for (int phaseIndex = 0; phaseIndex < kMaxPhaseUpdatesPerFrame; ++phaseIndex) {
        if (currentPhase_ == nullptr) {
            break;
        }
        FearWormEntryPhase* previousPhase = currentPhase_;
        currentPhase_->Update(*this, boss);
        if (currentPhase_ == previousPhase) {
            break;
        }
    }
}

void FearWormEntryState::Exit(FearWormEnemy& boss)
{
    if (currentPhase_ != nullptr) {
        currentPhase_->Exit(*this, boss);
        currentPhase_ = nullptr;
    }
}

void FearWormEntryState::ChangePhase(EntryPhase phase, FearWormEnemy& boss)
{
    if (currentPhase_ != nullptr && phase_ == phase) {
        return;
    }
    FearWormEntryPhase* nextPhase = nullptr;
    switch (phase) {
    case EntryPhase::JumOut:
        nextPhase = &jumOutState_;
        break;
    case EntryPhase::WaitForBody:
        nextPhase = &waitForBodyState_;
        break;
    default:
        return;
    }
    if (currentPhase_ != nullptr) {
        currentPhase_->Exit(*this, boss);
    }
    phase_ = phase;
    currentPhase_ = nextPhase;
    currentPhase_->Enter(*this, boss);
}

float FearWormEntryState::CalculateEntryDurationSeconds(size_t segmentCount)
{
    size_t delayedSegmentCount = 0;
    if (segmentCount > 0) {
        delayedSegmentCount = segmentCount - 1;
    }
    return kEntryShadowDurationSeconds + kEntryFlightDurationSeconds
        + static_cast<float>(delayedSegmentCount) * kEntrySegmentDelaySeconds;
}

Vector3 FearWormEntryState::CalculateEntryPosition(const Vector3& surfacePosition, const Vector3& battlePosition,
    float elapsedSeconds, size_t segmentIndex, float segmentSpacing)
{
    float flightRate = std::clamp((elapsedSeconds - kEntryShadowDurationSeconds
        - static_cast<float>(segmentIndex) * kEntrySegmentDelaySeconds) / kEntryFlightDurationSeconds, 0.0f, 1.0f);
    float easedRate = flightRate * flightRate * (3.0f - 2.0f * flightRate);
    Vector3 target = battlePosition;
    target.z -= static_cast<float>(segmentIndex) * segmentSpacing;
    Vector3 start = surfacePosition;
    start.y -= 12.0f;
    float arcHeight = (std::max)(battlePosition.y - surfacePosition.y + 25.0f, 50.0f);
    float arcEnvelope = std::sin(flightRate * 3.1415926535f);
    float waveEnvelope = arcEnvelope * arcEnvelope;
    float wavePhase = elapsedSeconds * 5.2f - static_cast<float>(segmentIndex) * 0.70f;
    // 頭から尾へ位相をずらし、着地点に近づくほど振幅を落とす。
    float lateralWave = std::sin(wavePhase) * 6.0f * waveEnvelope;
    float verticalWave = std::sin(wavePhase + 0.9f) * 1.8f * waveEnvelope;
    float forwardWave = std::cos(wavePhase) * 2.0f * waveEnvelope;
    return {
        start.x + (target.x - start.x) * easedRate + lateralWave,
        start.y + (target.y - start.y) * easedRate + arcEnvelope * arcHeight + verticalWave,
        start.z + (target.z - start.z) * easedRate + forwardWave
    };
}

Vector3 FearWormEntryState::EntrySurfacePosition(const Vector3& playerPosition) const
{
    return {
        playerPosition.x + kEntrySurfaceOffsetX,
        waterHeight_,
        playerPosition.z + kEntrySurfaceOffsetZ
    };
}

