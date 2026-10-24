#include "Tests/PlayerTestAccess.h"
#include "Engine/audio/SoundManager.h"
#include <cassert>

namespace {
void TestEntryBattleAndDeathTransitions()
{
    Player player;
    assert(PlayerTestAccess::IsEntry(player));
    assert(PlayerTestAccess::IsAlive(player));
    PlayerTestAccess::UpdateEntry(player);
    assert(PlayerTestAccess::IsBattle(player));

    // 戦闘中のロールは死亡開始時に解除される。
    EulerTransform transform{};
    PlayerTestAccess::UpdateRoll(player, transform, true, false);
    PlayerTestAccess::UpdateRoll(player, transform, true, false);
    assert(player.IsRolling());
    assert(player.ApplyDamage(100));
    assert(!PlayerTestAccess::IsBattle(player));
    assert(!PlayerTestAccess::IsAlive(player));
    assert(!player.IsRolling());
    assert(PlayerTestAccess::IsFalling(player));

    PlayerTestAccess::UpdateDeathAnimation(player, transform, 0.6f);
    // 死亡後の追加被弾で、落下タイマーが最初に戻らない。
    assert(!player.ApplyDamage(100));
    PlayerTestAccess::UpdateDeathAnimation(player, transform, 0.6f);
    assert(player.IsDeathExplosionReady());
}

void TestDamageAndInvincibility()
{
    Player health;
    assert(!health.ApplyDamage(0));
    assert(health.ApplyDamage(3));
    assert(health.GetCurrentHp() == 17);
    assert(!health.ApplyDamage(3));
    for (int frameIndex = 0; frameIndex < 59; ++frameIndex) {
        PlayerTestAccess::UpdateInvincibility(health);
    }
    assert(!health.ApplyDamage(3));
    PlayerTestAccess::UpdateInvincibility(health);
    assert(health.ApplyDamage(3));
    assert(health.GetCurrentHp() == 14);
}

void TestHealingAndDeath()
{
    Player health;
    assert(!health.Heal(1));
    assert(health.ApplyDamage(3));
    assert(!health.Heal(0));
    assert(health.Heal(100));
    assert(health.GetCurrentHp() == health.GetMaxHp());
    for (int frameIndex = 0; frameIndex < 60; ++frameIndex) {
        PlayerTestAccess::UpdateInvincibility(health);
    }
    assert(health.ApplyDamage(100));
    assert(health.GetCurrentHp() == 0);
    assert(health.IsDead());
    assert(PlayerTestAccess::IsFalling(health));
    assert(PlayerTestAccess::ShouldDraw(health));
    assert(!health.Heal(20));
    assert(!health.ApplyDamage(1));
    EulerTransform transform{};
    PlayerTestAccess::UpdateDeathAnimation(health, transform, 0.6f);
    assert(transform.translate.y < 0.0f);
    assert(!health.IsDeathExplosionReady());
    PlayerTestAccess::UpdateDeathAnimation(health, transform, 0.6f);
    assert(health.IsDeathExplosionReady());
    assert(!PlayerTestAccess::ShouldDraw(health));
    const float finalHeight = transform.translate.y;
    PlayerTestAccess::UpdateDeathAnimation(health, transform, 1.0f);
    assert(transform.translate.y == finalHeight);
}

#if defined(ENABLE_DEVELOPMENT_TOOLS)
void TestDevelopmentInvincibility()
{
    Player health;
    health.SetInvincibleMode(true);
    assert(health.IsInvincibleMode());
    assert(!health.ApplyDamage(100));
    assert(!health.IsDead());
    health.SetInvincibleMode(false);
    assert(health.ApplyDamage(100));
}
#endif
}

int main()
{
    // 更新頻度が変わっても、被弾後の無敵時間は1秒になる。
    const int frameRates[] = { 30, 120 };
    for (int framesPerSecond : frameRates) {
        Player health;
        assert(health.ApplyDamage(1));
        const float deltaTimeSeconds = 1.0f / static_cast<float>(framesPerSecond);
        for (int frameIndex = 0; frameIndex < framesPerSecond - 1; ++frameIndex) {
            PlayerTestAccess::UpdateInvincibility(health, deltaTimeSeconds);
        }
        assert(!health.ApplyDamage(1));
        PlayerTestAccess::UpdateInvincibility(health, deltaTimeSeconds);
        assert(health.ApplyDamage(1));
    }
    TestEntryBattleAndDeathTransitions();
    TestDamageAndInvincibility();
    TestHealingAndDeath();
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    TestDevelopmentInvincibility();
#endif
    SoundManager::GetInstance()->Finalize();
}
