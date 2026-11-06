#include "App/Game/Enemy/DashEnemy/DashEnemy.h"
#include "App/Game/Collision/GameplayCollisionSystem.h"
#include "Tests/PlayerTestAccess.h"
#include "Engine/CollisionManager/BoxCollider.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include <filesystem>
#include <fstream>
#include <cstdio>

struct DashEnemyTestAccess {
    static void Movement(DashEnemy& enemy, Vector3 start, Vector3 end, bool attacking = true) {
        enemy.InitializeHealth(4.0f);
        enemy.hasContactedPlayer_ = false;
        enemy.motion_.Reset(start, {});
        enemy.motion_.phase = attacking ? DashPhase::Dash : DashPhase::Locked;
        enemy.motion_.position = end;
        enemy.motion_.frameSeconds = 0.1f;
        enemy.motion_.segments[0] = { start, end, 0, 1, attacking };
        enemy.motion_.segmentCount = 1;
    }
    static void Warning(DashEnemy& enemy, Vector3 start, Vector3 direction) {
        enemy.motion_.Reset(start, {});
        enemy.motion_.phase = DashPhase::Locked;
        enemy.motion_.direction = direction;
    }
};

namespace {
bool MotionTests()
{
    Vector3 reference {}, heading {};
    for (int fps : { 120, 60, 30 }) {
        DashEnemyMotion motion;
        motion.Reset({ 15, 0, 120 }, {});
        const float delta = 1.0f / fps;
        for (int frame = 0; frame < fps * 2; ++frame) {
            DashTarget target;
            target.origin = target.position = { 0, 0, 48.0f * frame * delta };
            target.velocity = { 0, 0, 48 };
            motion.Advance(delta, target);
        }
        if (motion.phase != DashPhase::Dash || std::abs(motion.elapsed - 0.5f) > 0.0001f) { return false; }
        if (fps == 120) { reference = motion.position; heading = motion.direction; }
        else if (Vector3Length(motion.position - reference) > 0.02f || Vector3Length(motion.direction - heading) > 0.0001f) { return false; }
    }
    DashEnemyMotion motion;
    LevelData::DashEnemySpawnData settings;
    settings.dashSpeed = 180;
    motion.Reset({ 15, 0, 120 }, settings);
    // Reset may receive its own settings reference when a placement changes.
    motion.Reset({ 15, 0, 120 }, motion.settings);
    if (motion.settings.dashSpeed != 180) { return false; }
    DashTarget target;
    motion.Advance(1.1f, target);
    if (motion.phase != DashPhase::Locked) { return false; }
    const Vector3 lockedDirection = motion.direction, lockedPosition = motion.position;
    target.position = { 50, 30, 0 };
    motion.Advance(0.3f, target);
    if (Vector3Length(motion.direction - lockedDirection) > 0.00001f ||
        Vector3Length(motion.position - lockedPosition) > 0.00001f) { return false; }
    motion.Advance(0.2f, target);
    if (motion.phase != DashPhase::Dash || Vector3Length(motion.direction - lockedDirection) > 0.00001f) { return false; }
    const Vector3 position = motion.position;
    motion.Advance(0, target);
    motion.Advance(std::numeric_limits<float>::quiet_NaN(), target);
    if (Vector3Length(motion.position - position) > 0.00001f) { return false; }
    // No world-Z or water-height dependency: the same enemy can face along X.
    DashTarget sideways;
    sideways.forward = { 1, 0, 0 };
    sideways.right = { 0, 0, -1 };
    motion.Reset({ 120, 7, -15 }, {});
    motion.Advance(1.1f, sideways);
    if (motion.phase != DashPhase::Locked || motion.direction.x > -0.9f) { return false; }
    motion.Reset({ 0, 100, 120 }, {});
    motion.Advance(0.1f, target);
    return motion.phase == DashPhase::Waiting;
}

bool ContactTests()
{
    GameplayCollisionSystem system;
    std::vector<std::unique_ptr<BaseEnemy>> enemies;
    auto dash = std::make_unique<DashEnemy>();
    auto* enemy = dash.get();
    DashEnemyTestAccess::Movement(*enemy, { -10, 0, 0 }, { 10, 0, 0 });
    enemies.push_back(std::move(dash));
    Player player;
    const Vector3 previous { 0, -10, 0 };
    PlayerTestAccess::Transform(player).translate = { 0, 10, 0 };
    system.UpdateCombatCollisions(player, enemies, {}, {}, &previous);
    if (player.GetCurrentHp() != 18) { return false; }
    PlayerTestAccess::UpdateInvincibility(player, 2.0f);
    system.UpdateCombatCollisions(player, enemies, {}, {}, &previous);
    if (player.GetCurrentHp() != 18) { return false; } // same charge never hits twice

    DashEnemy warning;
    DashEnemyTestAccess::Movement(warning, { -10, 0, 0 }, { 10, 0, 0 }, false);
    if (warning.FindPlayerContact({}, {}).isHit) { return false; }
    // A bullet can hit a fast cube even when neither endpoint overlaps it.
    int32_t part = -1;
    const SweepHit bullet = enemy->SweepBullet(Sphere { {}, 0.25f }, {}, part);
    if (!bullet.isHit || bullet.time <= 0 || bullet.time >= 1 || part != 0) { return false; }
    const Vector3 safe { 0, 0, 20 };
    DashEnemy dodge;
    DashEnemyTestAccess::Movement(dodge, { 0, 0, 10 }, { 0, 0, -10 });
    if (dodge.FindPlayerContact(safe, safe).isHit ||
        dodge.FindPlayerContact({}, { 10, 0, 0 }).isHit) { return false; }
    // A lethal player shot cancels the charge's remaining collision checks.
    dodge.ApplyDamage(4.0f);
    if (!dodge.IsDead() || dodge.FindPlayerContact({}, {}).isHit ||
        dodge.SweepBullet(Sphere { {}, 0.25f }, {}, part).isHit) { return false; }
    return true;
}

bool ObstacleTests()
{
    GameplayCollisionSystem system;
    std::vector<std::unique_ptr<Object3d>> obstacles;
    BoxCollider wall;
    wall.SetCenter({});
    wall.SetSize({ 20, 20, 0.2f });
    auto object = std::make_unique<Object3d>();
    object->SetCollider(&wall);
    obstacles.push_back(std::move(object));
    std::vector<std::unique_ptr<BaseEnemy>> enemies;
    auto dash = std::make_unique<DashEnemy>();
    auto* enemy = dash.get();
    DashEnemyTestAccess::Movement(*enemy, { 0, 0, 10 }, { 0, 0, -10 });
    enemies.push_back(std::move(dash));
    system.UpdateEnemyMovementCollisions(enemies, obstacles);
    if (enemy->GetMotion().phase != DashPhase::Recovery || std::abs(enemy->GetPosition().z - 2.1f) > 0.001f) { return false; }
    Player behind;
    PlayerTestAccess::Transform(behind).translate = { 0, 0, -0.8f };
    system.UpdateCombatCollisions(behind, enemies, {}, obstacles);
    if (behind.GetCurrentHp() != 20) { return false; } // thin-wall occlusion
    Player front;
    PlayerTestAccess::Transform(front).translate = { 0, 0, 5 };
    system.UpdateCombatCollisions(front, enemies, {}, obstacles);
    if (front.GetCurrentHp() != 18) { return false; }
    int32_t part = 0;
    if (enemy->SweepBullet(Sphere { { 0, 0, -5 }, 0.1f }, {}, part).isHit) { return false; }
    DashEnemyTestAccess::Warning(*enemy, { 0, 0, 10 }, { 0, 0, -1 });
    system.UpdateEnemyMovementCollisions(enemies, obstacles);
    return std::abs(enemy->GetWarningLength() - 7.9f) < 0.001f;
}

bool DataTests()
{
    std::filesystem::create_directories("generated/verification");
    const std::string fixture = "generated/verification/dash-settings.json";
    {
        std::ofstream stream(fixture);
        stream << R"({"scene":[
          {"type":"EnemySpawn","file_name":"DASHENEMY","dashEnemy":{"dashSpeed":180,"lockSeconds":0.8,"damage":3}},
          {"type":"EnemySpawn","file_name":"DASHENEMY","dashEnemy":{"dashSpeed":0,"lockSeconds":-1,"damage":2.5}}
        ]})";
    }
    LevelDataLoader loader;
    const auto level = loader.Load(fixture);
    std::filesystem::remove(fixture);
    if (level.enemies.size() != 2 || level.objects.size() != 2 ||
        level.enemies[0].dashEnemy.dashSpeed != 180 || level.objects[0].dashEnemy.damage != 3 ||
        level.enemies[1].dashEnemy.dashSpeed != 220 || level.objects[1].dashEnemy.lockSeconds != 0.45f ||
        level.enemies[1].dashEnemy.damage != 2) { return false; }
    std::string stage = "resources/Scenes/stage01.json";
    if (!std::filesystem::exists(stage)) { stage = "project/" + stage; }
    int count = 0;
    for (const auto& enemy : loader.Load(stage).enemies) {
        if (enemy.fileName == "DASHENEMY") { ++count; }
    }
    return count == 2;
}
}

bool RunDashEnemyTests()
{
    if (!MotionTests()) { std::puts("DASHENEMY: motion/timing/lock failed"); return false; }
    if (!ContactTests()) { std::puts("DASHENEMY: relative contact/bullet sweep failed"); return false; }
    if (!ObstacleTests()) { std::puts("DASHENEMY: obstacle/occlusion/warning failed"); return false; }
    if (!DataTests()) { std::puts("DASHENEMY: settings/placement failed"); return false; }
    return true;
}
