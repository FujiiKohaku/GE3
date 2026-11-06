#pragma once

#include "App/Game/Enemy/NormalEnemy/NormalEnemy.h"
#include "App/Game/Enemy/PaceEnemy/PaceEnemyPath.h"

class PaceEnemy : public NormalEnemy {
public:
    std::string_view GetAppearanceId() const override { return "PaceEnemy"; }
    void Initialize(Model* model, Model* bulletModel, Player* player);
    void SetPosition(const Vector3& position) override;
    void ConfigureFlight(const LevelData::PaceEnemySpawnData& settings);
    void Update() override;
    void Draw() override;
    void DrawShadow(ShadowMapRenderer& renderer) override;
    void SubmitRaytracing(DxrRenderer& renderer) override;
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;

private:
    friend struct PaceEnemyTestAccess;
    void UpdateRailFrame();
    void Move() override;
    void Attack() override;
    Player* player_ = nullptr;
    LevelData::PaceEnemySpawnData settings_;
    Vector3 anchorPosition_ {};
    Vector3 flightOffset_ {};
    Vector3 railOrigin_ {};
    Vector3 railRight_ { 1.0f, 0.0f, 0.0f };
    Vector3 railUp_ { 0.0f, 1.0f, 0.0f };
    Vector3 railForward_ { 0.0f, 0.0f, 1.0f };
    float elapsedSeconds_ = 0.0f;
    bool hasStartedFlight_ = false;
    bool isPacing_ = false;
};
