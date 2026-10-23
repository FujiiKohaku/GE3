#pragma once
#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"
#include "App/Game/Player/Player.h"
#include <cmath>
#include <numbers>
#include <cstdio>

inline bool ReportJellyfishAttackTestFailure(int32_t line)
{
    std::printf("IceJellyfish attack assertion failed at line %d\n", line);
    return false;
}

inline bool RunJellyfishAttackTests()
{
    // 破壊後、姿勢の再更新前でも古い判定が後続弾を遮らない。
    {
        IceJellyfish collisionBoss;
        collisionBoss.bell_ = std::make_unique<Object3d>();
        collisionBoss.tentacleHp_.fill(0.0f);
        collisionBoss.tentacleHp_[0] = 9.0f;
        collisionBoss.coreExposed_ = true;
        collisionBoss.coreCollider_ = { { 0.0f, 0.0f, 10.0f }, 1.0f };
        collisionBoss.tentacleColliders_.push_back(CollisionManager::MakeOBB(
            { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, {}));
        collisionBoss.tentacleColliderParts_.push_back(collisionBoss.SegmentPartIndex(0, 0));
        const Sphere shot { { 0.0f, 0.0f, -10.0f }, 0.1f };
        const Vector3 movement { 0.0f, 0.0f, 30.0f };
        int32_t part = -1;
        if (!collisionBoss.SweepBullet(shot, movement, part).isHit || part != collisionBoss.SegmentPartIndex(0, 0))
            return ReportJellyfishAttackTestFailure(__LINE__);
        collisionBoss.tentacleHp_[0] = 0.0f;
        if (!collisionBoss.SweepBullet(shot, movement, part).isHit || part != collisionBoss.kCorePart)
            return ReportJellyfishAttackTestFailure(__LINE__);
        std::vector<EnemyCollisionPart> parts;
        collisionBoss.GetCollisionParts(parts);
        if (parts.size() != 1) return ReportJellyfishAttackTestFailure(__LINE__);
    }
    IceJellyfish boss;
    boss.tentacleHp_.fill(6.0f);
    // 浮遊のタイミングで一度だけ開始し、波状弾→氷槍→氷柱の順を保つ。
    boss.battleState_.UpdateAttackSequence(boss, -std::numbers::pi_v<float> / 2.0f, 0.0f);
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.0f);
    if (boss.battleState_.nextAttackIndex_ != 1 || boss.battleState_.pulseBarrageAttack_.pulseBarrageWave_ != 0) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.0f);
    if (boss.battleState_.nextAttackIndex_ != 1 || boss.battleState_.pulseBarrageAttack_.pulseBarrageWave_ != 1) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.battleState_.UpdateAttackSequence(boss, -std::numbers::pi_v<float> / 2.0f, 0.18f);
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.0f);
    if (boss.battleState_.nextAttackIndex_ != 2 || boss.battleState_.iceSpearAttack_.iceSpearVolley_ != 0 || boss.battleState_.pulseBarrageAttack_.pulseBarrageWave_ != 2) return ReportJellyfishAttackTestFailure(__LINE__);

    Player player;
    boss.player_ = &player;
    boss.battleState_.UpdateAttackSequence(boss, -std::numbers::pi_v<float> / 2.0f, 0.0f);
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.0f);
    if (boss.battleState_.nextAttackIndex_ != 0 || boss.battleState_.icePillarAttack_.icePillarWave_ != 0) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.player_ = nullptr; // 描画や弾を生成せず、並行動作と時間を検証する。
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.18f);
    if (boss.battleState_.pulseBarrageAttack_.pulseBarrageWave_ != -1 || std::abs(boss.battleState_.iceSpearAttack_.iceSpearTimer_ - 0.18f) > 0.0001f ||
        std::abs(boss.battleState_.icePillarAttack_.icePillarTimer_ - 0.18f) > 0.0001f) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.battleState_.iceSpearAttack_.Update(boss, 0.82f);
    boss.battleState_.iceSpearAttack_.Update(boss, 0.20f);
    boss.battleState_.iceSpearAttack_.Update(boss, 0.21f);
    if (boss.battleState_.iceSpearAttack_.iceSpearVolley_ != 3) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.battleState_.iceSpearAttack_.Update(boss, 1.0f);
    if (boss.battleState_.iceSpearAttack_.iceSpearVolley_ != -1 || boss.battleState_.iceSpearAttack_.iceSpearPoseWeight_ != 0.0f) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.battleState_.icePillarAttack_.Update(boss, 1.82f);
    if (!boss.battleState_.icePillarAttack_.icePillarModelSpawnedForWave_) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.player_ = &player;
    const float pillarTime = boss.battleState_.icePillarAttack_.icePillarTimer_;
    boss.battleState_.icePillarAttack_.hasHitPlayerByWave_[0] = true;
    boss.battleState_.icePillarAttack_.Start(boss);
    if (boss.battleState_.icePillarAttack_.icePillarTimer_ != pillarTime) return ReportJellyfishAttackTestFailure(__LINE__); // 氷柱の実行中は再開始しない。
    if (!boss.battleState_.icePillarAttack_.hasHitPlayerByWave_[0]) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.player_ = nullptr;
    boss.battleState_.icePillarAttack_.Update(boss, 7.0f);
    if (boss.battleState_.icePillarAttack_.icePillarWave_ != 1 || boss.battleState_.icePillarAttack_.icePillarTimer_ != 0.0f) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.battleState_.icePillarAttack_.Update(boss, 9.0f);
    boss.battleState_.icePillarAttack_.Update(boss, 9.0f);
    if (boss.battleState_.icePillarAttack_.icePillarWave_ != -1) return ReportJellyfishAttackTestFailure(__LINE__);
    boss.player_ = &player;
    boss.battleState_.icePillarAttack_.Start(boss);
    if (boss.battleState_.icePillarAttack_.hasHitPlayerByWave_ != std::array<bool, 3> {}) {
        return ReportJellyfishAttackTestFailure(__LINE__);
    }
    boss.player_ = nullptr;
    boss.tentacleHp_.fill(0.0f);
    boss.battleState_.iceSpearAttack_.Start(boss);
    if (boss.battleState_.iceSpearAttack_.iceSpearVolley_ != -1) return ReportJellyfishAttackTestFailure(__LINE__);

    // 分離前の姿勢・色を固定値で確認し、並行攻撃の合成順も維持する。
    IceJellyfish motionBoss;
    motionBoss.tentacleHp_.fill(100.0f);
    player.SetTranslate({ 4.0f, 5.0f, 100.0f });
    motionBoss.player_ = &player;
    motionBoss.bodyPosition_ = { 10.0f, 20.0f, 30.0f };
    IceJellyfishBattleState& battle = motionBoss.battleState_;
    battle.iceSpearAttack_.lockedTarget_ = { 30.0f, 12.0f, 160.0f };
    battle.iceSpearAttack_.iceSpearTentacles_ = { 0, 2, 4 };
    battle.iceSpearAttack_.iceSpearVolley_ = 0;
    battle.iceSpearAttack_.iceSpearPoseWeight_ = 0.5f;
    battle.icePillarAttack_.icePillarWave_ = 0;
    battle.icePillarAttack_.icePillarTimer_ = 1.0f;
    IceJellyfishAttackMotionContext context { 0, 6, 1.0f, { 1.0f, -4.0f, 2.0f }, 0.6f, 0.8f };
    const Vector3 normalDirection = Normalize(Vector3 { 0.2f, -0.9f, 0.3f });
    const Vector4 kBaseColor { 0.55f, 0.80f, 1.0f, 0.95f };
    const Vector3 combinedDirection = battle.ApplyAttackTentacleDirection(motionBoss, context, normalDirection);
    const Vector4 combinedColor = battle.ApplyAttackTentacleColor(motionBoss, context, kBaseColor);
    if (Vector3LengthSquared(combinedDirection - Vector3 { 0.14997533f, -0.82493063f, 0.54497417f }) > 0.00000001f
        || std::abs(combinedColor.x - 1.10f) > 0.00001f || std::abs(combinedColor.y - 0.625f) > 0.00001f
        || std::abs(combinedColor.z - 1.90f) > 0.00001f || std::abs(combinedColor.w - 0.975f) > 0.00001f) {
        return ReportJellyfishAttackTestFailure(__LINE__);
    }
    // 対象外の触手は通常姿勢・色を維持する。
    context.tentacleIndex = 1;
    const Vector4 inactiveColor = battle.ApplyAttackTentacleColor(motionBoss, context, kBaseColor);
    if (Vector3LengthSquared(battle.ApplyAttackTentacleDirection(motionBoss, context, normalDirection)
            - normalDirection) > 0.00000001f
        || inactiveColor.x != kBaseColor.x || inactiveColor.y != kBaseColor.y
        || inactiveColor.z != kBaseColor.z || inactiveColor.w != kBaseColor.w) {
        return ReportJellyfishAttackTestFailure(__LINE__);
    }
    context.tentacleIndex = 0;
    battle.iceSpearAttack_.Reset();
    const Vector4 pillarColor = battle.ApplyAttackTentacleColor(motionBoss, context, kBaseColor);
    if (Vector3LengthSquared(battle.ApplyAttackTentacleDirection(motionBoss, context, normalDirection)
            - Vector3 { 0.16027723f, -0.95794935f, 0.23800052f }) > 0.00000001f
        || std::abs(pillarColor.x - 0.875f) > 0.00001f || std::abs(pillarColor.y - 1.475f) > 0.00001f
        || std::abs(pillarColor.z - 1.85f) > 0.00001f || pillarColor.w != kBaseColor.w) {
        return ReportJellyfishAttackTestFailure(__LINE__);
    }
    battle.Exit(motionBoss);
    return Vector3LengthSquared(battle.ApplyAttackTentacleDirection(motionBoss, context, normalDirection)
        - normalDirection) < 0.00000001f && !battle.HasIcePillarModels();
}
