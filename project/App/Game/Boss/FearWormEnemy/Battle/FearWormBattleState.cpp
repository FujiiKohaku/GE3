// 戦闘中の移動、攻撃、照準とビームの処理。
#include "FearWormBattleState.h"
#include "../FearWormEnemy.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Math/MathStruct.h"
#include <algorithm>
#include <cmath>
#include "Engine/Camera/Camera.h"
#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"
#include "App/Game/Enemy/Bullet/Types/TrackingEnemyBullet.h"
#include <utility>
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"

void FearWormBattleState::Update(FearWormEnemy& boss)
{
    if (boss.player_ == nullptr) {
        return;
    }

    float movementSpeedRate = boss.MovementSpeedRate();
    Vector3 playerPosition = boss.player_->GetTranslate();

    UpdateMadMode(boss, movementSpeedRate);
    UpdateMissileAttack(boss);
    UpdateBattleMovement(boss, playerPosition, movementSpeedRate);
    UpdateBattleAttack(boss);
}

// 激昂中の攻撃と移動を更新し、通常時とは異なる圧力をかける。
void FearWormBattleState::UpdateMadMode(FearWormEnemy& boss, float movementSpeedRate)
{
    // HP30%以下かつ未発動かつ未完了のときに発狂トリガー
    bool shouldTriggerMad = (boss.HealthRate() <= 0.30f) && !boss.isMadModeActive_ && !boss.isMadModeFinished_;

    if (shouldTriggerMad) {
        boss.isMadModeActive_ = true;
        boss.madModeTimer_ = 0.0f;
        ChangeMovementPattern(boss, MovementPattern::Spiral);
        boss.spiralAngle_ = 0.0f;
        boss.barrageAngle_ = 0.0f;

        if (boss.isHeadChargeActive_) {
            FinishHeadChargeAttack(boss);
        }
        boss.isMissileAttackActive_ = false;
        boss.isFiringMissilesSequence_ = false;
    }

    // 発狂モードタイマー更新 (7秒制限: 2秒威嚇 + 5秒大暴れ)
    if (boss.isMadModeActive_) {
        boss.madModeTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        if (boss.madModeTimer_ >= 7.0f) {
            boss.isMadModeActive_ = false;
            boss.isMadModeFinished_ = true; // 終了フラグを立てて、再度発狂しないようにする
            ChangeMovementPattern(boss, MovementPattern::Orbit); // 通常パターンに復帰
            boss.movementPatternTimer_ = 0.0f;
            boss.missileAttackTimer_ = 0.0f;
        }
    }

    // 回転角度の更新
    boss.orbitAngle_ += TimeManager::GetInstance()->GetDeltaTime() * FearWormEnemy::kOrbitAngularSpeed * movementSpeedRate;

    if (boss.isMadModeActive_) {
        boss.spiralAngle_ += TimeManager::GetInstance()->GetDeltaTime() * 2.5f * movementSpeedRate;
        boss.barrageAngle_ += TimeManager::GetInstance()->GetDeltaTime() * 4.0f;
    }
}

// 部位からミサイルを撃つ順番と発射タイミングを管理する。
void FearWormBattleState::UpdateMissileAttack(FearWormEnemy& boss)
{
    // ミサイル攻撃の進行管理（発狂モード中は行わない）
    if (!boss.isMadModeActive_) {
        if (!boss.isMissileAttackActive_ && !boss.isFiringMissilesSequence_) {
            boss.missileAttackTimer_ += TimeManager::GetInstance()->GetDeltaTime();
            if (boss.missileAttackTimer_ >= FearWormEnemy::kMissileAttackInterval) {
                boss.isMissileAttackActive_ = true;
                boss.hasFiredMissiles_ = false;
                boss.missilePhaseTimer_ = 0.0f;
                ChangeMovementPattern(boss, MovementPattern::Line);
                boss.lineTransitionTimer_ = 0.0f;
                // 進行中のチャージ攻撃があればキャンセルする
                if (boss.isHeadChargeActive_) {
                    FinishHeadChargeAttack(boss);
                }
            }
        } else if (boss.isMissileAttackActive_) {
            // Line状態（前準備・チャージ警告演出）
            boss.missilePhaseTimer_ += TimeManager::GetInstance()->GetDeltaTime();
            if (boss.missilePhaseTimer_ >= FearWormEnemy::kMissileAimWaitDuration) {
                // Line準備完了！発射シーケンスを開始する（Line状態・移動パターンは維持）
                boss.isMissileAttackActive_ = false;

                // 生存しているセグメントのインデックスをリストアップ
                boss.missileTargetIndices_.clear();
                std::vector<size_t> aliveIndices;
                for (size_t index = 0; index < boss.segments_.size(); ++index) {
                    if (boss.segments_[index].isAlive) {
                        aliveIndices.push_back(index);
                    }
                }

                if (!aliveIndices.empty()) {
                    boss.isFiringMissilesSequence_ = true;
                    boss.missileFireTimer_ = FearWormEnemy::kMissileFireInterval; // 最初は即座に撃ち出すようにタイマーを満たす
                    boss.missileShotCount_ = 0;

                    // 生存している全部位から、頭側から順番にミサイルを発射する。
                    boss.missileTargetIndices_ = aliveIndices;
                } else {
                    // 生存セグメントがない（通常はあり得ないが安全のため）
                    ChangeMovementPattern(boss, MovementPattern::Orbit);
                    boss.movementPatternTimer_ = 0.0f;
                    boss.missileAttackTimer_ = 0.0f;
                }
            }
        }

        // 時間差ミサイル発射シーケンスの実行（Line状態のまま実行される）
        if (boss.isFiringMissilesSequence_ && boss.player_ != nullptr && boss.bulletModel_ != nullptr) {
            boss.missileFireTimer_ += TimeManager::GetInstance()->GetDeltaTime();
            int32_t maxShots = static_cast<int32_t>(boss.missileTargetIndices_.size());

            if (boss.missileShotCount_ < maxShots) {
                // 発射フェーズ
                if (boss.missileFireTimer_ >= FearWormEnemy::kMissileFireInterval) {
                    boss.missileFireTimer_ = 0.0f;
                    size_t segmentIndex = boss.missileTargetIndices_[boss.missileShotCount_];
                    if (segmentIndex < boss.segments_.size() && boss.segments_[segmentIndex].isAlive) {
                        FireSingleMissile(boss, segmentIndex);
                    }
                    boss.missileShotCount_++;
                }
            } else {
                // 撃ち終わった後の硬直フェーズ（隙を見せる）
                if (boss.missileFireTimer_ >= FearWormEnemy::kMissilePostFireCooldown) {
                    // 硬直終了！通常状態に戻り、Orbitへ復帰
                    boss.isFiringMissilesSequence_ = false;
                    boss.missileAttackTimer_ = 0.0f;
                    ChangeMovementPattern(boss, MovementPattern::Orbit);
                    boss.movementPatternTimer_ = 0.0f;
                }
            }
        }
    }
}

// プレイヤーを基準に移動先を決め、速度倍率を反映して頭を動かす。
void FearWormBattleState::UpdateBattleMovement(FearWormEnemy& boss, const Vector3& playerPosition, float movementSpeedRate)
{
    // 戦闘目的地を計算
    Vector3 target = BattleTarget(boss, playerPosition, movementSpeedRate);

    // 移動処理
    // 頭だけかつビーム発射フェーズ（Charge, Fire, FadeOut）のときは、X・Y軸の移動のみ静止し、Z軸の前進は維持する
    bool isBeamActive = !boss.HasAliveBodyParts() && (boss.beamState_ == BeamState::Charge || boss.beamState_ == BeamState::Fire || boss.beamState_ == BeamState::FadeOut);
    if (isBeamActive) {
        // Z座標のみ追従前進させる (floatのLerp)
        float t = boss.moveSpeed_ * movementSpeedRate;
        boss.segments_[0].position.z = boss.segments_[0].position.z + (target.z - boss.segments_[0].position.z) * t;
    } else {
        boss.segments_[0].position = Lerp(boss.segments_[0].position, target, boss.moveSpeed_ * movementSpeedRate);
    }
    boss.transform_.translate = boss.segments_[0].position;
}

// 戦闘中の攻撃タイマーを進め、選択された攻撃の処理へつなげる。
void FearWormBattleState::UpdateBattleAttack(FearWormEnemy& boss)
{
    // 攻撃処理の実行
    if (!boss.HasAliveBodyParts()) {
        // 頭部だけになったらビーム攻撃に移行
        if (boss.isHeadChargeActive_) {
            FinishHeadChargeAttack(boss);
        }
        boss.isMissileAttackActive_ = false;
        boss.isFiringMissilesSequence_ = false;

        UpdateBeamAttack(boss);
    } else if (boss.isMadModeActive_) {
        UpdateSpiralBarrage(boss);
    } else {
        if (!boss.isMissileAttackActive_ && !boss.isFiringMissilesSequence_) {
            boss.Attack();
        }
    }
}

// プレイヤーの周囲を回り込む軌道上の目標位置を求める。
Vector3 FearWormBattleState::CalculateOrbitTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 基本の楕円軌道を計算する。
    float orbitX = std::cos(context.orbitAngle) * FearWormEnemy::kOrbitRadiusX;

    float orbitY = std::sin(context.orbitAngle) * FearWormEnemy::kOrbitRadiusY;

    // 単調な周回にならないよう、横揺れと奥行きの揺れを加える。
    float lateralSway = std::sin(context.orbitAngle * FearWormEnemy::kOrbitLateralSwayFrequency) * FearWormEnemy::kOrbitLateralSwayAmplitude;

    float depthSway = std::sin(context.orbitAngle * FearWormEnemy::kOrbitDepthSwayFrequency) * FearWormEnemy::kOrbitDepthSwayAmplitude;

    result.x += orbitX + lateralSway;
    result.y += FearWormEnemy::kOrbitHeight + orbitY;
    result.z += context.forwardOffset + depthSway;

    return result;
}

// 巻き付くように周回する目標位置を求める。
Vector3 FearWormBattleState::CalculateCoilTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 通常の軌道角を速めて、巻き付くような回転角を作る。
    float coilAngle = context.orbitAngle * FearWormEnemy::kCoilAngleRate;

    // 時間経過で半径を伸縮させる。
    float radius = FearWormEnemy::kCoilBaseRadius + std::sin(context.patternTime * FearWormEnemy::kCoilRadiusFrequency) * FearWormEnemy::kCoilRadiusAmplitude;

    result.x += std::cos(coilAngle) * radius;
    result.y += FearWormEnemy::kCoilHeight + std::sin(coilAngle) * radius * FearWormEnemy::kCoilVerticalScale;
    result.z += context.forwardOffset + std::sin(coilAngle * FearWormEnemy::kCoilDepthFrequency) * FearWormEnemy::kCoilDepthAmplitude;

    return result;
}

// 左右や上下に揺れながら進む目標位置を求める。
Vector3 FearWormBattleState::CalculateWeaveTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 横方向と縦方向で異なる周期を使い、波打つ軌道を作る。
    float waveAngle = context.orbitAngle * FearWormEnemy::kWeaveAngleRate;

    result.x += std::sin(waveAngle) * FearWormEnemy::kWeaveWidth;
    result.y += FearWormEnemy::kWeaveHeight + std::sin(waveAngle * FearWormEnemy::kWeaveVerticalFrequency) * FearWormEnemy::kWeaveVerticalAmplitude;
    result.z += context.forwardOffset + std::cos(waveAngle) * FearWormEnemy::kWeaveDepthAmplitude;

    return result;
}

// ゆっくり漂う移動パターンの目標位置を求める。
Vector3 FearWormBattleState::CalculateDriftTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 各軸を異なる周期で揺らし、緩やかに漂う軌道を作る。
    result.x += std::sin(context.orbitAngle * FearWormEnemy::kDriftHorizontalFrequency) * FearWormEnemy::kDriftHorizontalAmplitude;
    result.y += FearWormEnemy::kDriftHeight + std::sin(context.orbitAngle * FearWormEnemy::kDriftVerticalFrequency) * FearWormEnemy::kDriftVerticalAmplitude;
    result.z += context.forwardOffset + std::cos(context.orbitAngle * FearWormEnemy::kDriftDepthFrequency) * FearWormEnemy::kDriftDepthAmplitude;

    return result;
}

// 渦を描く移動パターンの目標位置を求める。
Vector3 FearWormBattleState::CalculateSpiralTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;
    float baseDistZ = context.forwardOffset + 50.0f;

    // 最初の2秒間は画面中央に静止して威嚇
    if (context.madModeTime < 2.0f) {
        result.x += 0.0f;
        result.y += 7.0f;
        result.z += baseDistZ;
    } else {
        // 後半の5秒間で、中心から外側へ渦を描くように広がる (半径 0.0f 〜 25.0f)
        float t = (context.madModeTime - 2.0f) / 5.0f;
        float spiralRadius = t * 25.0f;

        result.x += std::cos(context.spiralAngle) * spiralRadius;
        result.y += 7.0f + std::sin(context.spiralAngle) * spiralRadius;
        result.z += baseDistZ;
    }

    // 地面にめり込まないように下限（Y = 1.5f）をかける
    if (result.y < 1.5f) {
        result.y = 1.5f;
    }

    return result;
}

// 直線移動パターンの目標位置を求める。
Vector3 FearWormBattleState::CalculateLineTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;
    result.x += 20.0f;
    result.y += 3.0f;
    result.z += context.forwardOffset + 75.0f;
    return result;
}

// 移動の種類に対応する軌道計算を選ぶ。
Vector3 FearWormBattleState::CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context)
{
    switch (type) {
    case FearWormMovementType::Coil: return CalculateCoilTarget(context);
    case FearWormMovementType::Weave: return CalculateWeaveTarget(context);
    case FearWormMovementType::Drift: return CalculateDriftTarget(context);
    case FearWormMovementType::Line: return CalculateLineTarget(context);
    case FearWormMovementType::Spiral: return CalculateSpiralTarget(context);
    default: return CalculateOrbitTarget(context);
    }
}

// 次に使用する移動パターンを選ぶ。
FearWormBattleState::FearWormMovementType FearWormBattleState::NextFearWormMovement(FearWormMovementType type)
{
    switch (type) {
    case FearWormMovementType::Orbit: return FearWormMovementType::Coil;
    case FearWormMovementType::Coil: return FearWormMovementType::Weave;
    case FearWormMovementType::Weave: return FearWormMovementType::Drift;
    case FearWormMovementType::Drift: return FearWormMovementType::Orbit;
    default: return type;
    }
}

// 戦闘用の軌道と経過時間をもとに、現在の追従先を計算する。
Vector3 FearWormBattleState::BattleTarget(FearWormEnemy& boss, const Vector3& playerPosition, float rate)
{
    UpdateMovementPattern(boss, rate);

    Vector3 adjustedPlayerPosition = playerPosition;

    // プレイヤーのカメラのピッチ角（上下の角度）を取得し、基準とするY座標を補正する
    if (boss.player_ != nullptr && boss.player_->GetCamera() != nullptr) {
        Camera* camera = boss.player_->GetCamera();

        // カメラのピッチ角を取得（ラジアン）
        // 上を向くと pitch < 0 になるため、符号を反転します
        float pitch = -camera->GetRotate().x;

        // プレイヤーとボスの奥行き（Z方向）の距離
        float distanceZ = std::abs(playerPosition.z - boss.GetPosition().z);

        // プレイヤーから見たカメラの視線の高低差（Yオフセット）を計算
        float lookOffsetY = distanceZ * std::sin(pitch);

        // ボスの基準高度を補正
        adjustedPlayerPosition.y += lookOffsetY;
    }

    return MovementTargetPosition(boss, adjustedPlayerPosition);
}

// 現在の移動状態に応じた目標位置を取得する。
Vector3 FearWormBattleState::MovementTargetPosition(const FearWormEnemy& boss, const Vector3& playerPosition)
{
    const FearWormMovementContext context { playerPosition, boss.orbitAngle_, boss.movementPatternTimer_,
        boss.parallelForwardOffset_, boss.madModeTimer_, boss.spiralAngle_ };
    return CalculateFearWormTarget(boss.movementPattern_, context);
}

// 経過時間を確認し、移動パターンの継続と切替を行う。
void FearWormBattleState::UpdateMovementPattern(FearWormEnemy& boss, float movementSpeedRate)
{
    if (boss.movementPattern_ == MovementPattern::Line || boss.movementPattern_ == MovementPattern::Spiral) return;
    boss.movementPatternTimer_ += TimeManager::GetInstance()->GetDeltaTime() * movementSpeedRate;
    if (boss.movementPatternTimer_ >= boss.movementPatternDuration_) {
        ChangeMovementPattern(boss, NextFearWormMovement(boss.movementPattern_));
    }
}

// 移動パターンを変更し、そのパターンで使う経過時間をリセットする。
void FearWormBattleState::ChangeMovementPattern(FearWormEnemy& boss, MovementPattern pattern)
{
    boss.movementPattern_ = pattern;
    if (pattern == MovementPattern::Line) {
        boss.lineTransitionTimer_ = 0.0f;
    } else if (pattern != MovementPattern::Spiral) {
        boss.movementPatternTimer_ = 0.0f;
    }
}

// 射撃タイミングを管理し、通常弾や特殊攻撃を開始する。
void FearWormBattleState::Attack(FearWormEnemy& boss)
{
    // 登場前、攻撃対象なし、弾モデルなしの場合は攻撃しない。
    if (boss.GetBossState() != FearWormEnemy::BossState::Battle) {
        return;
    }

    if (boss.player_ == nullptr) {
        return;
    }

    if (boss.bulletModel_ == nullptr) {
        return;
    }

    // 攻撃範囲外では進行中のチャージも解除する。
    Vector3 toPlayer = boss.player_->GetTranslate() - boss.GetPosition();
    float distance = Vector3Length(toPlayer);

    if (distance > FearWormEnemy::kAttackRange) {
        if (boss.isHeadChargeActive_) {
            FinishHeadChargeAttack(boss);
        }
        return;
    }

    // 頭部の照準とチャージ攻撃を優先して更新する。
    UpdateHeadAimDirection(boss);
    UpdateHeadChargeAttack(boss, boss.MovementSpeedRate());

    if (boss.isHeadChargeActive_) {
        return;
    }

    // チャージ中でなければ通常弾の発射間隔を数える。
    boss.fireTimer_ = boss.fireTimer_ + 1;
    if (boss.fireTimer_ < boss.fireInterval_) {
        return;
    }

    boss.fireTimer_ = 0;

    // 生存部位を順番に探し、見つかった最初の部位から発射する。
    int32_t segmentCount = static_cast<int32_t>(boss.segments_.size());
    for (int32_t count = 0; count < segmentCount; count = count + 1) {
        boss.fireSegmentIndex_ = boss.fireSegmentIndex_ + 1;

        if (boss.fireSegmentIndex_ >= segmentCount) {
            boss.fireSegmentIndex_ = 0;
        }

        if (!boss.segments_[boss.fireSegmentIndex_].isAlive) {
            continue;
        }

        FireBullet(boss, boss.segments_[boss.fireSegmentIndex_].position);
        break;
    }
}

// 頭の溜め攻撃を、準備・発射・終了の順に進める。
void FearWormBattleState::UpdateHeadChargeAttack(FearWormEnemy& boss, float attackSpeedRate)
{
    float deltaTime = TimeManager::GetInstance()->GetDeltaTime() * attackSpeedRate;

    if (!boss.isHeadChargeActive_) {
        if (boss.headChargeCooldownTimer_ > 0.0f) {
            boss.headChargeCooldownTimer_ -= deltaTime;
            if (boss.headChargeCooldownTimer_ < 0.0f) {
                boss.headChargeCooldownTimer_ = 0.0f;
            }
            return;
        }

        StartHeadChargeAttack(boss);
        return;
    }

    UpdateHeadAimDirection(boss);

    if (!boss.isHeadChargeFiring_) {
        boss.headChargeTimer_ += deltaTime;
        boss.headChargeEffectTimer_ += deltaTime;

        if (boss.headChargeEffectTimer_ >= FearWormEnemy::kHeadChargeEffectInterval) {
            boss.headChargeEffectTimer_ = 0.0f;
            EffectManager::GetInstance()->PlayEffect(
                "WormShotFlash",
                HeadMuzzlePosition(boss));
        }

        if (boss.headChargeTimer_ < FearWormEnemy::kHeadChargeDuration) {
            return;
        }

        boss.isHeadChargeFiring_ = true;
        boss.headChargeShotTimer_ = FearWormEnemy::kHeadChargeShotInterval;
        return;
    }

    boss.headChargeTimer_ += deltaTime;
    boss.headChargeShotTimer_ += deltaTime;

    if (boss.headChargeShotTimer_ < FearWormEnemy::kHeadChargeShotInterval) {
        return;
    }

    boss.headChargeShotTimer_ = 0.0f;
    FireChargedBullet(boss);
    boss.headChargeShotCount_ = boss.headChargeShotCount_ + 1;

    if (boss.headChargeShotCount_ >= FearWormEnemy::kHeadChargeShotMax) {
        FinishHeadChargeAttack(boss);
    }
}

// 頭の溜め攻撃に必要なタイマーと照準状態を準備する。
void FearWormBattleState::StartHeadChargeAttack(FearWormEnemy& boss)
{
    // チャージ状態へ移行し、各タイマーと発射数を初期化する。
    boss.isHeadChargeActive_ = true;
    boss.isHeadChargeFiring_ = false;
    boss.headChargeTimer_ = 0.0f;
    boss.headChargeShotTimer_ = 0.0f;
    boss.headChargeEffectTimer_ = 0.0f;
    boss.headChargeShotCount_ = 0;
    boss.fireTimer_ = 0;

    // 開始時点の照準を確定し、発射口にチャージ演出を出す。
    UpdateHeadAimDirection(boss);

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        HeadMuzzlePosition(boss));
}

// 溜め攻撃の状態を解除し、次の攻撃へ戻れるようにする。
void FearWormBattleState::FinishHeadChargeAttack(FearWormEnemy& boss)
{
    // チャージ状態を解除し、次回攻撃までの待機時間を設定する。
    boss.isHeadChargeActive_ = false;
    boss.isHeadChargeFiring_ = false;
    boss.headChargeTimer_ = 0.0f;
    boss.headChargeShotTimer_ = 0.0f;
    boss.headChargeEffectTimer_ = 0.0f;
    boss.headChargeShotCount_ = 0;
    boss.headChargeCooldownTimer_ = FearWormEnemy::kHeadChargeCooldown;
}

// プレイヤーの位置へ頭の狙いを合わせる。
void FearWormBattleState::UpdateHeadAimDirection(FearWormEnemy& boss)
{
    if (boss.player_ == nullptr) {
        return;
    }

    if (boss.segments_.empty()) {
        return;
    }

    Vector3 targetDirection = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);

    boss.headAimDirection_ = NormalizeSafe(
        Lerp(
            boss.headAimDirection_,
            targetDirection,
            FearWormEnemy::kHeadAimFollowRate));
}

// 指定した部位の位置から、通常射撃用の弾を生成する。
void FearWormBattleState::FireBullet(FearWormEnemy& boss, const Vector3& position)
{
    // 通常弾を生成し、発射位置からプレイヤーへの速度を設定する。
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->SetFogEnabled(false);
    bullet->Initialize(boss.bulletModel_);

    Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - position);
    Vector3 velocity { };
    velocity.x = direction.x * boss.bulletSpeed_;
    velocity.y = direction.y * boss.bulletSpeed_;
    velocity.z = direction.z * boss.bulletSpeed_;

    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    // 管理リストへ追加して発射エフェクトを再生する。
    boss.AddEnemyBullet(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        position);
}

// 溜め終わった頭から強化された弾を発射する。
void FearWormBattleState::FireChargedBullet(FearWormEnemy& boss)
{
    if (boss.player_ == nullptr) {
        return;
    }

    if (boss.bulletModel_ == nullptr) {
        return;
    }

    // 頭部の発射位置からプレイヤーへ向かう弾を生成する。
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->SetFogEnabled(false);
    bullet->Initialize(boss.bulletModel_);

    Vector3 position = HeadMuzzlePosition(boss);

    Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - position);

    Vector3 velocity { };
    velocity.x = direction.x * FearWormEnemy::kChargedBulletSpeed;
    velocity.y = direction.y * FearWormEnemy::kChargedBulletSpeed;
    velocity.z = direction.z * FearWormEnemy::kChargedBulletSpeed;

    // 通常弾より強い見た目、威力、当たり判定、寿命を設定する。
    bullet->SetScale({ FearWormEnemy::kChargedBulletScale,
        FearWormEnemy::kChargedBulletScale,
        FearWormEnemy::kChargedBulletScale });
    bullet->SetColor(FearWormEnemy::kChargedBulletColor);
    bullet->SetDamage(FearWormEnemy::kChargedBulletDamage);
    bullet->SetCollisionRadius(FearWormEnemy::kChargedBulletCollisionRadius);
    bullet->SetMaxLifeTime(FearWormEnemy::kChargedBulletLifeTime);
    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    // 管理リストへ追加して発射エフェクトを再生する。
    boss.AddEnemyBullet(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        position);
}

// 頭の向きと大きさを考慮して、弾を出す位置を求める。
Vector3 FearWormBattleState::HeadMuzzlePosition(const FearWormEnemy& boss)
{
    // 頭部中心から照準方向へ発射口分だけ前進させる。
    Vector3 position = boss.GetPosition();

    Vector3 direction = NormalizeSafe(boss.headAimDirection_);

    position.x += direction.x * FearWormEnemy::kHeadMuzzleOffset;
    position.y += direction.y * FearWormEnemy::kHeadMuzzleOffset;
    position.z += direction.z * FearWormEnemy::kHeadMuzzleOffset;

    return position;
}

// 頭が照準方向を向くように描画用の回転角を計算する。
Vector3 FearWormBattleState::HeadLookRotation(const FearWormEnemy& boss)
{
    // 正規化した照準方向をピッチ角とヨー角へ変換する。
    Vector3 direction = NormalizeSafe(boss.headAimDirection_);

    Vector3 rotate { };
    rotate.x = -std::asin(direction.y);
    rotate.y = std::atan2(direction.x, direction.z);
    rotate.z = 0.0f;

    return rotate;
}

// 指定した部位から追尾ミサイルを一発生成する。
void FearWormBattleState::FireSingleMissile(FearWormEnemy& boss, size_t segmentIndex)
{
    if (boss.player_ == nullptr || boss.bulletModel_ == nullptr) {
        return;
    }

    std::unique_ptr<TrackingEnemyBullet> bullet = std::make_unique<TrackingEnemyBullet>();
    bullet->Initialize(boss.bulletModel_);
    bullet->SetPlayer(boss.player_);

    // 発射座標を設定
    bullet->SetTranslate(boss.segments_[segmentIndex].position);

    // 発射エフェクトの再生
    EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.segments_[segmentIndex].position);

    // ボスの弾リストに追加
    boss.AddEnemyBullet(std::move(bullet));
}

// 任意軸まわりの回転行列を生成する関数 (ロドリゲスの回転公式)
Matrix4x4 FearWormBattleState::MakeRotateAxisAngleMatrix(const Vector3& axis, float angle)
{
    Vector3 u = NormalizeSafe(axis);
    float c = std::cos(angle);
    float s = std::sin(angle);
    float t = 1.0f - c;

    Matrix4x4 mat {};
    mat.m[0][0] = t * u.x * u.x + c;
    mat.m[0][1] = t * u.x * u.y + s * u.z;
    mat.m[0][2] = t * u.x * u.z - s * u.y;
    mat.m[0][3] = 0.0f;

    mat.m[1][0] = t * u.x * u.y - s * u.z;
    mat.m[1][1] = t * u.y * u.y + c;
    mat.m[1][2] = t * u.y * u.z + s * u.x;
    mat.m[1][3] = 0.0f;

    mat.m[2][0] = t * u.x * u.z + s * u.y;
    mat.m[2][1] = t * u.y * u.z - s * u.x;
    mat.m[2][2] = t * u.z * u.z + c;
    mat.m[2][3] = 0.0f;

    mat.m[3][0] = 0.0f;
    mat.m[3][1] = 0.0f;
    mat.m[3][2] = 0.0f;
    mat.m[3][3] = 1.0f;

    return mat;
}

// 発射角度を回転させながら弾を出し、渦状の弾幕を作る。
void FearWormBattleState::UpdateSpiralBarrage(FearWormEnemy& boss)
{
    // 最初の2秒間（威嚇中）は弾を撃たずにプレイヤーに猶予を与える
    if (boss.madModeTimer_ < 2.0f) {
        return;
    }

    boss.barrageFireTimer_++;
    if (boss.barrageFireTimer_ < 3) {
        return;
    }
    boss.barrageFireTimer_ = 0;

    Vector3 headPos = boss.GetPosition();
    Vector3 playerPos = boss.player_->GetTranslate();

    // ボスからプレイヤーへの方向ベクトルを基準にする（プレイヤー狙い補正）
    Vector3 toPlayer = NormalizeSafe(playerPos - headPos);

    // toPlayer に直交する直交座標系 (right, up) を構築
    Vector3 tempUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(toPlayer.y) > 0.99f) {
        tempUp = { 1.0f, 0.0f, 0.0f };
    }
    Vector3 right = NormalizeSafe(Cross(tempUp, toPlayer));
    Vector3 up = NormalizeSafe(Cross(toPlayer, right));

    float spread = (std::sin(boss.barrageAngle_ * 2.5f) * 0.22f) + 0.22f; // 0.0(正確に狙う)〜0.44(周囲に拡散)を変動させる

    // プレイヤー方向を軸とした直交平面上で対角線上に広がる2つの螺旋オフセットベクトル
    Vector3 offset1 = right * (std::cos(boss.barrageAngle_) * spread) + up * (std::sin(boss.barrageAngle_) * spread);
    Vector3 offset2 = right * (std::cos(boss.barrageAngle_ + FearWormEnemy::kPi) * spread) + up * (std::sin(boss.barrageAngle_ + FearWormEnemy::kPi) * spread);

    // プレイヤー方向ベクトルと螺旋オフセットを合成して最終的な発射方向を決定
    Vector3 dir1 = NormalizeSafe(toPlayer + offset1);
    Vector3 dir2 = NormalizeSafe(toPlayer + offset2);

    FireDirectionalBullet(boss, headPos, dir1);
    FireDirectionalBullet(boss, headPos, dir2);
}

// 指定された位置と方向を使って弾を生成する。
void FearWormBattleState::FireDirectionalBullet(FearWormEnemy& boss, const Vector3& position, const Vector3& direction)
{
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->SetFogEnabled(false);
    bullet->Initialize(boss.bulletModel_);

    Vector3 velocity { };
    velocity.x = direction.x * boss.bulletSpeed_;
    velocity.y = direction.y * boss.bulletSpeed_;
    velocity.z = direction.z * boss.bulletSpeed_;

    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    boss.AddEnemyBullet(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect("WormShotFlash", position);
}

// ビーム本体と溜め演出を描画するリソースを準備する。
void FearWormBattleState::InitializeBeam(FearWormEnemy& boss)
{
    // ビーム用クロスPlaneモデルのロード
    Model* beamModel = ModelManager::GetInstance()->CreateBeamCross("resources/Textures/beam.jpg");

    // ビームオブジェクトの初期化
    boss.beamPlane_ = std::make_unique<Object3d>();
    boss.beamPlane_->Initialize(Object3dManager::GetInstance());
    boss.beamPlane_->SetModel(beamModel);
    boss.beamPlane_->SetEnableLighting(false);

    // 初期状態の設定
    boss.beamState_ = BeamState::Wait;
    boss.beamTimer_ = 0.0f;
    boss.beamUVScrollOffset_ = 0.0f;
    boss.beamCurrentLength_ = 0.0f;
    boss.beamCurrentWidth_ = 0.0f;
    boss.beamRotateTheta_ = 0.0f;
}

// 頭の位置と狙いにビームを追従させ、ビーム状態を更新する。
void FearWormBattleState::UpdateBeamAttack(FearWormEnemy& boss)
{
    if (boss.player_ == nullptr) {
        return;
    }

    float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    boss.beamTimer_ += deltaTime;

    // UVスクロールオフセットを更新
    boss.beamUVScrollOffset_ += FearWormEnemy::kBeamUvScrollSpeed * deltaTime;
    if (boss.beamUVScrollOffset_ < -1.0f) {
        boss.beamUVScrollOffset_ += 1.0f;
    }

    // UVスクロール行列を作成してマテリアルにセット
    Vector3 uvTranslation = { 0.0f, boss.beamUVScrollOffset_, 0.0f }; // V方向にスクロール
    Matrix4x4 uvMat = MatrixMath::MakeTranslateMatrix(uvTranslation);
    if (boss.beamPlane_->GetMaterial()) {
        boss.beamPlane_->SetUvTransform(uvMat);
    }

    // ビームの自転角度を更新
    boss.beamRotateTheta_ += FearWormEnemy::kBeamRotateSpeedRadiansPerSecond * deltaTime;
    if (boss.beamRotateTheta_ > FearWormEnemy::kPi * 2.0f) {
        boss.beamRotateTheta_ -= FearWormEnemy::kPi * 2.0f;
    }

    // プレイヤーの座標
    Vector3 headPos = HeadMuzzlePosition(boss);

    // 照準を更新する前のマズル位置を、予兆・発射エフェクトで共有する。
    boss.beamMuzzlePosition_ = headPos;
    UpdateBeamState(boss, deltaTime);

    // 射向ベクトル (Lerpでゆっくり追従しているheadAimDirection_をそのままビームの進行方向軸とする)
    Vector3 zAxis = boss.headAimDirection_;
    float length = boss.beamCurrentLength_;
    if (length < 0.001f) length = 0.001f;

    // 正規直交基底の作成 (仮の上方向と射向ベクトルの外積から右方向を算出)
    Vector3 tempUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(zAxis.y) > 0.999f) {
        tempUp = { 0.0f, 0.0f, 1.0f }; // 真上・真下を向いている場合のフォールバック
    }

    Vector3 xAxis = NormalizeSafe(Cross(tempUp, zAxis));
    Vector3 yAxis = Cross(zAxis, xAxis); // すでに直交かつ正規化されている

    // 進行方向(zAxis)まわりの自転適用 (xAxis と yAxis を theta で回転)
    float cosT = std::cos(boss.beamRotateTheta_);
    float sinT = std::sin(boss.beamRotateTheta_);

    Vector3 xAxisRot = {
        xAxis.x * cosT + yAxis.x * sinT,
        xAxis.y * cosT + yAxis.y * sinT,
        xAxis.z * cosT + yAxis.z * sinT
    };

    Vector3 yAxisRot = {
        -xAxis.x * sinT + yAxis.x * cosT,
        -xAxis.y * sinT + yAxis.y * cosT,
        -xAxis.z * sinT + yAxis.z * cosT
    };

    // 回転行列の直接構築 (基底ベクトルを直接代入)
    Matrix4x4 matRot = MatrixMath::MakeIdentity4x4();
    matRot.m[0][0] = xAxisRot.x;  matRot.m[0][1] = xAxisRot.y;  matRot.m[0][2] = xAxisRot.z;
    matRot.m[1][0] = yAxisRot.x;  matRot.m[1][1] = yAxisRot.y;  matRot.m[1][2] = yAxisRot.z;
    matRot.m[2][0] = zAxis.x;     matRot.m[2][1] = zAxis.y;     matRot.m[2][2] = zAxis.z;

    // スケールと平行移動の合成 (Scale -> Rotate -> Translate)
    Matrix4x4 matScale = MatrixMath::Matrix4x4MakeScaleMatrix({ boss.beamCurrentWidth_, boss.beamCurrentWidth_, length });
    Matrix4x4 matTrans = MatrixMath::MakeTranslateMatrix(headPos);

    Matrix4x4 worldMat = MatrixMath::Multiply(MatrixMath::Multiply(matScale, matRot), matTrans);

    // ワールド行列を直接設定
    boss.beamPlane_->SetCustomWorldMatrix(worldMat);
    boss.beamPlane_->Update();
}

// 現在のビーム状態に必要な溜め演出や光線を描画する。
void FearWormBattleState::DrawBeam(FearWormEnemy& boss)
{
    if (boss.GetBossState() == FearWormEnemy::BossState::Wait || boss.segments_.empty()) {
        return;
    }

    if (!boss.segments_[0].isAlive) {
        return;
    }

    if (boss.beamState_ == BeamState::Charge || boss.beamState_ == BeamState::Fire || boss.beamState_ == BeamState::FadeOut) {
        if (boss.beamPlane_) {
            // 加算ブレンドを適用して、黒背景を透過させ発光感を出す
            Object3dManager::GetInstance()->SetBlendMode(kBlendModeAdd);
            boss.beamPlane_->Draw();
            // 通常ブレンドに戻す
            Object3dManager::GetInstance()->SetBlendMode(kBlendModeNormal);
        }
    }
}

// 発射中の光線とプレイヤーの距離を調べ、命中時のダメージを処理する。
void FearWormBattleState::CheckBeamCollision(FearWormEnemy& boss)
{
    boss.isBeamHittingPlayer_ = false; // 毎フレーム初期化

    if (boss.player_ == nullptr || boss.beamCurrentLength_ <= 0.0f || boss.beamCurrentWidth_ <= 0.0f) {
        return;
    }

    // ビームの始点と終点
    Vector3 beamStart = HeadMuzzlePosition(boss);
    Vector3 beamEnd = beamStart + boss.headAimDirection_ * boss.beamCurrentLength_;

    // プレイヤーの座標と半径
    Vector3 playerPos = boss.player_->GetTranslate();
    float playerRadius = 1.0f;
    float beamRadius = boss.beamCurrentWidth_ * 0.5f;

    // 線分 AB (ビーム) と点 C (プレイヤー) の最短距離を求める
    Vector3 AB = beamEnd - beamStart;
    Vector3 AC = playerPos - beamStart;

    float abLenSq = Vector3LengthSquared(AB);
    float t = 0.0f;
    if (abLenSq > 0.0f) {
        t = Dot(AC, AB) / abLenSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    // 最短点 P
    Vector3 P = beamStart + AB * t;

    // 最短距離の2乗
    Vector3 diff = playerPos - P;
    float distanceSq = Vector3LengthSquared(diff);

    // 衝突半径の合計の2乗
    float hitRadius = beamRadius + playerRadius;
    float hitRadiusSq = hitRadius * hitRadius;

    if (distanceSq <= hitRadiusSq) {
        boss.isBeamHittingPlayer_ = true; // プレイヤーへの接触状態をON

        boss.player_->ApplyDamage(FearWormEnemy::kBeamDamage); // HP減少は無敵時間が適用される

        // 無敵時間に関わらず、接触している間は被弾火花エフェクトを3フレームに1回に間引いて発生させる (FPS低下防止)
        boss.beamHitEffectFrameCount_++;
        if (boss.beamHitEffectFrameCount_ >= 3) {
            boss.beamHitEffectFrameCount_ = 0;
            EffectManager::GetInstance()->PlayEffect("DamageHit", playerPos);
        }
    } else {
        boss.beamHitEffectFrameCount_ = 0;
    }
}

// 待機・溜め・発射・消失のうち、現在のビーム状態を進める。
void FearWormBattleState::UpdateBeamState(FearWormEnemy& boss, float deltaTime)
{
    switch (boss.beamState_) {
    case BeamState::Wait: UpdateBeamWait(boss, deltaTime); break;
    case BeamState::Charge: UpdateBeamCharge(boss, deltaTime); break;
    case BeamState::Fire: UpdateBeamFire(boss, deltaTime); break;
    case BeamState::FadeOut: UpdateBeamFadeOut(boss, deltaTime); break;
    }
}

// 次のビームを開始できる条件が整うまで待つ。
void FearWormBattleState::UpdateBeamWait(FearWormEnemy& boss, float)
{
    boss.beamCurrentLength_ = 0.0f;
    boss.beamCurrentWidth_ = 0.0f;
    boss.Attack();
    if (boss.beamTimer_ >= 4.0f) ChangeBeamState(boss, BeamState::Charge);
}

// 溜め時間と見た目を更新し、準備が終わったら発射へ移る。
void FearWormBattleState::UpdateBeamCharge(FearWormEnemy& boss, float dt)
{
    boss.beamCurrentLength_ = 0.0f;
    boss.beamCurrentWidth_ = 0.0f;
    const Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
    boss.headAimDirection_ = NormalizeSafe(Lerp(boss.headAimDirection_, direction, 0.08f));
    boss.beamEffectTimer_ += dt;
    if (boss.beamEffectTimer_ >= 0.25f) {
        boss.beamEffectTimer_ = 0.0f;
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.beamMuzzlePosition_);
    }
    if (boss.beamTimer_ >= 1.5f) ChangeBeamState(boss, BeamState::Fire);
}

// 発射時間を進め、時間切れでビームを消す状態へ移る。
void FearWormBattleState::UpdateBeamFire(FearWormEnemy& boss, float dt)
{
    boss.beamCurrentLength_ = (std::min)(400.0f, boss.beamCurrentLength_ + 1000.0f * dt);
    boss.beamCurrentWidth_ = 3.5f;
    const Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
    boss.headAimDirection_ = NormalizeSafe(Lerp(boss.headAimDirection_, direction, 0.015f));
    CheckBeamCollision(boss);
    boss.beamEffectTimer_ += dt;
    if (boss.beamEffectTimer_ >= 0.2f) {
        boss.beamEffectTimer_ = 0.0f;
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.beamMuzzlePosition_);
    }
    if (boss.beamTimer_ >= 3.0f) ChangeBeamState(boss, BeamState::FadeOut);
}

// 光線を徐々に消し、次の発射までの待機状態へ戻す。
void FearWormBattleState::UpdateBeamFadeOut(FearWormEnemy& boss, float dt)
{
    boss.beamCurrentWidth_ = (std::max)(0.0f, boss.beamCurrentWidth_ - 8.0f * dt);
    const Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
    boss.headAimDirection_ = NormalizeSafe(Lerp(boss.headAimDirection_, direction, 0.01f));
    if (boss.beamTimer_ >= 0.5f) ChangeBeamState(boss, BeamState::Wait);
}

// ビーム状態を切り替え、各状態のタイマーや表示を設定する。
void FearWormBattleState::ChangeBeamState(FearWormEnemy& boss, BeamState state)
{
    if (boss.beamState_ == state) return;
    boss.beamState_ = state;
    boss.beamTimer_ = 0.0f;
    switch (state) {
    case BeamState::Charge: {
        if (boss.isHeadChargeActive_) FinishHeadChargeAttack(boss);
        boss.beamEffectTimer_ = 0.0f;
        if (boss.player_ && !boss.segments_.empty()) {
            boss.headAimDirection_ = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.beamMuzzlePosition_);
        }
        break;
    }
    case BeamState::Fire: {
        boss.beamEffectTimer_ = 0.0f;
        if (boss.player_ && !boss.segments_.empty()) {
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.beamMuzzlePosition_);
        }
        break;
    }
    default: break;
    }
}
