#pragma once

#include "App/Game/Enemy/NormalEnemy/NormalEnemy.h"
#include "App/Game/Enemy/TurretEnemy/TurretEnemyAim.h"
#include "App/Game/Enemy/TurretEnemy/TurretShotFeedback.h"
#include <array>

// 土台は固定し、旋回台と砲身を独立して動かす砲台。
class TurretEnemy : public BaseEnemy {
public:
    std::string_view GetAppearanceId() const override { return "TurretEnemy"; }
    void Initialize(Model* baseModel, Model* yawModel, Model* barrelModel, Model* bulletModel,
        Player* player, const NormalEnemySettings& settings);
    void SetPosition(const Vector3& position) override;
    void SetRotate(const Vector3& rotation) override;
    void Update() override;
    void Draw() override;
    void DrawShadow(ShadowMapRenderer& renderer) override;
    void SubmitRaytracing(DxrRenderer& renderer) override;
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;

private:
    struct LocalBounds {
        Vector3 center {};
        Vector3 size {};
    };
    void Move() override;
    void Attack() override;
    void UpdateAnimation() override;
    void UpdatePose();
    void ConfigurePart(Object3d& object, Model* model);
    static LocalBounds CalculateLocalBounds(const Model& model);
    std::unique_ptr<Object3d> yawObject_;
    std::unique_ptr<Object3d> barrelObject_;
    std::unique_ptr<Object3d> muzzleFlashObject_;
    std::array<LocalBounds, 3> localBounds_ {};
    TurretEnemyPose pose_;
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;
    float yawRadians_ = std::numbers::pi_v<float>;
    float pitchRadians_ = 0.0f;
    TurretShotCycle shotCycle_;
    float fireIntervalSeconds_ = 2.0f;
    float bulletSpeedPerFrame_ = 0.48f;
    float shotElapsedSeconds_ = TurretShotFeedback::kRecoilDurationSeconds;
    Vector3 shotMuzzlePosition_ {};
};
