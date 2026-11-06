#pragma once

#include "App/Game/Enemy/NormalEnemy/NormalEnemy.h"
#include "App/Game/Enemy/StrafeEnemy/StrafeEnemyPath.h"

class StrafeEnemy : public BaseEnemy {
public:
    std::string_view GetAppearanceId() const override { return "StrafeEnemy"; }
    void Initialize(Model* model, Model* bulletModel, Player* player, const NormalEnemySettings& settings);
    void ConfigureFlight(const LevelData::StrafeEnemySpawnData& settings);
    void SetPosition(const Vector3& position) override;
    void Update() override;
    void Draw() override;
    void DrawShadow(ShadowMapRenderer& renderer) override;
    void SubmitRaytracing(DxrRenderer& renderer) override;
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;

private:
    friend struct StrafeEnemyTestAccess;
    void UpdateRailFrame();
    void Move() override;
    void Attack() override;
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;
    LevelData::StrafeEnemySpawnData settings_;
    Vector3 anchorPosition_ {};
    Vector3 railOrigin_ {};
    Vector3 railRight_ { 1.0f, 0.0f, 0.0f };
    Vector3 railUp_ { 0.0f, 1.0f, 0.0f };
    Vector3 railForward_ { 0.0f, 0.0f, 1.0f };
    float elapsedSeconds_ = 0.0f;
    float bulletSpeedPerFrame_ = 0.48f;
    int scheduledShotsConsumed_ = 0;
    bool hasStartedFlight_ = false;
};
