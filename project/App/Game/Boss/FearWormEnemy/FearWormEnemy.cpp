// 頭と胴体が連なるボス。水面からの登場、移動、各種攻撃、部位破壊と死亡演出を扱う。
#include "FearWormEnemy.h"
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

// コンストラクタ。初期状態を待機状態に設定する。
FearWormEnemy::FearWormEnemy()
{
    ChangeState(BossState::Wait);
}

FearWormEnemy::BossState FearWormEnemy::GetBossState() const { return state_; }

// 現在のボス状態に対応する処理を呼び、登場・戦闘・死亡の流れを進める。
void FearWormEnemy::UpdateState()
{
    currentState_->Update(*this);
}

// 終了処理を実行してから状態を切り替え、切替先の開始処理を実行する。
void FearWormEnemy::ChangeState(BossState state)
{
    if (currentState_ != nullptr && state_ == state) {
        return;
    }

    FearWormState* nextState = nullptr;
    switch (state) {
    case BossState::Wait:
        nextState = &waitState_;
        break;
    case BossState::Entry:
        nextState = &entryState_;
        break;
    case BossState::Battle:
        nextState = &battleState_;
        break;
    case BossState::Dead:
        nextState = &deadState_;
        break;
    default:
        return;
    }

    if (currentState_ != nullptr) {
        currentState_->Exit(*this);
    }
    state_ = state;
    currentState_ = nextState;
    currentState_->Enter(*this);
}

// 敵としての共通状態、連結部位とビームなどの攻撃用リソースを準備する。
void FearWormEnemy::Initialize(Model* model, Model* bulletModel, Player* player)
{
    bulletModel_ = bulletModel;
    player_ = player;
    SetHealth(kHeadHp);
    moveSpeed_ = kBaseMoveSpeed;

    isMadModeActive_ = false;
    isMadModeFinished_ = false;
    madModeTimer_ = 0.0f;

    InitializeSegments(model);
    InitializeBeam();
    entryState_.Initialize();
    SetPosition(transform_.translate);
}

// ボスの配置を更新し、頭や連結部位の基準位置にも反映する。
void FearWormEnemy::SetPosition(const Vector3& position)
{
    // 基準位置と行動状態を初期状態へ戻す。
    transform_.translate = position;
    startPosition_ = position;
    ChangeState(BossState::Wait);
    entryState_.Reset();
    ChangeMovementPattern(MovementPattern::Orbit);
    movementPatternTimer_ = 0.0f;
    isHeadChargeActive_ = false;
    isHeadChargeFiring_ = false;
    headChargeTimer_ = 0.0f;
    headChargeShotTimer_ = 0.0f;
    headChargeCooldownTimer_ = kInitialHeadChargeCooldown;
    headChargeEffectTimer_ = 0.0f;
    headChargeShotCount_ = 0;
    headAimDirection_ = kDefaultHeadAimDirection;

    // 追尾ミサイル関連の初期化
    missileAttackTimer_ = 0.0f;
    isMissileAttackActive_ = false;
    missilePhaseTimer_ = 0.0f;
    hasFiredMissiles_ = false;
    isFiringMissilesSequence_ = false;
    missileFireTimer_ = 0.0f;
    missileShotCount_ = 0;
    missileTargetIndices_.clear();
    // 全セグメントを基準位置から後方へ等間隔に並べる。
    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        float offset = segmentSpacing_ * static_cast<float>(index);
        segments_[index].position = position;
        segments_[index].position.z -= offset;
    }
}

Vector3 FearWormEnemy::GetPosition() const
{
    if (segments_.empty()) {
        return transform_.translate;
    }

    return segments_[0].position;
}

// ボスの状態を進め、連結部位と描画オブジェクトを現在の位置へ追従させる。
void FearWormEnemy::Update()
{
    // 外部から撃破された場合も、死亡状態への移行は一度だけ行う。
    if (IsDead()) {
        ChangeState(BossState::Dead);
    }

    moveTime_ += TimeManager::GetInstance()->GetDeltaTime();
    UpdateState();

    // 各セグメントの移動・回転・拡縮・カラー等の更新
    // ※ Wait中も画面外で追従描画するために常に実行
    UpdateSegments();
    UpdateSegmentObjects();

    // 全胴体破壊時の頭部弱点化チェック
    if (GetBossState() == BossState::Battle && !vulnerableEffectPlayed_ && !HasAliveBodyParts()) {
        vulnerableEffectPlayed_ = true;
        PlayHeadVulnerableEffect(GetPosition());
    }
}

bool FearWormEnemy::IsDeathSequenceFinished() const
{
    return isDeathSequenceFinished_;
}

// 撃破後の演出へ切り替え、通常の戦闘処理を終える。
void FearWormEnemy::OnDeath()
{
    ChangeState(BossState::Dead);
}

// 激昂中の攻撃と移動を更新し、通常時とは異なる圧力をかける。
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

// 部位からミサイルを撃つ順番と発射タイミングを管理する。
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

// プレイヤーを基準に移動先を決め、速度倍率を反映して頭を動かす。
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

// 戦闘中の攻撃タイマーを進め、選択された攻撃の処理へつなげる。
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

// プレイヤーの周囲を回り込む軌道上の目標位置を求める。
Vector3 FearWormEnemy::CalculateOrbitTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 基本の楕円軌道を計算する。
    float orbitX = std::cos(context.orbitAngle) * kOrbitRadiusX;

    float orbitY = std::sin(context.orbitAngle) * kOrbitRadiusY;

    // 単調な周回にならないよう、横揺れと奥行きの揺れを加える。
    float lateralSway = std::sin(context.orbitAngle * kOrbitLateralSwayFrequency) * kOrbitLateralSwayAmplitude;

    float depthSway = std::sin(context.orbitAngle * kOrbitDepthSwayFrequency) * kOrbitDepthSwayAmplitude;

    result.x += orbitX + lateralSway;
    result.y += kOrbitHeight + orbitY;
    result.z += context.forwardOffset + depthSway;

    return result;
}

// 巻き付くように周回する目標位置を求める。
Vector3 FearWormEnemy::CalculateCoilTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 通常の軌道角を速めて、巻き付くような回転角を作る。
    float coilAngle = context.orbitAngle * kCoilAngleRate;

    // 時間経過で半径を伸縮させる。
    float radius = kCoilBaseRadius + std::sin(context.patternTime * kCoilRadiusFrequency) * kCoilRadiusAmplitude;

    result.x += std::cos(coilAngle) * radius;
    result.y += kCoilHeight + std::sin(coilAngle) * radius * kCoilVerticalScale;
    result.z += context.forwardOffset + std::sin(coilAngle * kCoilDepthFrequency) * kCoilDepthAmplitude;

    return result;
}

// 左右や上下に揺れながら進む目標位置を求める。
Vector3 FearWormEnemy::CalculateWeaveTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 横方向と縦方向で異なる周期を使い、波打つ軌道を作る。
    float waveAngle = context.orbitAngle * kWeaveAngleRate;

    result.x += std::sin(waveAngle) * kWeaveWidth;
    result.y += kWeaveHeight + std::sin(waveAngle * kWeaveVerticalFrequency) * kWeaveVerticalAmplitude;
    result.z += context.forwardOffset + std::cos(waveAngle) * kWeaveDepthAmplitude;

    return result;
}

// ゆっくり漂う移動パターンの目標位置を求める。
Vector3 FearWormEnemy::CalculateDriftTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 各軸を異なる周期で揺らし、緩やかに漂う軌道を作る。
    result.x += std::sin(context.orbitAngle * kDriftHorizontalFrequency) * kDriftHorizontalAmplitude;
    result.y += kDriftHeight + std::sin(context.orbitAngle * kDriftVerticalFrequency) * kDriftVerticalAmplitude;
    result.z += context.forwardOffset + std::cos(context.orbitAngle * kDriftDepthFrequency) * kDriftDepthAmplitude;

    return result;
}

// 渦を描く移動パターンの目標位置を求める。
Vector3 FearWormEnemy::CalculateSpiralTarget(const FearWormMovementContext& context)
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
Vector3 FearWormEnemy::CalculateLineTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;
    result.x += 20.0f;
    result.y += 3.0f;
    result.z += context.forwardOffset + 75.0f;
    return result;
}

// 移動の種類に対応する軌道計算を選ぶ。
Vector3 FearWormEnemy::CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context)
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
FearWormEnemy::FearWormMovementType FearWormEnemy::NextFearWormMovement(FearWormMovementType type)
{
    switch (type) {
    case FearWormMovementType::Orbit: return FearWormMovementType::Coil;
    case FearWormMovementType::Coil: return FearWormMovementType::Weave;
    case FearWormMovementType::Weave: return FearWormMovementType::Drift;
    case FearWormMovementType::Drift: return FearWormMovementType::Orbit;
    default: return type;
    }
}

// 通常の目標を使えない場合に備え、代わりの移動先を返す。
Vector3 FearWormEnemy::FallbackTarget()
{
    Vector3 target = startPosition_;
    target.x += std::sin(moveTime_ * kFallbackMoveXFrequency) * kFallbackMoveXAmplitude;
    target.y += std::sin(moveTime_ * kFallbackMoveYFrequency) * kFallbackMoveYAmplitude;
    target.z += std::sin(moveTime_ * kFallbackMoveZFrequency) * kFallbackMoveZAmplitude;
    return target;
}

// 戦闘用の軌道と経過時間をもとに、現在の追従先を計算する。
Vector3 FearWormEnemy::BattleTarget(const Vector3& playerPosition, float rate)
{
    UpdateMovementPattern(rate);

    Vector3 adjustedPlayerPosition = playerPosition;

    // プレイヤーのカメラのピッチ角（上下の角度）を取得し、基準とするY座標を補正する
    if (player_ != nullptr && player_->GetCamera() != nullptr) {
        Camera* camera = player_->GetCamera();

        // カメラのピッチ角を取得（ラジアン）
        // 上を向くと pitch < 0 になるため、符号を反転します
        float pitch = -camera->GetRotate().x;

        // プレイヤーとボスの奥行き（Z方向）の距離
        float distanceZ = std::abs(playerPosition.z - GetPosition().z);

        // プレイヤーから見たカメラの視線の高低差（Yオフセット）を計算
        float lookOffsetY = distanceZ * std::sin(pitch);

        // ボスの基準高度を補正
        adjustedPlayerPosition.y += lookOffsetY;
    }

    return MovementTargetPosition(adjustedPlayerPosition);
}

// 登場演出を始める位置をプレイヤー基準で求める。
Vector3 FearWormEnemy::EntryStartPosition(const Vector3& playerPosition) const
{
    // プレイヤーの左上かつ前方を登場開始位置にする。
    Vector3 result = playerPosition;
    result.x += kEntryOffsetX;
    result.y += kEntryOffsetY;
    result.z += parallelForwardOffset_ + kEntryExtraForwardOffset;
    return result;
}

// 現在の移動状態に応じた目標位置を取得する。
Vector3 FearWormEnemy::MovementTargetPosition(const Vector3& playerPosition) const
{
    const FearWormMovementContext context { playerPosition, orbitAngle_, movementPatternTimer_,
        parallelForwardOffset_, madModeTimer_, spiralAngle_ };
    return CalculateFearWormTarget(movementPattern_, context);
}

// 経過時間を確認し、移動パターンの継続と切替を行う。
void FearWormEnemy::UpdateMovementPattern(float movementSpeedRate)
{
    if (movementPattern_ == MovementPattern::Line || movementPattern_ == MovementPattern::Spiral) return;
    movementPatternTimer_ += TimeManager::GetInstance()->GetDeltaTime() * movementSpeedRate;
    if (movementPatternTimer_ >= movementPatternDuration_) {
        ChangeMovementPattern(NextFearWormMovement(movementPattern_));
    }
}

// 移動パターンを変更し、そのパターンで使う経過時間をリセットする。
void FearWormEnemy::ChangeMovementPattern(MovementPattern pattern)
{
    movementPattern_ = pattern;
    if (pattern == MovementPattern::Line) {
        lineTransitionTimer_ = 0.0f;
    } else if (pattern != MovementPattern::Spiral) {
        movementPatternTimer_ = 0.0f;
    }
}

// 頭から後ろへ順に追従位置を計算し、胴体がつながってうねる形を作る。
void FearWormEnemy::UpdateSegments()
{
    if (state_ == BossState::Entry) { return; }
    if (segments_.empty()) {
        return;
    }

    float blendT = 0.0f;
    if (movementPattern_ == MovementPattern::Line) {
        lineTransitionTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        blendT = (std::min)(lineTransitionTimer_, 1.0f);
    } else {
        lineTransitionTimer_ = 0.0f;
    }
    Segment* previousSegment = &segments_[0];
    int32_t aliveOrder = 0;

    for (size_t index = 1; index < segments_.size(); index = index + 1) {
        Segment& segment = segments_[index];

        if (!segment.isAlive) {
            continue;
        }
        aliveOrder++;

        // 通常の追従目標位置 (縦並び - 元の引き戻し拘束)
        Vector3 diff = segment.position - previousSegment->position;
        Vector3 direction = Normalize(diff);
        Vector3 posNormal = previousSegment->position + direction * segmentSpacing_;

        // Line状態の目標位置 (横並び＋うねうね)
        float waveScaleY = 3.2f;
        float waveScaleZ = 2.0f;
        float waveFrequency = 4.2f;

        Vector3 posLine = {
            segments_[0].position.x - (segmentSpacing_ * static_cast<float>(aliveOrder)),
            segments_[0].position.y + std::sin(moveTime_ * waveFrequency + static_cast<float>(aliveOrder) * 0.8f) * waveScaleY,
            segments_[0].position.z + std::cos(moveTime_ * (waveFrequency * 0.8f) + static_cast<float>(aliveOrder) * 0.6f) * waveScaleZ
        };

        // 通常位置とLine位置をブレンドして直接適用する
        // blendT = 0.0f のときは posNormal (従来と100%同一の挙動) になります
        segment.position = Lerp(posNormal, posLine, blendT);

        previousSegment = &segment;
    }
}

// 状態や残りHPに応じた移動速度の倍率を返す。
float FearWormEnemy::MovementSpeedRate() const
{
    // HPが減るほど移動と攻撃の速度倍率を上げる。
    float healthRate = HealthRate();

    float damageRate = 1.0f - healthRate;

    return 1.0f + damageRate * kMaximumMovementSpeedAdd;
}

// 射撃タイミングを管理し、通常弾や特殊攻撃を開始する。
void FearWormEnemy::Attack()
{
    // 登場前、攻撃対象なし、弾モデルなしの場合は攻撃しない。
    if (GetBossState() != BossState::Battle) {
        return;
    }

    if (player_ == nullptr) {
        return;
    }

    if (bulletModel_ == nullptr) {
        return;
    }

    // 攻撃範囲外では進行中のチャージも解除する。
    Vector3 toPlayer = player_->GetTranslate() - GetPosition();
    float distance = Vector3Length(toPlayer);

    if (distance > kAttackRange) {
        if (isHeadChargeActive_) {
            FinishHeadChargeAttack();
        }
        return;
    }

    // 頭部の照準とチャージ攻撃を優先して更新する。
    UpdateHeadAimDirection();
    UpdateHeadChargeAttack(MovementSpeedRate());

    if (isHeadChargeActive_) {
        return;
    }

    // チャージ中でなければ通常弾の発射間隔を数える。
    fireTimer_ = fireTimer_ + 1;
    if (fireTimer_ < fireInterval_) {
        return;
    }

    fireTimer_ = 0;

    // 生存部位を順番に探し、見つかった最初の部位から発射する。
    int32_t segmentCount = static_cast<int32_t>(segments_.size());
    for (int32_t count = 0; count < segmentCount; count = count + 1) {
        fireSegmentIndex_ = fireSegmentIndex_ + 1;

        if (fireSegmentIndex_ >= segmentCount) {
            fireSegmentIndex_ = 0;
        }

        if (!segments_[fireSegmentIndex_].isAlive) {
            continue;
        }

        FireBullet(segments_[fireSegmentIndex_].position);
        break;
    }
}

// 頭の溜め攻撃を、準備・発射・終了の順に進める。
void FearWormEnemy::UpdateHeadChargeAttack(float attackSpeedRate)
{
    float deltaTime = TimeManager::GetInstance()->GetDeltaTime() * attackSpeedRate;

    if (!isHeadChargeActive_) {
        if (headChargeCooldownTimer_ > 0.0f) {
            headChargeCooldownTimer_ -= deltaTime;
            if (headChargeCooldownTimer_ < 0.0f) {
                headChargeCooldownTimer_ = 0.0f;
            }
            return;
        }

        StartHeadChargeAttack();
        return;
    }

    UpdateHeadAimDirection();

    if (!isHeadChargeFiring_) {
        headChargeTimer_ += deltaTime;
        headChargeEffectTimer_ += deltaTime;

        if (headChargeEffectTimer_ >= kHeadChargeEffectInterval) {
            headChargeEffectTimer_ = 0.0f;
            EffectManager::GetInstance()->PlayEffect(
                "WormShotFlash",
                HeadMuzzlePosition());
        }

        if (headChargeTimer_ < kHeadChargeDuration) {
            return;
        }

        isHeadChargeFiring_ = true;
        headChargeShotTimer_ = kHeadChargeShotInterval;
        return;
    }

    headChargeTimer_ += deltaTime;
    headChargeShotTimer_ += deltaTime;

    if (headChargeShotTimer_ < kHeadChargeShotInterval) {
        return;
    }

    headChargeShotTimer_ = 0.0f;
    FireChargedBullet();
    headChargeShotCount_ = headChargeShotCount_ + 1;

    if (headChargeShotCount_ >= kHeadChargeShotMax) {
        FinishHeadChargeAttack();
    }
}

// 頭の溜め攻撃に必要なタイマーと照準状態を準備する。
void FearWormEnemy::StartHeadChargeAttack()
{
    // チャージ状態へ移行し、各タイマーと発射数を初期化する。
    isHeadChargeActive_ = true;
    isHeadChargeFiring_ = false;
    headChargeTimer_ = 0.0f;
    headChargeShotTimer_ = 0.0f;
    headChargeEffectTimer_ = 0.0f;
    headChargeShotCount_ = 0;
    fireTimer_ = 0;

    // 開始時点の照準を確定し、発射口にチャージ演出を出す。
    UpdateHeadAimDirection();

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        HeadMuzzlePosition());
}

// 溜め攻撃の状態を解除し、次の攻撃へ戻れるようにする。
void FearWormEnemy::FinishHeadChargeAttack()
{
    // チャージ状態を解除し、次回攻撃までの待機時間を設定する。
    isHeadChargeActive_ = false;
    isHeadChargeFiring_ = false;
    headChargeTimer_ = 0.0f;
    headChargeShotTimer_ = 0.0f;
    headChargeEffectTimer_ = 0.0f;
    headChargeShotCount_ = 0;
    headChargeCooldownTimer_ = kHeadChargeCooldown;
}

// プレイヤーの位置へ頭の狙いを合わせる。
void FearWormEnemy::UpdateHeadAimDirection()
{
    if (player_ == nullptr) {
        return;
    }

    if (segments_.empty()) {
        return;
    }

    Vector3 targetDirection = NormalizeSafe(player_->GetTranslate() - segments_[0].position);

    headAimDirection_ = NormalizeSafe(
        Lerp(
            headAimDirection_,
            targetDirection,
            kHeadAimFollowRate));
}

// 指定した部位の位置から、通常射撃用の弾を生成する。
void FearWormEnemy::FireBullet(const Vector3& position)
{
    // 通常弾を生成し、発射位置からプレイヤーへの速度を設定する。
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->SetFogEnabled(false);
    bullet->Initialize(bulletModel_);

    Vector3 direction = NormalizeSafe(player_->GetTranslate() - position);
    Vector3 velocity { };
    velocity.x = direction.x * bulletSpeed_;
    velocity.y = direction.y * bulletSpeed_;
    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    // 管理リストへ追加して発射エフェクトを再生する。
    AddEnemyBullet(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        position);
}

// 溜め終わった頭から強化された弾を発射する。
void FearWormEnemy::FireChargedBullet()
{
    if (player_ == nullptr) {
        return;
    }

    if (bulletModel_ == nullptr) {
        return;
    }

    // 頭部の発射位置からプレイヤーへ向かう弾を生成する。
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->SetFogEnabled(false);
    bullet->Initialize(bulletModel_);

    Vector3 position = HeadMuzzlePosition();

    Vector3 direction = NormalizeSafe(player_->GetTranslate() - position);

    Vector3 velocity { };
    velocity.x = direction.x * kChargedBulletSpeed;
    velocity.y = direction.y * kChargedBulletSpeed;
    velocity.z = direction.z * kChargedBulletSpeed;

    // 通常弾より強い見た目、威力、当たり判定、寿命を設定する。
    bullet->SetScale({ kChargedBulletScale,
        kChargedBulletScale,
        kChargedBulletScale });
    bullet->SetColor(kChargedBulletColor);
    bullet->SetDamage(kChargedBulletDamage);
    bullet->SetCollisionRadius(kChargedBulletCollisionRadius);
    bullet->SetMaxLifeTime(kChargedBulletLifeTime);
    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    // 管理リストへ追加して発射エフェクトを再生する。
    AddEnemyBullet(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        position);
}

// 頭の向きと大きさを考慮して、弾を出す位置を求める。
Vector3 FearWormEnemy::HeadMuzzlePosition() const
{
    // 頭部中心から照準方向へ発射口分だけ前進させる。
    Vector3 position = GetPosition();

    Vector3 direction = NormalizeSafe(headAimDirection_);

    position.x += direction.x * kHeadMuzzleOffset;
    position.y += direction.y * kHeadMuzzleOffset;
    position.z += direction.z * kHeadMuzzleOffset;

    return position;
}

// 頭が照準方向を向くように描画用の回転角を計算する。
Vector3 FearWormEnemy::HeadLookRotation() const
{
    // 正規化した照準方向をピッチ角とヨー角へ変換する。
    Vector3 direction = NormalizeSafe(headAimDirection_);

    Vector3 rotate { };
    rotate.x = -std::asin(direction.y);
    rotate.y = std::atan2(direction.x, direction.z);
    rotate.z = 0.0f;

    return rotate;
}

// 指定した部位から追尾ミサイルを一発生成する。
void FearWormEnemy::FireSingleMissile(size_t segmentIndex)
{
    if (player_ == nullptr || bulletModel_ == nullptr) {
        return;
    }

    std::unique_ptr<TrackingEnemyBullet> bullet = std::make_unique<TrackingEnemyBullet>();
    bullet->Initialize(bulletModel_);
    bullet->SetPlayer(player_);

    // 発射座標を設定
    bullet->SetTranslate(segments_[segmentIndex].position);

    // 発射エフェクトの再生
    EffectManager::GetInstance()->PlayEffect("WormShotFlash", segments_[segmentIndex].position);

    // ボスの弾リストに追加
    AddEnemyBullet(std::move(bullet));
}

// 任意軸まわりの回転行列を生成する関数 (ロドリゲスの回転公式)
Matrix4x4 FearWormEnemy::MakeRotateAxisAngleMatrix(const Vector3& axis, float angle)
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
void FearWormEnemy::UpdateSpiralBarrage()
{
    // 最初の2秒間（威嚇中）は弾を撃たずにプレイヤーに猶予を与える
    if (madModeTimer_ < 2.0f) {
        return;
    }

    barrageFireTimer_++;
    if (barrageFireTimer_ < 3) {
        return;
    }
    barrageFireTimer_ = 0;

    Vector3 headPos = GetPosition();
    Vector3 playerPos = player_->GetTranslate();

    // ボスからプレイヤーへの方向ベクトルを基準にする（プレイヤー狙い補正）
    Vector3 toPlayer = NormalizeSafe(playerPos - headPos);

    // toPlayer に直交する直交座標系 (right, up) を構築
    Vector3 tempUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(toPlayer.y) > 0.99f) {
        tempUp = { 1.0f, 0.0f, 0.0f };
    }
    Vector3 right = NormalizeSafe(Cross(tempUp, toPlayer));
    Vector3 up = NormalizeSafe(Cross(toPlayer, right));

    float spread = (std::sin(barrageAngle_ * 2.5f) * 0.22f) + 0.22f; // 0.0(正確に狙う)〜0.44(周囲に拡散)を変動させる

    // プレイヤー方向を軸とした直交平面上で対角線上に広がる2つの螺旋オフセットベクトル
    Vector3 offset1 = right * (std::cos(barrageAngle_) * spread) + up * (std::sin(barrageAngle_) * spread);
    Vector3 offset2 = right * (std::cos(barrageAngle_ + kPi) * spread) + up * (std::sin(barrageAngle_ + kPi) * spread);

    // プレイヤー方向ベクトルと螺旋オフセットを合成して最終的な発射方向を決定
    Vector3 dir1 = NormalizeSafe(toPlayer + offset1);
    Vector3 dir2 = NormalizeSafe(toPlayer + offset2);

    FireDirectionalBullet(headPos, dir1);
    FireDirectionalBullet(headPos, dir2);
}

// 指定された位置と方向を使って弾を生成する。
void FearWormEnemy::FireDirectionalBullet(const Vector3& position, const Vector3& direction)
{
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->SetFogEnabled(false);
    bullet->Initialize(bulletModel_);

    Vector3 velocity { };
    velocity.x = direction.x * bulletSpeed_;
    velocity.y = direction.y * bulletSpeed_;
    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    AddEnemyBullet(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect("WormShotFlash", position);
}

// ビーム本体と溜め演出を描画するリソースを準備する。
void FearWormEnemy::InitializeBeam()
{
    // ビーム用クロスPlaneモデルのロード
    Model* beamModel = ModelManager::GetInstance()->CreateBeamCross("resources/Textures/beam.jpg");

    // ビームオブジェクトの初期化
    beamPlane_ = std::make_unique<Object3d>();
    beamPlane_->Initialize(Object3dManager::GetInstance());
    beamPlane_->SetModel(beamModel);
    beamPlane_->SetEnableLighting(false);

    // 初期状態の設定
    beamState_ = BeamState::Wait;
    beamTimer_ = 0.0f;
    beamUVScrollOffset_ = 0.0f;
    beamCurrentLength_ = 0.0f;
    beamCurrentWidth_ = 0.0f;
    beamRotateTheta_ = 0.0f;
}

// 頭の位置と狙いにビームを追従させ、ビーム状態を更新する。
void FearWormEnemy::UpdateBeamAttack()
{
    if (player_ == nullptr) {
        return;
    }

    float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    beamTimer_ += deltaTime;

    // UVスクロールオフセットを更新
    beamUVScrollOffset_ += kBeamUvScrollSpeed * deltaTime;
    if (beamUVScrollOffset_ < -1.0f) {
        beamUVScrollOffset_ += 1.0f;
    }

    // UVスクロール行列を作成してマテリアルにセット
    Vector3 uvTranslation = { 0.0f, beamUVScrollOffset_, 0.0f }; // V方向にスクロール
    Matrix4x4 uvMat = MatrixMath::MakeTranslateMatrix(uvTranslation);
    if (beamPlane_->GetMaterial()) {
        beamPlane_->SetUvTransform(uvMat);
    }

    // ビームの自転角度を更新
    beamRotateTheta_ += kBeamRotateSpeedRadiansPerSecond * deltaTime;
    if (beamRotateTheta_ > kPi * 2.0f) {
        beamRotateTheta_ -= kPi * 2.0f;
    }

    // プレイヤーの座標
    Vector3 headPos = HeadMuzzlePosition();

    // 照準を更新する前のマズル位置を、予兆・発射エフェクトで共有する。
    beamMuzzlePosition_ = headPos;
    UpdateBeamState(deltaTime);

    // 射向ベクトル (Lerpでゆっくり追従しているheadAimDirection_をそのままビームの進行方向軸とする)
    Vector3 zAxis = headAimDirection_;
    float length = beamCurrentLength_;
    if (length < 0.001f) length = 0.001f;

    // 正規直交基底の作成 (仮の上方向と射向ベクトルの外積から右方向を算出)
    Vector3 tempUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(zAxis.y) > 0.999f) {
        tempUp = { 0.0f, 0.0f, 1.0f }; // 真上・真下を向いている場合のフォールバック
    }

    Vector3 xAxis = NormalizeSafe(Cross(tempUp, zAxis));
    Vector3 yAxis = Cross(zAxis, xAxis); // すでに直交かつ正規化されている

    // 進行方向(zAxis)まわりの自転適用 (xAxis と yAxis を theta で回転)
    float cosT = std::cos(beamRotateTheta_);
    float sinT = std::sin(beamRotateTheta_);

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
    Matrix4x4 matScale = MatrixMath::Matrix4x4MakeScaleMatrix({ beamCurrentWidth_, beamCurrentWidth_, length });
    Matrix4x4 matTrans = MatrixMath::MakeTranslateMatrix(headPos);

    Matrix4x4 worldMat = MatrixMath::Multiply(MatrixMath::Multiply(matScale, matRot), matTrans);

    // ワールド行列を直接設定
    beamPlane_->SetCustomWorldMatrix(worldMat);
    beamPlane_->Update();
}

// 現在のビーム状態に必要な溜め演出や光線を描画する。
void FearWormEnemy::DrawBeam()
{
    if (GetBossState() == BossState::Wait || segments_.empty()) {
        return;
    }

    if (!segments_[0].isAlive) {
        return;
    }

    if (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut) {
        if (beamPlane_) {
            // 加算ブレンドを適用して、黒背景を透過させ発光感を出す
            Object3dManager::GetInstance()->SetBlendMode(kBlendModeAdd);
            beamPlane_->Draw();
            // 通常ブレンドに戻す
            Object3dManager::GetInstance()->SetBlendMode(kBlendModeNormal);
        }
    }
}

// 発射中の光線とプレイヤーの距離を調べ、命中時のダメージを処理する。
void FearWormEnemy::CheckBeamCollision()
{
    isBeamHittingPlayer_ = false; // 毎フレーム初期化

    if (player_ == nullptr || beamCurrentLength_ <= 0.0f || beamCurrentWidth_ <= 0.0f) {
        return;
    }

    // ビームの始点と終点
    Vector3 beamStart = HeadMuzzlePosition();
    Vector3 beamEnd = beamStart + headAimDirection_ * beamCurrentLength_;

    // プレイヤーの座標と半径
    Vector3 playerPos = player_->GetTranslate();
    float playerRadius = 1.0f;
    float beamRadius = beamCurrentWidth_ * 0.5f;

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
        isBeamHittingPlayer_ = true; // プレイヤーへの接触状態をON

        player_->ApplyDamage(kBeamDamage); // HP減少は無敵時間が適用される

        // 無敵時間に関わらず、接触している間は被弾火花エフェクトを3フレームに1回に間引いて発生させる (FPS低下防止)
        beamHitEffectFrameCount_++;
        if (beamHitEffectFrameCount_ >= 3) {
            beamHitEffectFrameCount_ = 0;
            EffectManager::GetInstance()->PlayEffect("DamageHit", playerPos);
        }
    } else {
        beamHitEffectFrameCount_ = 0;
    }
}

// 待機・溜め・発射・消失のうち、現在のビーム状態を進める。
void FearWormEnemy::UpdateBeamState(float deltaTime)
{
    switch (beamState_) {
    case BeamState::Wait: UpdateBeamWait(deltaTime); break;
    case BeamState::Charge: UpdateBeamCharge(deltaTime); break;
    case BeamState::Fire: UpdateBeamFire(deltaTime); break;
    case BeamState::FadeOut: UpdateBeamFadeOut(deltaTime); break;
    }
}

// 次のビームを開始できる条件が整うまで待つ。
void FearWormEnemy::UpdateBeamWait(float)
{
    beamCurrentLength_ = 0.0f;
    beamCurrentWidth_ = 0.0f;
    Attack();
    if (beamTimer_ >= 4.0f) ChangeBeamState(BeamState::Charge);
}

// 溜め時間と見た目を更新し、準備が終わったら発射へ移る。
void FearWormEnemy::UpdateBeamCharge(float dt)
{
    beamCurrentLength_ = 0.0f;
    beamCurrentWidth_ = 0.0f;
    const Vector3 direction = NormalizeSafe(player_->GetTranslate() - segments_[0].position);
    headAimDirection_ = NormalizeSafe(Lerp(headAimDirection_, direction, 0.08f));
    beamEffectTimer_ += dt;
    if (beamEffectTimer_ >= 0.25f) {
        beamEffectTimer_ = 0.0f;
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", beamMuzzlePosition_);
    }
    if (beamTimer_ >= 1.5f) ChangeBeamState(BeamState::Fire);
}

// 発射時間を進め、時間切れでビームを消す状態へ移る。
void FearWormEnemy::UpdateBeamFire(float dt)
{
    beamCurrentLength_ = (std::min)(400.0f, beamCurrentLength_ + 1000.0f * dt);
    beamCurrentWidth_ = 3.5f;
    const Vector3 direction = NormalizeSafe(player_->GetTranslate() - segments_[0].position);
    headAimDirection_ = NormalizeSafe(Lerp(headAimDirection_, direction, 0.015f));
    CheckBeamCollision();
    beamEffectTimer_ += dt;
    if (beamEffectTimer_ >= 0.2f) {
        beamEffectTimer_ = 0.0f;
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", beamMuzzlePosition_);
    }
    if (beamTimer_ >= 3.0f) ChangeBeamState(BeamState::FadeOut);
}

// 光線を徐々に消し、次の発射までの待機状態へ戻す。
void FearWormEnemy::UpdateBeamFadeOut(float dt)
{
    beamCurrentWidth_ = (std::max)(0.0f, beamCurrentWidth_ - 8.0f * dt);
    const Vector3 direction = NormalizeSafe(player_->GetTranslate() - segments_[0].position);
    headAimDirection_ = NormalizeSafe(Lerp(headAimDirection_, direction, 0.01f));
    if (beamTimer_ >= 0.5f) ChangeBeamState(BeamState::Wait);
}

// ビーム状態を切り替え、各状態のタイマーや表示を設定する。
void FearWormEnemy::ChangeBeamState(BeamState state)
{
    if (beamState_ == state) return;
    beamState_ = state;
    beamTimer_ = 0.0f;
    switch (state) {
    case BeamState::Charge: {
        if (isHeadChargeActive_) FinishHeadChargeAttack();
        beamEffectTimer_ = 0.0f;
        if (player_ && !segments_.empty()) {
            headAimDirection_ = NormalizeSafe(player_->GetTranslate() - segments_[0].position);
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", beamMuzzlePosition_);
        }
        break;
    }
    case BeamState::Fire: {
        beamEffectTimer_ = 0.0f;
        if (player_ && !segments_.empty()) {
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", beamMuzzlePosition_);
        }
        break;
    }
    default: break;
    }
}

// 頭・胴体・尾の部位データと描画オブジェクトを生成する。
void FearWormEnemy::InitializeSegments(Model* model)
{
    // 体のセグメントを初期化
    segments_.clear();
    segments_.reserve(kWormSegmentCount);

    // 体作成
    for (int32_t index = 0; index < kWormSegmentCount; index = index + 1) {
        Segment segment { };
        segment.object = std::make_unique<Object3d>();
        segment.object->Initialize(Object3dManager::GetInstance());
        segment.object->SetMaterial("resources/Shaders/Object3D/ShadowStandard");
        segment.object->SetCastShadow(true);
        segment.object->SetReceiveShadow(true);
        segment.object->SetModel(model);
        segment.object->SetEnableLighting(false);
        segment.isHead = false;
        segment.isAlive = true;
        segment.hp = kBodyHp;

        float scale = kBodyScale;
        if (index == 0) { // 頭のセグメント
            segment.isHead = true;
            segment.hp = kHeadHp;
            scale = kHeadScale;
        }

        if (index == kWormSegmentCount - 1) { // 尾のセグメント
            scale = kTailScale;
        }

        segment.scale = { scale, scale, scale };
        segment.radius = scale * kCollisionRadiusScale;

        segments_.push_back(std::move(segment));
    }
}

// 計算済みの部位位置と向きを、描画用のオブジェクトへ反映する。
void FearWormEnemy::UpdateSegmentObjects()
{
    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        Segment& segment = segments_[index];

        if (segment.object == nullptr) {
            continue;
        }

        if (segment.hitFlashTimer > 0.0f) {
            segment.hitFlashTimer -= TimeManager::GetInstance()->GetDeltaTime();
            if (segment.hitFlashTimer < 0.0f) {
                segment.hitFlashTimer = 0.0f;
            }
        }

        Vector4 color { };
        if (segment.isHead) {
            color = kHeadColor;

            if (!HasAliveBodyParts()) {
                color = kVulnerableHeadColor;
            }

            if (isHeadChargeActive_) {
                color = kChargingHeadColor;
            }

            if (isHeadChargeFiring_) {
                color = kFiringHeadColor;
            }
        } else {
            color = kBodyColor;
        }

        if (segment.hitFlashTimer > 0.0f) {
            color = kHitFlashColor;
        }

        float pulse = 1.0f + std::sin(moveTime_ * kBodyPulseFrequency + static_cast<float>(index)) * kBodyPulseAmplitude;
        if (segment.isHead && isHeadChargeActive_) {
            pulse += kChargePulseAdd;
            pulse += std::sin(headChargeTimer_ * kChargePulseFrequency) * kChargePulseAmplitude;
        }

        Vector3 scale { };
        scale.x = segment.scale.x * pulse;
        scale.y = segment.scale.y * pulse;
        scale.z = segment.scale.z * pulse;

        Vector3 rotate { };
        if (segment.isHead && GetBossState() == BossState::Dead) {
            // 死亡中は死亡状態の更新で回転と座標を設定しUpdateも呼ぶため、共通の上書き処理をバイパスする
            segment.object->SetScale(scale);
            segment.object->SetColor(color);
            continue;
        }

        rotate.y = moveTime_ + static_cast<float>(index) * kSegmentRotationOffset;
        bool isBeamAiming = !HasAliveBodyParts() && (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut);
        if (segment.isHead && (isHeadChargeActive_ || isBeamAiming)) {
            rotate = HeadLookRotation();
        }

        segment.object->SetScale(scale);
        segment.object->SetRotate(rotate);
        segment.object->SetTranslate(segment.position);
        segment.object->SetColor(color);
        segment.object->Update();
    }
}

// 現在表示するモデルや追加パーツを描画する。
void FearWormEnemy::Draw()
{
    if (state_ == BossState::Entry) {
        entryState_.Draw();
    }
    if (GetBossState() != BossState::Wait) {
        for (Segment& segment : segments_) {
            if (state_ == BossState::Entry && segment.position.y + segment.scale.y < entryState_.GetWaterHeight()) {
                continue;
            }
            if (!segment.isAlive) {
                continue;
            }

            if (segment.object == nullptr) {
                continue;
            }

            segment.object->Draw();
        }
    }

    DrawBeam();
}

// 影を落とす対象のモデルを、影用の描画処理へ渡す。
void FearWormEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (GetBossState() == BossState::Wait) { return; }
    for (Segment& segment : segments_) {
        if (state_ == BossState::Entry && segment.position.y < entryState_.GetWaterHeight()) { continue; }
        if (segment.isAlive && segment.object != nullptr) { segment.object->DrawShadow(renderer); }
    }
}

// 描画対象をレイトレーシング用のシーンへ登録する。
void FearWormEnemy::SubmitRaytracing(DxrRenderer& renderer) {
    if (GetBossState() == BossState::Wait) { return; }
    for (const Segment& segment : segments_) {
        if (state_ == BossState::Entry && segment.position.y < entryState_.GetWaterHeight()) { continue; }
        if (segment.isAlive && segment.object) { segment.object->SubmitRaytracing(renderer); }
    }
    if (!segments_.empty() && segments_[0].isAlive && beamPlane_
        && (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut)) {
        beamPlane_->SubmitRaytracing(renderer);
    }
}

// 現在存在する部位の位置と半径を、衝突判定用の一覧へ書き出す。
void FearWormEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (GetBossState() == BossState::Wait || GetBossState() == BossState::Entry) {
        return;
    }

    // 生存中の部位だけを当たり判定として登録する。
    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        const Segment& segment = segments_[index];

        if (!segment.isAlive) {
            continue;
        }

        EnemyCollisionPart part { };
        part.position = segment.position;
        part.radius = segment.radius;
        part.partIndex = static_cast<int32_t>(index);

        parts.push_back(part);
    }
}

// 部位が生存し、現在の状態でダメージを受けられるかを調べる。
bool FearWormEnemy::IsCollisionPartDamageable(int32_t partIndex) const
{
    if (state_ != BossState::Battle) { return false; }
    // 範囲外または破壊済みの部位にはダメージを通さない。
    if (!IsValidSegmentIndex(partIndex)) {
        return false;
    }

    const Segment& segment = segments_[partIndex];
    if (!segment.isAlive) {
        return false;
    }

    // 胴体が残っている間は頭部を無敵にする。
    if (segment.isHead && HasAliveBodyParts()) {
        return false;
    }

    return true;
}

// 命中した部位のHPを減らし、破壊や頭の弱点状態の変化を反映する。
void FearWormEnemy::ApplyDamageToPart(int32_t partIndex, float damage)
{
    if (IsDead() || !std::isfinite(damage) || damage <= 0.0f) { return; }
    if (!IsValidSegmentIndex(partIndex)) {
        return;
    }

    // ダメージ無効の部位ならガード演出だけを出す。
    if (!IsCollisionPartDamageable(partIndex)) {
        OnCollisionPartGuarded(partIndex, segments_[partIndex].position);
        return;
    }

    // HPを減らし、まだ残っていれば被弾表示だけ更新する。
    Segment& segment = segments_[partIndex];
    segment.hp -= damage;
    segment.hitFlashTimer = kDamageHitFlashDuration;

    if (segment.hp > 0.0f) {
        return;
    }

    // 頭部のHPが尽きた場合はボス撃破へ移行する。
    if (segment.isHead) {
        SetDead(true);
        return;
    }

    // 胴体の場合は対象部位だけを破壊する。
    segment.isAlive = false;
    PlayBodyBreakEffect(segment.position);

    // 最後の胴体が壊れた瞬間に頭部の弱点化を通知する。
    if (!vulnerableEffectPlayed_ && !HasAliveBodyParts()) {
        vulnerableEffectPlayed_ = true;
        PlayHeadVulnerableEffect(GetPosition());
    }
}

// ダメージを防いだ部位に、ガードされたことが分かる演出を出す。
void FearWormEnemy::OnCollisionPartGuarded(int32_t partIndex, const Vector3& position)
{
    if (!IsValidSegmentIndex(partIndex)) {
        return;
    }

    // 対象部位を短時間点滅させ、命中位置にガード演出を出す。
    segments_[partIndex].hitFlashTimer = kGuardHitFlashDuration;
    PlayHeadGuardEffect(position);
}

// 胴体が破壊された位置に、破片や爆発の演出を出す。
void FearWormEnemy::PlayBodyBreakEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("WormBodyBreak", position);
}

// 頭が攻撃を防いだ位置へ、ガードのエフェクトを出す。
void FearWormEnemy::PlayHeadGuardEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("WormHeadGuard", position);
}

// 頭の弱点に命中した位置へ、ダメージのエフェクトを出す。
void FearWormEnemy::PlayHeadVulnerableEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("WormHeadVulnerable", position);
}

// 残りHPの割合を求め、速度や攻撃の変化に使う。
float FearWormEnemy::HealthRate() const
{
    float currentHealth = 0.0f;
    float maxHealth = 0.0f;

    // 全部位の最大HPと、生存部位の残りHPを合計する。
    for (const Segment& segment : segments_) {
        if (segment.isHead) {
            maxHealth += kHeadHp;
        } else {
            maxHealth += kBodyHp;
        }

        if (!segment.isAlive) {
            continue;
        }

        if (segment.hp > 0.0f) {
            currentHealth += segment.hp;
        }
    }

    if (maxHealth <= kMinimumHealth) {
        return 0.0f;
    }

    // 最大HPに対する割合を求め、0～1の範囲に収める。
    float healthRate = currentHealth / maxHealth;

    if (healthRate < 0.0f) {
        healthRate = 0.0f;
    }

    if (healthRate > 1.0f) {
        healthRate = 1.0f;
    }

    return healthRate;
}

// 生き残っている胴体部位があるかを確認する。
bool FearWormEnemy::HasAliveBodyParts() const
{
    // 先頭の頭部を除き、生存している胴体を探す。
    for (size_t index = 1; index < segments_.size(); index = index + 1) {
        if (segments_[index].isAlive) {
            return true;
        }
    }

    return false;
}

// 部位番号が配列の有効範囲内かを確認し、範囲外アクセスを防ぐ。
bool FearWormEnemy::IsValidSegmentIndex(int32_t partIndex) const
{
    // 負数と配列末尾より後ろの番号を無効とする。
    if (partIndex < 0) {
        return false;
    }

    if (partIndex >= static_cast<int32_t>(segments_.size())) {
        return false;
    }

    return true;
}

// 頭の残りHPを割合で返し、ボスのHP表示に使う。
float FearWormEnemy::GetHeadHpFraction() const
{
    if (segments_.empty()) {
        return 0.0f;
    }
    return (std::max)(0.0f, segments_[0].hp) / kHeadHp;
}

// 胴体の残りHPをまとめて割合で返し、ボスのHP表示に使う。
float FearWormEnemy::GetBodyHpFraction() const
{
    if (segments_.size() <= 1) {
        return 0.0f;
    }

    float totalMaxHp = (segments_.size() - 1) * kBodyHp;
    float currentTotalHp = 0.0f;

    for (size_t index = 1; index < segments_.size(); ++index) {
        if (segments_[index].isAlive) {
            currentTotalHp += (std::max)(0.0f, segments_[index].hp);
        }
    }

    return currentTotalHp / totalMaxHp;
}
