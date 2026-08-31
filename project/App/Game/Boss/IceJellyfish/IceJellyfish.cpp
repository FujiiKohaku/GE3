#include "IceJellyfish.h"
#include "IceJellyfishCollision.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/math/MatrixMath.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Effect/EffectManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>

std::unique_ptr<Object3d> IceJellyfish::CreatePart(
    Camera* camera, const char* modelPath, const Vector4& color, bool ice)
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(Object3dManager::GetInstance());
    object->SetModel(ModelManager::GetInstance()->Load(modelPath));
    object->SetCamera(camera);
    object->SetColor(color);
    object->SetEnableLighting(ice);
    object->SetEnableEnvironmentMap(ice);
    object->SetEnvironmentMapStrength(0.38f);
    object->GetMaterial()->shininess = 96.0f;
    return object;
}

void IceJellyfish::Initialize(Camera* camera, const Vector3& position)
{
    basePosition_ = position;
    animationTime_ = 0.0f;
    hp_ = kMaxHp;
    hitFlashTime_ = 0.0f;
    tentacleColliders_.reserve(kTentacleCount * kSegmentsPerTentacle);
    bell_ = CreatePart(camera, "Boss/IceJellyfish/IceJellyfishBell.obj",
        { 0.65f, 0.86f, 1.0f, 1.0f }, true);
    core_ = CreatePart(camera, "Boss/IceJellyfish/IceJellyfishCore.obj",
        { 2.8f, 1.25f, 0.25f, 1.0f }, false);

    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            const char* modelPath = "Boss/IceJellyfish/IceJellyfishSegment.obj";
            if (segment + 1 == kSegmentsPerTentacle) {
                modelPath = "Boss/IceJellyfish/IceJellyfishTip.obj";
            }
            tentacles_[tentacle][segment] = CreatePart(camera, modelPath,
                { 0.55f, 0.80f, 1.0f, 1.0f }, true);
        }
    }
    Update(0.0f, 0.0f);
}

void IceJellyfish::Update(float deltaTime, float playerRailZ)
{
    if (bell_ == nullptr || IsDead()) {
        return;
    }
    animationTime_ += (std::max)(deltaTime, 0.0f);
    hitFlashTime_ = (std::max)(0.0f, hitFlashTime_ - deltaTime);
    core_->SetColor({ 2.8f, 1.25f, 0.25f, 1.0f });
    if (hitFlashTime_ > 0.0f) {
        core_->SetColor({ 4.0f, 3.4f, 2.8f, 1.0f });
    }
    const float time = animationTime_;
    Vector3 position = basePosition_;
    // Stage03 is straight. Keep the prototype in view after approaching it,
    // including at the end of the rail, without changing stage progression.
    position.z = (std::max)(position.z, playerRailZ + kHoverDistance);
    position.x += std::sin(time * 0.37f) * 3.0f;
    position.y += std::sin(time * 0.85f) * 2.4f;

    const Vector3 rotation = {
        std::sin(time * 0.51f) * 0.035f,
        std::sin(time * 0.27f) * 0.12f,
        std::sin(time * 0.63f) * 0.045f
    };
    const Matrix4x4 bodyMatrix = MatrixMath::MakeAffineMatrix(
        { 1.0f, 1.0f, 1.0f }, rotation, position);
    const float pulse = std::sin(time * 0.85f);
    const Matrix4x4 bellScale = MatrixMath::Matrix4x4MakeScaleMatrix(
        { 1.0f + pulse * 0.018f, 1.0f - pulse * 0.025f, 1.0f + pulse * 0.018f });
    bell_->SetCustomWorldMatrix(MatrixMath::Multiply(bellScale, bodyMatrix));
    bell_->Update();

    const float coreScale = 1.0f + std::sin(time * 1.7f) * 0.05f;
    const Matrix4x4 coreLocal = MatrixMath::MakeAffineMatrix(
        { coreScale, coreScale, coreScale }, Vector3 { 0.0f, time * 0.18f, 0.0f },
        { 0.0f, -2.0f, 0.0f });
    core_->SetCustomWorldMatrix(MatrixMath::Multiply(coreLocal, bodyMatrix));
    core_->Update();

    coreCollider_ = {
        MatrixMath::Transform({}, core_->GetWorldMatrix()), 3.6f * coreScale
    };
    IceJellyfishCollision::BuildBell(bodyMatrix, pulse, bellColliders_);
    tentacleColliders_.clear();

    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        const float azimuth = 2.0f * std::numbers::pi_v<float> *
            static_cast<float>(tentacle) / static_cast<float>(kTentacleCount) + 0.25f;
        const float radialX = std::cos(azimuth);
        const float radialZ = std::sin(azimuth);
        Vector3 joint = { radialX * 10.5f, -1.2f, radialZ * 10.5f };
        float bend = 0.30f;

        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            const float index = static_cast<float>(segment);
            const float phase = time * 1.05f - index * 0.65f + azimuth * 1.3f;
            bend += std::sin(phase) * (0.065f + index * 0.012f);
            const float sway = std::sin(phase * 0.73f) * 0.12f;
            Vector3 down = {
                radialX * std::sin(bend) - radialZ * sway,
                -std::cos(bend),
                radialZ * std::sin(bend) + radialX * sway
            };
            const float inverseLength = 1.0f / std::sqrt(
                down.x * down.x + down.y * down.y + down.z * down.z);
            down = down * inverseLength;
            const Vector3 up = down * -1.0f;
            const Vector3 tangent = { -radialZ, 0.0f, radialX };
            Vector3 right = {
                up.y * tangent.z - up.z * tangent.y,
                up.z * tangent.x - up.x * tangent.z,
                up.x * tangent.y - up.y * tangent.x
            };
            const float inverseRightLength = 1.0f / std::sqrt(
                right.x * right.x + right.y * right.y + right.z * right.z);
            right = right * inverseRightLength;
            const Vector3 forward = {
                right.y * up.z - right.z * up.y,
                right.z * up.x - right.x * up.z,
                right.x * up.y - right.y * up.x
            };
            const float length = (5.4f - index * 0.32f) * 1.5f;
            // Broad roots taper strongly to a slender final tip.
            const float width = 6.6f - index * 1.0f;
            // The OBJ pivot is the root and its length runs down local -Y.
            Matrix4x4 local = MatrixMath::MakeIdentity4x4();
            local.m[0][0] = right.x * width;
            local.m[0][1] = right.y * width;
            local.m[0][2] = right.z * width;
            local.m[1][0] = up.x * length;
            local.m[1][1] = up.y * length;
            local.m[1][2] = up.z * length;
            local.m[2][0] = forward.x * width;
            local.m[2][1] = forward.y * width;
            local.m[2][2] = forward.z * width;
            local.m[3][0] = joint.x;
            local.m[3][1] = joint.y;
            local.m[3][2] = joint.z;
            Object3d& object = *tentacles_[tentacle][segment];
            object.SetCustomWorldMatrix(MatrixMath::Multiply(local, bodyMatrix));
            object.Update();
            tentacleColliders_.push_back(IceJellyfishCollision::TransformBox(
                object.GetWorldMatrix(), { 0.0f, -0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }));
            joint = joint + down * (length + 0.18f);
        }
    }
}

void IceJellyfish::Draw()
{
    if (bell_ == nullptr || IsDead()) {
        return;
    }
    for (auto& tentacle : tentacles_) {
        for (auto& segment : tentacle) {
            segment->Draw();
        }
    }
    core_->Draw();
    bell_->Draw();
}

void IceJellyfish::UpdateDrawMatrices()
{
    if (bell_ == nullptr || IsDead()) {
        return;
    }
    bell_->Update();
    core_->Update();
    for (auto& tentacle : tentacles_) {
        for (auto& segment : tentacle) {
            segment->Update();
        }
    }
}

SweepHit IceJellyfish::SweepBullet(const Sphere& bullet, const Vector3& movement, bool& hitCore) const
{
    hitCore = false;
    if (bell_ == nullptr || IsDead()) {
        return {};
    }
    return IceJellyfishCollision::Sweep(bullet, movement,
        bellColliders_, tentacleColliders_, coreCollider_, hitCore);
}

void IceJellyfish::OnBulletHit(bool hitCore, float damage, const Vector3& position)
{
    if (IsDead()) {
        return;
    }
    if (!hitCore) {
        EffectManager::GetInstance()->PlayEffect("HitEffect", position);
        return;
    }
    hp_ = (std::max)(0.0f, hp_ - (std::max)(damage, 0.0f));
    hitFlashTime_ = 0.12f;
    core_->SetColor({ 4.0f, 3.4f, 2.8f, 1.0f });
    if (IsDead()) {
        EffectManager::GetInstance()->PlayEffect("Explosion", coreCollider_.center);
    }
}

void IceJellyfish::RegisterRaycastTargets(CollisionObjectId& nextObjectId) const
{
    if (bell_ == nullptr || IsDead()) {
        return;
    }
    CollisionManager* manager = CollisionManager::GetInstance();
    for (const OBB& box : bellColliders_) {
        manager->RegisterRaycastObbTarget(nextObjectId++, box);
    }
    for (const OBB& box : tentacleColliders_) {
        manager->RegisterRaycastObbTarget(nextObjectId++, box);
    }
    manager->RegisterRaycastSphereTarget(nextObjectId++, coreCollider_);
}

void IceJellyfish::DrawCollisionDebug() const
{
    if (bell_ == nullptr || IsDead()) {
        return;
    }
    DebugRenderer* renderer = DebugRenderer::GetInstance();
    for (const OBB& box : bellColliders_) {
        renderer->AddWireOBB(box.center, box.size, box.orientation[0],
            box.orientation[1], box.orientation[2], { 0.1f, 0.7f, 1.0f, 1.0f });
    }
    for (const OBB& box : tentacleColliders_) {
        renderer->AddWireOBB(box.center, box.size, box.orientation[0],
            box.orientation[1], box.orientation[2], { 0.8f, 0.3f, 1.0f, 1.0f });
    }
    renderer->AddWireSphere(coreCollider_.center, coreCollider_.radius,
        { 1.0f, 0.6f, 0.1f, 1.0f }, 2.0f);
}
