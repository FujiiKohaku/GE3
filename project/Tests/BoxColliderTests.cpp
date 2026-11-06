#include "Engine/CollisionManager/BoxCollider.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
bool Near(const Vector3& a, const Vector3& b)
{
    return Vector3Length(a - b) < 0.0001f;
}

bool CheckBounds(const BoxCollider& collider)
{
    const OBB expected = CollisionManager::MakeOBB(collider.GetCenter(), collider.GetSize(), collider.GetRotation());
    const OBB& cached = collider.GetOBB();
    if (!Near(expected.center, cached.center) || !Near(expected.size, cached.size)) { return false; }
    for (int axis = 0; axis < 3; ++axis) {
        if (!Near(expected.orientation[axis], cached.orientation[axis])) { return false; }
    }
    const float maximum = (std::numeric_limits<float>::max)();
    Vector3 low { maximum, maximum, maximum }, high { -maximum, -maximum, -maximum };
    // Independently calculate the AABB from the eight OBB corners.
    for (int x : { -1, 1 }) {
        for (int y : { -1, 1 }) {
            for (int z : { -1, 1 }) {
                const Vector3 corner = expected.center + expected.orientation[0] * (expected.size.x * x * 0.5f) +
                    expected.orientation[1] * (expected.size.y * y * 0.5f) +
                    expected.orientation[2] * (expected.size.z * z * 0.5f);
                low.x = (std::min)(low.x, corner.x); low.y = (std::min)(low.y, corner.y); low.z = (std::min)(low.z, corner.z);
                high.x = (std::max)(high.x, corner.x); high.y = (std::max)(high.y, corner.y); high.z = (std::max)(high.z, corner.z);
            }
        }
    }
    return Near(collider.GetAABB().center, (low + high) * 0.5f) && Near(collider.GetAABB().size, high - low);
}
}

bool RunBoxColliderTests()
{
    BoxCollider collider;
    if (!CheckBounds(collider)) { return false; }
    collider.SetCenter({ 2, 3, 4 });
    collider.SetSize({ 4, 6, 8 });
    collider.SetRotation({ 0.3f, 0.7f, -0.2f });
    if (!CheckBounds(collider)) { return false; }
    const OBB* address = &collider.GetOBB();
    const AABB* aabbAddress = &collider.GetAABB();
    for (int repeat = 0; repeat < 100; ++repeat) {
        // Object3d submits these same values every frame, even for static props.
        collider.SetCenter({ 2, 3, 4 });
        collider.SetSize({ 4, 6, 8 });
        collider.SetRotation({ 0.3f, 0.7f, -0.2f });
        if (&collider.GetOBB() != address || &collider.GetAABB() != aabbAddress || !CheckBounds(collider)) { return false; }
    }
    const Vector3 direction = collider.GetOBB().orientation[0];
    collider.SetCenter({ -15, 12, 30 });
    if (!CheckBounds(collider) || !Near(direction, collider.GetOBB().orientation[0])) { return false; }
    collider.SetSize({ 7, 2, 3 });
    if (!CheckBounds(collider)) { return false; }
    collider.SetRotation({ 0.8f, -0.2f, 1.1f });
    if (!CheckBounds(collider)) { return false; }
    collider.SetSize({ -1, 2, 3 });
    collider.SetSize({ std::numeric_limits<float>::quiet_NaN(), 2, 3 });
    if (!Near(collider.GetSize(), { 7, 2, 3 }) || !CheckBounds(collider)) { return false; }
    // Multiple setters before a read and copying a dirty collider must stay valid.
    collider.SetRotation({ 0.1f, 0.9f, 0.2f });
    collider.SetSize({ 3, 4, 5 });
    collider.SetCenter({ 1, -2, 8 });
    BoxCollider copy = collider;
    if (!CheckBounds(copy) || !CheckBounds(collider)) { return false; }

    // Raycast must use the latest cached position and size after setters, too.
    auto owned = std::make_unique<BoxCollider>();
    owned->SetCenter({ 0, 0, 10 });
    owned->SetSize({ 2, 2, 2 });
    constexpr CollisionLayer layer = 1u << 30;
    owned->SetLayer(layer, kCollisionLayerAll);
    auto* manager = CollisionManager::GetInstance();
    BoxCollider* registered = manager->RegisterCollider(std::move(owned));
    RaycastHit hit;
    const Ray ray { { 0, 0, 0 }, { 0, 0, 1 } };
    bool passed = manager->Raycast(ray, hit, layer) && hit.collider == registered && std::abs(hit.distance - 9) < 0.0001f;
    registered->SetCenter({ 0, 0, 20 });
    registered->SetSize({ 2, 2, 4 });
    passed = passed && manager->Raycast(ray, hit, layer) && std::abs(hit.distance - 18) < 0.0001f;
    registered->SetRotation({ 0, 1.57079632679f, 0 });
    passed = passed && manager->Raycast(ray, hit, layer) && std::abs(hit.distance - 19) < 0.0001f;
    manager->UnregisterCollider(registered);
    return passed;
}
