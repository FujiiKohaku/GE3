// 頭・触手・氷柱で構成するボス。攻撃パターン、部位ごとのHPと当たり判定を扱う。
#include "IceJellyfish.h"

#include "App/Game/Player/Player.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace {
constexpr float kBodyAlpha = 0.95f;
// 調整用の設定値。意味が異なる値は、数値が同じでも個別に保持する。
constexpr float kNormalMapStrength = 0.18f;
constexpr float kEnvironmentMapStrength = 0.10f;
constexpr float kBodyShininess = 96.0f;
constexpr float kSurfaceRoughness = 0.28f;
constexpr float kSurfaceSpecularStrength = 0.70f;
constexpr float kHitFlashSeconds = 0.12f;
constexpr float kCoreDamageMultiplier = 1.5f;
constexpr float kCoreCollisionRadius = 3.6f;
constexpr Vector4 kBellColor = { 0.65f, 0.86f, 1.0f, kBodyAlpha };
constexpr Vector4 kCoreColor = { 2.8f, 1.25f, 0.25f, 1.0f };
constexpr Vector4 kTentacleColor = { 0.55f, 0.80f, 1.0f, kBodyAlpha };
constexpr Vector4 kPillarColor = { 1.1f, 1.65f, 2.6f, 1.0f };
constexpr Vector3 kSpotLightOffset = { -55.0f, 100.0f, -45.0f };
constexpr Vector3 kPointLightOffset = { 45.0f, 20.0f, -35.0f };
constexpr Vector3 kUpperFogOffset = { 0.0f, 25.0f, 0.0f };
constexpr Vector4 kBellDebugColor = { 0.1f, 0.7f, 1.0f, 1.0f };
constexpr Vector4 kTentacleDebugColor = { 0.8f, 0.3f, 1.0f, 1.0f };
constexpr Vector4 kPillarDebugColor = { 1.0f, 0.15f, 0.05f, 1.0f };
constexpr Vector4 kCoreDebugColor = { 1.0f, 0.6f, 0.1f, 1.0f };
constexpr float kLowerFogHeight = 4.0f;
constexpr float kPillarDebugCenterHeight = 19.0f;
constexpr float kPillarDebugLineWidth = 3.0f;
constexpr float kCoreDebugLineWidth = 2.0f;


void ConfigureJellyfishBodyMaterial(Object3d& object)
{
    object.SetMaterial("resources/Shaders/Object3D/IceJellyfish");
    object.SetNormalMap("resources/Textures/Normals/ice_detail.png", kNormalMapStrength);
    object.SetVertexShaderPath("resources/Shaders/Object3D/IceJellyfish/Render.VS.hlsl");
    object.SetMotionVectorShader(L"resources/Shaders/Object3D/IceJellyfish/MotionVector.VS.hlsl");
    ShadowMaterialSettings shadowMaterial;
    shadowMaterial.vertexShaderPath = L"resources/Shaders/ShadowMap/JellyfishDepth.VS.hlsl";
    shadowMaterial.isDoubleSided = true;
    object.SetShadowMaterial(shadowMaterial);
    object.SetTransparent(true);
    object.SetTransparentDepthWrite(true);
    object.SetEnableEnvironmentMap(true);
    object.SetEnvironmentMapStrength(kEnvironmentMapStrength);
    object.SetShininess(kBodyShininess);
    object.SetSurfaceProperties(kSurfaceRoughness, 0.0f, kSurfaceSpecularStrength);
}

struct TransparentPart {
    Object3d* object = nullptr;
    float viewDepth = 0.0f;
};

TransparentPart MakeTransparentPart(Object3d& object, const Camera& camera)
{
    const Vector3 worldPosition = MatrixMath::Transform({}, object.GetWorldMatrix());
    const Vector3 viewPosition = MatrixMath::Transform(worldPosition, camera.GetViewMatrix());
    return { &object, viewPosition.z };
}

bool IsFartherPart(const TransparentPart& left, const TransparentPart& right)
{
    return left.viewDepth > right.viewDepth;
}

}

IceJellyfish::IceJellyfish()
{
    ChangeState(BossState::Wait);
}

void IceJellyfish::ChangeState(BossState state)
{
    if (currentState_ != nullptr && state_ == state) {
        return;
    }
    IceJellyfishState* nextState = nullptr;
    switch (state) {
    case BossState::Wait: nextState = &waitState_; break;
    case BossState::Entry: nextState = &entryState_; break;
    case BossState::Battle: nextState = &battleState_; break;
    case BossState::Dead: nextState = &deadState_; break;
    default: return;
    }
    if (currentState_ != nullptr) {
        currentState_->Exit(*this);
    }
    state_ = state;
    currentState_ = nextState;
    currentState_->Enter(*this);
}

void IceJellyfish::ResetStates()
{
    if (currentState_ != nullptr) {
        currentState_->Exit(*this);
    }
    currentState_ = nullptr;
    battleState_.Reset();
    deadState_.Reset();
    ChangeState(BossState::Wait);
}

void IceJellyfish::UpdateState()
{
    // Waitと空のEntryを通過しても、戦闘・死亡の時間更新は1回だけにする。
    for (int32_t stateIndex = 0; stateIndex < kMaxStateUpdatesPerFrame; stateIndex++) {
        IceJellyfishState* previousState = currentState_;
        currentState_->Update(*this);
        if (currentState_ == previousState) {
            break;
        }
    }
}

void IceJellyfish::OnDeath()
{
    ChangeState(BossState::Dead);
}

// モデルと描画設定から、ボスの一部として使うオブジェクトを生成する。
std::unique_ptr<Object3d> IceJellyfish::CreatePart(
    Camera* camera, const char* modelPath, const Vector4& color, bool ice)
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(Object3dManager::GetInstance());
    object->SetMaterial("resources/Shaders/Object3D/ShadowStandard");
    object->SetCastShadow(true);
    object->SetReceiveShadow(true);
    object->SetModel(ModelManager::GetInstance()->Load(modelPath));
    object->SetCamera(camera);
    object->SetColor(color);
    object->SetEnableLighting(ice);
    object->SetEnableEnvironmentMap(false);
    object->SetEnvironmentMapStrength(0.0f);
    object->SetShininess(0.0f);
    return object;
}

// 頭・触手と氷柱を用意し、攻撃と部位HPを初期状態にする。
void IceJellyfish::Initialize(Camera* camera, Model* bulletModel, Player* player)
{
    camera_ = camera;
    bulletModel_ = bulletModel;
    player_ = player;
    InitializeHealth(kMaxHp);
    ResetStates();
    animationTime_ = 0.0f;
    poseController_.Reset();
    hitFlashTime_ = 0.0f;
    coreExposed_ = false;

    bell_ = CreatePart(camera, "Boss/IceJellyfish/IceJellyfishBell.obj",
        kBellColor, true);
    ConfigureJellyfishBodyMaterial(*bell_);
    core_ = CreatePart(camera, "Boss/IceJellyfish/IceJellyfishCore.obj",
        kCoreColor, false);

    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        tentacleHp_[tentacle] = kTentacleMaxHp;
        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            tentacles_[tentacle][segment] = CreatePart(camera,
                "Boss/IceJellyfish/IceJellyfishSoftSegment.obj",
                kTentacleColor, true);
            ConfigureJellyfishBodyMaterial(*tentacles_[tentacle][segment]);
        }
    }

    for (std::unique_ptr<Object3d>& pillar : icePillarModels_) {
        pillar = CreatePart(camera, "Environment/Ice/IceSpike.obj",
            kPillarColor, false);
    }

    tentacleColliders_.reserve(kTentacleCount * kSegmentsPerTentacle);
    tentacleColliderParts_.reserve(kTentacleCount * kSegmentsPerTentacle);
    UpdatePartTransforms();
}

// 配置の基準位置を変更し、部位の位置も更新する。
void IceJellyfish::SetPosition(const Vector3& position)
{
    basePosition_ = position;
    bodyPosition_ = position;
    hoverCenterX_ = position.x;
    hoverCenterY_ = position.y;
    transform_.translate = position;
    UpdatePartTransforms();
}

// 登場や撃破演出が参照する頭・胴体などの位置をまとめる。
IceJellyfish::PresentationPositions IceJellyfish::GetPresentationPositions() const
{
    const Vector3 position = GetPosition();
    PresentationPositions positions;
    positions.spotLightPosition = position + kSpotLightOffset;
    positions.pointLightPosition = position + kPointLightOffset;
    positions.fogCenters[0] = { position.x, kLowerFogHeight, position.z };
    positions.fogCenters[1] = position + kUpperFogOffset;
    return positions;
}

// 全体の時計を1回進め、現在の状態と共通の部位姿勢を更新する。
void IceJellyfish::Update()
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    animationTime_ += (std::max)(deltaTimeSeconds, 0.0f);
    hitFlashTime_ = (std::max)(0.0f, hitFlashTime_ - deltaTimeSeconds);
    if (IsDead()) {
        ChangeState(BossState::Dead);
    }
    UpdateState();
    // Stateが決めた位置を、取得用の位置と共通Transformへ反映する。
    basePosition_ = bodyPosition_;
    transform_.translate = bodyPosition_;
    UpdatePartTransforms();
}

// 指定した触手の節が破壊されずに残っているかを調べる。
bool IceJellyfish::IsSegmentAlive(size_t tentacle, size_t segment) const
{
    const float segmentThreshold = kSegmentHp * static_cast<float>(segment);
    return tentacleHp_[tentacle] > segmentThreshold;
}

// すべての触手の破壊状態を確認する。
bool IceJellyfish::AreAllTentaclesDestroyed() const
{
    for (float hp : tentacleHp_) {
        if (!(hp <= 0.0f)) return false;
    }
    return true;
}

// 触手番号と節番号を、当たり判定で共通に使う部位番号へ変換する。
int32_t IceJellyfish::SegmentPartIndex(size_t tentacle, size_t segment) const
{
    return kFirstSegmentPart + static_cast<int32_t>(tentacle * kSegmentsPerTentacle + segment);
}

// 共通姿勢を反映したモデルから、ゲーム用の当たり判定を更新する。
void IceJellyfish::UpdatePartTransforms()
{
    if (bell_ == nullptr) return;
    poseController_.Update(*this);
    const Matrix4x4& coreWorld = core_->GetWorldMatrix();
    const float coreScale = Vector3Length({ coreWorld.m[0][0], coreWorld.m[0][1], coreWorld.m[0][2] });
    coreCollider_ = { MatrixMath::Transform({}, coreWorld), kCoreCollisionRadius * coreScale };
    BuildBellColliders(bell_->GetWorldMatrix(), 0.0f, bellColliders_,
        poseController_.GetBellAnimationParameters(animationTime_));
    tentacleColliders_.clear();
    tentacleColliderParts_.clear();
    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            if (IsSegmentAlive(tentacle, segment)) {
                tentacleColliders_.push_back(MakeCollisionBox(
                    tentacles_[tentacle][segment]->GetWorldMatrix(),
                    { 0.0f, -0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }));
                tentacleColliderParts_.push_back(SegmentPartIndex(tentacle, segment));
            }
        }
    }
}

// 影を落とす対象のモデルを、影用の描画処理へ渡す。
void IceJellyfish::DrawShadow(ShadowMapRenderer& renderer)
{
    if (bell_ == nullptr || camera_ == nullptr || (IsDead() && IsDeathSequenceFinished())) { return; }
    if (battleState_.HasIcePillarModels()) {
        for (const auto& pillar : icePillarModels_) { pillar->DrawShadow(renderer); }
    }
    core_->DrawShadow(renderer);
    bell_->DrawShadow(renderer, true);
    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            if (IsSegmentAlive(tentacle, segment)) {
                tentacles_[tentacle][segment]->DrawShadow(renderer, true);
            }
        }
    }
}

// 現在表示するモデルや追加パーツを描画する。
void IceJellyfish::Draw()
{
    if (bell_ == nullptr || camera_ == nullptr || (IsDead() && IsDeathSequenceFinished())) return;
    if (battleState_.HasIcePillarModels()) {
        for (const std::unique_ptr<Object3d>& pillar : icePillarModels_) pillar->Draw();
    }
    core_->Draw();

    // Draw opaque parts first, then sort the transparent body in camera space.
    std::array<TransparentPart, kTentacleCount * kSegmentsPerTentacle + 1> parts {};
    size_t partCount = 0;
    parts[partCount++] = MakeTransparentPart(*bell_, *camera_);
    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            if (IsSegmentAlive(tentacle, segment)) {
                parts[partCount++] = MakeTransparentPart(*tentacles_[tentacle][segment], *camera_);
            }
        }
    }
    std::sort(parts.begin(), parts.begin() + partCount, IsFartherPart);
    for (size_t part = 0; part < partCount; ++part) {
        parts[part].object->Draw();
    }
}

// 描画対象をレイトレーシング用のシーンへ登録する。
void IceJellyfish::SubmitRaytracing(DxrRenderer& renderer) {
    if (!bell_ || !camera_ || (IsDead() && IsDeathSequenceFinished())) { return; }
    if (battleState_.HasIcePillarModels()) {
        for (const auto& pillar : icePillarModels_) {
            if (pillar) { pillar->SubmitRaytracing(renderer); }
        }
    }
    if (core_) { core_->SubmitRaytracing(renderer); }
    bell_->SubmitRaytracing(renderer);
    for (size_t tentacleIndex = 0; tentacleIndex < kTentacleCount; ++tentacleIndex) {
        for (size_t segmentIndex = 0; segmentIndex < kSegmentsPerTentacle; ++segmentIndex) {
            if (IsSegmentAlive(tentacleIndex, segmentIndex) && tentacles_[tentacleIndex][segmentIndex]) {
                tentacles_[tentacleIndex][segmentIndex]->SubmitRaytracing(renderer);
            }
        }
    }
}

// 弾の移動区間と各部位を調べ、最も手前で当たった部位を返す。
SweepHit IceJellyfish::SweepBullet(
    const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const
{
    SweepHit nearest {};
    partIndex = kBellPart;
    if (bell_ == nullptr || IsDead()) return nearest;

    if (!coreExposed_) {
        for (const OBB& box : bellColliders_) {
            ConsiderNearestHit(nearest, partIndex, CollisionManager::SweepSphere(bullet, movement, box), kBellPart);
        }
    }
    for (size_t index = 0; index < tentacleColliders_.size(); ++index) {
        if (!IsCollisionPartDamageable(tentacleColliderParts_[index])) continue;
        ConsiderNearestHit(nearest, partIndex, CollisionManager::SweepSphere(bullet, movement, tentacleColliders_[index]),
            tentacleColliderParts_[index]);
    }
    ConsiderNearestHit(nearest, partIndex, CollisionManager::SweepSphere(bullet, movement, coreCollider_), kCorePart);
    return nearest;
}

// 命中した部位へダメージを渡し、命中位置を使う演出につなげる。
void IceJellyfish::OnBulletHit(int32_t partIndex, float damage, const Vector3& position)
{
    if (IsCollisionPartDamageable(partIndex)) {
        ApplyDamageToPart(partIndex, damage);
        EffectManager::GetInstance()->PlayEffect("HitEffect", position);
    } else {
        OnCollisionPartGuarded(partIndex, position);
    }
}

// 指定部位が現在ダメージを受けられるかを確認する。
bool IceJellyfish::IsCollisionPartDamageable(int32_t partIndex) const
{
    if (partIndex == kCorePart) return coreExposed_;
    if (partIndex >= kFirstSegmentPart &&
        partIndex < kFirstSegmentPart + static_cast<int32_t>(kTentacleCount * kSegmentsPerTentacle)) {
        const int32_t flat = partIndex - kFirstSegmentPart;
        return IsSegmentAlive(flat / static_cast<int32_t>(kSegmentsPerTentacle),
            flat % static_cast<int32_t>(kSegmentsPerTentacle));
    }
    return false;
}

// 頭や触手など、命中先の部位に対応するHPを減らす。
void IceJellyfish::ApplyDamageToPart(int32_t partIndex, float damage)
{
    if (IsDead() || !std::isfinite(damage) || damage <= 0.0f) return;
    if (partIndex == kCorePart) {
        if (!coreExposed_) return;
        hitFlashTime_ = kHitFlashSeconds;
        SetHealth((std::max)(0.0f, GetHealth() - damage * kCoreDamageMultiplier));
    } else if (partIndex >= kFirstSegmentPart &&
        partIndex < kFirstSegmentPart + static_cast<int32_t>(kTentacleCount * kSegmentsPerTentacle)) {
        const int32_t flat = partIndex - kFirstSegmentPart;
        const size_t tentacle = static_cast<size_t>(flat) / kSegmentsPerTentacle;
        const float before = tentacleHp_[tentacle];
        tentacleHp_[tentacle] = (std::max)(0.0f, before - damage);
        for (size_t remaining = kSegmentsPerTentacle; remaining > 0; --remaining) {
            const size_t segment = remaining - 1;
            const bool wasAlive = before > kSegmentHp * static_cast<float>(segment);
            if (wasAlive && !IsSegmentAlive(tentacle, segment)) {
                const Vector3 burstPosition = MatrixMath::Transform(
                    {}, tentacles_[tentacle][segment]->GetWorldMatrix());
                EffectManager::GetInstance()->PlayEffect("Explosion", burstPosition);
            }
        }
        coreExposed_ = AreAllTentaclesDestroyed();
    } else {
        return;
    }

    if (GetHealth() <= 0.0f) {
        SetDead(true);
        EffectManager::GetInstance()->PlayEffect("Explosion", coreCollider_.center);
    }
}

// 防御中の部位に命中したことを、エフェクトで伝える。
void IceJellyfish::OnCollisionPartGuarded(int32_t, const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("HitEffect", position);
}

// 現在の部位配置を、接触判定に使う球の一覧へ変換する。
void IceJellyfish::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    parts.clear();
    parts.push_back({ coreCollider_.center, coreCollider_.radius, kCorePart });
    for (size_t index = 0; index < tentacleColliders_.size(); ++index) {
        if (!IsCollisionPartDamageable(tentacleColliderParts_[index])) continue;
        const OBB& box = tentacleColliders_[index];
        const float radius = (std::max)(box.size.x, (std::max)(box.size.y, box.size.z));
        parts.push_back({ box.center, radius, tentacleColliderParts_[index] });
    }
}

// 照準が頭や触手を狙えるように、各部位をレイ判定へ登録する。
void IceJellyfish::RegisterRaycastTargets(CollisionObjectId& nextObjectId) const
{
    if (bell_ == nullptr || IsDead()) return;
    CollisionManager* manager = CollisionManager::GetInstance();
    if (!coreExposed_) {
        for (const OBB& box : bellColliders_) manager->RegisterRaycastObbTarget(nextObjectId++, box);
    }
    for (size_t index = 0; index < tentacleColliders_.size(); ++index) {
        if (IsCollisionPartDamageable(tentacleColliderParts_[index])) {
            manager->RegisterRaycastObbTarget(nextObjectId++, tentacleColliders_[index]);
        }
    }
    manager->RegisterRaycastSphereTarget(nextObjectId++, coreCollider_);
}

// 部位や特殊攻撃の当たり判定を、開発用の線で可視化する。
void IceJellyfish::DrawCollisionDebug() const
{
    if (bell_ == nullptr || IsDead()) return;
    DebugRenderer* renderer = DebugRenderer::GetInstance();
    if (!coreExposed_) {
        for (const OBB& box : bellColliders_) {
            renderer->AddWireOBB(box.center, box.size, box.orientation[0], box.orientation[1],
                box.orientation[2], kBellDebugColor);
        }
    }
    for (size_t index = 0; index < tentacleColliders_.size(); ++index) {
        if (!IsCollisionPartDamageable(tentacleColliderParts_[index])) continue;
        const OBB& box = tentacleColliders_[index];
        renderer->AddWireOBB(box.center, box.size, box.orientation[0], box.orientation[1],
            box.orientation[2], kTentacleDebugColor);
    }
    if (battleState_.HasIcePillarModels()) {
        constexpr Vector3 axisX = { 1.0f, 0.0f, 0.0f };
        constexpr Vector3 axisY = { 0.0f, 1.0f, 0.0f };
        constexpr Vector3 axisZ = { 0.0f, 0.0f, 1.0f };
        constexpr Vector3 collisionSize = { 13.0f, 42.0f, 8.0f };
        for (const Vector3& position : battleState_.GetIcePillarPositions()) {
            const Vector3 collisionCenter = {
                position.x,
                position.y + kPillarDebugCenterHeight,
                position.z
            };
            renderer->AddWireOBB(collisionCenter, collisionSize,
                axisX, axisY, axisZ, kPillarDebugColor, kPillarDebugLineWidth);
        }
    }
    renderer->AddWireSphere(coreCollider_.center, coreCollider_.radius,
        kCoreDebugColor, kCoreDebugLineWidth);
}

// 頭の残りHPをHUD用の割合として返す。
float IceJellyfish::GetHeadHpFraction() const
{
    return std::clamp(GetHealth() / kMaxHp, 0.0f, 1.0f);
}

// 触手などの残りHPをHUD用の割合として返す。
float IceJellyfish::GetBodyHpFraction() const
{
    float remaining = 0.0f;
    for (float tentacle : tentacleHp_) remaining += tentacle;
    return remaining / (kTentacleMaxHp * static_cast<float>(kTentacleCount));
}

// 今回の命中が現在の候補より手前なら、命中結果と部位番号を置き換える。
void IceJellyfish::ConsiderNearestHit(SweepHit& nearest, int32_t& partIndex,
    const SweepHit& hit, int32_t candidatePart)
{
    if (hit.isHit && (!nearest.isHit || hit.time < nearest.time)) {
        nearest = hit;
        partIndex = candidatePart;
    }
}
