#pragma once

#include "App/Game/Enemy/BaseEnemy.h"
#include "NormalEnemySettings.h"

class Player;
class Model;

class NormalEnemy : public BaseEnemy {
public:
    std::string_view GetAppearanceId() const override { return "NormalEnemy"; }
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    void Update() override;
    void SetRadialShotEnabled(bool isEnabled);
    void ApplySettings(const NormalEnemySettings& settings);

protected:
    void SetBulletSpeed(float bulletSpeed);
    void SetFireInterval(int32_t fireIntervalFrames);

private:
    void Attack() override;
    void FireBullet();
    void SpawnBullet(const Vector3& direction, bool shouldPlayShotSound);

private:
    Player* player_ = nullptr;

    Model* bulletModel_ = nullptr;

    int32_t fireTimer_ = 0;

    int32_t fireInterval_ = 120;

    float bulletSpeed_ = 0.48f;
    bool isRadialShotEnabled_ = true;
};
