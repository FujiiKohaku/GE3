#include "App/Game/Collision/GameplayCollisionSystem.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "App/Game/Enemy/PaceEnemy/PaceEnemy.h"
#include "App/Game/Enemy/StrafeEnemy/StrafeEnemy.h"
#include "App/Game/Enemy/TurretEnemy/TurretEnemyAim.h"
#include "App/Game/Enemy/TurretEnemy/TurretShotFeedback.h"
#include "App/Game/Enemy/Bullet/Types/TurretEnemyBullet.h"
#include "App/Game/Hazard/WaterPillarHazard.h"
#include "App/Game/Hazard/ElectricGate.h"
#include "Tests/PlayerTestAccess.h"
#include "App/Game/Player/Combat/Bullet/Types/MissileBullet.h"
#include "App/Game/Player/Combat/Bullet/Types/HomingMissileBullet.h"
#include "Engine/3D/Object3d.h"
#include "Engine/CollisionManager/BoxCollider.h"
#include "Engine/Serialization/JsonFile.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>
#include <limits>

class TestTurretEnemyBullet : public TurretEnemyBullet {
public:
    Vector3 GetLaunchVelocity() const { return velocity_; }
    Vector3 GetDisplayScale() const { return transform_.scale; }
};

bool RunTurretEnemyBulletTests()
{
    TestTurretEnemyBullet bullet;
    bullet.SetLaunchDirection({ 0.0f, 0.0f, 5.0f }, 0.48f);
    if (Vector3Length(bullet.GetLaunchVelocity() - Vector3 { 0.0f, 0.0f, 2.4f }) > 0.0001f ||
        Vector3Length(bullet.GetDisplayScale() - Vector3 { 0.45f, 0.45f, 0.45f }) > 0.0001f ||
        bullet.GetCollisionRadius() != 2.25f || bullet.GetDamage() != 1) { return false; }
    bullet.SetLaunchDirection({ -3.0f, 4.0f, 0.0f }, 0.48f);
    if (std::abs(Vector3Length(bullet.GetLaunchVelocity()) - 2.4f) > 0.0001f ||
        bullet.GetLaunchVelocity().x >= 0.0f || bullet.GetLaunchVelocity().y <= 0.0f) { return false; }
    bullet.SetLaunchDirection({}, 0.48f);
    if (Vector3Length(bullet.GetLaunchVelocity()) != 0.0f) { return false; }
    bullet.SetLaunchDirection({ 0.0f, 0.0f, 1.0f }, std::numeric_limits<float>::quiet_NaN());
    if (Vector3Length(bullet.GetLaunchVelocity()) != 0.0f) { return false; }
    // 両端では重ならない高速移動も、実際の敵弾と同じ半径・区間判定で拾える。
    const Sphere playerSphere { {}, 1.0f };
    const Sphere start { { 2.0f, 0.0f, -20.0f }, bullet.GetCollisionRadius() * 0.5f };
    const Vector3 movement { 0.0f, 0.0f, 40.0f };
    const SweepHit hit = CollisionManager::SweepSphere(start, movement, playerSphere);
    if (CollisionManager::Intersect(start, playerSphere).isHit ||
        CollisionManager::Intersect(Sphere { start.center + movement, start.radius }, playerSphere).isHit ||
        !hit.isHit || hit.time <= 0.0f || hit.time >= 1.0f) { return false; }
    return !CollisionManager::SweepSphere(Sphere { { 2.3f, 0.0f, -20.0f }, start.radius }, movement, playerSphere).isHit;
}

struct StrafeEnemyTestAccess {
    static void Configure(StrafeEnemy& enemy, bool isActive)
    {
        enemy.transform_.translate = { 0.0f, 2.0f, 10.0f };
        enemy.transform_.scale = { 4.0f, 2.5f, 4.0f };
        enemy.hasStartedFlight_ = isActive;
    }
};

bool RunStrafeEnemyCollisionTests()
{
    StrafeEnemy enemy;
    StrafeEnemyTestAccess::Configure(enemy, false);
    std::vector<EnemyCollisionPart> parts;
    enemy.GetCollisionParts(parts);
    if (!parts.empty()) { return false; }
    StrafeEnemyTestAccess::Configure(enemy, true);
    enemy.GetCollisionParts(parts);
    if (parts.size() != 1 || !parts[0].isBox || parts[0].box.size.y != 2.5f) { return false; }
    int32_t partIndex = -1;
    const Vector3 movement { 0.0f, 0.0f, 20.0f };
    if (!enemy.SweepBullet(Sphere { { 1.9f, 2.0f, 0.0f }, 0.1f }, movement, partIndex).isHit ||
        enemy.SweepBullet(Sphere { { 2.2f, 2.0f, 0.0f }, 0.1f }, movement, partIndex).isHit) { return false; }
    enemy.SetDead(true);
    parts.clear();
    enemy.GetCollisionParts(parts);
    return parts.empty();
}

struct PaceEnemyTestAccess {
    static void Configure(PaceEnemy& enemy, const Vector3& scale, const Vector3& rotation = {}, bool isActive = true)
    {
        enemy.transform_.translate = { 0.0f, 0.0f, 10.0f };
        enemy.transform_.scale = scale;
        enemy.transform_.rotate = rotation;
        enemy.hasStartedFlight_ = isActive;
        enemy.SetHealth(10.0f);
    }
    static float GetHealth(const PaceEnemy& enemy) { return enemy.GetHealth(); }
};

bool RunTurretShotFeedbackTests()
{
    for (int frameRate : { 30, 60, 120 }) {
        TurretShotCycle cycle;
        int firstShotFrame = -1;
        int secondShotFrame = -1;
        for (int frame = 1; frame <= frameRate * 3; ++frame) {
            if (cycle.Advance(true, true, 1.0f / frameRate, 2.0f)) {
                if (firstShotFrame < 0) { firstShotFrame = frame; }
                else if (secondShotFrame < 0) { secondShotFrame = frame; }
                else { return false; }
            }
        }
        const float firstSeconds = static_cast<float>(firstShotFrame) / frameRate;
        const float intervalSeconds = static_cast<float>(secondShotFrame - firstShotFrame) / frameRate;
        if (firstShotFrame < 0 || secondShotFrame < 0 || firstSeconds < 0.349f ||
            firstSeconds > 0.351f + 1.0f / frameRate || std::abs(intervalSeconds - 2.0f) > 0.001f + 1.0f / frameRate) { return false; }
        cycle.Advance(false, false, 0.1f, 2.0f);
        if (cycle.Advance(true, false, 1.0f, 2.0f) || cycle.Advance(true, true, 0.2f, 2.0f) ||
            !cycle.Advance(true, true, 0.15f, 2.0f)) { return false; }
        if (cycle.Advance(true, false, 3.0f, 2.0f) || !cycle.Advance(true, true, 0.01f, 2.0f) ||
            cycle.Advance(true, true, 0.01f, 2.0f)) { return false; }
    }
    if (TurretShotFeedback::CalculateRecoil(0.0f) != 0.0f ||
        std::abs(TurretShotFeedback::CalculateRecoil(TurretShotFeedback::kRecoilKickSeconds) - 0.2f) > 0.0001f ||
        TurretShotFeedback::CalculateRecoil(TurretShotFeedback::kRecoilDurationSeconds) != 0.0f) { return false; }
    EulerTransform transform {};
    transform.scale = { 3.5f, 3.5f, 3.5f };
    transform.rotate = { 0.0f, 0.6f, 0.0f };
    const auto restPose = TurretEnemyAim::CalculatePose(transform, 1.1f, -0.2f);
    const auto recoilPose = TurretEnemyAim::CalculatePose(transform, 1.1f, -0.2f, 0.2f);
    if (Vector3Length(recoilPose.muzzlePosition - (restPose.muzzlePosition - restPose.forward * 0.7f)) > 0.001f ||
        Vector3Length(restPose.forward - recoilPose.forward) > 0.001f) { return false; }
    for (const Vector3& direction : { Vector3 { 0.0f, 0.0f, 1.0f }, Vector3 { 0.0f, 1.0f, 0.0f }, Vector3 { -1.0f, 0.0f, 0.0f } }) {
        const Vector3 head { 8.0f, -2.0f, 12.0f };
        const auto matrix = TurretShotFeedback::CalculateTrailWorld(head, direction, 8.0f, 0.9f);
        if (Vector3Length(MatrixMath::Transform(Vector3 {}, matrix) - (head - direction * 8.0f)) > 0.001f ||
            Vector3Length(MatrixMath::Transform(Vector3 { 0.0f, 0.0f, 1.0f }, matrix) - head) > 0.001f) { return false; }
    }
    return true;
}

bool RunTurretEnemyAimTests()
{
    EulerTransform transform {};
    transform.scale = { 1.0f, 1.0f, 1.0f };
    auto pose = TurretEnemyAim::CalculatePose(transform, 0.0f, 0.0f);
    if (Vector3Length(pose.muzzlePosition - Vector3 { 0.0f, 1.15f, 2.28f }) > 0.001f ||
        Vector3Length(pose.forward - Vector3 { 0.0f, 0.0f, 1.0f }) > 0.001f) { return false; }
    pose = TurretEnemyAim::CalculatePose(transform, std::numbers::pi_v<float>, -0.3f);
    const Vector3 barrelPivot = MatrixMath::Transform(Vector3 {}, pose.barrelWorld);
    if (Vector3Length(barrelPivot - Vector3 { 0.0f, 1.12f, 0.0f }) > 0.001f ||
        pose.forward.y <= 0.0f || pose.forward.z >= 0.0f) { return false; }
    transform.scale = { 3.5f, 3.5f, 3.5f };
    transform.translate = { 5.0f, -29.5f, 300.0f };
    transform.rotate = { 0.0f, 0.7f, 0.0f };
    pose = TurretEnemyAim::CalculatePose(transform, 0.0f, 0.0f);
    const Vector3 playerPosition { -8.0f, -20.0f, 210.0f };
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
    if (!TurretEnemyAim::CalculateTargetAngles(pose.baseWorld, playerPosition, yawRadians, pitchRadians)) { return false; }
    pose = TurretEnemyAim::CalculatePose(transform, yawRadians, pitchRadians);
    if (Dot(Normalize(playerPosition - pose.muzzlePosition), pose.forward) < 0.9999f ||
        Vector3Length(MatrixMath::Transform(Vector3 {}, pose.baseWorld) - transform.translate) > 0.001f) { return false; }
    for (int frameRate : { 30, 60, 120 }) {
        float angleRadians = 0.0f;
        for (int frame = 0; frame < frameRate; ++frame) {
            angleRadians = TurretEnemyAim::ApproachAngle(angleRadians, std::numbers::pi_v<float>, 1.0f / frameRate);
        }
        if (std::abs(std::abs(angleRadians) - std::numbers::pi_v<float> * 0.5f) > 0.001f) { return false; }
    }
    constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0f;
    const float wrapped = TurretEnemyAim::ApproachAngle(179.0f * kDegreesToRadians, -179.0f * kDegreesToRadians, 1.0f);
    if (std::abs(wrapped + 179.0f * kDegreesToRadians) > 0.001f) { return false; }
    const Vector3 highTarget = MatrixMath::Transform(Vector3 { 0.0f, 1000.0f, 1.0f }, pose.baseWorld);
    TurretEnemyAim::CalculateTargetAngles(pose.baseWorld, highTarget, yawRadians, pitchRadians);
    if (pitchRadians != TurretEnemyAim::kMinimumPitchRadians) { return false; }
    return true;
}

bool RunPaceEnemyBoxTests()
{
    PaceEnemy enemy;
    PaceEnemyTestAccess::Configure(enemy, { 4.0f, 4.0f, 4.0f });
    std::vector<EnemyCollisionPart> parts;
    enemy.GetCollisionParts(parts);
    if (parts.size() != 1 || !parts[0].isBox || parts[0].box.size.x != 4.0f) { return false; }
    if (parts[0].IntersectSphere(Sphere { { 2.5f, 0.0f, 10.0f }, 0.1f }).isHit ||
        !parts[0].IntersectSphere(Sphere { { 2.05f, 0.0f, 10.0f }, 0.1f }).isHit ||
        std::abs(parts[0].GetSurfaceDistance({ 2.3f, 2.4f, 10.0f }) - 0.5f) > 0.001f) { return false; }
    int32_t partIndex = -1;
    const Vector3 movement { 0.0f, 0.0f, 20.0f };
    if (enemy.SweepBullet(Sphere { { 2.5f, 0.0f, 0.0f }, 0.1f }, movement, partIndex).isHit ||
        !enemy.SweepBullet(Sphere { { 1.9f, 1.9f, 0.0f }, 0.1f }, movement, partIndex).isHit || partIndex != 0) { return false; }
    CollisionManager* manager = CollisionManager::GetInstance();
    manager->ClearRaycastSphereTargets();
    manager->ClearRaycastObbTargets();
    CollisionObjectId nextObjectId = 1;
    enemy.RegisterRaycastTargets(nextObjectId);
    RaycastHit rayHit;
    const bool hasOutsideHit = manager->Raycast(Ray { { 2.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, rayHit);
    const bool hasCornerHit = manager->Raycast(Ray { { 1.9f, 1.9f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, rayHit);
    manager->ClearRaycastObbTargets();
    if (hasOutsideHit || !hasCornerHit || nextObjectId != 2) { return false; }
    // 非一様なスケールと回転にも、同じ表示サイズから判定を追従させる。
    constexpr float kQuarterTurnRadians = 1.5707963268f;
    PaceEnemyTestAccess::Configure(enemy, { 8.0f, 4.0f, 2.0f }, { 0.0f, 0.0f, kQuarterTurnRadians });
    if (enemy.SweepBullet(Sphere { { 3.0f, 0.0f, 0.0f }, 0.1f }, movement, partIndex).isHit ||
        !enemy.SweepBullet(Sphere { { 0.0f, 3.0f, 0.0f }, 0.1f }, movement, partIndex).isHit) { return false; }
    PaceEnemyTestAccess::Configure(enemy, { 4.0f, 4.0f, 4.0f }, {}, false);
    parts.clear();
    enemy.GetCollisionParts(parts);
    if (!parts.empty() || enemy.SweepBullet(Sphere { {}, 0.1f }, movement, partIndex).isHit) { return false; }
    PaceEnemyTestAccess::Configure(enemy, { 4.0f, 4.0f, 4.0f });
    enemy.SetDead(true);
    parts.clear();
    enemy.GetCollisionParts(parts);
    return parts.empty();
}

// 描画資源を使わず、実際のBaseEnemy::SweepBulletで部位判定を行う。
class CollisionTestEnemy final : public BaseEnemy {
public:
    explicit CollisionTestEnemy(float positionZ) {
        parts_.push_back({ { 0.0f, 0.0f, positionZ }, 1.0f, 0 });
    }
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override {
        parts.insert(parts.end(), parts_.begin(), parts_.end());
    }
    void AddPart(float positionZ, int32_t partIndex) {
        parts_.push_back({ { 0.0f, 0.0f, positionZ }, 1.0f, partIndex });
    }
    bool IsCollisionPartDamageable(int32_t) const override { return false; }
private:
    std::vector<EnemyCollisionPart> parts_;
};

class ExplosionTestEnemy final : public BaseEnemy {
public:
    ExplosionTestEnemy(float positionZ, float health, bool isGuarded = false)
    {
        SetHealth(health);
        parts_.push_back({ { 0.0f, 0.0f, positionZ }, 1.0f, 0 });
        if (isGuarded) { guardedPartIndex_ = 0; }
    }
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override
    {
        parts.insert(parts.end(), parts_.begin(), parts_.end());
    }
    void AddPart(float positionZ, int32_t partIndex)
    {
        parts_.push_back({ { 0.0f, 0.0f, positionZ }, 1.0f, partIndex });
    }
    bool IsCollisionPartDamageable(int32_t partIndex) const override { return partIndex != guardedPartIndex_; }
    void OnBulletHit(int32_t partIndex, float damage, const Vector3& position) override
    {
        ++hitCount_;
        lastPartIndex_ = partIndex;
        BaseEnemy::OnBulletHit(partIndex, damage, position);
    }
    float GetRemainingHealth() const { return GetHealth(); }
    int GetHitCount() const { return hitCount_; }
    int32_t GetLastPartIndex() const { return lastPartIndex_; }
protected:
    void OnDeath() override {}
private:
    std::vector<EnemyCollisionPart> parts_;
    int32_t guardedPartIndex_ = -1;
    int32_t lastPartIndex_ = -1;
    int hitCount_ = 0;
};

bool RunElectricGateCollisionTests()
{
    const Vector3 gatePosition { 12.0f, -34.0f, 400.0f };
    constexpr float kScale = 5.0f;
    constexpr float kPlayerRadius = 1.0f;
    const float arcHeights[] = { -23.0f, -16.25f };
    for (float height : arcHeights) {
        const Vector3 start { gatePosition.x, height, 380.0f };
        const Vector3 end { gatePosition.x, height, 420.0f };
        // 両端では接触しない大きな移動でも、放電を横切った場合は検出する。
        if (ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, kScale, start, start, kPlayerRadius) ||
            ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, kScale, end, end, kPlayerRadius) ||
            !ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, kScale, start, end, kPlayerRadius) ||
            !ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, kScale, end, start, kPlayerRadius)) { return false; }
    }
    const Vector3 safeStart { gatePosition.x, -19.625f, 380.0f };
    const Vector3 safeEnd { gatePosition.x, -19.625f, 420.0f };
    if (ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, kScale, safeStart, safeEnd, kPlayerRadius)) { return false; }
    const Vector3 edge { gatePosition.x + 11.2f, -23.0f, 400.0f };
    const Vector3 outside { gatePosition.x + 11.3f, -23.0f, 400.0f };
    if (!ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, kScale, edge, edge, kPlayerRadius) ||
        ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, kScale, outside, outside, kPlayerRadius)) { return false; }
    constexpr float kQuarterTurn = 1.57079632679f;
    const Vector3 rotatedStart { -8.0f, -23.0f, 400.0f };
    const Vector3 rotatedEnd { 32.0f, -23.0f, 400.0f };
    if (!ElectricGate::CheckDischargeCollision(gatePosition, kQuarterTurn, kScale, rotatedStart, rotatedEnd, kPlayerRadius)) { return false; }
    const Vector3 rotatedOutsideStart { -8.0f, -23.0f, 415.0f };
    const Vector3 rotatedOutsideEnd { 32.0f, -23.0f, 415.0f };
    if (ElectricGate::CheckDischargeCollision(gatePosition, kQuarterTurn, kScale, rotatedOutsideStart, rotatedOutsideEnd, kPlayerRadius) ||
        ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, 0.0f, edge, edge, kPlayerRadius) ||
        ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, kScale, edge, edge, -1.0f) ||
        ElectricGate::CheckDischargeCollision(gatePosition, std::numeric_limits<float>::quiet_NaN(), kScale, edge, edge, kPlayerRadius)) { return false; }
    const int frameRates[] = { 30, 60, 120 };
    for (int frameRate : frameRates) {
        Player player;
        const float deltaTimeSeconds = 1.0f / static_cast<float>(frameRate);
        for (int frameIndex = 0; frameIndex < frameRate; ++frameIndex) {
            PlayerTestAccess::UpdateInvincibility(player, deltaTimeSeconds);
            const float progress = static_cast<float>(frameIndex) / static_cast<float>(frameRate);
            const Vector3 start { gatePosition.x, -23.0f, 380.0f + progress * 40.0f };
            const Vector3 end { start.x, start.y, start.z + 40.0f * deltaTimeSeconds };
            if (ElectricGate::CheckDischargeCollision(gatePosition, 0.0f, kScale, start, end, kPlayerRadius)) {
                player.ApplyDamage(ElectricGate::kContactDamage);
            }
        }
        if (player.GetCurrentHp() != 19 || player.ApplyDamage(ElectricGate::kContactDamage)) { return false; }
        PlayerTestAccess::UpdateInvincibility(player, 1.0f);
        if (!player.ApplyDamage(ElectricGate::kContactDamage) || player.GetCurrentHp() != 18) { return false; }
    }
    return true;
}

bool RunGameplayCollisionTests()
{
    if (!RunStrafeEnemyCollisionTests()) { std::puts("FAILED: StrafeEnemy inactive state or box bounds"); return false; }
    if (!RunTurretShotFeedbackTests()) { std::puts("FAILED: turret first shot, cooldown, recoil or trail transform"); return false; }
    if (!RunTurretEnemyBulletTests()) { std::puts("FAILED: TurretEnemyBullet launch, size or swept collision"); return false; }
    if (!RunTurretEnemyAimTests()) { std::puts("FAILED: TurretEnemy aim, hierarchy or FPS independence"); return false; }
    if (!RunPaceEnemyBoxTests()) { std::puts("FAILED: PaceEnemy box bounds, raycast, scale or rotation"); return false; }
    {
        PaceEnemy enemy;
        PaceEnemyTestAccess::Configure(enemy, { 4.0f, 4.0f, 4.0f });
        GameplayCollisionSystem::ApplyExplosionToEnemy(enemy, Sphere { { 2.5f, 0.0f, 10.0f }, 0.1f }, 1.0f, nullptr);
        if (PaceEnemyTestAccess::GetHealth(enemy) != 10.0f) { return false; }
        GameplayCollisionSystem::ApplyExplosionToEnemy(enemy, Sphere { { 2.05f, 0.0f, 10.0f }, 0.1f }, 1.0f, nullptr);
        if (PaceEnemyTestAccess::GetHealth(enemy) != 9.0f) { return false; }
    }
    using CollisionSystem = GameplayCollisionSystem;
    const Sphere bullet { { 0.0f, 0.0f, 0.0f }, 0.25f };
    const Vector3 movement { 0.0f, 0.0f, 100.0f };
    std::vector<std::unique_ptr<BaseEnemy>> enemies;
    std::vector<DestructibleLevelObject> destructibles;
    enemies.push_back(std::make_unique<CollisionTestEnemy>(60.0f));
    enemies.push_back(std::make_unique<CollisionTestEnemy>(20.0f));
    BaseEnemy* nearEnemy = enemies[1].get();
    auto hit = CollisionSystem::FindFirstPlayerBulletHit(
        bullet, movement, enemies, nullptr, destructibles, -30.0f);
    if (hit.enemy != nearEnemy || !hit.hit.isHit) { return false; }
    std::swap(enemies[0], enemies[1]);
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        bullet, movement, enemies, nullptr, destructibles, -30.0f);
    if (hit.enemy != nearEnemy) { return false; }

    CollisionTestEnemy boss(10.0f);
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        bullet, movement, enemies, &boss, destructibles, -30.0f);
    if (hit.enemy != &boss) { return false; }
    // 部位の登録順と防御状態に関係なく、手前の部位を選ぶ。
    CollisionTestEnemy multipartEnemy(80.0f);
    multipartEnemy.AddPart(5.0f, 7);
    int32_t partIndex = -1;
    const SweepHit partHit = multipartEnemy.SweepBullet(bullet, movement, partIndex);
    if (!partHit.isHit || partIndex != 7) { return false; }

    Object3d obstacle;
    BoxCollider collider;
    collider.SetCenter({ 0.0f, 0.0f, 40.0f });
    collider.SetSize({ 4.0f, 4.0f, 4.0f });
    obstacle.SetCollider(&collider);
    destructibles.push_back({ &obstacle, 5.0f, false });
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        bullet, movement, enemies, nullptr, destructibles, -30.0f);
    if (hit.enemy != nearEnemy || hit.destructible != nullptr) { return false; }
    collider.SetCenter({ 0.0f, 0.0f, 5.0f });
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        bullet, movement, enemies, &boss, destructibles, -30.0f);
    if (hit.destructible != &destructibles[0] || hit.enemy != nullptr) { return false; }
    destructibles[0].destroyed = true;
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        bullet, movement, enemies, &boss, destructibles, -30.0f);
    if (hit.enemy != &boss) { return false; }

    // 複数の破壊可能物も、登録順ではなく接触時刻で選ぶ。
    enemies.clear();
    destructibles[0].destroyed = false;
    collider.SetCenter({ 0.0f, 0.0f, 50.0f });
    Object3d secondObstacle;
    BoxCollider secondCollider;
    secondCollider.SetCenter({ 0.0f, 0.0f, 10.0f });
    secondCollider.SetSize({ 4.0f, 4.0f, 4.0f });
    secondObstacle.SetCollider(&secondCollider);
    destructibles.push_back({ &secondObstacle, 5.0f, false });
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        bullet, movement, enemies, nullptr, destructibles, -30.0f);
    if (hit.destructible != &destructibles[1]) { return false; }

    // 床に到達した位置より先にある敵には命中しない。
    destructibles.clear();
    const Sphere fallingBullet { { 0.0f, 10.0f, 0.0f }, 0.25f };
    const Vector3 fallingMovement { 0.0f, -20.0f, 100.0f };
    enemies.push_back(std::make_unique<CollisionTestEnemy>(50.0f));
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        fallingBullet, fallingMovement, enemies, nullptr, destructibles, 5.0f);
    if (!hit.hit.isHit || hit.enemy != nullptr || hit.destructible != nullptr ||
        std::abs(hit.hit.time - 0.25f) > 0.0001f ||
        std::abs(hit.hit.position.z - 25.0f) > 0.0001f) { return false; }
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        fallingBullet, fallingMovement, enemies, nullptr, destructibles, -10.0f);
    if (hit.enemy != enemies[0].get()) { return false; }
    // 海中ルートでは水面による弾の消滅を止め、敵との判定は維持する。
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        fallingBullet, fallingMovement, enemies, nullptr, destructibles, 5.0f, false);
    if (hit.enemy != enemies[0].get()) { return false; }
    const Sphere submergedBullet { { 0.0f, -45.0f, 0.0f }, 0.25f };
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        submergedBullet, movement, enemies, nullptr, destructibles, -30.0f, false);
    if (hit.hit.isHit) { return false; }
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        submergedBullet, movement, enemies, nullptr, destructibles, -30.0f);
    if (!hit.hit.isHit || hit.hit.time != 0.0f) { return false; }
    enemies[0]->SetDead(true);
    hit = CollisionSystem::FindFirstPlayerBulletHit(
        bullet, movement, enemies, nullptr, destructibles, -30.0f);
    if (hit.hit.isHit) { return false; }

    // 実際の弾種の設定で、範囲・直撃除外・防御・部位重複・得点を検証する。
    MissileBullet missile;
    HomingMissileBullet homing;
    const PlayerBullet& missileBase = missile;
    const PlayerBullet& homingBase = homing;
    std::filesystem::path explosionPath = "resources/Effects/MissileExplosion/Effect.json";
    if (!std::filesystem::exists(explosionPath)) { explosionPath = std::filesystem::path("project") / explosionPath; }
    auto explosionConfig = JsonFile::Load(explosionPath);
    const auto explosionSettings = MissileBullet::GetExplosionSettingsFromConfig(explosionConfig);
    if (explosionSettings.radius != 8.0f || explosionSettings.damage != 5.0f ||
        homingBase.GetExplosionSettings().radius != 0.0f || homingBase.GetExplosionSettings().damage != 0.0f) { return false; }
    if (missileBase.GetExplosionSettings().radius != 0.0f) { return false; } // 未登録の演出では見えない爆風を作らない。
    explosionConfig["Particle"]["EndScale"] = 12.0f;
    if (MissileBullet::GetExplosionSettingsFromConfig(explosionConfig).radius != 12.0f) { return false; }
    explosionConfig["Render"]["MeshType"] = "Board";
    if (MissileBullet::GetExplosionSettingsFromConfig(explosionConfig).radius != 0.0f) { return false; }
    enemies.clear();
    auto direct = std::make_unique<ExplosionTestEnemy>(0.0f, 20.0f);
    auto* directEnemy = direct.get(); enemies.push_back(std::move(direct));
    auto nearby = std::make_unique<ExplosionTestEnemy>(3.0f, 5.0f);
    auto* nearbyEnemy = nearby.get(); enemies.push_back(std::move(nearby));
    auto edge = std::make_unique<ExplosionTestEnemy>(9.0f, 20.0f);
    auto* edgeEnemy = edge.get(); enemies.push_back(std::move(edge));
    auto outside = std::make_unique<ExplosionTestEnemy>(9.01f, 20.0f);
    auto* outsideEnemy = outside.get(); enemies.push_back(std::move(outside));
    auto guarded = std::make_unique<ExplosionTestEnemy>(1.0f, 20.0f, true);
    auto* guardedEnemy = guarded.get(); enemies.push_back(std::move(guarded));
    auto multipart = std::make_unique<ExplosionTestEnemy>(1.0f, 20.0f);
    multipart->AddPart(2.0f, 7);
    auto* multipartTarget = multipart.get(); enemies.push_back(std::move(multipart));
    auto weakPoint = std::make_unique<ExplosionTestEnemy>(1.0f, 20.0f, true);
    weakPoint->AddPart(2.0f, 7);
    auto* weakPointEnemy = weakPoint.get(); enemies.push_back(std::move(weakPoint));
    auto defeated = std::make_unique<ExplosionTestEnemy>(1.0f, 20.0f);
    defeated->SetDead(true);
    auto* defeatedEnemy = defeated.get(); enemies.push_back(std::move(defeated));
    auto explosionBoss = std::make_unique<ExplosionTestEnemy>(1.0f, 5.0f);
    auto* bossTarget = explosionBoss.get(); enemies.push_back(std::move(explosionBoss));
    // ボスが敵一覧にも入っていても二重に判定しない。
    const uint32_t blastScore = CollisionSystem::ApplyExplosionDamage({}, explosionSettings, enemies, bossTarget, directEnemy);
    if (blastScore != 1100u || directEnemy->GetHitCount() != 0 || directEnemy->GetRemainingHealth() != 20.0f ||
        !nearbyEnemy->IsDead() || nearbyEnemy->GetHitCount() != 1 || !bossTarget->IsDead() || bossTarget->GetHitCount() != 1 ||
        edgeEnemy->GetRemainingHealth() != 15.0f || outsideEnemy->GetHitCount() != 0 || guardedEnemy->GetHitCount() != 0 ||
        multipartTarget->GetRemainingHealth() != 15.0f || multipartTarget->GetHitCount() != 1 ||
        weakPointEnemy->GetHitCount() != 1 || weakPointEnemy->GetLastPartIndex() != 7 || defeatedEnemy->GetHitCount() != 0) { return false; }
    // ホーミングや不正な設定では、周囲の敵もボスも追加ダメージを受けない。
    if (CollisionSystem::ApplyExplosionDamage({}, homingBase.GetExplosionSettings(), enemies, bossTarget, nullptr) != 0 ||
        edgeEnemy->GetRemainingHealth() != 15.0f) { return false; }
    const PlayerBulletExplosionSettings invalidExplosion { (std::numeric_limits<float>::quiet_NaN)(), 5.0f };
    if (CollisionSystem::ApplyExplosionDamage({}, invalidExplosion, enemies, bossTarget, nullptr) != 0 ||
        edgeEnemy->GetRemainingHealth() != 15.0f) { return false; }
    return true;
}

bool RunWaterPillarHeightTests()
{
    WaterPillarHazard pillar;
    pillar.Initialize(nullptr, { 3.0f, 10.0f, 5.0f }, 0.0f, 0.0f);
    pillar.Update(0.0f, 0.0f); // Waiting -> Warning
    if (pillar.CheckCollision({ 3.0f, 11.0f, 5.0f })) { return false; }
    pillar.Update(0.0f, 3.0f); // Warning -> Rising
    pillar.Update(0.0f, 0.5f);
    if (pillar.CheckCollision({ 3.0f, 11.0f, 5.0f })) { return false; }
    pillar.Update(0.0f, 0.13f); // 表示高は根元から約37。高さ45には届かない。
    if (!pillar.CheckCollision({ 3.0f, 46.0f, 5.0f }) ||
        pillar.CheckCollision({ 3.0f, 55.0f, 5.0f }) ||
        pillar.CheckCollision({ 3.0f, 9.0f, 5.0f }) ||
        pillar.CheckCollision({ 7.0f, 46.0f, 5.0f })) { return false; }
    pillar.Update(0.0f, 0.08f); // Rising -> Active。最大高さまで当たる。
    if (!pillar.CheckCollision({ 3.0f, 55.0f, 5.0f }) ||
        pillar.CheckCollision({ 3.0f, 63.0f, 5.0f })) { return false; }
    pillar.Update(0.0f, 3.0f); // Active -> Fading
    if (pillar.CheckCollision({ 3.0f, 11.0f, 5.0f })) { return false; }
    return true;
}

int main(int argumentCount, char** arguments)
{
    if (!RunElectricGateCollisionTests()) {
        std::puts("FAILED: electric gate sweep, safe gap, rotation, radius or damage invincibility");
        return 1;
    }
    std::puts("PASS: electric gate two arcs, safe gap, swept crossing, rotation and damage at 30/60/120 FPS");
    if (!RunGameplayCollisionTests()) {
        std::puts("FAILED: gameplay collision ordering");
        return 1;
    }
    std::puts("PASS: player bullet ordering, missile sphere radius, blast damage, guards, score and homing exclusion");
    if (argumentCount == 2 && std::strcmp(arguments[1], "--player-bullets-only") == 0) { return 0; }
    if (!RunWaterPillarHeightTests()) {
        std::puts("FAILED: water pillar collision height");
        return 1;
    }
    std::puts("PASS: water pillar warning, rising height, active height, radius and fading");
    return 0;
}
