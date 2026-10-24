#pragma once

#include "App/Game/Common/GameTiming.h"
#include "App/Game/Enemy/NormalEnemy/NormalEnemy.h"
#include "App/Game/Enemy/PaintEnemy/PaintShooterEnemy.h"
#include "App/Game/Enemy/MoveEnemy/MoveEnemy.h"
#include "App/Game/Enemy/Bullet/Types/SwarmPulseBullet.h"
#include "App/Game/Player/Combat/Bullet/Types/HomingMissileBullet.h"
#include "FearWormTimingTests.h"
#include <array>
#include <cmath>
#include <cstdio>

class SwarmPulseTimingProbe final : public SwarmPulseBullet {
public:
    void Step() { Move(); }
    Vector3 Position() const { return transform_.translate; }
    float RotationRadians() const { return transform_.rotate.z; }
};

class HomingTimingProbe final : public HomingMissileBullet {
public:
    void Step() { Move(); }
    Vector3 Position() const { return transform_.translate; }
};

inline bool RunAppTimingTests()
{
    FearWormTestClock testClock;
    const std::array<int32_t, 3> frameRates = { 30, 60, 120 };
    std::array<Vector3, 3> swarmPositions {};
    std::array<Vector3, 3> homingPositions {};
    for (size_t rateIndex = 0; rateIndex < frameRates.size(); rateIndex++) {
        const int32_t framesPerSecond = frameRates[rateIndex];
        testClock.SetDelta(1.0f / static_cast<float>(framesPerSecond));
        Player player;
        NormalEnemy normal;
        normal.player_ = &player;
        normal.transform_.translate = { 0.0f, 0.0f, 50.0f };
        NormalEnemySettings settings;
        settings.fireIntervalFrames = 90;
        normal.ApplySettings(settings);
        PaintShooterEnemy paint;
        paint.player_ = &player;
        paint.transform_.translate = { 0.0f, 0.0f, 50.0f };
        MoveEnemy moving;
        moving.player_ = &player;
        moving.transform_.translate = { 0.0f, 0.0f, 50.0f };
        for (int32_t stepIndex = 0; stepIndex < framesPerSecond * 2; stepIndex++) {
            // 描画資源がないため弾生成は行わず、実際の射程判定・周期更新を通す。
            normal.Attack();
            paint.Attack();
            moving.Attack();
            player.ApplyRailAreaForce({ 0.01f, 0.02f, 0.0f });
        }
        if (std::abs(normal.fireIntervalSeconds_ - 1.5f) > 0.000001f
            || std::abs(normal.fireElapsedSeconds_ - 0.5f) > 0.00001f
            || std::abs(paint.fireElapsedSeconds_ - 0.5f) > 0.00001f
            || std::abs(moving.fireElapsedSeconds_) > 0.00001f
            || std::abs(player.GetRailOffset().x - 1.2f) > 0.00001f
            || std::abs(player.GetRailOffset().y - 2.4f) > 0.00001f) {
            std::printf("App timing: enemy/rail mismatch at %d FPS\n", framesPerSecond);
            return false;
        }

        SwarmPulseTimingProbe swarm;
        swarm.SetTranslate({});
        swarm.SetSwarmVelocity({ 0.1f, 0.2f, 0.3f });
        BaseEnemy target;
        target.SetPosition({ 50.0f, 10.0f, 200.0f });
        auto targets = std::make_shared<const std::vector<BaseEnemy*>>(std::vector<BaseEnemy*> { &target });
        HomingTimingProbe homing;
        homing.SetTranslate({});
        homing.SetVelocity({ 0.0f, 0.0f, 1.0f });
        homing.SetTarget(&target, targets);
        for (int32_t stepIndex = 0; stepIndex < framesPerSecond; stepIndex++) {
            swarm.Step();
            homing.Step();
        }
        swarmPositions[rateIndex] = swarm.Position();
        homingPositions[rateIndex] = homing.Position();
        if (std::abs(swarm.RotationRadians() - 13.2f) > 0.0001f) {
            return false;
        }
        const Vector3 pausedSwarmPosition = swarm.Position();
        const Vector3 pausedHomingPosition = homing.Position();
        const Vector3 pausedRailOffset = player.GetRailOffset();
        const float pausedFireSeconds = normal.fireElapsedSeconds_;
        testClock.SetDelta(0.0f);
        swarm.Step();
        homing.Step();
        normal.Attack();
        player.ApplyRailAreaForce({ 0.01f, 0.02f, 0.0f });
        if (Vector3LengthSquared(swarm.Position() - pausedSwarmPosition) != 0.0f
            || Vector3LengthSquared(homing.Position() - pausedHomingPosition) != 0.0f
            || Vector3LengthSquared(player.GetRailOffset() - pausedRailOffset) != 0.0f
            || normal.fireElapsedSeconds_ != pausedFireSeconds) {
            return false;
        }
    }
    if (Vector3LengthSquared(swarmPositions[0] - swarmPositions[1]) > 0.000001f
        || Vector3LengthSquared(swarmPositions[2] - swarmPositions[1]) > 0.000001f
        || Vector3LengthSquared(homingPositions[0] - homingPositions[1]) > 1.0f
        || Vector3LengthSquared(homingPositions[2] - homingPositions[1]) > 1.0f) {
        std::puts("App timing: bullet trajectory mismatch.");
        return false;
    }
    std::puts("App timing: 30/60/120 FPS, enemy intervals, legacy settings, rail forces, bullets and pause passed.");
    return true;
}
