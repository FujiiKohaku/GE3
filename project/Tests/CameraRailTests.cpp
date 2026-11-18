#include "App/Game/Camera/CameraRail.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <memory>

int main()
{
    CameraRail rail;
    assert(rail.Configure({}));
    const Vector3 main { 10, 20, 30 };
    const Vector3 right { 0, 0, -1 }, up { 0, 1, 0 }, forward { 1, 0, 0 };
    auto position = rail.GetBasePosition(500, main, right, up, forward);
    assert(position.x == 10 && position.y == 20 && position.z == 30);
    std::vector<CameraRailPoint> points {
        {100, {}}, {200, {20, 10, 5}}, {400, {20, 10, 5}}, {500, {}}
    };
    assert(rail.Configure(points));
    assert(rail.GetOffsetAtPlayerDistance(99).x == 0);
    assert(rail.GetOffsetAtPlayerDistance(501).x == 0);
    position = rail.GetBasePosition(200, main, right, up, forward);
    assert(std::abs(position.x - 15) < 0.001f);
    assert(std::abs(position.y - 30) < 0.001f);
    assert(std::abs(position.z - 10) < 0.001f);
    assert(std::abs(rail.GetOffsetAtPlayerDistance(100.01f).x) < 0.001f);
    assert(std::abs(rail.GetOffsetAtPlayerDistance(499.99f).x) < 0.001f);
    const auto before = rail.GetOffsetAtPlayerDistance(150);
    rail.GetOffsetAtPlayerDistance(450); // 呼び出し順・フレーム数に依存しない。
    assert(rail.GetOffsetAtPlayerDistance(150).x == before.x);
    points[1].distance = points[0].distance;
    assert(!rail.Configure(points));
    points[1].distance = 200;
    points[0].offset.x = 1;
    assert(!CameraRail::IsValid(points));
    points[0].offset.x = 0;
    points[1].offset.y = std::numeric_limits<float>::infinity();
    assert(!CameraRail::IsValid(points));
    assert(rail.Configure({}));
    assert(rail.GetOffsetAtPlayerDistance(200).x == 0);
    assert(rail.Configure({{0, {}}, {100, {}}, {200, {20, 0, 0}},
        {300, {}}, {400, {}}, {500, {20, 0, 0}}, {600, {}}}));
    assert(rail.GetOffsetAtPlayerDistance(50).x == 0);
    assert(rail.GetOffsetAtPlayerDistance(350).x == 0);
    std::unique_ptr<Rail> base = std::make_unique<CameraRail>();
}
