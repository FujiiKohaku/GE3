#include "FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Math/MathStruct.h"

namespace {
constexpr float kInitialOrbitAngle = 2.35f;
constexpr float kMissileAimWaitDuration = 2.0f;   // 整列完了（前準備）を待つ時間（秒）
constexpr float kMissileAttackInterval = 18.0f;  // ミサイル攻撃の間隔（秒）
constexpr float kMissileFireInterval = 0.45f;    // ミサイルの発射間隔（秒）
constexpr float kOrbitAngularSpeed = 1.15f;
}

void FearWormEnemy::UpdateWait()
{
    float movementSpeedRate = MovementSpeedRate();
    Vector3 target;

    if (player_ == nullptr) {
        target = FallbackTarget();
    } else {
        Vector3 playerPosition = player_->GetTranslate();

        // 起動条件チェック
        if (playerPosition.z >= startPosition_.z - activationLeadDistance_) {
            ChangeState(BossState::Entry); // 登場状態へ移行
            return;
        }

        target = EntryStartPosition(playerPosition);
    }

    // 移動処理
    segments_[0].position = Lerp(segments_[0].position, target, moveSpeed_ * movementSpeedRate);
    transform_.translate = segments_[0].position;
}

void FearWormEnemy::EnterEntry()
{
    if (player_ == nullptr) { return; }
    const Vector3 playerPosition = player_->GetTranslate();
    // 登場用タイマーと移動パターンを初期状態へ戻す。
    enterTimer_ = 0.0f;
    orbitAngle_ = kInitialOrbitAngle;
    ChangeMovementPattern(MovementPattern::Orbit);
    movementPatternTimer_ = 0.0f;

    // 頭部を登場開始位置へ移す。
    Vector3 entryPosition = EntryStartPosition(playerPosition); // playerの左上に登場

    segments_[0].position = entryPosition;
    transform_.translate = entryPosition;
}

void FearWormEnemy::UpdateEntry()
{
    if (player_ == nullptr) {
        ChangeState(BossState::Wait);
        return;
    }

    float movementSpeedRate = MovementSpeedRate();
    Vector3 playerPosition = player_->GetTranslate();

    // 回転角度の更新
    orbitAngle_ += TimeManager::GetInstance()->GetDeltaTime() * kOrbitAngularSpeed * movementSpeedRate;

    // 登場目的地を計算
    Vector3 target = EntryTarget(playerPosition);

    // 移動処理
    segments_[0].position = Lerp(segments_[0].position, target, moveSpeed_ * movementSpeedRate);
    transform_.translate = segments_[0].position;

    // 登場時間が終わったら戦闘状態へ移行
    if (enterTimer_ >= enterDuration_) {
        ChangeState(BossState::Battle);
    }
}

void FearWormEnemy::UpdateBattle()
{
    if (player_ == nullptr) {
        return;
    }

    float movementSpeedRate = MovementSpeedRate();
    Vector3 playerPosition = player_->GetTranslate();

    UpdateMadMode(movementSpeedRate);
    UpdateMissileAttack();
    UpdateBattleMovement(playerPosition, movementSpeedRate);
    UpdateBattleAttack();
}

void FearWormEnemy::UpdateMadMode(float movementSpeedRate)
{
    // HP30%以下かつ未発動かつ未完了のときに発狂トリガー
    bool shouldTriggerMad = (HealthRate() <= 0.30f) && !isMadModeActive_ && !isMadModeFinished_;

    if (shouldTriggerMad) {
        isMadModeActive_ = true;
        madModeTimer_ = 0.0f;
        ChangeMovementPattern(MovementPattern::Spiral);
        spiralAngle_ = 0.0f;
        barrageAngle_ = 0.0f;

        if (isHeadChargeActive_) {
            FinishHeadChargeAttack();
        }
        isMissileAttackActive_ = false;
        isFiringMissilesSequence_ = false;
    }

    // 発狂モードタイマー更新 (7秒制限: 2秒威嚇 + 5秒大暴れ)
    if (isMadModeActive_) {
        madModeTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        if (madModeTimer_ >= 7.0f) {
            isMadModeActive_ = false;
            isMadModeFinished_ = true; // 終了フラグを立てて、再度発狂しないようにする
            ChangeMovementPattern(MovementPattern::Orbit); // 通常パターンに復帰
            movementPatternTimer_ = 0.0f;
            missileAttackTimer_ = 0.0f;
        }
    }

    // 回転角度の更新
    orbitAngle_ += TimeManager::GetInstance()->GetDeltaTime() * kOrbitAngularSpeed * movementSpeedRate;

    if (isMadModeActive_) {
        spiralAngle_ += TimeManager::GetInstance()->GetDeltaTime() * 2.5f * movementSpeedRate;
        barrageAngle_ += TimeManager::GetInstance()->GetDeltaTime() * 4.0f;
    }
}

void FearWormEnemy::UpdateMissileAttack()
{
    // ミサイル攻撃の進行管理（発狂モード中は行わない）
    if (!isMadModeActive_) {
        if (!isMissileAttackActive_ && !isFiringMissilesSequence_) {
            missileAttackTimer_ += TimeManager::GetInstance()->GetDeltaTime();
            if (missileAttackTimer_ >= kMissileAttackInterval) {
                isMissileAttackActive_ = true;
                hasFiredMissiles_ = false;
                missilePhaseTimer_ = 0.0f;
                ChangeMovementPattern(MovementPattern::Line);
                lineTransitionTimer_ = 0.0f;
                // 進行中のチャージ攻撃があればキャンセルする
                if (isHeadChargeActive_) {
                    FinishHeadChargeAttack();
                }
            }
        } else if (isMissileAttackActive_) {
            // Line状態（前準備・チャージ警告演出）
            missilePhaseTimer_ += TimeManager::GetInstance()->GetDeltaTime();
            if (missilePhaseTimer_ >= kMissileAimWaitDuration) {
                // Line準備完了！発射シーケンスを開始する（Line状態・移動パターンは維持）
                isMissileAttackActive_ = false;

                // 生存しているセグメントのインデックスをリストアップ
                missileTargetIndices_.clear();
                std::vector<size_t> aliveIndices;
                for (size_t index = 0; index < segments_.size(); ++index) {
                    if (segments_[index].isAlive) {
                        aliveIndices.push_back(index);
                    }
                }

                if (!aliveIndices.empty()) {
                    isFiringMissilesSequence_ = true;
                    missileFireTimer_ = kMissileFireInterval; // 最初は即座に撃ち出すようにタイマーを満たす
                    missileShotCount_ = 0;

                    // 生存している全部位から、頭側から順番にミサイルを発射する。
                    missileTargetIndices_ = aliveIndices;
                } else {
                    // 生存セグメントがない（通常はあり得ないが安全のため）
                    ChangeMovementPattern(MovementPattern::Orbit);
                    movementPatternTimer_ = 0.0f;
                    missileAttackTimer_ = 0.0f;
                }
            }
        }

        // 時間差ミサイル発射シーケンスの実行（Line状態のまま実行される）
        if (isFiringMissilesSequence_ && player_ != nullptr && bulletModel_ != nullptr) {
            missileFireTimer_ += TimeManager::GetInstance()->GetDeltaTime();
            int32_t maxShots = static_cast<int32_t>(missileTargetIndices_.size());

            if (missileShotCount_ < maxShots) {
                // 発射フェーズ
                if (missileFireTimer_ >= kMissileFireInterval) {
                    missileFireTimer_ = 0.0f;
                    size_t segmentIndex = missileTargetIndices_[missileShotCount_];
                    if (segmentIndex < segments_.size() && segments_[segmentIndex].isAlive) {
                        FireSingleMissile(segmentIndex);
                    }
                    missileShotCount_++;
                }
            } else {
                // 撃ち終わった後の硬直フェーズ（隙を見せる）
                constexpr float kMissilePostFireCooldown = 1.8f; // 撃ち終わった後の隙（秒）
                if (missileFireTimer_ >= kMissilePostFireCooldown) {
                    // 硬直終了！通常状態に戻り、Orbitへ復帰
                    isFiringMissilesSequence_ = false;
                    missileAttackTimer_ = 0.0f;
                    ChangeMovementPattern(MovementPattern::Orbit);
                    movementPatternTimer_ = 0.0f;
                }
            }
        }
    }
}

void FearWormEnemy::UpdateBattleMovement(const Vector3& playerPosition, float movementSpeedRate)
{
    // 戦闘目的地を計算
    Vector3 target = BattleTarget(playerPosition, movementSpeedRate);

    // 移動処理
    // 頭だけかつビーム発射フェーズ（Charge, Fire, FadeOut）のときは、X・Y軸の移動のみ静止し、Z軸の前進は維持する
    bool isBeamActive = !HasAliveBodyParts() && (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut);
    if (isBeamActive) {
        // Z座標のみ追従前進させる (floatのLerp)
        float t = moveSpeed_ * movementSpeedRate;
        segments_[0].position.z = segments_[0].position.z + (target.z - segments_[0].position.z) * t;
    } else {
        segments_[0].position = Lerp(segments_[0].position, target, moveSpeed_ * movementSpeedRate);
    }
    transform_.translate = segments_[0].position;
}

void FearWormEnemy::UpdateBattleAttack()
{
    // 攻撃処理の実行
    if (!HasAliveBodyParts()) {
        // 頭部だけになったらビーム攻撃に移行
        if (isHeadChargeActive_) {
            FinishHeadChargeAttack();
        }
        isMissileAttackActive_ = false;
        isFiringMissilesSequence_ = false;

        UpdateBeamAttack();
    } else if (isMadModeActive_) {
        UpdateSpiralBarrage();
    } else {
        if (!isMissileAttackActive_ && !isFiringMissilesSequence_) {
            Attack();
        }
    }
}

void FearWormEnemy::EnterDead()
{
    // 攻撃状態をクリア（発射済みの弾はManager側で飛び続ける）
    isHeadChargeActive_ = false;
    isHeadChargeFiring_ = false;
    ChangeBeamState(BeamState::Wait);
    beamCurrentLength_ = 0.0f;
    beamCurrentWidth_ = 0.0f;
    isDeathSequenceFinished_ = false;
    deathSequenceTimer_ = 0.0f;

    // 1. 胴体セグメントは即座にすべて非表示にする
    for (size_t index = 1; index < segments_.size(); ++index) {
        segments_[index].isAlive = false;
    }

    // 2. 頭部の位置で大爆発エフェクトを即座に再生
    EffectManager::GetInstance()->PlayEffect("WormDeathExplosion", segments_[0].position);

    // 3. 落下物理パラメータの初期化
    // 前方（Zマイナス方向＝手前）へ飛び出しながら、上方向にポンと跳ね上がる初速を設定
    deathVelocity_ = { 0.0f, 11.5f, -9.0f };
    // 落下中にクルクルと回転させる回転速度を設定
    deathRotation_ = { 1.8f, 2.5f, 0.8f };
    currentDeathRotation_ = segments_[0].object->GetRotate();
}

void FearWormEnemy::UpdateDead()
{
    if (isDeathSequenceFinished_) {
        return;
    }

    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    deathSequenceTimer_ += deltaTime;

    // 1. 重力を適用して落下速度を下方向（Yマイナス）に引っ張る
    constexpr float kGravity = 26.0f; // ゲーム的な落下速度に合わせた重力加速度
    deathVelocity_.y -= kGravity * deltaTime;

    // 2. 速度を頭部座標に適用
    segments_[0].position.x += deathVelocity_.x * deltaTime;
    segments_[0].position.y += deathVelocity_.y * deltaTime;
    segments_[0].position.z += deathVelocity_.z * deltaTime;

    // 3. 回転速度を頭部回転角に適用
    currentDeathRotation_.x += deathRotation_.x * deltaTime;
    currentDeathRotation_.y += deathRotation_.y * deltaTime;
    currentDeathRotation_.z += deathRotation_.z * deltaTime;
    segments_[0].object->SetRotate(currentDeathRotation_);

    // 4. 頭部の座標だけを更新するため、Object3dの更新を呼ぶ
    segments_[0].object->SetTranslate(segments_[0].position);
    segments_[0].object->Update();

    // 5. 落下時間（1.6秒）が経過するか、十分に下へ落ちたら演出を完了する
    constexpr float kDeathFallbackDuration = 1.6f;
    if (deathSequenceTimer_ >= kDeathFallbackDuration || segments_[0].position.y < -35.0f) {
        isDeathSequenceFinished_ = true;
    }
}
