#pragma once

#include "App/Game/Boss/FearWormEnemy/Battle/State/FearWormTiming.h"
#include "App/Game/Enemy/Bullet/Types/TrackingEnemyBullet.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Player/Player.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

// 公開APIだけで一定のdeltaTimeを準備する。シミュレーション中は時計を更新しない。
class FearWormTestClock {
public:
    FearWormTestClock()
    {
        previousMaxDeltaSeconds_ = TimeManager::GetInstance()->GetMaxDeltaTime();
        previousTimeScale_ = TimeManager::GetInstance()->GetTimeScale();
    }

    ~FearWormTestClock()
    {
        TimeManager* timeManager = TimeManager::GetInstance();
        timeManager->Initialize();
        timeManager->SetMaxDeltaTime(previousMaxDeltaSeconds_);
        timeManager->SetTimeScale(previousTimeScale_);
    }

    void SetDelta(float deltaTimeSeconds)
    {
        TimeManager* timeManager = TimeManager::GetInstance();
        timeManager->SetMaxDeltaTime(0.001f);
        timeManager->SetTimeScale(deltaTimeSeconds / 0.001f);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        timeManager->Update();
    }

private:
    float previousMaxDeltaSeconds_ = 0.0f;
    float previousTimeScale_ = 1.0f;
};

class FearWormTrackingBulletProbe final : public TrackingEnemyBullet {
public:
    FearWormTrackingBulletProbe()
    {
        transform_.translate = {};
        velocity_ = {};
    }
    void Step() { Move(); }
    Vector3 MotionPosition() const { return transform_.translate; }
};

inline bool RunFearWormTimingTests()
{
    FearWormTestClock testClock;
    const std::array<int32_t, 3> frameRates = { 30, 60, 120 };
    std::array<Vector3, 3> trackedPositions {};
    for (size_t rateIndex = 0; rateIndex < frameRates.size(); rateIndex++) {
        const int32_t framesPerSecond = frameRates[rateIndex];
        const float deltaTimeSeconds = 1.0f / static_cast<float>(framesPerSecond);
        float normalElapsedSeconds = 0.0f;
        float barrageElapsedSeconds = 0.0f;
        int32_t normalShotCount = 0;
        int32_t barrageShotCount = 0;
        float followPosition = 0.0f;
        for (int32_t stepIndex = 0; stepIndex < framesPerSecond * 22; stepIndex++) {
            normalShotCount += FearWormTiming::AdvanceInterval(normalElapsedSeconds,
                22.0f / 60.0f, deltaTimeSeconds, 3);
            barrageShotCount += FearWormTiming::AdvanceInterval(barrageElapsedSeconds,
                0.05f, deltaTimeSeconds, 3);
            if (stepIndex < framesPerSecond) {
                followPosition += (1.0f - followPosition)
                    * FearWormTiming::CalculateFollowBlend(0.015f, deltaTimeSeconds);
            }
        }
        const float expectedFollowPosition = 1.0f - std::pow(0.985f, 60.0f);
        if (normalShotCount != 60 || barrageShotCount != 440
            || std::abs(followPosition - expectedFollowPosition) > 0.00001f) {
            std::printf("FearWorm timing failed at %d FPS: normal=%d barrage=%d follow=%f\n",
                framesPerSecond, normalShotCount, barrageShotCount, followPosition);
            return false;
        }

        testClock.SetDelta(deltaTimeSeconds);
        FearWormTrackingBulletProbe missile;
        for (int32_t stepIndex = 0; stepIndex < framesPerSecond * 3 / 10; stepIndex++) {
            missile.Step();
        }
        // 60FPSでの従来の打ち上げ距離は約8.31。積分の刻みによる差は許容する。
        if (std::abs(missile.MotionPosition().y - 8.31f) > 0.3f) {
            std::printf("FearWorm missile failed at %d FPS: y=%f\n", framesPerSecond, missile.MotionPosition().y);
            return false;
        }
        Player targetPlayer;
        FearWormTrackingBulletProbe trackedMissile;
        trackedMissile.SetPlayer(&targetPlayer);
        for (int32_t stepIndex = 0; stepIndex < framesPerSecond * 3; stepIndex++) {
            trackedMissile.Step();
        }
        trackedPositions[rateIndex] = trackedMissile.MotionPosition();
        const Vector3 pausedPosition = missile.MotionPosition();
        testClock.SetDelta(0.0f);
        missile.Step();
        if (Vector3LengthSquared(missile.MotionPosition() - pausedPosition) != 0.0f) {
            return false;
        }
    }
    // 打ち上げから追尾終了後までを通す。位相境界と数値積分による差を許容する。
    if (Vector3LengthSquared(trackedPositions[0] - trackedPositions[1]) > 9.0f
        || Vector3LengthSquared(trackedPositions[2] - trackedPositions[1]) > 9.0f) {
        std::printf("FearWorm tracked missile mismatch: y=%f/%f/%f\n",
            trackedPositions[0].y, trackedPositions[1].y, trackedPositions[2].y);
        return false;
    }
    float elapsedSeconds = 0.0f;
    if (FearWormTiming::AdvanceInterval(elapsedSeconds, 0.05f, 5.0f, 3) != 3
        || FearWormTiming::AdvanceInterval(elapsedSeconds, 0.05f, 0.001f, 3) != 0
        || FearWormTiming::CalculateFollowBlend(0.1f, 0.0f) != 0.0f) {
        return false;
    }
    elapsedSeconds = 0.1f;
    if (FearWormTiming::AdvanceInterval(elapsedSeconds, 0.05f, 0.0f, 3) != 0
        || elapsedSeconds != 0.1f) {
        return false;
    }
    std::puts("FearWorm timing: 30/60/120 FPS, intervals, follow, missile and pause passed.");
    return true;
}
