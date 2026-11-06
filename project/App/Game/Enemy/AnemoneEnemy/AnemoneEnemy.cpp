#include "AnemoneEnemy.h"
#include "AnemoneShotPattern.h"
#include "App/Game/Common/GameTiming.h"
#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Model.h"
#include "Engine/math/MatrixMath.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Logger/Logger.h"
#include <limits>
#include <algorithm>

void AnemoneEnemy::Initialize(Model* model, Model* bulletModel, Player* player, float waterHeight)
{
    BaseEnemy::Initialize(model);
    InitializeHealth(8.0f);
    transform_.scale = { kDisplayScale, kDisplayScale, kDisplayScale };
    player_ = player;
    bulletModel_ = bulletModel;
    waterHeight_ = waterHeight;
    fireElapsedSeconds_ = 1.5f;
    object_->SetColor({ 1, 1, 1, 1 });
    object_->SetNormalMapStrength(0.0f);
    object_->SetSurfaceProperties(0.55f, 0.0f, 0.3f);
    localBounds_.clear();
    for (const auto& primitive : model->GetModelData().primitives) {
        if (primitive.vertices.empty()) { continue; }
        const float max = (std::numeric_limits<float>::max)();
        Vector3 minimum { max, max, max }, maximum { -max, -max, -max };
        for (const auto& vertex : primitive.vertices) {
            minimum.x = (std::min)(minimum.x, vertex.position.x);
            minimum.y = (std::min)(minimum.y, vertex.position.y);
            minimum.z = (std::min)(minimum.z, vertex.position.z);
            maximum.x = (std::max)(maximum.x, vertex.position.x);
            maximum.y = (std::max)(maximum.y, vertex.position.y);
            maximum.z = (std::max)(maximum.z, vertex.position.z);
        }
        localBounds_.push_back({ (minimum + maximum) * 0.5f, maximum - minimum });
    }
}

Vector3 AnemoneEnemy::GetPosition() const
{
    return MatrixMath::Transform({ 0, 3.5f, 0 },
        MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate));
}

Vector3 AnemoneEnemy::GetMuzzlePosition() const
{
    return MatrixMath::Transform({ 0, 3.9f, 0.7f },
        MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate));
}

void AnemoneEnemy::Update()
{
    if (IsDead()) { return; }
    if (player_ && Dot(transform_.translate - player_->GetTranslate(), player_->GetFlightForward()) < -60.0f) {
        SetDead(true);
        return;
    }
    BaseEnemy::Update();
}

bool AnemoneEnemy::AdvanceShotCycle(float deltaSeconds)
{
    if (!player_ || player_->IsDead() || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) { return false; }
    if (!AnemoneShotPattern::IsTargetInRange(GetMuzzlePosition(), player_->GetTranslate(),
        player_->GetFlightForward(), waterHeight_)) { return false; }
    return GameTiming::AdvanceInterval(fireElapsedSeconds_, AnemoneShotPattern::kFireIntervalSeconds,
        deltaSeconds, 1) != 0;
}

void AnemoneEnemy::Attack()
{
    if (!bulletModel_ || !AdvanceShotCycle(TimeManager::GetInstance()->GetDeltaTime())) { return; }
    const Vector3 muzzle = GetMuzzlePosition();
    const auto directions = AnemoneShotPattern::CalculateDirections(muzzle, player_->GetTranslate(),
        player_->GetAutomaticWorldVelocity());
    for (size_t index = 0; index < directions.size(); ++index) {
        auto bullet = std::make_unique<NormalEnemyBullet>();
        bullet->Initialize(bulletModel_);
        bullet->SetTranslate(muzzle);
        bullet->SetVelocity(directions[index] * AnemoneShotPattern::kBulletSpeed);
        bullet->SetColor({ 1.0f, 0.4f, 0.08f, 1.0f });
        bullet->SetScale({ 0.45f, 0.45f, 0.45f });
        AddEnemyBullet(std::move(bullet), index == 0);
    }
    Logger::Log("[AnemoneEnemy] Fired five-shot fan at z=" + std::to_string(transform_.translate.z));
}

void AnemoneEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (IsDead()) { return; }
    const Matrix4x4 world = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    for (size_t index = 0; index < localBounds_.size(); ++index) {
        const auto& bounds = localBounds_[index];
        EnemyCollisionPart part {};
        part.isBox = true;
        part.partIndex = static_cast<int32_t>(index);
        part.position = MatrixMath::Transform(bounds.center, world);
        part.box = CollisionManager::MakeOBB(part.position,
            { bounds.size.x * transform_.scale.x, bounds.size.y * transform_.scale.y,
              bounds.size.z * transform_.scale.z }, transform_.rotate);
        part.radius = Vector3Length(part.box.size) * 0.5f;
        parts.push_back(part);
    }
}
