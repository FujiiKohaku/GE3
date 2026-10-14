#pragma once

#include "App/Game/Enemy/Bullet/EnemyBullet.h"

#include <algorithm>
#include <memory>
#include <vector>

class EnemyBulletManager {
public:
    void SetFogExclusionEnabled(bool enabled)
    {
        fogExclusionEnabled_ = enabled;
    }

    void Add(std::unique_ptr<EnemyBullet> bullet)
    {
        if (bullet) {
            if (fogExclusionEnabled_) {
                bullet->SetFogEnabled(false);
            }
            bullets_.push_back(std::move(bullet));
        }
    }

    void Update(
        const Vector3& playerPosition,
        const Vector3& playerForward)
    {
        constexpr float kBulletCullDistanceBehindPlayer = 120.0f;

        for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
            bullet->Update();

            const Vector3 playerToBullet =
                bullet->GetPosition() - playerPosition;
            const float forwardDistance =
                Dot(playerToBullet, playerForward);
            if (forwardDistance < -kBulletCullDistanceBehindPlayer) {
                bullet->SetDead();
            }
        }

        std::erase_if(
            bullets_,
            [](const std::unique_ptr<EnemyBullet>& bullet) {
                return !bullet->IsAlive();
            });
    }

    void Draw()
    {
        for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
            bullet->Draw();
        }
    }

    const std::vector<std::unique_ptr<EnemyBullet>>& GetBullets() const
    {
        return bullets_;
    }

    void Clear()
    {
        bullets_.clear();
    }

private:
    bool fogExclusionEnabled_ = false;
    std::vector<std::unique_ptr<EnemyBullet>> bullets_;
};
