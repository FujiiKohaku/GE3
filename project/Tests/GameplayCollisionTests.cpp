#include "App/Game/Collision/GameplayCollisionSystem.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "App/Game/Hazard/WaterPillarHazard.h"
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

bool RunGameplayCollisionTests()
{
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
