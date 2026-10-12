#include "FearWormState.h"
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

class FearWormWaitState final : public FearWormState {
public:
    BossState GetType() const override { return BossState::Wait; }

    void Update(FearWormEnemy& boss) const override
    {
        float movementSpeedRate = boss.MovementSpeedRate();
        Vector3 target;

        if (boss.player_ == nullptr) {
            target = boss.FallbackTarget();
        } else {
            Vector3 playerPosition = boss.player_->GetTranslate();

            // 起動条件チェック
            if (playerPosition.z >= boss.startPosition_.z - boss.activationLeadDistance_) {
                boss.ChangeState(BossState::Entry); // 登場状態へ移行
                return;
            }

            target = boss.EntryStartPosition(playerPosition);
        }

        // 移動処理
        boss.segments_[0].position = Lerp(boss.segments_[0].position, target, boss.moveSpeed_ * movementSpeedRate);
        boss.transform_.translate = boss.segments_[0].position;
    }
};

class FearWormEntryState final : public FearWormState {
public:
    BossState GetType() const override { return BossState::Entry; }
    void Enter(FearWormEnemy& boss) const override
    {
        if (boss.player_ == nullptr) { return; }
        const Vector3 playerPosition = boss.player_->GetTranslate();
        // 登場用タイマーと移動パターンを初期状態へ戻す。
        boss.enterTimer_ = 0.0f;
        boss.orbitAngle_ = kInitialOrbitAngle;
        boss.movementPattern_ = FearWormEnemy::MovementPattern::Orbit;
        boss.movementPatternTimer_ = 0.0f;

        // 頭部を登場開始位置へ移す。
        Vector3 entryPosition = boss.EntryStartPosition(playerPosition); // playerの左上に登場

        boss.segments_[0].position = entryPosition;
        boss.transform_.translate = entryPosition;
    }
    void Update(FearWormEnemy& boss) const override
    {
        if (boss.player_ == nullptr) {
            boss.ChangeState(BossState::Wait);
            return;
        }

        float movementSpeedRate = boss.MovementSpeedRate();
        Vector3 playerPosition = boss.player_->GetTranslate();

        // 回転角度の更新
        boss.orbitAngle_ += TimeManager::GetInstance()->GetDeltaTime() * kOrbitAngularSpeed * movementSpeedRate;

        // 登場目的地を計算
        Vector3 target = boss.EntryTarget(playerPosition);

        // 移動処理
        boss.segments_[0].position = Lerp(boss.segments_[0].position, target, boss.moveSpeed_ * movementSpeedRate);
        boss.transform_.translate = boss.segments_[0].position;

        // 登場時間が終わったら戦闘状態へ移行
        if (boss.enterTimer_ >= boss.enterDuration_) {
            boss.ChangeState(BossState::Battle);
        }
    }
};

class FearWormBattleState final : public FearWormState {
public:
    BossState GetType() const override { return BossState::Battle; }
    void Update(FearWormEnemy& boss) const override
    {
        if (boss.player_ == nullptr) {
            return;
        }

        float movementSpeedRate = boss.MovementSpeedRate();
        Vector3 playerPosition = boss.player_->GetTranslate();

        // HP30%以下かつ未発動かつ未完了のときに発狂トリガー
        bool shouldTriggerMad = (boss.HealthRate() <= 0.30f) && !boss.isMadModeActive_ && !boss.isMadModeFinished_;

        if (shouldTriggerMad) {
            boss.isMadModeActive_ = true;
            boss.madModeTimer_ = 0.0f;
            boss.movementPattern_ = FearWormEnemy::MovementPattern::Spiral;
            boss.spiralAngle_ = 0.0f;
            boss.barrageAngle_ = 0.0f;

            if (boss.isHeadChargeActive_) {
                boss.FinishHeadChargeAttack();
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
                boss.movementPattern_ = FearWormEnemy::MovementPattern::Orbit; // 通常パターンに復帰
                boss.movementPatternTimer_ = 0.0f;
                boss.missileAttackTimer_ = 0.0f;
            }
        }

        bool isMadMode = boss.isMadModeActive_;

        // 回転角度の更新
        boss.orbitAngle_ += TimeManager::GetInstance()->GetDeltaTime() * kOrbitAngularSpeed * movementSpeedRate;

        if (isMadMode) {
            boss.spiralAngle_ += TimeManager::GetInstance()->GetDeltaTime() * 2.5f * movementSpeedRate;
            boss.barrageAngle_ += TimeManager::GetInstance()->GetDeltaTime() * 4.0f;
        }

        // ミサイル攻撃の進行管理（発狂モード中は行わない）
        if (!isMadMode) {
            if (!boss.isMissileAttackActive_ && !boss.isFiringMissilesSequence_) {
                boss.missileAttackTimer_ += TimeManager::GetInstance()->GetDeltaTime();
                if (boss.missileAttackTimer_ >= kMissileAttackInterval) {
                    boss.isMissileAttackActive_ = true;
                    boss.hasFiredMissiles_ = false;
                    boss.missilePhaseTimer_ = 0.0f;
                    boss.movementPattern_ = FearWormEnemy::MovementPattern::Line;
                    boss.lineTransitionTimer_ = 0.0f;
                    // 進行中のチャージ攻撃があればキャンセルする
                    if (boss.isHeadChargeActive_) {
                        boss.FinishHeadChargeAttack();
                    }
                }
            } else if (boss.isMissileAttackActive_) {
                // Line状態（前準備・チャージ警告演出）
                boss.missilePhaseTimer_ += TimeManager::GetInstance()->GetDeltaTime();
                if (boss.missilePhaseTimer_ >= kMissileAimWaitDuration) {
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
                        boss.missileFireTimer_ = kMissileFireInterval; // 最初は即座に撃ち出すようにタイマーを満たす
                        boss.missileShotCount_ = 0;

                        // 生存している全部位から、頭側から順番にミサイルを発射する。
                        boss.missileTargetIndices_ = aliveIndices;
                    } else {
                        // 生存セグメントがない（通常はあり得ないが安全のため）
                        boss.movementPattern_ = FearWormEnemy::MovementPattern::Orbit;
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
                    if (boss.missileFireTimer_ >= kMissileFireInterval) {
                        boss.missileFireTimer_ = 0.0f;
                        size_t segmentIndex = boss.missileTargetIndices_[boss.missileShotCount_];
                        if (segmentIndex < boss.segments_.size() && boss.segments_[segmentIndex].isAlive) {
                            boss.FireSingleMissile(segmentIndex);
                        }
                        boss.missileShotCount_++;
                    }
                } else {
                    // 撃ち終わった後の硬直フェーズ（隙を見せる）
                    constexpr float kMissilePostFireCooldown = 1.8f; // 撃ち終わった後の隙（秒）
                    if (boss.missileFireTimer_ >= kMissilePostFireCooldown) {
                        // 硬直終了！通常状態に戻り、Orbitへ復帰
                        boss.isFiringMissilesSequence_ = false;
                        boss.missileAttackTimer_ = 0.0f;
                        boss.movementPattern_ = FearWormEnemy::MovementPattern::Orbit;
                        boss.movementPatternTimer_ = 0.0f;
                    }
                }
            }
        }

        // 戦闘目的地を計算
        Vector3 target = boss.BattleTarget(playerPosition, movementSpeedRate);

        // 移動処理
        // 頭だけかつビーム発射フェーズ（Charge, Fire, FadeOut）のときは、X・Y軸の移動のみ静止し、Z軸の前進は維持する
        bool isBeamActive = !boss.HasAliveBodyParts() && (boss.beamState_ == FearWormEnemy::BeamState::Charge || boss.beamState_ == FearWormEnemy::BeamState::Fire || boss.beamState_ == FearWormEnemy::BeamState::FadeOut);
        if (isBeamActive) {
            // Z座標のみ追従前進させる (floatのLerp)
            float t = boss.moveSpeed_ * movementSpeedRate;
            boss.segments_[0].position.z = boss.segments_[0].position.z + (target.z - boss.segments_[0].position.z) * t;
        } else {
            boss.segments_[0].position = Lerp(boss.segments_[0].position, target, boss.moveSpeed_ * movementSpeedRate);
        }
        boss.transform_.translate = boss.segments_[0].position;

        // 攻撃処理の実行
        if (!boss.HasAliveBodyParts()) {
            // 頭部だけになったらビーム攻撃に移行
            if (boss.isHeadChargeActive_) {
                boss.FinishHeadChargeAttack();
            }
            boss.isMissileAttackActive_ = false;
            boss.isFiringMissilesSequence_ = false;

            boss.UpdateBeamAttack();
        } else if (isMadMode) {
            boss.UpdateSpiralBarrage();
        } else {
            if (!boss.isMissileAttackActive_ && !boss.isFiringMissilesSequence_) {
                boss.Attack();
            }
        }
    }
};

class FearWormDeadState final : public FearWormState {
public:
    BossState GetType() const override { return BossState::Dead; }
    void Enter(FearWormEnemy& boss) const override
    {
        // 攻撃状態をクリア（発射済みの弾はManager側で飛び続ける）
        boss.isHeadChargeActive_ = false;
        boss.isHeadChargeFiring_ = false;
        boss.beamState_ = FearWormEnemy::BeamState::Wait;
        boss.beamCurrentLength_ = 0.0f;
        boss.beamCurrentWidth_ = 0.0f;
        boss.isDeathSequenceFinished_ = false;
        boss.deathSequenceTimer_ = 0.0f;

        // 1. 胴体セグメントは即座にすべて非表示にする
        for (size_t index = 1; index < boss.segments_.size(); ++index) {
            boss.segments_[index].isAlive = false;
        }

        // 2. 頭部の位置で大爆発エフェクトを即座に再生
        EffectManager::GetInstance()->PlayEffect("WormDeathExplosion", boss.segments_[0].position);

        // 3. 落下物理パラメータの初期化
        // 前方（Zマイナス方向＝手前）へ飛び出しながら、上方向にポンと跳ね上がる初速を設定
        boss.deathVelocity_ = { 0.0f, 11.5f, -9.0f };
        // 落下中にクルクルと回転させる回転速度を設定
        boss.deathRotation_ = { 1.8f, 2.5f, 0.8f };
        boss.currentDeathRotation_ = boss.segments_[0].object->GetRotate();
    }
    void Update(FearWormEnemy& boss) const override
    {
        if (boss.isDeathSequenceFinished_) {
            return;
        }

        const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
        boss.deathSequenceTimer_ += deltaTime;

        // 1. 重力を適用して落下速度を下方向（Yマイナス）に引っ張る
        constexpr float kGravity = 26.0f; // ゲーム的な落下速度に合わせた重力加速度
        boss.deathVelocity_.y -= kGravity * deltaTime;

        // 2. 速度を頭部座標に適用
        boss.segments_[0].position.x += boss.deathVelocity_.x * deltaTime;
        boss.segments_[0].position.y += boss.deathVelocity_.y * deltaTime;
        boss.segments_[0].position.z += boss.deathVelocity_.z * deltaTime;

        // 3. 回転速度を頭部回転角に適用
        boss.currentDeathRotation_.x += boss.deathRotation_.x * deltaTime;
        boss.currentDeathRotation_.y += boss.deathRotation_.y * deltaTime;
        boss.currentDeathRotation_.z += boss.deathRotation_.z * deltaTime;
        boss.segments_[0].object->SetRotate(boss.currentDeathRotation_);

        // 4. 頭部の座標だけを更新するため、Object3dの更新を呼ぶ
        boss.segments_[0].object->SetTranslate(boss.segments_[0].position);
        boss.segments_[0].object->Update();

        // 5. 落下時間（1.6秒）が経過するか、十分に下へ落ちたら演出を完了する
        constexpr float kDeathFallbackDuration = 1.6f;
        if (boss.deathSequenceTimer_ >= kDeathFallbackDuration || boss.segments_[0].position.y < -35.0f) {
            boss.isDeathSequenceFinished_ = true;
        }
    }
};

FearWormEnemy::FearWormEnemy() : currentState_(&GetState(BossState::Wait)) {}

FearWormEnemy::BossState FearWormEnemy::GetBossState() const
{
    return currentState_->GetType();
}


const FearWormState& FearWormEnemy::GetState(BossState state)
{
    // 状態には可変データがないため共有でき、遷移時のメモリ確保も不要。
    static const FearWormWaitState wait;
    static const FearWormEntryState entry;
    static const FearWormBattleState battle;
    static const FearWormDeadState dead;
    switch (state) {
    case BossState::Wait: return wait;
    case BossState::Entry: return entry;
    case BossState::Battle: return battle;
    case BossState::Dead: return dead;
    }
    return wait;
}

void FearWormEnemy::ChangeState(BossState state)
{
    const FearWormState& next = GetState(state);
    if (currentState_ == &next) { return; }
    currentState_->Exit(*this);
    currentState_ = &next;
    currentState_->Enter(*this);
}

