#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"
#include "Engine/Math/MatrixMath.h"
#include <cmath>
#include <iostream>
#include <cstdlib>

namespace {
SweepHit SweepJellyfishCollision(const Sphere& bullet, const Vector3& movement,
    const std::vector<OBB>& bell, const std::vector<OBB>& tentacles,
    const Sphere& core, bool& hitCore)
{
    SweepHit nearest {};
    hitCore = false;
    for (const OBB& box : bell) {
        const SweepHit hit = CollisionManager::SweepSphere(bullet, movement, box);
        if (hit.isHit && (!nearest.isHit || hit.time < nearest.time)) {
            nearest = hit;
        }
    }
    for (const OBB& box : tentacles) {
        const SweepHit hit = CollisionManager::SweepSphere(bullet, movement, box);
        if (hit.isHit && (!nearest.isHit || hit.time < nearest.time)) {
            nearest = hit;
        }
    }
    const SweepHit coreHit = CollisionManager::SweepSphere(bullet, movement, core);
    if (coreHit.isHit && (!nearest.isHit || coreHit.time < nearest.time)) {
        nearest = coreHit;
        hitCore = true;
    }
    return nearest;
}
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

int RunIceJellyfishCollisionTests()
{
    const Matrix4x4 identity = MatrixMath::MakeIdentity4x4();
    std::vector<OBB> bell;
    std::vector<OBB> tentacles;
    const Sphere core { { 0.0f, -2.0f, 0.0f }, 3.6f };
    bool hitCore = false;
    IceJellyfish::BuildBellColliders(identity, 0.0f, bell);
    Check(bell.size() == 208, "bell panels and icicle boxes are present");

    const std::vector<OBB> unanimatedBell = bell;
    for (float time : { 0.0f, 1.4279967f, 4.283990f }) {
        const Vector4 animation { time, 1.5f, 0.0f, 0.0f };
        IceJellyfish::BuildBellColliders(identity, 0.0f, bell, animation);
        // シェーダー式から独立に求めた、先頭の氷の縁の中心位置。
        const float phase = time * 1.10f;
        const float radialScale = 1.0f + 1.5f * 0.020f * std::sin(phase);
        const float rimWeight = (21.45f * 21.45f) / (22.0f * 22.0f);
        const float wave = std::sin(phase + 21.45f * 0.16f) * 0.65f +
            std::sin(phase - 0.45f - 21.45f * 0.11f) * 0.35f;
        const Vector3 surface { 21.45f * radialScale, -1.55f + 1.5f * rimWeight * wave, 0.0f };
        const OBB& tooth = bell[192];
        const Vector3 delta = surface - tooth.center;
        Check(std::abs(Dot(delta, tooth.orientation[0])) <= tooth.size.x * 0.5f + 0.001f &&
            std::abs(Dot(delta, tooth.orientation[1])) <= tooth.size.y * 0.5f + 0.001f &&
            std::abs(Dot(delta, tooth.orientation[2])) <= tooth.size.z * 0.5f + 0.001f,
            "shader-deformed rim remains inside its collider at different phases");
        Check(Vector3Length(tooth.center - unanimatedBell[192].center) > 0.1f,
            "bell collision actually moves with shader animation");
        bool orthogonal = true;
        for (const OBB& box : bell) {
            orthogonal &= std::abs(Dot(box.orientation[0], box.orientation[1])) < 0.0001f &&
                std::abs(Dot(box.orientation[1], box.orientation[2])) < 0.0001f &&
                std::abs(Dot(box.orientation[0], box.orientation[2])) < 0.0001f;
        }
        Check(orthogonal, "shader deformation preserves orthogonal collision axes");
    }
    IceJellyfish::BuildBellColliders(identity, 0.0f, bell);

    const std::vector<OBB> restBell = bell;
    const Matrix4x4 contractedWorld = MatrixMath::MakeAffineMatrix(
        { 0.9f, 0.8f, 0.9f }, Vector3 {}, { 3.0f, 4.0f, 5.0f });
    IceJellyfish::BuildBellColliders(contractedWorld, 0.0f, bell);
    Check(Vector3Length(bell[0].center - MatrixMath::Transform(restBell[0].center, contractedWorld)) < 0.0001f,
        "bell collider follows model contraction and lift");
    for (const OBB& box : bell) {
        Check(std::abs(Dot(box.orientation[0], box.orientation[1])) < 0.0001f &&
            std::abs(Dot(box.orientation[1], box.orientation[2])) < 0.0001f &&
            std::abs(Dot(box.orientation[0], box.orientation[2])) < 0.0001f,
            "contracted bell keeps orthogonal OBB axes");
    }
    const Sphere rimShot { { 24.5f, 2.0f, 5.0f }, 0.1f };
    bool touchedOldRim = false;
    bool touchedContractedRim = false;
    std::vector<OBB> translatedRestBell;
    IceJellyfish::BuildBellColliders(MatrixMath::MakeAffineMatrix(
        { 1.0f, 1.0f, 1.0f }, Vector3 {}, { 3.0f, 4.0f, 5.0f }), 0.0f, translatedRestBell);
    for (const OBB& box : translatedRestBell) touchedOldRim |= CollisionManager::SweepSphere(rimShot, {}, box).isHit;
    for (const OBB& box : bell) touchedContractedRim |= CollisionManager::SweepSphere(rimShot, {}, box).isHit;
    Check(touchedOldRim && !touchedContractedRim, "contracted bell no longer blocks shots at the old rim");
    IceJellyfish::BuildBellColliders(identity, 0.0f, bell);

    SweepHit hit = SweepJellyfishCollision(
        Sphere { { 0.0f, -20.0f, 0.0f }, 0.2f }, { 0.0f, 50.0f, 0.0f },
        bell, tentacles, core, hitCore);
    Check(hit.isHit && hitCore, "a shot from below enters the hollow bell and hits the core");
    hit = SweepJellyfishCollision(
        Sphere { { 0.0f, 30.0f, 0.0f }, 0.2f }, { 0.0f, -60.0f, 0.0f },
        bell, tentacles, core, hitCore);
    Check(hit.isHit && !hitCore && hit.time < 0.4f, "a fast shot from above hits armor before the core");
    hit = SweepJellyfishCollision(
        Sphere { { 8.0f, 0.0f, -10.0f }, 0.2f }, { 0.0f, 0.0f, 20.0f },
        bell, tentacles, core, hitCore);
    Check(!hit.isHit, "empty interior is not filled by the bell collision");
    hit = SweepJellyfishCollision(
        Sphere { { 0.0f, -20.0f, 0.0f }, 0.2f }, { 0.0f, 1.0f, 0.0f },
        bell, tentacles, core, hitCore);
    Check(!hit.isHit, "sweep does not extend beyond this frame's movement");
    hit = SweepJellyfishCollision(
        Sphere { core.center, 0.2f }, {}, bell, tentacles, core, hitCore);
    Check(hit.isHit && hitCore && hit.time == 0.0f, "starting inside the core resolves without movement");

    const Matrix4x4 segmentWorld = MatrixMath::MakeAffineMatrix(
        { 6.6f, 8.1f, 6.6f }, Vector3 { 0.3f, 0.5f, 0.8f }, { 30.0f, 5.0f, 0.0f });
    const OBB segment = IceJellyfish::MakeCollisionBox(
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
    hit = SweepJellyfishCollision(
        Sphere { { 0.0f, -10.0f, -20.0f }, 0.2f }, { 0.0f, 0.0f, 40.0f },
        bell, tentacles, core, hitCore);
    Check(!hit.isHit, "bullet passes between separated tentacles");
    tentacles = { CollisionManager::MakeOBB({ 0.0f, -2.0f, 8.0f }, { 2.0f, 2.0f, 2.0f }, {}) };
    const Sphere approach { { 0.0f, -2.0f, -20.0f }, 0.2f };
    hit = SweepJellyfishCollision(approach, { 0.0f, 0.0f, 40.0f }, bell, tentacles, core, hitCore);
    Check(hit.isHit && hitCore, "core wins when armor is behind it");
    tentacles[0].center.z = -8.0f;
    hit = SweepJellyfishCollision(approach, { 0.0f, 0.0f, 40.0f }, bell, tentacles, core, hitCore);
    Check(hit.isHit && !hitCore, "front armor prevents damage to a core behind it");

    const Matrix4x4 body = MatrixMath::MakeAffineMatrix(
        { 1.0f, 1.0f, 1.0f }, Vector3 { 0.04f, 0.12f, -0.045f }, { 3.0f, 22.4f, 3000.0f });
    IceJellyfish::BuildBellColliders(body, 1.0f, bell);
    tentacles.clear();
    const Sphere movingCore { MatrixMath::Transform(core.center, body), core.radius };
    const Vector3 start = MatrixMath::Transform({ 0.0f, -20.0f, 0.0f }, body);
    const Vector3 end = MatrixMath::Transform({ 0.0f, 30.0f, 0.0f }, body);
    hit = SweepJellyfishCollision(Sphere { start, 0.2f }, end - start,
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
    return 0;
}

int main()
{
    return RunIceJellyfishCollisionTests();
}
