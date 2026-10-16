#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/audio/SoundManager.h"

#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/Model.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Enemy/Bullet/EnemyBulletManager.h"
#include <cmath>
#include <algorithm>

EnemyBulletManager* BaseEnemy::bulletManager_ = nullptr;

void BaseEnemy::Initialize(Model* model)
{
    object_ = std::make_unique<Object3d>();

    object_->Initialize(
        Object3dManager::GetInstance());
    object_->SetEnableLighting(true);
    object_->SetMaterial("resources/Shaders/Object3D/ShadowStandard");
    object_->SetNormalMap("resources/Textures/Normals/metal_detail.png", 0.2f);
    object_->SetCastShadow(true);
    object_->SetReceiveShadow(true);
    transform_.scale = {
        2.0f,
        2.0f,
        2.0f
    };
    object_->SetModel(model);
    object_->SetSurfaceProperties(0.38f, 0.60f, 0.75f);

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void BaseEnemy::Update()
{
    if (isDead_) {
        return;
    }

    Move();

    Attack();

    UpdateAnimation();

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    UpdateAppearanceParts();
    object_->Update();
}

void BaseEnemy::SetAppearance(Model* model)
{
    if (object_ == nullptr || model == nullptr) {
        return;
    }
    object_->SetModel(model);
    appearanceParts_.clear();
    appearanceTimeSeconds_ = 0.0f;
    appearanceRecoilSeconds_ = 0.0f;
    appearanceEyeOffset_ = {};
    appearanceEyeTravel_ = {};
    appearanceEyeAnchor_ = {};
    object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    object_->SetEnableLighting(true);
    object_->SetNormalMapStrength(0.0f);
    object_->SetSurfaceProperties(0.25f, 0.15f, 0.65f);
}
void BaseEnemy::SetAppearanceMaterial(const std::string& materialPath)
{
    if (object_ != nullptr) {
        object_->SetMaterial(materialPath);
    }
    appearanceMaterialPath_ = materialPath;
    for (AppearancePart& part : appearanceParts_) { part.object_->SetMaterial(materialPath); }
}

void BaseEnemy::AddAppearancePart(Model* model, EnemyAppearancePart motion)
{
    if (model == nullptr || object_ == nullptr) { return; }
    AppearancePart part;
    part.motion_ = motion;
    float vertexRadius = 0.0f;
    for (const MeshPrimitive& primitive : model->GetModelData().primitives) {
        for (const VertexData& vertex : primitive.vertices) {
            const Vector4& position = vertex.position;
            vertexRadius = (std::max)(vertexRadius, std::sqrt(position.x * position.x + position.y * position.y + position.z * position.z));
        }
    }
    std::vector<EnemyCollisionPart> collisionParts;
    GetCollisionParts(collisionParts);
    if (!collisionParts.empty()) {
        const float scale = (std::max)({ transform_.scale.x, transform_.scale.y, transform_.scale.z, 0.0001f });
        part.maxTranslate_ = (std::max)(0.0f, collisionParts.front().radius / scale - vertexRadius - kEnemyAppearanceBodyHover - 0.002f);
    }
    if (motion == EnemyAppearancePart::Eye) {
        const float pupilRadius = model->GetBoundsRadius();
        appearanceEyeAnchor_ = model->GetBoundsCenter();
        appearanceEyeTravel_ = { (std::min)(pupilRadius * 0.80f, 0.14f),
            (std::min)(pupilRadius * 0.55f, 0.10f), 0 };
    }
    part.object_ = std::make_unique<Object3d>();
    part.object_->Initialize(Object3dManager::GetInstance());
    part.object_->SetModel(model);
    part.object_->SetEnableLighting(true);
    part.object_->SetNormalMapStrength(0.0f);
    part.object_->SetMaterial(appearanceMaterialPath_);
    part.object_->SetCastShadow(object_->GetCastShadow());
    part.object_->SetReceiveShadow(object_->GetReceiveShadow());
    part.object_->SetScale(transform_.scale);
    part.object_->SetRotate(transform_.rotate);
    part.object_->SetTranslate(transform_.translate);
    part.object_->Update();
    appearanceParts_.push_back(std::move(part));
    appearancePhaseRadians_ = transform_.translate.x * 0.37f + transform_.translate.z * 0.13f;
}

void BaseEnemy::UpdateAppearanceParts()
{
    if (appearanceParts_.empty()) { return; }
    const float deltaSeconds = TimeManager::GetInstance()->GetDeltaTime();
    appearanceTimeSeconds_ += deltaSeconds;
    appearanceRecoilSeconds_ = (std::max)(0.0f, appearanceRecoilSeconds_ - deltaSeconds);
    const Matrix4x4 rotation = MatrixMath::MakeAffineMatrix({ 1, 1, 1 }, transform_.rotate, { 0, 0, 0 });
    const Matrix4x4 localToWorld = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, { 0, 0, 0 });
    const Vector3 bodyOffset { 0, kEnemyAppearanceBodyHover * std::sin(appearanceTimeSeconds_ * 2.0f + appearancePhaseRadians_), 0 };
    const Vector3 localEyeAnchor { (appearanceEyeAnchor_.x + bodyOffset.x) * transform_.scale.x,
        (appearanceEyeAnchor_.y + bodyOffset.y) * transform_.scale.y,
        (appearanceEyeAnchor_.z + bodyOffset.z) * transform_.scale.z };
    const Vector3 localTarget = MatrixMath::Transform(appearanceTarget_ - transform_.translate, MatrixMath::Transpose(rotation)) - localEyeAnchor;
    const Vector3 desiredEyeOffset = CalculateEnemyEyeTranslate(localTarget, appearanceEyeTravel_);
    const float smoothing = 1.0f - std::exp(-14.0f * deltaSeconds);
    appearanceEyeOffset_ += (desiredEyeOffset - appearanceEyeOffset_) * smoothing;
    object_->SetTranslate(transform_.translate + MatrixMath::Transform(bodyOffset, localToWorld));
    for (AppearancePart& part : appearanceParts_) {
        const Vector3 desiredOffset = CalculateEnemyPartTranslate(part.motion_, appearanceTimeSeconds_,
            appearancePhaseRadians_, appearanceEyeOffset_, appearanceRecoilSeconds_);
        const Vector3 partOffset = LimitEnemyPartTranslate(desiredOffset, part.maxTranslate_);
        part.object_->SetScale(transform_.scale);
        part.object_->SetRotate(transform_.rotate);
        part.object_->SetTranslate(transform_.translate + MatrixMath::Transform(bodyOffset + partOffset, localToWorld));
        part.object_->SetColor(object_->GetMaterial()->color);
        part.object_->Update();
    }
}

void BaseEnemy::Draw()
{
    if (!isDead_) {
        object_->Draw();
        for (AppearancePart& part : appearanceParts_) { part.object_->Draw(); }
    }

}

void BaseEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (!isDead_ && object_ != nullptr) { object_->DrawShadow(renderer); }
    if (!isDead_) {
        for (AppearancePart& part : appearanceParts_) { part.object_->DrawShadow(renderer); }
    }
}

void BaseEnemy::SubmitRaytracing(DxrRenderer& renderer) {
    if (!isDead_ && object_) { object_->SubmitRaytracing(renderer); }
    if (!isDead_) {
        for (AppearancePart& part : appearanceParts_) { part.object_->SubmitRaytracing(renderer); }
    }
}

void BaseEnemy::SetBulletManager(EnemyBulletManager* bulletManager)
{
    bulletManager_ = bulletManager;
}

void BaseEnemy::AddEnemyBullet(std::unique_ptr<EnemyBullet> bullet, bool shouldPlayShotSound)
{
    if (bulletManager_ != nullptr) {
        if (shouldPlayShotSound) {
            appearanceRecoilSeconds_ = 0.16f;
            SoundManager::GetInstance()->Play("EnemyShot");
        }
        bulletManager_->Add(std::move(bullet));
    }
}

void BaseEnemy::Move()
{
    if (!waypoints_.empty()) {
        float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
        Vector3 target = waypoints_[currentWaypointIndex_];
        Vector3 currentPos = GetPosition();

        Vector3 dir = { target.x - currentPos.x, target.y - currentPos.y, target.z - currentPos.z };
        float distance = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

        if (distance < 0.5f) {
            currentWaypointIndex_ = (currentWaypointIndex_ + 1) % waypoints_.size();
        } else {
            currentPos.x += (dir.x / distance) * moveSpeed_ * deltaTime;
            currentPos.y += (dir.y / distance) * moveSpeed_ * deltaTime;
            currentPos.z += (dir.z / distance) * moveSpeed_ * deltaTime;
            SetPosition(currentPos);
        }
    }
}

void BaseEnemy::Attack()
{
}

bool BaseEnemy::IsDead() const
{
    return isDead_;
}

Vector3 BaseEnemy::GetPosition() const
{
    return transform_.translate;
}

void BaseEnemy::SetPosition(const Vector3& position)
{
    transform_.translate = position;
}

void BaseEnemy::SetRotate(const Vector3& rotate)
{
    transform_.rotate = rotate;
}

void BaseEnemy::SetEnableLighting(bool enable)
{
    if (object_ != nullptr) {
        object_->SetEnableLighting(enable);
    }
}

void BaseEnemy::SetDead(bool isDead)
{
    if (isDead_ == isDead) {
        return;
    }

    isDead_ = isDead;

    if (isDead_) {
        OnDeath();
    }
}

void BaseEnemy::ApplyDamage(float damage)
{
    if (isDead_ || !std::isfinite(damage) || damage <= 0.0f) { return; }
    hp_ = (std::max)(0.0f, hp_ - damage);

    OnDamage(damage);

    if (hp_ <= 0.0f) {
        SetDead(true);
    }
}

void BaseEnemy::ApplyDamageToPart(int32_t, float damage)
{
    ApplyDamage(damage);
}

void BaseEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    EnemyCollisionPart part {};
    part.position = GetPosition();
    part.radius = 3.0f;
    part.partIndex = 0;

    parts.push_back(part);
}

bool BaseEnemy::IsCollisionPartDamageable(int32_t) const
{
    return true;
}

void BaseEnemy::OnCollisionPartGuarded(int32_t, const Vector3&)
{
}

void BaseEnemy::UpdateAnimation()
{
}

void BaseEnemy::OnDamage(float)
{
}

void BaseEnemy::OnDeath()
{
}

void BaseEnemy::SetHealth(float hp)
{
    if (std::isfinite(hp) && hp >= 0.0f) {
        hp_ = hp;
    }
}

void BaseEnemy::InitializeHealth(float hp)
{
    if (!std::isfinite(hp) || hp <= 0.0f) { return; }
    hp_ = hp;
    isDead_ = false;
}

SweepHit BaseEnemy::SweepBullet(const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const
{
    if (IsDead()) { return {}; }
    std::vector<EnemyCollisionPart> parts;
    GetCollisionParts(parts);
    for (const auto& part : parts) {
        SweepHit hit = CollisionManager::SweepSphere(bullet, movement, Sphere { part.position, part.radius });
        if (hit.isHit) {
            partIndex = part.partIndex;
            // 通常敵の従来の部位順序とエフェクト位置を維持する。
            hit.position = part.position;
            return hit;
        }
    }
    return {};
}
void BaseEnemy::OnBulletHit(int32_t partIndex, float damage, const Vector3& position)
{
    if (IsCollisionPartDamageable(partIndex)) { ApplyDamageToPart(partIndex, damage); }
    else { OnCollisionPartGuarded(partIndex, position); }
}
void BaseEnemy::RegisterRaycastTargets(CollisionObjectId& nextObjectId) const
{
    if (IsDead()) { return; }
    std::vector<EnemyCollisionPart> parts;
    GetCollisionParts(parts);
    for (const auto& part : parts) {
        CollisionManager::GetInstance()->RegisterRaycastSphereTarget(nextObjectId++, Sphere { part.position, part.radius });
    }
}
