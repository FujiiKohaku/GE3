#include "App/Game/SavePoint/SavePoint.h"
#include "App/Game/Stage/StageCatalog.h"
#include <cassert>
#include <numbers>

namespace {
void TestPassage()
{
    SavePointSettings settings;
    settings.position = { 0.0f, -30.0f, 600.0f };
    assert(SavePoint::IsPassageCrossed(settings, { 0, -20, 599 }, { 0, -20, 601 }));
    // 高速通過でも、境界との交点で範囲を判定する。
    assert(SavePoint::IsPassageCrossed(settings, { -100, -20, 500 }, { 100, -20, 700 }));
    assert(!SavePoint::IsPassageCrossed(settings, { 0, -20, 601 }, { 0, -20, 599 }));
    assert(!SavePoint::IsPassageCrossed(settings, { 0, -20, 601 }, { 0, -20, 602 }));
    assert(!SavePoint::IsPassageCrossed(settings, { 40, -20, 599 }, { 40, -20, 601 }));
    assert(!SavePoint::IsPassageCrossed(settings, { 0, 10, 599 }, { 0, 10, 601 }));
    assert(!SavePoint::IsPassageCrossed(settings, { 0, -31, 599 }, { 0, -31, 601 }));
    assert(!SavePoint::IsPassageCrossed(settings, { 0, -20, 600 }, { 0, -20, 600 }));
    assert(SavePoint::IsPassageCrossed(settings, { 0, -20, 600 }, { 0, -20, 601 }));
    settings.yawRadians = std::numbers::pi_v<float> * 0.5f;
    assert(SavePoint::IsPassageCrossed(settings, { -1, -20, 600 }, { 1, -20, 600 }));
    assert(!SavePoint::IsPassageCrossed(settings, { 1, -20, 600 }, { -1, -20, 600 }));
}

void TestFrameRates()
{
    SavePointSettings settings;
    for (int frameRate : { 30, 60, 144 }) {
        int passageCount = 0;
        Vector3 previousPosition = { 0, 10, -20 };
        for (int frame = 1; frame <= frameRate; ++frame) {
            const Vector3 position = { 0, 10, -20.0f + 40.0f * frame / frameRate };
            if (SavePoint::IsPassageCrossed(settings, previousPosition, position)) { ++passageCount; }
            previousPosition = position;
        }
        assert(passageCount == 1);
    }
}

void TestStageConfiguration()
{
    StageCatalog* catalog = StageCatalog::GetInstance();
    assert(catalog->Load());
    const StageSettings* stage = catalog->Find("stage01");
    assert(stage != nullptr);
    assert(stage->savePointDistances.size() == 1);
    assert(stage->savePointDistances[0] == 670.0f);
    assert(stage->savePointDistances[0] < stage->railBranch.startDistance);
    assert(catalog->Find("stage02")->savePointDistances.empty());
}
}

int main()
{
    TestPassage();
    TestFrameRates();
    TestStageConfiguration();
}
