#pragma once
#include "App/Game/Boss/Encounter/BaseBossFactory.h"
#include "App/Game/Boss/Encounter/BossEncounterController.h"
#include <array>

inline bool RunBossPresentationTests()
{
    class TestBoss final : public BaseBoss {
    public:
        Vector3 position { 10.0f, 20.0f, 30.0f };
        Vector3 GetPosition() const override { return position; }
        bool IsDeathSequenceFinished() const override { return false; }
        bool IsMadModeActive() const override { return false; }
        bool IsBeamHittingPlayer() const override { return false; }
        float GetHeadHpFraction() const override { return 1.0f; }
        float GetBodyHpFraction() const override { return 1.0f; }
    } boss;
    const auto* factory = FindBossFactory("IceJellyfish");
    auto presentation = factory->CreatePresentation();
    if (!presentation || FindBossFactory("AngerBlock")->CreatePresentation() ||
        FindBossFactory("FearWorm")->CreatePresentation()) return false;
    Vector3 spot {}, point {};
    std::array<Vector3, 2> fog {};
    int fogCalls = 0;
    BossPresentationContext context;
    context.setSpotPosition = [&](const Vector3& position) { spot = position; };
    context.setPointPosition = [&](const Vector3& position) { point = position; };
    context.setFogCenter = [&](uint32_t index, const Vector3& position) { fog[index] = position; ++fogCalls; };
    presentation->Update(boss, context);
    if (spot.x != -45.0f || spot.y != 120.0f || spot.z != -15.0f ||
        point.x != 55.0f || point.y != 40.0f || point.z != -5.0f || fogCalls != 0) return false;
    context.followFog = true;
    presentation->Update(boss, context);
    if (fogCalls != 2 || fog[0].x != 10.0f || fog[0].y != 4.0f || fog[0].z != 30.0f ||
        fog[1].x != 10.0f || fog[1].y != 45.0f || fog[1].z != 30.0f) return false;
    boss.position = { 40.0f, 60.0f, 80.0f };
    presentation->Update(boss, context);
    if (fog[0].x != 40.0f || fog[0].y != 4.0f || fog[0].z != 80.0f || spot.x != -15.0f) return false;
    BossEncounterController encounter;
    encounter.UpdatePresentation(context); // 未出現なら何も更新しない。
    if (fogCalls != 4) return false;
    presentation->Update(boss, {}); // 演出先がない環境でも安全に更新できる。
    return true;
}
