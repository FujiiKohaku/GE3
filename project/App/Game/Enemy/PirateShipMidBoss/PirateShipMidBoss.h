#pragma once

#include "App/Game/Enemy/BaseEnemy.h"
#include <memory>

class Model;
class Object3d;
class Player;

class PirateShipMidBoss final : public BaseEnemy {
public:
    void Initialize(Model* cubeModel, Model* bulletModel, Player* player);
    void Update() override;
    void Draw() override;
    void SetPosition(const Vector3& position) override;
    Vector3 GetPosition() const override { return shipPosition_; }
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;
    bool IsCollisionPartDamageable(int32_t partIndex) const override;
    void ApplyDamageToPart(int32_t partIndex, float damage) override;

private:
    enum class State { Emerging, Battle, Sinking };
    static std::unique_ptr<Object3d> CreatePart(Model* model, const Vector3& scale, const Vector4& color);
    void UpdateParts();
    void FireCannons();

    std::unique_ptr<Object3d> hull_;
    std::unique_ptr<Object3d> deck_;
    std::unique_ptr<Object3d> cabin_;
    std::unique_ptr<Object3d> mast_;
    std::unique_ptr<Object3d> sail_;
    std::unique_ptr<Object3d> leftCannons_;
    std::unique_ptr<Object3d> rightCannons_;
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;
    Vector3 surfacePosition_ {};
    Vector3 shipPosition_ {};
    State state_ = State::Emerging;
    float stateTimer_ = 0.0f;
    float battleTimer_ = 0.0f;
    float shotTimer_ = 0.0f;
    float shipHp_ = 80.0f;
    float horizontalVelocity_ = -8.0f;
};
