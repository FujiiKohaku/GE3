#include "PaceEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Logger/Logger.h"

namespace {
constexpr float kPassedDistance = 30.0f;
constexpr float kTeleportDistance = 240.0f;
}

void PaceEnemy::Initialize(Model* model, Model* bulletModel, Player* player)
{
    transform_ = {};
    NormalEnemy::Initialize(model, bulletModel, player);
    constexpr float kDisplayScale = 4.0f;
    transform_.scale = { kDisplayScale, kDisplayScale, kDisplayScale };
    object_->SetScale(transform_.scale);
    SetRadialShotEnabled(false);
    player_ = player;
    settings_ = {};
    elapsedSeconds_ = 0.0f;
    hasStartedFlight_ = false;
    isPacing_ = false;
}

void PaceEnemy::SetPosition(const Vector3& position)
{
    BaseEnemy::SetPosition(position);
    anchorPosition_ = position;
    elapsedSeconds_ = 0.0f;
    hasStartedFlight_ = false;
    isPacing_ = false;
}

void PaceEnemy::ConfigureFlight(const LevelData::PaceEnemySpawnData& settings)
{
    settings_ = settings;
    if (!PaceEnemyPath::IsValidSettings(settings_)) {
        Logger::Warning("Invalid PaceEnemy flight settings; using defaults");
        settings_ = {};
    }
}

void PaceEnemy::UpdateRailFrame()
{
    railForward_ = Normalize(player_->GetFlightForward());
    if (IsNearlyZero(railForward_)) { railForward_ = { 0.0f, 0.0f, 1.0f }; }
    railRight_ = Normalize(Cross({ 0.0f, 1.0f, 0.0f }, railForward_));
    if (IsNearlyZero(railRight_)) { railRight_ = { 1.0f, 0.0f, 0.0f }; }
    railUp_ = Normalize(Cross(railForward_, railRight_));
    const Vector3& railOffset = player_->GetRailOffset();
    // 自機の手動移動を除き、レールの前進と曲がりに追従する。
    railOrigin_ = player_->GetTranslate() - railRight_ * railOffset.x -
        railUp_ * railOffset.y - railForward_ * railOffset.z;
}

void PaceEnemy::Update()
{
    if (IsDead() || player_ == nullptr || player_->IsDead()) { return; }
    const float deltaSeconds = TimeManager::GetInstance()->GetDeltaTime();
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) { return; }
    const Vector3 previousRailOrigin = railOrigin_;
    UpdateRailFrame();
    if (!hasStartedFlight_) {
        const float forwardDistance = Dot(anchorPosition_ - railOrigin_, railForward_);
        if (forwardDistance < -kPassedDistance) { SetDead(true); return; }
        if (forwardDistance > settings_.activationDistance) { return; }
        hasStartedFlight_ = true;
    } else if (Vector3Length(railOrigin_ - previousRailOrigin) > kTeleportDistance) {
        SetDead(true);
        return;
    }
    elapsedSeconds_ += deltaSeconds;
    const PaceEnemyPathSample sample = PaceEnemyPath::Sample(elapsedSeconds_, settings_);
    if (sample.hasFinished) { SetDead(true); return; }
    isPacing_ = sample.isPacing;
    flightOffset_ = sample.position;
    NormalEnemy::Update();
}

void PaceEnemy::Move()
{
    transform_.translate = railOrigin_ + railRight_ * flightOffset_.x +
        railUp_ * flightOffset_.y + railForward_ * flightOffset_.z;
}

void PaceEnemy::Attack()
{
    if (isPacing_) { NormalEnemy::Attack(); }
}

void PaceEnemy::Draw()
{
    if (hasStartedFlight_) { BaseEnemy::Draw(); }
}

void PaceEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (hasStartedFlight_) { BaseEnemy::DrawShadow(renderer); }
}

void PaceEnemy::SubmitRaytracing(DxrRenderer& renderer)
{
    if (hasStartedFlight_) { BaseEnemy::SubmitRaytracing(renderer); }
}

void PaceEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (!hasStartedFlight_ || IsDead()) { return; }
    EnemyCollisionPart part;
    part.position = transform_.translate;
    // 元のCUBEは一辺1。表示スケールがそのまま箱の各辺になる。
    const Vector3 size { std::abs(transform_.scale.x), std::abs(transform_.scale.y), std::abs(transform_.scale.z) };
    part.isBox = true;
    part.box = CollisionManager::MakeOBB(transform_.translate, size, transform_.rotate);
    // カメラ付近かの判定などに使う包絡球。命中判定にはboxを使う。
    part.radius = Vector3Length(size) * 0.5f;
    parts.push_back(part);
}
