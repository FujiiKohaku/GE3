#include "App/Game/Player/Health/PlayerHealth.h"
#include "Engine/audio/SoundManager.h"
#include <cassert>

namespace {
void TestDamageAndInvincibility()
{
    PlayerHealth health;
    assert(!health.ApplyDamage(0, false));
    assert(!health.ApplyDamage(3, true));
    assert(health.ApplyDamage(3, false));
    assert(health.GetCurrentHp() == 17);
    assert(!health.ApplyDamage(3, false));
    for (int frameIndex = 0; frameIndex < 59; ++frameIndex) {
        health.UpdateInvincibility();
    }
    assert(!health.ApplyDamage(3, false));
    health.UpdateInvincibility();
    assert(health.ApplyDamage(3, false));
    assert(health.GetCurrentHp() == 14);
}

void TestHealingAndDeath()
{
    PlayerHealth health;
    assert(!health.Heal(1));
    assert(health.ApplyDamage(3, false));
    assert(!health.Heal(0));
    assert(health.Heal(100));
    assert(health.GetCurrentHp() == health.GetMaxHp());
    for (int frameIndex = 0; frameIndex < 60; ++frameIndex) {
        health.UpdateInvincibility();
    }
    assert(health.ApplyDamage(100, false));
    assert(health.GetCurrentHp() == 0);
    assert(health.IsDead());
    assert(health.IsFalling());
    assert(health.ShouldDraw());
    assert(!health.Heal(20));
    assert(!health.ApplyDamage(1, false));
    EulerTransform transform{};
    health.UpdateDeathAnimation(transform, 0.6f);
    assert(transform.translate.y < 0.0f);
    assert(!health.IsDeathExplosionReady());
    health.UpdateDeathAnimation(transform, 0.6f);
    assert(health.IsDeathExplosionReady());
    assert(!health.ShouldDraw());
    const float finalHeight = transform.translate.y;
    health.UpdateDeathAnimation(transform, 1.0f);
    assert(transform.translate.y == finalHeight);
}

#if defined(ENABLE_DEVELOPMENT_TOOLS)
void TestDevelopmentInvincibility()
{
    PlayerHealth health;
    health.SetInvincibleMode(true);
    assert(health.IsInvincibleMode());
    assert(!health.ApplyDamage(100, false));
    assert(!health.IsDead());
    health.SetInvincibleMode(false);
    assert(health.ApplyDamage(100, false));
}
#endif
}

int main()
{
    TestDamageAndInvincibility();
    TestHealingAndDeath();
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    TestDevelopmentInvincibility();
#endif
    SoundManager::GetInstance()->Finalize();
}
