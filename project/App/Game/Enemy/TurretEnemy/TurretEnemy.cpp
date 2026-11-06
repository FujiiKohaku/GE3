#include "TurretEnemy.h"
#include "App/Game/Enemy/Bullet/Types/TurretEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/audio/SoundManager.h"
#include <limits>

namespace {
constexpr float kAttackRange = 170.0f;
constexpr float kPassedDistance = 40.0f;
constexpr float kAimToleranceCosine = 0.9961947f; // 5度
}

void TurretEnemy::ConfigurePart(Object3d& object, Model* model)
{
    object.SetModel(model);
    object.SetMaterial("resources/Shaders/Object3D/ShadowStandard");
    object.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    object.SetNormalMapStrength(0.0f);
    object.SetSurfaceProperties(0.4f, 0.35f, 0.65f);
    object.SetCastShadow(true);
    object.SetReceiveShadow(true);
}

TurretEnemy::LocalBounds TurretEnemy::CalculateLocalBounds(const Model& model)
{
    const float maximumValue = (std::numeric_limits<float>::max)();
    Vector3 minimum { maximumValue, maximumValue, maximumValue };
    Vector3 maximum { -maximumValue, -maximumValue, -maximumValue };
    for (const MeshPrimitive& primitive : model.GetModelData().primitives) {
        for (const VertexData& vertex : primitive.vertices) {
            minimum.x = (std::min)(minimum.x, vertex.position.x);
            minimum.y = (std::min)(minimum.y, vertex.position.y);
            minimum.z = (std::min)(minimum.z, vertex.position.z);
            maximum.x = (std::max)(maximum.x, vertex.position.x);
            maximum.y = (std::max)(maximum.y, vertex.position.y);
            maximum.z = (std::max)(maximum.z, vertex.position.z);
        }
    }
    return { (minimum + maximum) * 0.5f, maximum - minimum };
}

void TurretEnemy::Initialize(Model* baseModel, Model* yawModel, Model* barrelModel, Model* bulletModel,
    Player* player, const NormalEnemySettings& settings)
{
    transform_ = {};
    BaseEnemy::Initialize(baseModel);
    NormalEnemySettings appliedSettings = settings;
    if (!IsValidNormalEnemySettings(appliedSettings)) { appliedSettings = {}; }
    InitializeHealth(appliedSettings.hp);
    fireIntervalSeconds_ = static_cast<float>(appliedSettings.fireIntervalFrames) / 60.0f;
    bulletSpeedPerFrame_ = appliedSettings.bulletSpeedPerFrame;
    player_ = player;
    bulletModel_ = bulletModel;
    const float scale = TurretEnemyAim::kDisplayScale;
    transform_.scale = { scale, scale, scale };
    yawRadians_ = std::numbers::pi_v<float>;
    pitchRadians_ = 0.0f;
    shotCycle_.Reset();
    shotElapsedSeconds_ = TurretShotFeedback::kRecoilDurationSeconds;
    ConfigurePart(*object_, baseModel);
    yawObject_ = std::make_unique<Object3d>();
    yawObject_->Initialize(Object3dManager::GetInstance());
    ConfigurePart(*yawObject_, yawModel);
    barrelObject_ = std::make_unique<Object3d>();
    barrelObject_->Initialize(Object3dManager::GetInstance());
    ConfigurePart(*barrelObject_, barrelModel);
    muzzleFlashObject_ = std::make_unique<Object3d>();
    muzzleFlashObject_->Initialize(Object3dManager::GetInstance());
    muzzleFlashObject_->SetModel(bulletModel);
    muzzleFlashObject_->SetPixelShaderPath("resources/Shaders/Object3D/TurretMuzzle/Render.PS.hlsl");
    muzzleFlashObject_->SetEnableLighting(false);
    muzzleFlashObject_->SetTransparent(true);
    muzzleFlashObject_->SetTransparentDepthWrite(false);
    muzzleFlashObject_->SetFogEnabled(false);
    muzzleFlashObject_->SetCastShadow(false);
    muzzleFlashObject_->SetReceiveShadow(false);
    localBounds_[0] = CalculateLocalBounds(*baseModel);
    localBounds_[1] = CalculateLocalBounds(*yawModel);
    localBounds_[2] = CalculateLocalBounds(*barrelModel);
    UpdatePose();
}

void TurretEnemy::SetPosition(const Vector3& position)
{
    BaseEnemy::SetPosition(position);
    UpdatePose();
}

void TurretEnemy::SetRotate(const Vector3& rotation)
{
    BaseEnemy::SetRotate(rotation);
    UpdatePose();
}

void TurretEnemy::UpdatePose()
{
    pose_ = TurretEnemyAim::CalculatePose(transform_, yawRadians_, pitchRadians_,
        TurretShotFeedback::CalculateRecoil(shotElapsedSeconds_));
    if (object_ != nullptr) { object_->SetCustomWorldMatrix(pose_.baseWorld); object_->Update(); }
    if (yawObject_ != nullptr) { yawObject_->SetCustomWorldMatrix(pose_.yawWorld); yawObject_->Update(); }
    if (barrelObject_ != nullptr) { barrelObject_->SetCustomWorldMatrix(pose_.barrelWorld); barrelObject_->Update(); }
}

void TurretEnemy::Update()
{
    if (IsDead()) { return; }
    if (player_ != nullptr) {
        const float forwardDistance = Dot(transform_.translate - player_->GetTranslate(), player_->GetFlightForward());
        if (forwardDistance < -kPassedDistance) { SetDead(true); return; }
    }
    BaseEnemy::Update();
}

void TurretEnemy::Move()
{
    if (player_ == nullptr || player_->IsDead()) { return; }
    float targetYawRadians = yawRadians_;
    float targetPitchRadians = pitchRadians_;
    if (TurretEnemyAim::CalculateTargetAngles(pose_.baseWorld, player_->GetTranslate(), targetYawRadians, targetPitchRadians)) {
        const float deltaSeconds = TimeManager::GetInstance()->GetDeltaTime();
        yawRadians_ = TurretEnemyAim::ApproachAngle(yawRadians_, targetYawRadians, deltaSeconds);
        pitchRadians_ = TurretEnemyAim::ApproachAngle(pitchRadians_, targetPitchRadians, deltaSeconds);
    }
    UpdatePose();
}

void TurretEnemy::Attack()
{
    if (player_ == nullptr || player_->IsDead() || bulletModel_ == nullptr) { return; }
    const Vector3 toPlayer = player_->GetTranslate() - pose_.muzzlePosition;
    const float distance = Vector3Length(toPlayer);
    const float forwardDistance = Dot(transform_.translate - player_->GetTranslate(), player_->GetFlightForward());
    const bool isInRange = forwardDistance > 0.0f && distance >= 0.001f && distance <= kAttackRange;
    bool isAligned = false;
    if (isInRange) { isAligned = Dot(toPlayer * (1.0f / distance), pose_.forward) >= kAimToleranceCosine; }
    const float deltaSeconds = TimeManager::GetInstance()->GetDeltaTime();
    if (!shotCycle_.Advance(isInRange, isAligned, deltaSeconds, fireIntervalSeconds_)) { return; }
    std::unique_ptr<TurretEnemyBullet> bullet = std::make_unique<TurretEnemyBullet>();
    bullet->Initialize(bulletModel_);
    bullet->SetTranslate(pose_.muzzlePosition);
    bullet->SetLaunchDirection(pose_.forward, bulletSpeedPerFrame_);
    AddEnemyBullet(std::move(bullet), false);
    SoundManager::GetInstance()->Play("TurretShot");
    shotMuzzlePosition_ = pose_.muzzlePosition;
    shotElapsedSeconds_ = 0.0f;
}

void TurretEnemy::UpdateAnimation()
{
    const float deltaSeconds = TimeManager::GetInstance()->GetDeltaTime();
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) { return; }
    const bool wasRecoiling = shotElapsedSeconds_ < TurretShotFeedback::kRecoilDurationSeconds;
    shotElapsedSeconds_ = (std::min)(TurretShotFeedback::kRecoilDurationSeconds, shotElapsedSeconds_ + deltaSeconds);
    if (wasRecoiling) { UpdatePose(); }
    if (shotElapsedSeconds_ < TurretShotFeedback::kFlashDurationSeconds) {
        const float flashRatio = 1.0f - shotElapsedSeconds_ / TurretShotFeedback::kFlashDurationSeconds;
        const float flashScale = 0.55f + flashRatio * 0.65f;
        muzzleFlashObject_->SetTranslate(shotMuzzlePosition_);
        muzzleFlashObject_->SetScale({ flashScale, flashScale, flashScale });
        muzzleFlashObject_->SetColor({ 1.8f, 1.35f, 2.0f, flashRatio });
        muzzleFlashObject_->Update();
    }
}

void TurretEnemy::Draw()
{
    if (IsDead()) { return; }
    BaseEnemy::Draw();
    yawObject_->Draw();
    barrelObject_->Draw();
    if (shotElapsedSeconds_ < TurretShotFeedback::kFlashDurationSeconds) { muzzleFlashObject_->Draw(); }
}

void TurretEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (IsDead()) { return; }
    BaseEnemy::DrawShadow(renderer);
    yawObject_->DrawShadow(renderer);
    barrelObject_->DrawShadow(renderer);
}

void TurretEnemy::SubmitRaytracing(DxrRenderer& renderer)
{
    if (IsDead()) { return; }
    BaseEnemy::SubmitRaytracing(renderer);
    yawObject_->SubmitRaytracing(renderer);
    barrelObject_->SubmitRaytracing(renderer);
}

void TurretEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (IsDead() || object_ == nullptr) { return; }
    const std::array<Matrix4x4, 3> worldMatrices { pose_.baseWorld, pose_.yawWorld, pose_.barrelWorld };
    for (std::size_t index = 0; index < localBounds_.size(); ++index) {
        const Matrix4x4& matrix = worldMatrices[index];
        EnemyCollisionPart part;
        part.partIndex = static_cast<int32_t>(index);
        part.isBox = true;
        part.position = MatrixMath::Transform(localBounds_[index].center, matrix);
        part.box.center = part.position;
        part.box.size = localBounds_[index].size * TurretEnemyAim::kDisplayScale;
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            part.box.orientation[axisIndex] = Normalize(Vector3 {
                matrix.m[axisIndex][0], matrix.m[axisIndex][1], matrix.m[axisIndex][2] });
        }
        part.radius = Vector3Length(part.box.size) * 0.5f;
        parts.push_back(part);
    }
}
