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
    assert(stage->railHeightRange.shouldUseFloorMinimum);
    assert(!stage->railLowerHeightRange.shouldUseFloorMinimum);
    assert(stage->railControlPoints.front().y == -20.0f);
    assert(stage->railControlPoints.back().z == 7200.0f);
    assert(!catalog->Find("stage02")->railBranch.isEnabled);
}

void TestHeightRangeSelection()
{
    RailHeightRangeSettings mainSettings;
    mainSettings.shouldUseFloorMinimum = true;
    RailHeightRangeSettings lowerSettings;
    RailHeightRange range = CalculateRailHeightRange(mainSettings, lowerSettings, {}, false,
        100.0f, -20.0f, -30.0f, 0.0f);
    assert(range.minimumHeight == -27.0f && range.maximumHeight == -5.0f);
    range = CalculateRailHeightRange(mainSettings, lowerSettings, {}, true,
        2000.0f, -85.0f, -30.0f, -65.0f);
    assert(range.minimumHeight == -100.0f && range.maximumHeight == -70.0f);

    RailHeightZoneSettings zone;
    zone.startDistance = 200.0f;
    zone.endDistance = 400.0f;
    zone.range.minimumOffsetY = -25.0f;
    zone.range.maximumOffsetY = 30.0f;
    const std::vector<RailHeightZoneSettings> zones { zone };
    const float distances[] = { 199.0f, 200.0f, 399.0f, 400.0f };
    const float expectedMaximums[] = { -5.0f, 10.0f, 10.0f, -5.0f };
    for (int distanceIndex = 0; distanceIndex < 4; ++distanceIndex) {
        range = CalculateRailHeightRange(mainSettings, lowerSettings, zones, false,
            distances[distanceIndex], -20.0f, -30.0f, 0.0f);
        assert(range.maximumHeight == expectedMaximums[distanceIndex]);
    }
    range = CalculateRailHeightRange(mainSettings, lowerSettings, zones, true,
        300.0f, -85.0f, -30.0f, -65.0f);
    assert(range.minimumHeight == -100.0f && range.maximumHeight == -70.0f);
    assert(!StageCatalog::GetInstance()->Find("stage02")->railHeightRange.shouldUseFloorMinimum);
}

void TestHeightRangeClampAndValidation()
{
    RailHeightRange range { -100.0f, -70.0f };
    assert(range.ClampOffset(-30.0f, -85.0f, 1.0f) == -15.0f);
    assert(range.ClampOffset(30.0f, -85.0f, 1.0f) == 15.0f);
    assert(range.ClampOffset(2.0f, -85.0f, 1.0f) == 2.0f);
    assert(range.ClampOffset(40.0f, -85.0f, -0.5f) == 30.0f);
    assert(range.ClampOffset(2.0f, -85.0f, 0.0f) == 2.0f);
    RailHeightRangeSettings settings;
    assert(settings.IsValid());
    settings.minimumOffsetY = settings.maximumOffsetY;
    assert(!settings.IsValid());
    settings = {};
    settings.maximumOffsetY = std::numeric_limits<float>::infinity();
    assert(!settings.IsValid());
}
}

int main()
{
    TestDefaultAndUpperRoute();
    TestLowerRouteAndMerge();
    TestFrameRatesAndSkippedBranch();
    TestStageConfiguration();
    TestHeightRangeSelection();
    TestHeightRangeClampAndValidation();
}
