#include "App/Game/Boss/IceJellyfish/IceJellyfishCollision.h"
#include <cmath>
#include <iostream>
#include <cstdlib>

namespace {
int checks = 0;

void Check(bool passed, const char* description)
{
    ++checks;
    if (!passed) {
        std::cerr << "FAILED: " << description << '\n';
        std::exit(1);
    }
}
}

int main()
{
    const Matrix4x4 identity = MatrixMath::MakeIdentity4x4();
    std::vector<OBB> bell;
    std::vector<OBB> tentacles;
    const Sphere core { { 0.0f, -2.0f, 0.0f }, 3.6f };
    bool hitCore = false;
    IceJellyfishCollision::BuildBell(identity, 0.0f, bell);
    Check(bell.size() == 208, "bell panels and icicle boxes are present");

    SweepHit hit = IceJellyfishCollision::Sweep(
        Sphere { { 0.0f, -20.0f, 0.0f }, 0.2f }, { 0.0f, 50.0f, 0.0f },
        bell, tentacles, core, hitCore);
    Check(hit.isHit && hitCore, "a shot from below enters the hollow bell and hits the core");
    hit = IceJellyfishCollision::Sweep(
        Sphere { { 0.0f, 30.0f, 0.0f }, 0.2f }, { 0.0f, -60.0f, 0.0f },
        bell, tentacles, core, hitCore);
    Check(hit.isHit && !hitCore && hit.time < 0.4f, "a fast shot from above hits armor before the core");
    hit = IceJellyfishCollision::Sweep(
        Sphere { { 8.0f, 0.0f, -10.0f }, 0.2f }, { 0.0f, 0.0f, 20.0f },
        bell, tentacles, core, hitCore);
    Check(!hit.isHit, "empty interior is not filled by the bell collision");
    hit = IceJellyfishCollision::Sweep(
        Sphere { { 0.0f, -20.0f, 0.0f }, 0.2f }, { 0.0f, 1.0f, 0.0f },
        bell, tentacles, core, hitCore);
    Check(!hit.isHit, "sweep does not extend beyond this frame's movement");
    hit = IceJellyfishCollision::Sweep(
        Sphere { core.center, 0.2f }, {}, bell, tentacles, core, hitCore);
    Check(hit.isHit && hitCore && hit.time == 0.0f, "starting inside the core resolves without movement");

    const Matrix4x4 segmentWorld = MatrixMath::MakeAffineMatrix(
        { 6.6f, 8.1f, 6.6f }, Vector3 { 0.3f, 0.5f, 0.8f }, { 30.0f, 5.0f, 0.0f });
    const OBB segment = IceJellyfishCollision::TransformBox(
        segmentWorld, { 0.0f, -0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f });
    Check(std::abs(segment.size.y - 8.1f) < 0.0001f, "longer link scale is included in its OBB");
    const Vector3 expectedCenter = MatrixMath::Transform({ 0.0f, -0.5f, 0.0f }, segmentWorld);
    Check(Vector3Length(segment.center - expectedCenter) < 0.0001f, "OBB follows the root-pivot mesh center");
    Check(std::abs(Dot(segment.orientation[0], segment.orientation[1])) < 0.0001f,
        "rotated OBB axes remain orthogonal");
    const Sphere fastBullet { segment.center - segment.orientation[0] * 50.0f, 0.15f };
    hit = CollisionManager::SweepSphere(fastBullet, segment.orientation[0] * 100.0f, segment);
    Check(hit.isHit && hit.time > 0.4f && hit.time < 0.5f, "fast bullet hits a rotated tentacle link");

    bell.clear();
    tentacles = {
        CollisionManager::MakeOBB({ -6.0f, -10.0f, 0.0f }, { 2.0f, 8.0f, 2.0f }, {}),
        CollisionManager::MakeOBB({ 6.0f, -10.0f, 0.0f }, { 2.0f, 8.0f, 2.0f }, {})
    };
    hit = IceJellyfishCollision::Sweep(
        Sphere { { 0.0f, -10.0f, -20.0f }, 0.2f }, { 0.0f, 0.0f, 40.0f },
        bell, tentacles, core, hitCore);
    Check(!hit.isHit, "bullet passes between separated tentacles");
    tentacles = { CollisionManager::MakeOBB({ 0.0f, -2.0f, 8.0f }, { 2.0f, 2.0f, 2.0f }, {}) };
    const Sphere approach { { 0.0f, -2.0f, -20.0f }, 0.2f };
    hit = IceJellyfishCollision::Sweep(approach, { 0.0f, 0.0f, 40.0f }, bell, tentacles, core, hitCore);
    Check(hit.isHit && hitCore, "core wins when armor is behind it");
    tentacles[0].center.z = -8.0f;
    hit = IceJellyfishCollision::Sweep(approach, { 0.0f, 0.0f, 40.0f }, bell, tentacles, core, hitCore);
    Check(hit.isHit && !hitCore, "front armor prevents damage to a core behind it");

    const Matrix4x4 body = MatrixMath::MakeAffineMatrix(
        { 1.0f, 1.0f, 1.0f }, Vector3 { 0.04f, 0.12f, -0.045f }, { 3.0f, 22.4f, 3000.0f });
    IceJellyfishCollision::BuildBell(body, 1.0f, bell);
    tentacles.clear();
    const Sphere movingCore { MatrixMath::Transform(core.center, body), core.radius };
    const Vector3 start = MatrixMath::Transform({ 0.0f, -20.0f, 0.0f }, body);
    const Vector3 end = MatrixMath::Transform({ 0.0f, 30.0f, 0.0f }, body);
    hit = IceJellyfishCollision::Sweep(Sphere { start, 0.2f }, end - start,
        bell, tentacles, movingCore, hitCore);
    Check(hit.isHit && hitCore, "pulsing and tilting the bell preserves the opening");

    CollisionManager* manager = CollisionManager::GetInstance();
    manager->RegisterRaycastSphereTarget(1, core, 4u);
    manager->RegisterRaycastObbTarget(2,
        CollisionManager::MakeOBB({ 0.0f, -2.0f, -8.0f }, { 2.0f, 2.0f, 2.0f }, {}), 2u);
    const Ray aim { approach.center, { 0.0f, 0.0f, 1.0f } };
    RaycastHit aimHit {};
    Check(manager->Raycast(aim, aimHit) && aimHit.objectId == 2, "aim ray stops on front armor");
    Check(manager->Raycast(aim, aimHit, 4u) && aimHit.objectId == 1, "OBB raycast respects layer masks");
    manager->ClearRaycastObbTargets();
    Check(manager->Raycast(aim, aimHit) && aimHit.objectId == 1, "clearing OBB targets removes stale armor");
    manager->ClearRaycastSphereTargets();
    Check(!manager->Raycast(aim, aimHit), "scene cleanup removes all dynamic aim targets");
    CollisionManager::Finalize();
    std::cout << "Passed " << checks << " ice jellyfish collision checks.\n";
}
