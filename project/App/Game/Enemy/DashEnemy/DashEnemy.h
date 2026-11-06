#pragma once
#include "App/Game/Enemy/BaseEnemy.h"
#include "DashEnemyMotion.h"

class Player;

class DashEnemy : public BaseEnemy {
public:
    static constexpr float kBodySize = 4.0f;
    static constexpr float kAttackRadius = 2.0f;
    std::string_view GetAppearanceId() const override { return "DASHENEMY"; }
    void Initialize(Model* cube, Player* player, const LevelData::DashEnemySpawnData& settings);
    void SetPosition(const Vector3& position) override;
    Vector3 GetPosition() const override { return motion_.position; }
    void Update() override;
    void Draw() override;
    void DrawShadow(ShadowMapRenderer& renderer) override;
    void SubmitRaytracing(DxrRenderer& renderer) override;
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;
    SweepHit SweepBullet(const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const override;
    const DashEnemyMotion& GetMotion() const { return motion_; }
    void StopAtObstacle(size_t segment, float fraction);
    void SetWarningLength(float length);
    float GetWarningLength() const { return warningLength_; }
    SweepHit FindPlayerContact(const Vector3& previous, const Vector3& current) const;
    void ConsumePlayerContact() { hasContactedPlayer_ = true; }

private:
    friend struct DashEnemyTestAccess;
    void UpdateVisuals();
    DashEnemyMotion motion_;
    Player* player_ = nullptr;
    std::array<std::unique_ptr<Object3d>, 3> warningObjects_;
    std::unique_ptr<Object3d> trail_;
    float warningLength_ = 160.0f;
    bool hasContactedPlayer_ = false;
    bool hasPreviousOrigin_ = false;
    Vector3 previousOrigin_ {};
};
