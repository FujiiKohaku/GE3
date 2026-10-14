#include "FearWormEnemy.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Time/TimeManager.h"

namespace {
constexpr float kHeadHp = 20.0f;
constexpr float kBaseMoveSpeed = 0.10f;
constexpr float kInitialHeadChargeCooldown = 1.80f;
constexpr Vector3 kDefaultHeadAimDirection = { 0.0f, 0.0f, -1.0f };
}

FearWormEnemy::FearWormEnemy() = default;

FearWormEnemy::BossState FearWormEnemy::GetBossState() const { return state_; }

void FearWormEnemy::UpdateState()
{
    switch (state_) {
    case BossState::Wait:   UpdateWait(); break;
    case BossState::Entry:  UpdateEntry(); break;
    case BossState::Battle: UpdateBattle(); break;
    case BossState::Dead:   UpdateDead(); break;
    }
}

void FearWormEnemy::ChangeState(BossState state)
{
    if (state_ == state) return;
    state_ = state;
    switch (state_) {
    case BossState::Entry: EnterEntry(); break;
    case BossState::Dead:  EnterDead(); break;
    default: break;
    }
}

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
    SetPosition(transform_.translate);
}

void FearWormEnemy::SetPosition(const Vector3& position)
{
    // 基準位置と行動状態を初期状態へ戻す。
    transform_.translate = position;
    startPosition_ = position;
    ChangeState(BossState::Wait);
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

void FearWormEnemy::OnDeath()
{
    ChangeState(BossState::Dead);
}
