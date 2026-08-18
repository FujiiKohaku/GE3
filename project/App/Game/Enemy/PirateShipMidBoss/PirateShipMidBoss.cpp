#include "PirateShipMidBoss.h"

#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

namespace {

float SmoothStep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

std::unique_ptr<Object3d> PirateShipMidBoss::CreatePart(
    Model* model, const Vector3& scale, const Vector4& color)
{
    auto part = std::make_unique<Object3d>();
    part->Initialize(Object3dManager::GetInstance());
    part->SetModel(model);
    part->SetScale(scale);
    part->SetColor(color);
    part->SetEnableLighting(true);
    return part;
}

void PirateShipMidBoss::Initialize(Model* cubeModel, Model* bulletModel, Player* player)
{
    player_ = player;
    bulletModel_ = bulletModel;
    hp_ = shipHp_;
    hull_ = CreatePart(cubeModel, { 14.0f, 4.0f, 28.0f }, { 0.12f, 0.055f, 0.025f, 1.0f });
    deck_ = CreatePart(cubeModel, { 12.0f, 1.5f, 19.0f }, { 0.28f, 0.12f, 0.045f, 1.0f });
    cabin_ = CreatePart(cubeModel, { 6.5f, 5.0f, 7.0f }, { 0.18f, 0.07f, 0.025f, 1.0f });
    mast_ = CreatePart(cubeModel, { 0.8f, 14.0f, 0.8f }, { 0.16f, 0.07f, 0.025f, 1.0f });
    sail_ = CreatePart(cubeModel, { 0.55f, 7.5f, 8.5f }, { 0.06f, 0.055f, 0.05f, 1.0f });
    leftCannons_ = CreatePart(cubeModel, { 4.0f, 1.2f, 9.0f }, { 0.06f, 0.07f, 0.075f, 1.0f });
    rightCannons_ = CreatePart(cubeModel, { 4.0f, 1.2f, 9.0f }, { 0.06f, 0.07f, 0.075f, 1.0f });
}

void PirateShipMidBoss::SetPosition(const Vector3& position)
{
    surfacePosition_ = position;
    shipPosition_ = position + Vector3 { 0.0f, -28.0f, 0.0f };
    UpdateParts();
}

void PirateShipMidBoss::Update()
{
    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    stateTimer_ += deltaTime;
    if (state_ == State::Emerging) {
        const float rise = SmoothStep(stateTimer_ / 3.2f);
        const float startX = surfacePosition_.x - 62.0f;
        const float endX = surfacePosition_.x + 30.0f;
        shipPosition_.x = startX + (endX - startX) * rise;
        shipPosition_.y = surfacePosition_.y - 25.0f + rise * 27.0f;
        if (player_ != nullptr) {
            const float startZ = player_->GetTranslate().z - 68.0f;
            const float endZ = player_->GetTranslate().z + 92.0f;
            shipPosition_.z = startZ + (endZ - startZ) * rise;
        }
        if (stateTimer_ >= 3.2f) {
            state_ = State::Battle;
            stateTimer_ = 0.0f;
        }
    } else if (state_ == State::Battle) {
        battleTimer_ += deltaTime;
        shotTimer_ += deltaTime;
        if (player_ != nullptr) {
            const Vector3 playerPosition = player_->GetTranslate();
            const float targetZ = playerPosition.z + 92.0f;
            shipPosition_.z += (targetZ - shipPosition_.z) * 0.055f;
        }
        shipPosition_.x += horizontalVelocity_ * deltaTime;
        const float minX = surfacePosition_.x - 65.0f;
        const float maxX = surfacePosition_.x + 65.0f;
        if (shipPosition_.x <= minX) {
            shipPosition_.x = minX;
            horizontalVelocity_ = std::abs(horizontalVelocity_);
        } else if (shipPosition_.x >= maxX) {
            shipPosition_.x = maxX;
            horizontalVelocity_ = -std::abs(horizontalVelocity_);
        }
        const float targetY = surfacePosition_.y + 2.0f + std::sin(battleTimer_ * 1.35f) * 0.7f;
        shipPosition_.y += (targetY - shipPosition_.y) * 0.08f;
        shipPosition_.y = std::clamp(
            shipPosition_.y,
            surfacePosition_.y,
            surfacePosition_.y + 5.0f);
        if (shotTimer_ >= 1.65f) {
            shotTimer_ = 0.0f;
            FireCannons();
        }
    } else {
        shipPosition_.y -= 7.0f * deltaTime;
        shipPosition_.z += 5.0f * deltaTime;
        shipPosition_.x += 1.5f * deltaTime;
        if (stateTimer_ >= 3.0f) SetDead(true);
    }

    UpdateParts();
}

void PirateShipMidBoss::FireCannons()
{
    if (player_ == nullptr || bulletModel_ == nullptr) return;
    const Vector3 target = player_->GetTranslate();
    for (int32_t index = -1; index <= 1; ++index) {
        const Vector3 muzzle = shipPosition_ + Vector3 { -13.5f, 3.0f, static_cast<float>(index) * 7.0f };
        Vector3 direction = Normalize(target - muzzle + Vector3 { 0.0f, static_cast<float>(index) * 0.5f, 0.0f });
        auto bullet = std::make_unique<NormalEnemyBullet>();
        bullet->Initialize(bulletModel_);
        bullet->SetTranslate(muzzle);
        bullet->SetVelocity(direction * 0.72f);
        bullet->SetColor({ 0.95f, 0.45f, 0.08f, 1.0f });
        bullet->SetDamage(1);
        AddEnemyBullet(std::move(bullet));
        EffectManager::GetInstance()->PlayEffect("NormalBulletImpactFlash", muzzle);
    }
}

void PirateShipMidBoss::UpdateParts()
{
    const float roll = state_ == State::Sinking
        ? (std::min)(stateTimer_ * 0.22f, 0.65f)
        : std::sin(battleTimer_ * 1.6f) * 0.025f;
    auto updatePart = [roll](Object3d* part, const Vector3& position) {
        part->SetTranslate(position);
        part->SetRotate({ 0.0f, 0.0f, roll });
        part->Update();
    };
    updatePart(hull_.get(), shipPosition_);
    updatePart(deck_.get(), shipPosition_ + Vector3 { 0.0f, 4.7f, 1.0f });
    updatePart(cabin_.get(), shipPosition_ + Vector3 { 0.0f, 8.0f, 8.0f });
    updatePart(mast_.get(), shipPosition_ + Vector3 { 0.0f, 14.0f, -3.0f });
    updatePart(sail_.get(), shipPosition_ + Vector3 { 0.0f, 16.0f, -2.0f });
    updatePart(leftCannons_.get(), shipPosition_ + Vector3 { -13.0f, 5.0f, 0.0f });
    updatePart(rightCannons_.get(), shipPosition_ + Vector3 { 13.0f, 5.0f, 0.0f });
}

void PirateShipMidBoss::Draw()
{
    hull_->Draw();
    deck_->Draw();
    cabin_->Draw();
    mast_->Draw();
    sail_->Draw();
    leftCannons_->Draw();
    rightCannons_->Draw();
}

void PirateShipMidBoss::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    parts.clear();
    if (state_ != State::Battle) return;
    parts.push_back({ shipPosition_, 14.0f, 0 });
    parts.push_back({ shipPosition_ + Vector3 { 0.0f, 8.0f, 8.0f }, 6.0f, 1 });
}

bool PirateShipMidBoss::IsCollisionPartDamageable(int32_t) const
{
    return state_ == State::Battle;
}

void PirateShipMidBoss::ApplyDamageToPart(int32_t, float damage)
{
    if (state_ != State::Battle) return;
    shipHp_ = (std::max)(0.0f, shipHp_ - damage);
    hp_ = shipHp_;
    if (shipHp_ <= 0.0f) {
        state_ = State::Sinking;
        stateTimer_ = 0.0f;
        EffectManager::GetInstance()->PlayEffect("Explosion", shipPosition_ + Vector3 { 0.0f, 7.0f, 0.0f });
    }
}
