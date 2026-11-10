#pragma once
#include "App/Game/Enemy/Bullet/EnemyBulletManager.h"

// 通常敵とボスが同じ敵弾管理へ弾を渡す。
class EnemyBulletService {
public:
    static void SetManager(EnemyBulletManager* bulletManager) { bulletManager_ = bulletManager; }
    static EnemyBulletManager* GetManager() { return bulletManager_; }
    static std::unique_ptr<NormalEnemyBullet> AcquireNormalBullet(Model* model)
    {
        if (bulletManager_ != nullptr) { return bulletManager_->AcquireNormalBullet(model); }
        auto bullet = std::make_unique<NormalEnemyBullet>();
        bullet->Initialize(model);
        return bullet;
    }
private:
    inline static EnemyBulletManager* bulletManager_ = nullptr;
};
