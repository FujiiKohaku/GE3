#pragma once
#include "App/Game/Enemy/BaseEnemy.h"

class Player;

// 海底に固定された試作敵。開いた姿でプレイヤーへ扇状弾を撃つ。
class AnemoneEnemy : public BaseEnemy {
public:
    static constexpr float kDisplayScale = 3.5f;
    void Initialize(Model* model, Model* bulletModel, Player* player, float waterHeight);
    void Update() override;
    Vector3 GetPosition() const override;
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;

private:
    friend struct AnemoneEnemyTestAccess;
    struct LocalBounds { Vector3 center; Vector3 size; };
    void Move() override {}
    void Attack() override;
    bool AdvanceShotCycle(float deltaSeconds);
    Vector3 GetMuzzlePosition() const;
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;
    std::vector<LocalBounds> localBounds_;
    float waterHeight_ = -30.0f;
    float fireElapsedSeconds_ = 1.5f; // 射程に入って0.9秒後に最初の射撃。
};
