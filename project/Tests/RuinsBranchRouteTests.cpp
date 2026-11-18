#include "App/Game/Stage/StageCatalog.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include "Engine/Math/MatrixMath.h"
#include <cassert>
#include <cmath>
#include <limits>

namespace {
bool Near(const Vector3& a, const Vector3& b, float e = 0.02f)
{
    return std::abs(a.x-b.x)<e && std::abs(a.y-b.y)<e && std::abs(a.z-b.z)<e;
}
}
int main()
{
    auto* catalog = StageCatalog::GetInstance();
    assert(catalog->Load());
    const auto& stage = *catalog->Find("stage01");
    auto settings = stage.ruinsBranch;
    assert(settings.isEnabled && settings.IsValid(stage.railLength));
    assert(settings.isForced);
    assert(std::abs(settings.startDistance-stage.railBranch.endDistance-stage.railSpeed*60*20)<0.01f);
    assert(settings.endDistance < stage.bossSpawnDistance);
    assert(stage.bossSpawnDistance == stage.bossPosition.z);
    assert(Near(settings.controlPoints.front(), {0,-20,settings.startDistance}));
    assert(Near(settings.controlPoints.back(), {0,-20,settings.endDistance}));
    // 実際のレベルローダーとエンジンの行列を使い、入口・入口直後の位置を照合する。
    LevelDataLoader loader;
    const auto level = loader.Load(stage.layoutFile);
    const auto model = std::find_if(level.objects.begin(), level.objects.end(),
        [](const auto& o) { return o.name == "Stage01_RuinsField_Alpha"; });
    assert(model != level.objects.end());
    assert(Near(model->rotation, {}));
    const auto matrix = MatrixMath::MakeAffineMatrix(model->scale, model->rotation, model->translation);
    assert(Near(settings.controlPoints[40], MatrixMath::Transform({0,16,0}, matrix)));
    constexpr float diagonal = 11.3137085f;
    assert(Near(settings.controlPoints[41], MatrixMath::Transform({-diagonal,16,diagonal}, matrix)));
    RuinsBranchRoute forced;
    forced.Initialize(settings);
    assert(forced.IsActive(settings.startDistance));
    forced.Update(settings.startDistance-1,settings.startDistance,20,true);
    assert(forced.IsActive(settings.startDistance)); // 右に寄っていても必ず進入。
    forced.Initialize(settings);
    forced.Update(0,(settings.startDistance+settings.endDistance)*.5f,0,false);
    assert(forced.IsActive((settings.startDistance+settings.endDistance)*.5f)); // 途中からの再開も同じ経路。
    settings.isForced = false; // 任意分岐の既存動作も保持する。
    const Vector3 main {0,-20,settings.startDistance};
    RuinsBranchRoute route;
    route.Initialize(settings);
    assert(route.IsPreviewVisible(settings.startDistance-1));
    assert(!route.IsPreviewVisible(settings.startDistance));
    route.Update(settings.startDistance-1,settings.startDistance,-10,true);
    assert(route.IsActive(settings.startDistance));
    assert(Near(route.GetPosition(settings.startDistance,main),main));
    assert(Near(route.GetPosition(settings.endDistance,main),settings.controlPoints.back()));
    assert(!route.IsActive(settings.endDistance+1));
    assert(route.GetPosition(settings.startDistance+(settings.endDistance-settings.startDistance)*.25f,main).x < -100);
    route.Update(settings.startDistance+1,settings.startDistance+2,20,true);
    assert(route.IsActive(settings.startDistance+2)); // 選択後に右へ操作してもルートは変わらない。
    route.Update(0,settings.startDistance-1,0,true);
    assert(!route.IsActive(settings.startDistance));
    route.Update(settings.startDistance-1,settings.startDistance,0,true);
    assert(!route.IsActive(settings.startDistance));
    route.Initialize(settings);
    route.Update(0,settings.endDistance+1,-20,true);
    assert(!route.IsActive(settings.startDistance)); // 区間全体へのデバッグスキップ。
    route.Initialize(settings);
    route.Update(settings.startDistance-1,settings.startDistance,-20,false);
    assert(!route.IsActive(settings.startDistance));
    settings.controlPoints[1].x=std::numeric_limits<float>::quiet_NaN();
    assert(!settings.IsValid(stage.railLength));
    assert(!catalog->Find("stage02")->ruinsBranch.isEnabled);
}
