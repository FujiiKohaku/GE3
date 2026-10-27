#include "App/Game/Stage/StageCatalog.h"
#include <cassert>
#include <cmath>
#include <limits>

namespace {
RailBranchSettings MakeSettings()
{
    RailBranchSettings settings;
    settings.isEnabled = true;
    return settings;
}

void TestDefaultAndUpperRoute()
{
    RailBranchRoute route;
    route.Initialize({});
    route.Update(899.0f, 901.0f, -10.0f, true);
    assert(!route.IsLowerRouteSelected());
    route.Initialize(MakeSettings());
    assert(!route.IsPreviewVisible(639.0f));
    assert(route.IsPreviewVisible(640.0f));
    assert(!route.IsPreviewVisible(900.0f));
    route.Update(899.0f, 900.0f, 0.0f, true);
    route.Update(900.0f, 950.0f, -10.0f, true);
    assert(!route.IsLowerRouteSelected());
    assert(route.GetOffset(1500.0f).y == 0.0f);
}

void TestLowerRouteAndMerge()
{
    RailBranchRoute route;
    route.Initialize(MakeSettings());
    route.Update(899.0f, 901.0f, -1.0f, true);
    route.Update(901.0f, 1200.0f, 10.0f, true);
    assert(route.IsLowerRouteSelected());
    assert(route.GetOffset(899.0f).y == 0.0f);
    assert(route.GetOffset(900.0f).y == 0.0f);
    assert(std::abs(route.GetOffset(901.0f).y) < 0.0001f);
    assert(route.GetOffset(1500.0f).y == -65.0f);
    assert(std::abs(route.GetOffset(2099.0f).y) < 0.0001f);
    assert(route.GetOffset(2100.0f).y == 0.0f);
    assert(!route.IsLowerRouteActive(2100.0f));
    for (int distance = 900; distance <= 2100; ++distance) {
        const Vector3 offset = route.GetOffset(static_cast<float>(distance));
        assert(offset.y >= -65.001f && offset.y <= 0.001f);
        assert(offset.x == 0.0f && offset.z == 0.0f);
    }
    // リトライや巻き戻しでは、選択を再度行える。
    route.Update(1500.0f, 0.0f, -1.0f, true);
    assert(!route.IsLowerRouteSelected());
    route.Update(899.0f, 901.0f, 1.0f, true);
    assert(!route.IsLowerRouteSelected());
}

void TestFrameRatesAndSkippedBranch()
{
    const int frameRates[] = { 30, 60, 120 };
    for (int framesPerSecond : frameRates) {
        RailBranchRoute route;
        route.Initialize(MakeSettings());
        float distance = 0.0f;
        while (distance < 2200.0f) {
            const float nextDistance = distance + 48.0f / static_cast<float>(framesPerSecond);
            route.Update(distance, nextDistance, -1.0f, true);
            distance = nextDistance;
        }
        assert(route.IsLowerRouteSelected());
        assert(route.GetOffset(distance).y == 0.0f);
    }
    RailBranchRoute route;
    route.Initialize(MakeSettings());
    route.Update(0.0f, 5400.0f, -1.0f, true);
    assert(!route.IsLowerRouteSelected());
    route.Initialize(MakeSettings());
    route.Update(899.0f, 901.0f, -1.0f, false);
    assert(!route.IsLowerRouteSelected());
}

void TestStageConfiguration()
{
    RailBranchSettings settings = MakeSettings();
    assert(settings.IsValid(7200.0f));
    settings.endDistance = settings.startDistance;
    assert(!settings.IsValid(7200.0f));
    settings = MakeSettings();
    settings.offset.y = std::numeric_limits<float>::quiet_NaN();
    assert(!settings.IsValid(7200.0f));
    StageCatalog* catalog = StageCatalog::GetInstance();
    assert(catalog->Load());
    const StageSettings* stage = catalog->Find("stage01");
    assert(stage != nullptr && stage->railBranch.isEnabled);
    assert(stage->railBranch.endDistance < stage->bossSpawnDistance);
    assert(stage->railControlPoints.front().y == -20.0f);
    assert(stage->railControlPoints.back().z == 7200.0f);
    assert(!catalog->Find("stage02")->railBranch.isEnabled);
}
}

int main()
{
    TestDefaultAndUpperRoute();
    TestLowerRouteAndMerge();
    TestFrameRatesAndSkippedBranch();
    TestStageConfiguration();
}
