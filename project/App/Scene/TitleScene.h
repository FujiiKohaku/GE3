#pragma once

#include "BaseScene.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"

#include <array>
#include <memory>
#include <random>

class TitleScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    enum class SceneState {
        Title,
        Playing,
        GameOver,
        Clear,
    };

    enum class BossPattern {
        Vertical,
        WaveSpread,
        Tracking,
        Rush,
    };

    struct Actor {
        Vector2 position = { 0.0f, 0.0f };
        Vector2 velocity = { 0.0f, 0.0f };
        float radius = 0.0f;
        bool active = false;
    };

    struct PlayerBullet {
        Actor actor;
        bool charged = false;
    };

    struct Enemy {
        Actor actor;
        int shotTimer = 0;
        int nextShotDelay = 120;
        int explosionTimer = 0;
        float basePositionY = 0.0f;
        float waveAngle = 0.0f;
        float waveSpeed = 0.0f;
        float waveAmplitude = 0.0f;
    };

    static constexpr int kPlayerBulletCount = 32;
    static constexpr int kEnemyCount = 10;
    static constexpr int kEnemyBulletCount = 16;
    static constexpr int kBossBulletCount = 32;

    void ResetGame();
    void UpdateTitle();
    void UpdatePlaying();
    void UpdatePlayer();
    void UpdatePlayerBullets();
    void UpdateEnemies();
    void UpdateEnemyBullets();
    void UpdateBoss();
    void UpdateBossBullets();
    void CheckCollisions();
    void UpdateSprites();
    void UpdateTexts();

    void FirePlayerBullet(bool charged);
    void FireEnemyBullet(const Vector2& position);
    void FireBossBullet();
    void FireBossSpread();
    void FireBossRadial();
    void FireBossBulletVelocity(const Vector2& velocity);
    void DamagePlayer();

    bool IsCircleHit(const Actor& first, const Actor& second) const;
    bool IsOutsideScreen(const Vector2& position, float margin) const;
    float RandomFloat(float minimum, float maximum);
    int RandomInt(int minimum, int maximum);
    std::unique_ptr<Sprite> CreateSprite(
        const Vector2& position,
        const Vector2& size,
        const Vector4& color);
    std::unique_ptr<Sprite> CreateTexturedSprite(
        const char* texturePath,
        const Vector2& position,
        const Vector2& size);

private:
    SceneState sceneState_ = SceneState::Title;

    Actor player_;
    int playerHp_ = 10;
    int playerInvincibleTimer_ = 0;
    bool wasChargePressed_ = false;
    int chargeTime_ = 0;
    int shotCooldownTimer_ = 0;
    int chargeShotFlashTimer_ = 0;

    std::array<PlayerBullet, kPlayerBulletCount> playerBullets_;
    std::array<Enemy, kEnemyCount> enemies_;
    std::array<Actor, kEnemyBulletCount> enemyBullets_;
    std::array<Actor, kBossBulletCount> bossBullets_;

    Actor boss_;
    int bossHp_ = 20;
    int bossMoveDirection_ = 1;
    int bossShotTimer_ = 0;
    int bossPatternTimer_ = 0;
    BossPattern bossPattern_ = BossPattern::Vertical;

    int gameTimer_ = 0;
    int enemySpawnTimer_ = 0;
    int nextEnemySpawnDelay_ = 90;
    int nextEnemyIndex_ = 0;
    float backgroundPositionX_ = 640.0f;
    float backgroundPositionX2_ = 1920.0f;

    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> backgroundSprite2_;
    std::unique_ptr<Sprite> titlePanelSprite_;
    std::unique_ptr<Sprite> startSprite_;
    std::unique_ptr<Sprite> clearSprite_;
    std::array<std::unique_ptr<Sprite>, 4> playerSprites_;
    std::array<std::unique_ptr<Sprite>, kPlayerBulletCount> playerBulletSprites_;
    std::array<std::unique_ptr<Sprite>, kEnemyCount> enemySprites_;
    std::array<std::unique_ptr<Sprite>, kEnemyCount> explosionSprites_;
    std::array<std::unique_ptr<Sprite>, kEnemyBulletCount> enemyBulletSprites_;
    std::unique_ptr<Sprite> bossSprite_;
    std::array<std::unique_ptr<Sprite>, kBossBulletCount> bossBulletSprites_;
    std::unique_ptr<Sprite> playerHpBackSprite_;
    std::unique_ptr<Sprite> playerHpSprite_;
    std::unique_ptr<Sprite> bossHpBackSprite_;
    std::unique_ptr<Sprite> bossHpSprite_;
    std::unique_ptr<Sprite> chargeBackSprite_;
    std::unique_ptr<Sprite> chargeSprite_;
    std::unique_ptr<Sprite> chargeEffectSprite_;

    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> guideText_;
    std::unique_ptr<Text> stateText_;

    std::mt19937 randomEngine_;
};
