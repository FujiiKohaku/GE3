// 編隊内の担当位置へ移動する敵。編隊の形・進行・射撃を個体ごとに計算する。
#include "App/Game/Enemy/SwarmEnemy/SwarmEnemy.h"

#include "App/Game/Enemy/Bullet/Types/SwarmPulseBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/math/MathStruct.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>
#include <numbers>

namespace {
constexpr float kSwarmExitX = 40.0f;
constexpr float kSwarmBulletSpeed = 1.15f;
constexpr float kFadeOutDuration = 0.70f;
constexpr float kAttackIntervalBase = 0.90f;
constexpr int32_t kMaximumShots = 2;
}

// 編隊の基準位置・個体番号・進行方向などを記録し、群れの一体として配置する。
void SwarmEnemy::Initialize(
    Model* model,
    Model* bulletModel,
    Player* player,
    const std::shared_ptr<SwarmGroupState>& groupState,
    SwarmFormationType formationType,
    int32_t slotIndex,
    int32_t travelDirection,
    float baseHeight)
{
    BaseEnemy::Initialize(model);

    bulletModel_ = bulletModel;
    player_ = player;
    groupState_ = groupState;
    formationType_ = formationType;
    slotIndex_ = slotIndex;
    travelDirection_ = travelDirection;
    baseHeight_ = baseHeight;
    if (player_ != nullptr) {
        previousPlayerPositionZ_ = player_->GetTranslate().z;
        forwardPositionZ_ = previousPlayerPositionZ_ + forwardDistance_;
    }
    SetHealth(1.0f);
    transform_.scale = { 0.58f, 0.58f, 0.58f };
    attackTimer_ =
        0.35f +
        static_cast<float>(slotIndex_ % 6) * 0.13f;

    centerStartX_ = -38.0f;
    if (travelDirection_ < 0) {
        centerStartX_ = 38.0f;
        transform_.rotate.y = std::numbers::pi_v<float>;
    }

    if (object_ != nullptr) {
        object_->SetScale(transform_.scale);
        object_->SetEnableLighting(false);

        baseColor_ = GetFormationColor(formationType_);
        object_->SetColor(baseColor_);
    }

    Move();
    if (object_ != nullptr) {
        object_->SetScale(transform_.scale);
        object_->SetRotate(transform_.rotate);
        object_->SetTranslate(transform_.translate);
        object_->Update();
    }
}

// 編隊内の移動と射撃を、基底クラスの更新経路で進める。
void SwarmEnemy::Update()
{
    BaseEnemy::Update();
}

// 見た目に対応する現在の位置と半径を、当たり判定として書き出す。
void SwarmEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (isFading_) {
        return;
    }

    EnemyCollisionPart part {};
    part.position = transform_.translate;
    part.radius = 1.15f;
    part.partIndex = 0;
    parts.push_back(part);
}

// 編隊の移動と個体ごとの揺れを合成し、担当位置へ追従させる。
void SwarmEnemy::Move()
{
    if (player_ == nullptr) {
        return;
    }

    if (isFading_) {
        fadeTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        float fadeRatio = 1.0f - fadeTimer_ / kFadeOutDuration;
        if (fadeRatio < 0.0f) {
            fadeRatio = 0.0f;
        }

        if (object_ != nullptr) {
            Vector4 fadeColor = baseColor_;
            fadeColor.w = fadeRatio;
            object_->SetColor(fadeColor);
        }

        if (fadeTimer_ >= kFadeOutDuration) {
            escaped_ = true;
            SetDead(true);
        }
        return;
    }

    moveTime_ += TimeManager::GetInstance()->GetDeltaTime();

    float centerX = centerStartX_;
    centerX += static_cast<float>(travelDirection_) * crossingSpeed_ * moveTime_;

    Vector3 formationOffset = CalculateFormationOffset();
    // 敵はプレイヤー更新前に動くため、直前の移動に使った倍率を参照する。
    // 通常の進行分だけ追従し、ブーストで縮めた距離は解除後も維持する。
    const float playerPositionZ = player_->GetTranslate().z;
    const float playerMovementZ = playerPositionZ - previousPlayerPositionZ_;
    forwardPositionZ_ += playerMovementZ / player_->GetForwardSpeedMultiplier();
    previousPlayerPositionZ_ = playerPositionZ;
    transform_.translate.x = centerX + formationOffset.x;
    transform_.translate.y = baseHeight_ + formationOffset.y;
    transform_.translate.z =
        forwardPositionZ_ + formationOffset.z;
    transform_.rotate.z =
        std::sin(moveTime_ * 4.0f + static_cast<float>(slotIndex_) * 0.25f) * 0.28f;

    bool passedExit = false;
    if (travelDirection_ > 0 && transform_.translate.x > kSwarmExitX) {
        passedExit = true;
    }
    if (travelDirection_ < 0 && transform_.translate.x < -kSwarmExitX) {
        passedExit = true;
    }

    if (passedExit) {
        isFading_ = true;
        fadeTimer_ = 0.0f;
    }
}

// 外見を変更し、群れの表示に必要な設定を反映する。
void SwarmEnemy::SetAppearance(Model* model)
{
    BaseEnemy::SetAppearance(model);
    // フェードアウト中も、隊形の色で氷のテクスチャを上書きしない。
    baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
}

// 編隊の進行状態や発射間隔を調べ、移動しながら撃つ弾を生成する。
void SwarmEnemy::Attack()
{
    if (IsDead()) {
        return;
    }
    if (isFading_) {
        return;
    }
    if (player_ == nullptr || bulletModel_ == nullptr) {
        return;
    }
    if (shotsFired_ >= kMaximumShots) {
        return;
    }

    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    if (deltaTimeSeconds <= 0.0f) {
        return;
    }
    attackTimer_ -= deltaTimeSeconds;
    if (attackTimer_ > 0.0f) {
        return;
    }

    FireMovingBullet();
    shotsFired_ += 1;
    const float intervalSeconds =
        kAttackIntervalBase +
        static_cast<float>(slotIndex_ % 4) * 0.12f;
    // 発射は1更新1回に制限し、周期の端数だけを次へ引き継ぐ。
    attackTimer_ = intervalSeconds - std::fmod(-attackTimer_, intervalSeconds);
}

// 群れの一体を撃破した際の演出を開始する。
void SwarmEnemy::OnDeath()
{
    if (!groupState_) {
        return;
    }

    groupState_->RecordRemoval(escaped_);
    if (escaped_) { return; }

    BaseEnemy::OnDeath();
}

// 群れの進行方向などをもとに、揺れながら飛ぶ弾を発射する。
void SwarmEnemy::FireMovingBullet()
{
    if (player_ == nullptr) {
        return;
    }
    if (bulletModel_ == nullptr) {
        return;
    }

    Vector3 targetPosition = player_->GetTranslate();

    // 群れ全体が正確になりすぎないよう、通常弾より控えめに45%先読みする。
    constexpr float kPredictionRatio = 0.45f;
    const float distance = Vector3Length(
        targetPosition - transform_.translate);
    if (kSwarmBulletSpeed > 0.0f) {
        const float flightFrames = distance / kSwarmBulletSpeed;
        targetPosition += player_->GetAutomaticWorldVelocity() *
            (flightFrames * kPredictionRatio);
    }

    Vector3 direction = Normalize(targetPosition - transform_.translate);
    Vector3 velocity = direction * kSwarmBulletSpeed;

    std::unique_ptr<SwarmPulseBullet> bullet =
        std::make_unique<SwarmPulseBullet>();
    bullet->Initialize(bulletModel_);
    bullet->SetTranslate(transform_.translate);
    bullet->SetSwarmVelocity(velocity);
    bullet->SetWavePhase(static_cast<float>(slotIndex_) * 0.72f);

    AddEnemyBullet(std::move(bullet));
    EffectManager::GetInstance()->PlayEffect("WormShotFlash", transform_.translate);
}

// 現在の編隊形状における、この個体の相対位置を求める。
Vector3 SwarmEnemy::CalculateFormationOffset() const
{
    const FormationContext context { slotIndex_, travelDirection_, moveTime_ };
    return CalculateFormationOffset(formationType_, context);
}

// 個体番号を角度へ割り当て、渦状の編隊内の位置を求める。
Vector3 SwarmEnemy::CalculateSpiralOffset(const FormationContext& context)
{
    float slotRate =
        static_cast<float>(context.slotIndex) / static_cast<float>(kSwarmMemberCount);
    float angle = slotRate * std::numbers::pi_v<float> * 2.0f;
    angle += context.elapsedSeconds * 2.4f;

    Vector3 offset {};
    offset.x = std::cos(angle) * 4.5f;
    offset.y = std::sin(angle) * 8.0f;
    offset.z = std::sin(angle * 2.0f) * 3.0f;
    return offset;
}

// 個体を行と列へ割り当て、壁状の編隊内の位置を求める。
Vector3 SwarmEnemy::CalculateWallOffset(const FormationContext& context)
{
    constexpr int32_t kColumnCount = 6;
    int32_t column = context.slotIndex % kColumnCount;
    int32_t row = context.slotIndex / kColumnCount;

    Vector3 offset {};
    offset.x = (static_cast<float>(column) - 2.5f) * 2.5f;
    offset.y = (static_cast<float>(row) - 1.0f) * 6.0f;
    offset.z =
        std::sin(context.elapsedSeconds * 3.0f + static_cast<float>(column) * 0.5f) * 1.2f;
    return offset;
}

// 文字や記号を形作る編隊内の位置を求める。
Vector3 SwarmEnemy::CalculateGlyphOffset(const FormationContext& context)
{
    constexpr int32_t kLeftBranchCount = 9;
    Vector3 offset {};
    float branchRate = 0.0f;

    if (context.slotIndex < kLeftBranchCount) {
        branchRate =
            static_cast<float>(context.slotIndex) /
            static_cast<float>(kLeftBranchCount - 1);
        offset.x = -9.0f + branchRate * 9.0f;
        offset.y = 8.0f - branchRate * 14.0f;
    } else {
        int32_t rightIndex = context.slotIndex - kLeftBranchCount;
        branchRate =
            static_cast<float>(rightIndex) /
            static_cast<float>(kSwarmMemberCount - kLeftBranchCount - 1);
        offset.x = branchRate * 9.0f;
        offset.y = -6.0f + branchRate * 14.0f;
    }

    offset.y +=
        std::sin(context.elapsedSeconds * 3.2f + static_cast<float>(context.slotIndex) * 0.35f) *
        0.45f;
    offset.z =
        std::cos(context.elapsedSeconds * 2.0f + static_cast<float>(context.slotIndex) * 0.22f) *
        1.3f;
    return offset;
}

// 菱形に広がる編隊内の位置を求める。
Vector3 SwarmEnemy::CalculateDiamondOffset(const FormationContext& context)
{
    int32_t row = 0;
    int32_t indexInRow = context.slotIndex;
    int32_t rowCount = 2;

    if (context.slotIndex >= 2 && context.slotIndex < 6) {
        row = 1;
        indexInRow = context.slotIndex - 2;
        rowCount = 4;
    }
    if (context.slotIndex >= 6 && context.slotIndex < 12) {
        row = 2;
        indexInRow = context.slotIndex - 6;
        rowCount = 6;
    }
    if (context.slotIndex >= 12 && context.slotIndex < 16) {
        row = 3;
        indexInRow = context.slotIndex - 12;
        rowCount = 4;
    }
    if (context.slotIndex >= 16) {
        row = 4;
        indexInRow = context.slotIndex - 16;
        rowCount = 2;
    }

    Vector3 offset {};
    offset.x =
        (static_cast<float>(indexInRow) -
         static_cast<float>(rowCount - 1) * 0.5f) *
        2.6f;
    offset.y = 8.0f - static_cast<float>(row) * 4.0f;
    offset.z =
        std::sin(context.elapsedSeconds * 2.4f + static_cast<float>(row) * 0.6f) *
        0.8f;
    return offset;
}

// 波状に並ぶ編隊内の位置を求める。
Vector3 SwarmEnemy::CalculateWaveOffset(const FormationContext& context)
{
    float slotCenter =
        static_cast<float>(context.slotIndex) -
        static_cast<float>(kSwarmMemberCount - 1) * 0.5f;
    float wavePhase =
        static_cast<float>(context.slotIndex) * 0.72f +
        context.elapsedSeconds * 2.8f;

    Vector3 offset {};
    offset.x = slotCenter * 1.45f;
    offset.y = std::sin(wavePhase) * 6.5f;
    offset.z = std::cos(wavePhase) * 2.2f;
    return offset;
}

// 矢印状に並ぶ編隊内の位置を求める。
Vector3 SwarmEnemy::CalculateArrowOffset(const FormationContext& context)
{
    constexpr int32_t kWingCount = 9;
    int32_t wingIndex = context.slotIndex;
    float verticalDirection = 1.0f;

    if (context.slotIndex >= kWingCount) {
        wingIndex = context.slotIndex - kWingCount;
        verticalDirection = -1.0f;
    }

    float wingRate =
        static_cast<float>(wingIndex) /
        static_cast<float>(kWingCount - 1);

    Vector3 offset {};
    offset.x =
        static_cast<float>(context.travelDirection) *
        (-10.0f + wingRate * 13.0f);
    offset.y = verticalDirection * (8.0f - wingRate * 8.0f);
    offset.y +=
        std::sin(context.elapsedSeconds * 3.0f + static_cast<float>(context.slotIndex) * 0.3f) *
        0.35f;
    offset.z =
        std::cos(context.elapsedSeconds * 2.2f + static_cast<float>(context.slotIndex) * 0.2f) *
        1.1f;
    return offset;
}

// 現在の編隊形状における、この個体の相対位置を求める。
Vector3 SwarmEnemy::CalculateFormationOffset(SwarmFormationType type, const FormationContext& context)
{
    switch (type) {
    case SwarmFormationType::Wall: return CalculateWallOffset(context);
    case SwarmFormationType::Glyph: return CalculateGlyphOffset(context);
    case SwarmFormationType::Diamond: return CalculateDiamondOffset(context);
    case SwarmFormationType::Wave: return CalculateWaveOffset(context);
    case SwarmFormationType::Arrow: return CalculateArrowOffset(context);
    case SwarmFormationType::Spiral:
    default: return CalculateSpiralOffset(context);
    }
}

// 編隊の種類に対応する表示色を返す。
Vector4 SwarmEnemy::GetFormationColor(SwarmFormationType type)
{
    switch (type) {
    case SwarmFormationType::Glyph:
    case SwarmFormationType::Arrow: return { 1.0f, 0.25f, 0.82f, 1.0f };
    case SwarmFormationType::Diamond: return { 1.0f, 0.82f, 0.18f, 1.0f };
    case SwarmFormationType::Wave: return { 0.35f, 1.0f, 0.30f, 1.0f };
    case SwarmFormationType::Wall:
    case SwarmFormationType::Spiral:
    default: return { 0.20f, 0.95f, 1.0f, 1.0f };
    }
}
