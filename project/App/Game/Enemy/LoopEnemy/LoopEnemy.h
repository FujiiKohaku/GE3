#pragma once

#include "App/Game/Enemy/NormalEnemy/NormalEnemy.h"
#include "App/Game/Enemy/LoopEnemy/LoopEnemyPath.h"

// 通常弾を撃ちながら、進入・一周・離脱の順に飛ぶ敵。
class LoopEnemy : public NormalEnemy {
public:
    std::string_view GetAppearanceId() const override { return "LoopEnemy"; }
    void Initialize(Model* model, Model* bulletModel, Player* player);
    void SetPosition(const Vector3& position) override;
    void ConfigureFlight(bool isEnteringFromLeft, float spawnDelaySeconds, float activationDistance);
    void Update() override;
    void Draw() override;
    void DrawShadow(ShadowMapRenderer& renderer) override;
    void SubmitRaytracing(DxrRenderer& renderer) override;
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;

private:
    void UpdateRailFrame();
    void Move() override;

    Player* player_ = nullptr;
    Vector3 anchorPosition_ {};
    Vector3 railOrigin_ {};
    Vector3 railRight_ { 1.0f, 0.0f, 0.0f };
    Vector3 railUp_ { 0.0f, 1.0f, 0.0f };
    Vector3 railForward_ { 0.0f, 0.0f, 1.0f };
    Vector3 centerOffset_ {};
    float elapsedSeconds_ = 0.0f;
    float spawnDelaySeconds_ = 0.0f;
    float activationDistance_ = 120.0f;
    bool isEnteringFromLeft_ = false;
    bool hasTriggeredFlight_ = false;
    bool hasStartedFlight_ = false;
    bool hasEscaped_ = false;
};
