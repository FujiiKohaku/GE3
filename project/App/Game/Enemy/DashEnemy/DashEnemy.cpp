#include "DashEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/math/MatrixMath.h"
#include "Engine/Logger/Logger.h"

namespace {
Matrix4x4 LineWorld(const Vector3& start, const Vector3& direction, float length, float width)
{
    const Vector3 axis = std::abs(direction.y) > 0.99f ? Vector3 { 1, 0, 0 } : Vector3 { 0, 1, 0 };
    const Vector3 right = Normalize(Cross(axis, direction));
    const Vector3 up = Normalize(Cross(direction, right));
    const Vector3 axes[] { right * width, up * width, direction * length };
    Matrix4x4 world = MatrixMath::MakeIdentity4x4();
    for (int index = 0; index < 3; ++index) {
        world.m[index][0] = axes[index].x;
        world.m[index][1] = axes[index].y;
        world.m[index][2] = axes[index].z;
    }
    const Vector3 center = start + direction * (length * 0.5f);
    world.m[3][0] = center.x; world.m[3][1] = center.y; world.m[3][2] = center.z;
    return world;
}
}

void DashEnemy::Initialize(Model* cube, Player* player, const LevelData::DashEnemySpawnData& settings)
{
    BaseEnemy::Initialize(cube);
    InitializeHealth(settings.hp);
    player_ = player;
    motion_.Reset({}, settings);
    transform_.scale = { kBodySize, kBodySize, kBodySize };
    object_->SetEnableLighting(false);
    object_->SetNormalMapStrength(0);
    for (size_t index = 0; index < warningObjects_.size() + 1; ++index) {
        auto line = std::make_unique<Object3d>();
        line->Initialize(Object3dManager::GetInstance());
        line->SetModel(cube);
        line->SetPixelShaderPath("resources/Shaders/Object3D/DashWarning/Render.PS.hlsl");
        line->SetEnableLighting(false);
        line->SetTransparent(true);
        line->SetTransparentDepthWrite(false);
        line->SetCastShadow(false);
        line->SetReceiveShadow(false);
        line->SetFogEnabled(false);
        if (index == warningObjects_.size()) { trail_ = std::move(line); }
        else { warningObjects_[index] = std::move(line); }
    }
}

void DashEnemy::SetPosition(const Vector3& position)
{
    BaseEnemy::SetPosition(position);
    motion_.Reset(position, motion_.settings);
    hasContactedPlayer_ = false;
    hasPreviousOrigin_ = false;
    warningLength_ = motion_.settings.dashDistance;
}

void DashEnemy::Update()
{
    if (IsDead() || !player_) { return; }
    if (player_->IsDead()) { motion_.segmentCount = 0; return; }
    const float seconds = TimeManager::GetInstance()->GetDeltaTime();
    DashTarget target;
    target.position = player_->GetTranslate();
    target.velocity = player_->GetAutomaticWorldVelocity() * 60.0f;
    target.forward = player_->GetFlightForward();
    if (Vector3Length(target.forward) < 0.001f) { target.forward = { 0, 0, 1 }; }
    target.forward = Normalize(target.forward);
    const Vector3 axis = std::abs(target.forward.y) > 0.99f ? Vector3 { 1, 0, 0 } : Vector3 { 0, 1, 0 };
    target.right = Normalize(Cross(axis, target.forward));
    target.up = Normalize(Cross(target.forward, target.right));
    const Vector3 offset = player_->GetRailOffset();
    target.origin = target.position - target.right * offset.x - target.up * offset.y - target.forward * offset.z;
    if (player_->IsAllRangeMode()) { target.origin = target.position; }
    if (hasPreviousOrigin_ && Vector3Length(target.origin - previousOrigin_) > 300.0f) { SetDead(true); return; }
    previousOrigin_ = target.origin;
    hasPreviousOrigin_ = true;
    const DashPhase previous = motion_.phase;
    motion_.Advance(seconds, target);
    if (motion_.phase != previous) {
        Logger::Log("[DASHENEMY] Phase=" + std::to_string(static_cast<int>(motion_.phase)));
    }
    if (motion_.phase == DashPhase::Finished) { SetDead(true); return; }
    UpdateVisuals();
}

void DashEnemy::UpdateVisuals()
{
    transform_.translate = motion_.position;
    if (!object_) { return; }
    Vector4 color { 0.70f, 0.78f, 0.88f, 1 };
    if (motion_.phase == DashPhase::Locked) { color = { 1, 0.20f, 0.16f, 1 }; }
    if (motion_.phase == DashPhase::Dash) { color = { 1, 0.04f, 0.02f, 1 }; }
    object_->SetColor(color);
    object_->SetScale({ kBodySize, kBodySize, kBodySize });
    object_->SetTranslate(motion_.position);
    object_->Update();
    if (motion_.IsWarning()) {
        const Vector3 axis = std::abs(motion_.direction.y) > 0.99f ? Vector3 { 1, 0, 0 } : Vector3 { 0, 1, 0 };
        const Vector3 right = Normalize(Cross(axis, motion_.direction));
        for (size_t index = 0; index < warningObjects_.size(); ++index) {
            const float lateral = index == 0 ? 0.0f : index == 1 ? -3.3f : 3.3f;
            warningObjects_[index]->SetCustomWorldMatrix(LineWorld(motion_.position + right * lateral,
                motion_.direction, warningLength_, index == 0 ? 0.28f : 0.10f));
            const float alpha = index == 0 ? (motion_.phase == DashPhase::Locked ? 0.95f : 0.5f) : 0.32f;
            warningObjects_[index]->SetColor({ 1, 0, 0, alpha });
            warningObjects_[index]->Update();
        }
    }
    if (motion_.phase == DashPhase::Dash) {
        const float length = (std::min)(12.0f, motion_.elapsed * motion_.settings.dashSpeed);
        trail_->SetCustomWorldMatrix(LineWorld(motion_.position - motion_.direction * length,
            motion_.direction, length, 1.5f));
        trail_->SetColor({ 1, 0, 0, 0.22f });
        trail_->Update();
    }
}

void DashEnemy::StopAtObstacle(size_t segment, float fraction)
{
    motion_.StopAtObstacle(segment, fraction);
    Logger::Log("[DASHENEMY] Stopped at obstacle");
    UpdateVisuals();
}

void DashEnemy::SetWarningLength(float length)
{
    warningLength_ = std::clamp(length, 0.0f, motion_.settings.dashDistance);
    UpdateVisuals();
}

void DashEnemy::Draw()
{
    if (IsDead() || !motion_.IsVisible() || (player_ && player_->IsDead())) { return; }
    BaseEnemy::Draw();
    if (motion_.IsWarning() && warningLength_ > 0.001f) {
        for (const auto& line : warningObjects_) { line->Draw(); }
    }
    if (motion_.phase == DashPhase::Dash && motion_.elapsed > 0.001f) { trail_->Draw(); }
}

void DashEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (motion_.IsVisible()) { BaseEnemy::DrawShadow(renderer); }
}

void DashEnemy::SubmitRaytracing(DxrRenderer& renderer)
{
    if (motion_.IsVisible()) { BaseEnemy::SubmitRaytracing(renderer); }
}

void DashEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (IsDead() || !motion_.IsVisible()) { return; }
    EnemyCollisionPart part {};
    part.isBox = true;
    part.position = motion_.position;
    part.box = CollisionManager::MakeOBB(part.position, { kBodySize, kBodySize, kBodySize }, {});
    part.radius = kBodySize * 0.8660254f;
    parts.push_back(part);
}

SweepHit DashEnemy::SweepBullet(const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const
{
    if (IsDead() || !motion_.IsVisible()) { return {}; }
    partIndex = 0;
    SweepHit nearest {};
    if (motion_.segmentCount == 0) {
        return CollisionManager::SweepSphere(bullet, movement,
            CollisionManager::MakeOBB(motion_.position, { kBodySize, kBodySize, kBodySize }, {}));
    }
    for (size_t index = 0; index < motion_.segmentCount; ++index) {
        const auto& segment = motion_.segments[index];
        const float duration = segment.finish - segment.begin;
        if (duration <= 0) { continue; }
        const Vector3 enemyMovement = segment.end - segment.start;
        const Sphere start { bullet.center + movement * segment.begin, bullet.radius };
        SweepHit hit = CollisionManager::SweepSphere(start, movement * duration - enemyMovement,
            CollisionManager::MakeOBB(segment.start, { kBodySize, kBodySize, kBodySize }, {}));
        if (!hit.isHit) { continue; }
        hit.position += enemyMovement * hit.time;
        hit.time = segment.begin + duration * hit.time;
        if (!nearest.isHit || hit.time < nearest.time) { nearest = hit; }
    }
    return nearest;
}

SweepHit DashEnemy::FindPlayerContact(const Vector3& previous, const Vector3& current) const
{
    if (IsDead() || hasContactedPlayer_) { return {}; }
    const Vector3 playerMovement = current - previous;
    for (size_t index = 0; index < motion_.segmentCount; ++index) {
        const auto& segment = motion_.segments[index];
        if (!segment.attacking || segment.finish <= segment.begin) { continue; }
        const float duration = segment.finish - segment.begin;
        const Vector3 enemyMovement = segment.end - segment.start;
        const Sphere target { previous + playerMovement * segment.begin, 1.0f };
        SweepHit hit = CollisionManager::SweepSphere(Sphere { segment.start, kAttackRadius },
            enemyMovement - playerMovement * duration, target);
        if (hit.isHit) {
            hit.position = segment.start + enemyMovement * hit.time;
            hit.time = segment.begin + duration * hit.time;
            return hit;
        }
    }
    return {};
}
