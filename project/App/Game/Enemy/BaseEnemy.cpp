// 敵のHP・移動・攻撃・外見と部位の当たり判定をまとめる基底クラス。
#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/audio/SoundManager.h"

#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/Model.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Enemy/Bullet/EnemyBulletManager.h"
#include <cmath>
#include <algorithm>

EnemyBulletManager* BaseEnemy::bulletManager_ = nullptr;

// モデルと材質を設定し、敵として共通の描画オブジェクトを作る。
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

// 生存中だけ移動・攻撃・アニメーションを進め、外見パーツを追従させる。
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

// 本体モデルを差し替え、以前の追加パーツや外見アニメーションをリセットする。
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
// 本体と追加パーツに適用する材質を変更する。
void BaseEnemy::SetAppearanceMaterial(const std::string& materialPath)
{
    if (object_ != nullptr) {
        object_->SetMaterial(materialPath);
    }
    appearanceMaterialPath_ = materialPath;
    for (AppearancePart& part : appearanceParts_) { part.object_->SetMaterial(materialPath); }
}

// 目や脚などの別モデルを追加し、担当する動きと基準位置を記録する。
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

// 視線・揺れ・反動による移動を計算し、本体に追従するパーツの姿勢へ反映する。
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

// 現在表示するモデルや追加パーツを描画する。
void BaseEnemy::Draw()
{
    if (!isDead_) {
        object_->Draw();
        for (AppearancePart& part : appearanceParts_) { part.object_->Draw(); }
    }

}

// 影を落とす対象のモデルを、影用の描画処理へ渡す。
void BaseEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (!isDead_ && object_ != nullptr) { object_->DrawShadow(renderer); }
    if (!isDead_) {
        for (AppearancePart& part : appearanceParts_) { part.object_->DrawShadow(renderer); }
    }
}

// 描画対象をレイトレーシング用のシーンへ登録する。
void BaseEnemy::SubmitRaytracing(DxrRenderer& renderer) {
    if (!isDead_ && object_) { object_->SubmitRaytracing(renderer); }
    if (!isDead_) {
        for (AppearancePart& part : appearanceParts_) { part.object_->SubmitRaytracing(renderer); }
    }
}

// 生成した敵弾の引き渡し先を設定する。管理クラスの所有権は受け取らない。
void BaseEnemy::SetBulletManager(EnemyBulletManager* bulletManager)
{
    bulletManager_ = bulletManager;
}

// 生成した弾の所有権を敵弾管理へ渡し、必要なら発射音も鳴らす。
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

// 標準では射撃しない。具体的な敵の派生クラスで攻撃を実装する。
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

// 死亡状態の変更を反映し、撃破されたタイミングで死亡処理を呼ぶ。
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

// ダメージをHPへ反映し、被弾時やHPが尽きたときの処理につなげる。
void BaseEnemy::ApplyDamage(float damage)
{
    if (isDead_ || !std::isfinite(damage) || damage <= 0.0f) { return; }
    hp_ = (std::max)(0.0f, hp_ - damage);

    OnDamage(damage);

    if (hp_ <= 0.0f) {
        SetDead(true);
    }
}

// 部位別HPを持たない敵では、命中部位を分けずに本体へダメージを渡す。
void BaseEnemy::ApplyDamageToPart(int32_t, float damage)
{
    ApplyDamage(damage);
}

// 標準では敵本体を一つの球として、衝突判定用の一覧へ書き出す。
void BaseEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    EnemyCollisionPart part {};
    part.position = GetPosition();
    part.radius = 3.0f;
    part.partIndex = 0;

    parts.push_back(part);
}

// 標準の敵がダメージを受けられる状態かを返す。
bool BaseEnemy::IsCollisionPartDamageable(int32_t) const
{
    return true;
}

// 防御時の演出を派生クラスで定義するための窓口。標準では何も行わない。
void BaseEnemy::OnCollisionPartGuarded(int32_t, const Vector3&)
{
}

// 標準の追加アニメーション処理は空にし、必要な敵だけが上書きする。
void BaseEnemy::UpdateAnimation()
{
}

// 被弾時の追加反応を派生クラスで定義するための窓口。標準では何も行わない。
void BaseEnemy::OnDamage(float)
{
}

// HPが尽きて撃破された場合に爆発を再生し、退場による削除では再生しない。
void BaseEnemy::OnDeath()
{
    if (hp_ > 0.0f) {
        return;
    }
    EffectManager* effectManager = EffectManager::GetInstance();
    // 半透明の煙を先に描き、炎・火花・閃光を手前に重ねる。
    effectManager->PlayEffect("ExplosionSmoke", transform_.translate);
    effectManager->PlayEffect("ExplosionFlame", transform_.translate);
    effectManager->PlayEffect("ExplosionSparks", transform_.translate);
    effectManager->PlayEffect("ExplosionFlash", transform_.translate);
}

// 敵の最大HPと現在HPを設定する。
void BaseEnemy::SetHealth(float hp)
{
    if (std::isfinite(hp) && hp >= 0.0f) {
        hp_ = hp;
    }
}

// HPの初期値と死亡状態を初期化する。
void BaseEnemy::InitializeHealth(float hp)
{
    if (!std::isfinite(hp) || hp <= 0.0f) { return; }
    hp_ = hp;
    isDead_ = false;
}

// 弾の前フレームからの移動区間を調べ、高速の弾でも当たりをすり抜けないようにする。
SweepHit BaseEnemy::SweepBullet(const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const
{
    if (IsDead()) { return {}; }
    std::vector<EnemyCollisionPart> parts;
    GetCollisionParts(parts);
    SweepHit nearest {};
    for (const auto& part : parts) {
        SweepHit hit = CollisionManager::SweepSphere(bullet, movement, Sphere { part.position, part.radius });
        if (hit.isHit && (!nearest.isHit || hit.time < nearest.time)) {
            partIndex = part.partIndex;
            // 命中順は接触時刻で決め、演出は従来どおり部位の中心へ出す。
            hit.position = part.position;
            nearest = hit;
        }
    }
    return nearest;
}
// 命中位置と部位番号を受け取り、部位へのダメージ処理へ渡す。
void BaseEnemy::OnBulletHit(int32_t partIndex, float damage, const Vector3& position)
{
    if (IsCollisionPartDamageable(partIndex)) { ApplyDamageToPart(partIndex, damage); }
    else { OnCollisionPartGuarded(partIndex, position); }
}
// 照準が敵を狙えるように、部位の球をレイ判定へ登録する。
void BaseEnemy::RegisterRaycastTargets(CollisionObjectId& nextObjectId) const
{
    if (IsDead()) { return; }
    std::vector<EnemyCollisionPart> parts;
    GetCollisionParts(parts);
    for (const auto& part : parts) {
        CollisionManager::GetInstance()->RegisterRaycastSphereTarget(nextObjectId++, Sphere { part.position, part.radius });
    }
}
