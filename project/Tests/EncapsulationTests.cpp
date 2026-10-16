#include "App/Game/Enemy/Bullet/EnemyBulletManager.h"
#include "App/Game/Enemy/SwarmEnemy/SwarmEnemy.h"
#include "App/Game/Player/Weapon/Common/PlayerHomingLock.h"
#include "App/Game/Boss/Common/BaseBoss.h"
#include <cassert>
#include <type_traits>
#include <utility>
#if defined(VERIFY_GAME_OBJECTS)
#include "App/Scene/Common/SceneManager.h"
#include "App/Game/Enemy/MoveEnemy/MoveEnemy.h"
#include "Engine/Animation/AnimationActor.h"
#include "Engine/CollisionManager/BoxCollider.h"
#include "Engine/Renderer/SceneRenderResolution.h"
#include "Engine/Reflection/ScreenSpaceReflection.h"
#include "Engine/PostEffect/CopyImageRenderer.h"
#include "Engine/PostEffect/Bloom/BloomRenderer.h"
#include "Engine/PostEffect/Fog/FogManager.h"
#include <limits>

static_assert(std::is_same_v<decltype(std::declval<Camera&>().GetTranslate()), const Vector3&>);
static_assert(std::is_same_v<decltype(std::declval<Object3d&>().GetMaterial()), const Material*>);
static_assert(std::is_same_v<decltype(std::declval<AnimationActor&>().GetSkeleton()), const Skeleton*>);
static_assert(std::is_same_v<decltype(std::declval<Skeleton&>().GetJoints()), const std::vector<Joint>&>);
static_assert(std::is_same_v<decltype(std::declval<Joint&>().GetTransform()), const QuaternionTransform&>);
static_assert(std::is_same_v<decltype(std::declval<CopyImageRenderer&>().GetPostEffectParameter()), const CopyImageRenderer::PostEffectParameter&>);

void TestEngineEncapsulation()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Camera camera;
    camera.SetNearClip(-1); camera.SetFarClip(camera.GetNearClip()); camera.SetFovY(0);
    camera.SetFovY(nan); camera.SetTranslate({nan, 0, 0});
    assert(camera.GetNearClip() == 0.1f && camera.GetFarClip() == 1000 && camera.GetFovY() == 0.45f);
    assert(camera.GetTranslate().z == -20);
    BoxCollider collider;
    collider.SetSize({1, 2, 3}); collider.SetSize({-1, 2, 3}); collider.SetSize({nan, 2, 3});
    assert(collider.GetSize().x == 1);
    const int width = SceneRenderResolution::GetWidth();
    SceneRenderResolution::SetSize(0, -1);
    assert(SceneRenderResolution::GetWidth() == width);
    ScreenSpaceReflection reflection;
    auto settings = reflection.GetSettings(); settings.maxDistance = -1;
    assert(!reflection.SetSettings(settings));
    settings = reflection.GetSettings(); settings.historyWeight = nan;
    assert(!reflection.SetSettings(settings));
    Node node {}; node.name = "root"; node.transform = {{1,1,1}, {0,0,0,1}, {0,0,0}};
    Node child = node; child.name = "child"; node.children.push_back(child);
    Skeleton skeleton = Skeleton::CreateSkeleton(node);
    assert(skeleton.GetJoints().size() == 2 && skeleton.GetJoints()[1].GetParent().value() == 0);
    assert(!skeleton.SetJointRotation(99, {0,0,0,1}));
    assert(!skeleton.SetJointRotation(0, {0,0,0,0}));
    assert(!skeleton.OffsetJointTranslation(0, {nan,0,0}));
    assert(skeleton.OffsetJointTranslation(0, {2,0,0}));
    assert(skeleton.GetJoints()[1].GetSkeletonSpaceMatrix().m[3][0] == 2);
    Node other = node; other.name = "different";
    assert(!skeleton.ApplyPose(Skeleton::CreateSkeleton(other)));

    CopyImageRenderer::PostEffectParameter parameters {};
    parameters.pixelSize = 1; parameters.focusRange = 0.01f; parameters.toneExposure = 1;
    parameters.outlineNearClip = 0.1f; parameters.outlineFarClip = 1000;
    parameters.radialBlurSampleCount = 1; parameters.motionBlurSampleCount = 1; parameters.bokehSides = 3;
    parameters.ssaoSettings.y = 1;
    assert(CopyImageRenderer::IsValidParameter(parameters));
    parameters.radialBlurSampleCount = 128;
    assert(CopyImageRenderer::IsValidParameter(parameters));
    parameters.radialBlurSampleCount = 129;
    assert(!CopyImageRenderer::IsValidParameter(parameters));
    parameters.radialBlurSampleCount = 1;
    parameters.pixelSize = 0; assert(!CopyImageRenderer::IsValidParameter(parameters));
    parameters.pixelSize = 1; parameters.toneExposure = nan;
    assert(!CopyImageRenderer::IsValidParameter(parameters));
    BloomRenderer::BloomParameter bloom {}; bloom.blurSigma = 1;
    assert(BloomRenderer::IsValidParameter(bloom));
    bloom.blurSigma = 0; assert(!BloomRenderer::IsValidParameter(bloom));
    FogData fog {}; fog.distance = {0, 100, 1, 1}; fog.noise.scale = 1;
    fog.volumetric.stepLength = 1; fog.volumetric.maxDistance = 100;
    fog.nearClip = 0.1f; fog.farClip = 100; fog.fovY = 0.45f; fog.aspectRatio = 1;
    assert(FogManager::IsValidParameter(fog));
    fog.distance.end = 0; assert(!FogManager::IsValidParameter(fog));
    fog.distance.end = 100; fog.distance.density = nan; assert(!FogManager::IsValidParameter(fog));
    int stored = 3;
    {
        SettingsEdit<int> edit(stored, [&stored](int, int candidate) { if (candidate < 0) return false; stored = candidate; return true; });
        edit.Get() = -1;
        assert(stored == 3 && !edit.Commit());
    }
    assert(stored == 3);
}
#endif

// Callers cannot modify the owning containers through the public getters.
static_assert(std::is_same_v<decltype(std::declval<const EnemyBulletManager&>().GetBullets()),
    const std::vector<std::unique_ptr<EnemyBullet>>&>);
static_assert(std::is_same_v<decltype(std::declval<const PlayerHomingLock&>().GetTargets()),
    std::shared_ptr<const std::vector<BaseEnemy*>>>);
// The active boss hierarchy shares enemy behavior and supports polymorphic deletion.
static_assert(std::is_base_of_v<BaseEnemy, BaseBoss>);
static_assert(std::is_abstract_v<BaseBoss>);
static_assert(std::has_virtual_destructor_v<BaseBoss>);

int main()
{
    SwarmGroupState group(3);
    group.RecordRemoval(false);
    group.RecordRemoval(true);
    group.RecordRemoval(false);
    group.RecordRemoval(true); // Exhausted groups cannot count another removal.
    assert(group.GetActiveCount() == 0);
    assert(group.GetDefeatedCount() == 2);
    assert(group.GetEscapedCount() == 1);
    assert(group.GetTotalCount() == group.GetActiveCount() +
        group.GetDefeatedCount() + group.GetEscapedCount());
    SwarmGroupState invalid(-1);
    invalid.RecordRemoval(false);
    assert(invalid.GetTotalCount() == 0 && invalid.GetDefeatedCount() == 0);
#if defined(VERIFY_GAME_OBJECTS)
    TestEngineEncapsulation();
    BaseEnemy enemy;
    enemy.ApplyDamage(-10.0f);
    enemy.ApplyDamage(std::numeric_limits<float>::quiet_NaN());
    assert(!enemy.IsDead());
    enemy.ApplyDamage(1.0f);
    assert(enemy.IsDead()); // Invalid damage must not have increased the initial HP.
    BaseBullet bullet;
    bullet.SetDamage(-1);
    bullet.SetCollisionRadius(-1.0f);
    bullet.SetCollisionRadius(std::numeric_limits<float>::infinity());
    assert(bullet.GetDamage() == 1 && bullet.GetCollisionRadius() == 1.0f);
    MoveEnemy movingEnemy;
    movingEnemy.SetMoveSpeed(2.0f);
    movingEnemy.SetMoveSpeed(-1.0f);
    movingEnemy.SetMoveSpeed(std::numeric_limits<float>::quiet_NaN());
    assert(movingEnemy.GetMoveSpeed() == 2.0f);
    auto* scene = SceneManager::GetInstance();
    scene->SetPaintProgress(2.0f);
    assert(scene->GetPaintProgress() == 1.0f);
    scene->SetPaintProgress(std::numeric_limits<float>::quiet_NaN());
    assert(scene->GetPaintProgress() == 1.0f);
    scene->SetBlackHoleRadius(-1.0f);
    assert(scene->GetBlackHoleRadius() == 0.0f);
    scene->SetPaintPatternType(2);
    scene->SetPaintPatternType(99);
    assert(scene->GetPaintPatternType() == 2);
    Vector4 color {};
    float duration = 0.0f;
    PageTransition::RequestReveal({}, 0.0f);
    assert(PageTransition::RevealRequest::GetInstance().Consume(color, duration));
    assert(duration == 0.01f);
    assert(!PageTransition::RevealRequest::GetInstance().Consume(color, duration));
#endif
}
