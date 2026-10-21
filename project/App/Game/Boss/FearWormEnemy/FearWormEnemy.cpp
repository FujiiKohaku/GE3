// 頭と胴体が連なるボス。水面からの登場、移動、各種攻撃、部位破壊と死亡演出を扱う。
#include "FearWormEnemy.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Math/MathStruct.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include "Engine/3D/Object3dManager.h"

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
    player_ = player;
    SetHealth(kHeadHp);
    moveSpeed_ = kBaseMoveSpeed;

    battleState_.Initialize(bulletModel);

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
    waitState_.SetStartPosition(position);
    ChangeState(BossState::Wait);
    entryState_.Reset();
    battleState_.Reset(*this);
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
    return deadState_.IsDeathSequenceFinished();
}

// 撃破後の演出へ切り替え、通常の戦闘処理を終える。
void FearWormEnemy::OnDeath()
{
    ChangeState(BossState::Dead);
}

// 移動の種類に対応する軌道計算を選ぶ。
Vector3 FearWormEnemy::CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context)
{
    return FearWormBattleState::CalculateFearWormTarget(type, context);
}


// 現在の移動状態に応じた目標位置を取得する。
Vector3 FearWormEnemy::MovementTargetPosition(const Vector3& playerPosition) const
{
    return battleState_.MovementTargetPosition(*this, playerPosition);
}


// 頭から後ろへ順に追従位置を計算し、胴体がつながってうねる形を作る。
void FearWormEnemy::UpdateSegments()
{
    if (state_ == BossState::Entry) { return; }
    if (segments_.empty()) {
        return;
    }

    const float formationBlend = battleState_.UpdateFormationBlend();
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

        segment.position = battleState_.CalculateFormationPosition(*this, aliveOrder, posNormal, formationBlend);

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


// 頭が照準方向を向くように描画用の回転角を計算する。
Vector3 FearWormEnemy::HeadLookRotation() const
{
    return battleState_.HeadLookRotation(*this);
}

// ビーム本体と溜め演出を描画するリソースを準備する。
void FearWormEnemy::InitializeBeam()
{
    battleState_.InitializeBeam(*this);
}

// 現在のビーム状態に必要な溜め演出や光線を描画する。
void FearWormEnemy::DrawBeam()
{
    battleState_.DrawBeam(*this);
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

            color = battleState_.HeadAttackColor(color);
        } else {
            color = kBodyColor;
        }

        if (segment.hitFlashTimer > 0.0f) {
            color = kHitFlashColor;
        }

        float pulse = 1.0f + std::sin(moveTime_ * kBodyPulseFrequency + static_cast<float>(index)) * kBodyPulseAmplitude;
        if (segment.isHead) {
            pulse = battleState_.ApplyHeadChargePulse(pulse);
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
        if (segment.isHead && battleState_.ShouldAimHead(*this)) {
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
    battleState_.SubmitRaytracing(*this, renderer);
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
