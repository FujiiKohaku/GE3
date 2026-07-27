#include "TitleScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/input/Input.h"
#include "SceneManager.h"

#include <cmath>
#include <string>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kTitleTexture = "resources/images/OP1.png";
constexpr const char* kStartTexture = "resources/images/statgame2.png";
constexpr const char* kClearTexture = "resources/images/ED2.png";
constexpr const char* kBackgroundTexture = "resources/images/haikei.png";
constexpr const char* kBackgroundTexture2 = "resources/images/haikei2.png";
constexpr const char* kPlayerTextures[4] = {
    "resources/images/playermove1.png",
    "resources/images/playermove2.png",
    "resources/images/playermove3.png",
    "resources/images/playermove4.png",
};
constexpr const char* kEnemyTexture = "resources/images/enemy.png";
constexpr const char* kEnemyTexture2 = "resources/images/enemy2.png";
constexpr const char* kBossTexture = "resources/images/boss.png";
constexpr const char* kBulletTexture = "resources/images/bullet.png";
constexpr const char* kExplosionTexture = "resources/images/bomb.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";

constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;
constexpr int kMaxChargeTime = 120;
constexpr int kShotCooldownFrame = 18;
constexpr int kBossAppearFrame = 25 * 60;
constexpr int kBossPatternFrame = 240;
constexpr float kPi = 3.14159265358979323846f;

constexpr Vector4 kBackgroundColor = { 0.015f, 0.02f, 0.055f, 1.0f };
constexpr Vector4 kPanelColor = { 0.04f, 0.08f, 0.16f, 0.94f };
constexpr Vector4 kPlayerColor = { 0.15f, 0.9f, 1.0f, 1.0f };
constexpr Vector4 kPlayerHitColor = { 1.0f, 1.0f, 1.0f, 0.35f };
constexpr Vector4 kBulletColor = { 0.25f, 1.0f, 0.55f, 1.0f };
constexpr Vector4 kChargedBulletColor = { 1.0f, 0.95f, 0.15f, 1.0f };
constexpr Vector4 kEnemyColor = { 1.0f, 0.25f, 0.35f, 1.0f };
constexpr Vector4 kEnemyBulletColor = { 1.0f, 0.55f, 0.15f, 1.0f };
constexpr Vector4 kBossColor = { 0.75f, 0.2f, 1.0f, 1.0f };
constexpr Vector4 kBossBulletColor = { 1.0f, 0.15f, 0.7f, 1.0f };
constexpr Vector4 kGaugeBackColor = { 0.12f, 0.12f, 0.17f, 1.0f };
constexpr Vector4 kPlayerHpColor = { 0.1f, 0.9f, 0.35f, 1.0f };
constexpr Vector4 kBossHpColor = { 0.95f, 0.15f, 0.2f, 1.0f };
constexpr Vector4 kChargeColor = { 0.15f, 0.65f, 1.0f, 1.0f };
}

void TitleScene::Initialize()
{
    std::random_device randomDevice;
    randomEngine_.seed(randomDevice());

    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    backgroundSprite_ = CreateTexturedSprite(
        kBackgroundTexture,
        { kScreenWidth * 0.5f, kScreenHeight * 0.5f },
        { kScreenWidth, kScreenHeight });
    backgroundSprite2_ = CreateTexturedSprite(
        kBackgroundTexture2,
        { kScreenWidth * 1.5f, kScreenHeight * 0.5f },
        { kScreenWidth, kScreenHeight });
    titlePanelSprite_ = CreateTexturedSprite(
        kTitleTexture,
        { kScreenWidth * 0.5f, kScreenHeight * 0.5f },
        { kScreenWidth, kScreenHeight });
    startSprite_ = CreateTexturedSprite(
        kStartTexture,
        { 640.0f, 590.0f },
        { 384.0f, 64.0f });
    clearSprite_ = CreateTexturedSprite(
        kClearTexture,
        { 640.0f, 340.0f },
        { 768.0f, 128.0f });
    for (int i = 0; i < 4; ++i) {
        playerSprites_[i] = CreateTexturedSprite(
            kPlayerTextures[i],
            { 120.0f, 360.0f },
            { 96.0f, 96.0f });
    }

    for (int i = 0; i < kPlayerBulletCount; ++i) {
        playerBulletSprites_[i] = CreateTexturedSprite(
            kBulletTexture,
            { -100.0f, -100.0f },
            { 32.0f, 32.0f });
    }
    for (int i = 0; i < kEnemyCount; ++i) {
        const char* enemyTexture = kEnemyTexture;
        if (i % 2 == 0) {
            enemyTexture = kEnemyTexture2;
        }
        enemySprites_[i] = CreateTexturedSprite(
            enemyTexture,
            { -100.0f, -100.0f },
            { 96.0f, 96.0f });
        explosionSprites_[i] = CreateTexturedSprite(
            kExplosionTexture,
            { -100.0f, -100.0f },
            { 112.0f, 112.0f });
    }
    for (int i = 0; i < kEnemyBulletCount; ++i) {
        enemyBulletSprites_[i] = CreateTexturedSprite(
            kBulletTexture,
            { -100.0f, -100.0f },
            { 24.0f, 24.0f });
    }

    bossSprite_ = CreateTexturedSprite(
        kBossTexture,
        { -200.0f, -200.0f },
        { 160.0f, 160.0f });
    for (int i = 0; i < kBossBulletCount; ++i) {
        bossBulletSprites_[i] = CreateTexturedSprite(
            kBulletTexture,
            { -100.0f, -100.0f },
            { 28.0f, 28.0f });
    }

    playerHpBackSprite_ =
        CreateSprite({ 140.0f, 38.0f }, { 220.0f, 24.0f }, kGaugeBackColor);
    playerHpSprite_ =
        CreateSprite({ 140.0f, 38.0f }, { 200.0f, 14.0f }, kPlayerHpColor);
    bossHpBackSprite_ =
        CreateSprite({ 1040.0f, 38.0f }, { 420.0f, 24.0f }, kGaugeBackColor);
    bossHpSprite_ =
        CreateSprite({ 1040.0f, 38.0f }, { 400.0f, 14.0f }, kBossHpColor);
    chargeBackSprite_ =
        CreateSprite({ 140.0f, 68.0f }, { 220.0f, 14.0f }, kGaugeBackColor);
    chargeSprite_ =
        CreateSprite({ 140.0f, 68.0f }, { 0.0f, 8.0f }, kChargeColor);
    chargeEffectSprite_ = CreateTexturedSprite(
        kBulletTexture,
        { -100.0f, -100.0f },
        { 32.0f, 32.0f });

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetPosition({ 640.0f, 260.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(64.0f);
    titleText_->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f });
    titleText_->SetText("AL SHOOTING");

    guideText_ = std::make_unique<Text>();
    guideText_->Initialize(kDefaultFont);
    guideText_->SetPosition({ 640.0f, 500.0f });
    guideText_->SetAnchorPoint({ 0.5f, 0.5f });
    guideText_->SetFontSize(25.0f);
    guideText_->SetColor({ 0.82f, 0.9f, 1.0f, 1.0f });
    guideText_->SetText(
        "WASD: MOVE   SHIFT: BOOST   E: SHOT / HOLD: CHARGE");

    stateText_ = std::make_unique<Text>();
    stateText_->Initialize(kDefaultFont);
    stateText_->SetPosition({ 640.0f, 660.0f });
    stateText_->SetAnchorPoint({ 0.5f, 0.5f });
    stateText_->SetFontSize(34.0f);
    stateText_->SetColor({ 1.0f, 0.9f, 0.25f, 1.0f });
    stateText_->SetText("PRESS SPACE TO START");

    ResetGame();
    sceneState_ = SceneState::Title;
    UpdateSprites();
    UpdateTexts();
}

void TitleScene::ResetGame()
{
    player_.position = { 120.0f, 360.0f };
    player_.velocity = { 0.0f, 0.0f };
    player_.radius = 28.0f;
    player_.active = true;
    playerHp_ = 10;
    playerInvincibleTimer_ = 0;
    wasChargePressed_ = false;
    chargeTime_ = 0;
    shotCooldownTimer_ = 0;
    chargeShotFlashTimer_ = 0;

    for (int i = 0; i < kPlayerBulletCount; ++i) {
        playerBullets_[i].actor = Actor();
        playerBullets_[i].charged = false;
    }
    for (int i = 0; i < kEnemyCount; ++i) {
        enemies_[i].actor = Actor();
        enemies_[i].actor.radius = 30.0f;
        enemies_[i].shotTimer = 0;
        enemies_[i].nextShotDelay = 120;
        enemies_[i].explosionTimer = 0;
        enemies_[i].basePositionY = 0.0f;
        enemies_[i].waveAngle = 0.0f;
        enemies_[i].waveSpeed = 0.0f;
        enemies_[i].waveAmplitude = 0.0f;
    }
    for (int i = 0; i < kEnemyBulletCount; ++i) {
        enemyBullets_[i] = Actor();
        enemyBullets_[i].radius = 8.0f;
    }
    for (int i = 0; i < kBossBulletCount; ++i) {
        bossBullets_[i] = Actor();
        bossBullets_[i].radius = 10.0f;
    }

    boss_ = Actor();
    boss_.position = { 1100.0f, 360.0f };
    boss_.radius = 62.0f;
    bossHp_ = 20;
    bossMoveDirection_ = 1;
    bossShotTimer_ = 0;
    bossPatternTimer_ = 0;
    bossPattern_ = BossPattern::Vertical;

    gameTimer_ = 0;
    enemySpawnTimer_ = 0;
    nextEnemySpawnDelay_ = RandomInt(55, 130);
    nextEnemyIndex_ = 0;
    backgroundPositionX_ = 640.0f;
    backgroundPositionX2_ = 1920.0f;
}

void TitleScene::Update()
{
    if (sceneState_ == SceneState::Title) {
        UpdateTitle();
    } else if (sceneState_ == SceneState::Playing) {
        UpdatePlaying();
    } else {
        if (Input::GetInstance()->IsKeyTrigger(DIK_SPACE)) {
            ResetGame();
            sceneState_ = SceneState::Playing;
        }
    }

    UpdateSprites();
    UpdateTexts();
}

void TitleScene::UpdateTitle()
{
    if (Input::GetInstance()->IsKeyTrigger(DIK_SPACE)) {
        ResetGame();
        sceneState_ = SceneState::Playing;
    }
}

void TitleScene::UpdatePlaying()
{
    ++gameTimer_;
    backgroundPositionX_ -= 1.0f;
    backgroundPositionX2_ -= 1.0f;
    if (backgroundPositionX_ <= -640.0f) {
        backgroundPositionX_ = backgroundPositionX2_ + 1280.0f;
    }
    if (backgroundPositionX2_ <= -640.0f) {
        backgroundPositionX2_ = backgroundPositionX_ + 1280.0f;
    }
    if (shotCooldownTimer_ > 0) {
        --shotCooldownTimer_;
    }
    if (chargeShotFlashTimer_ > 0) {
        --chargeShotFlashTimer_;
    }
    UpdatePlayer();
    UpdatePlayerBullets();
    UpdateEnemies();
    UpdateEnemyBullets();
    UpdateBoss();
    UpdateBossBullets();
    CheckCollisions();

    if (playerInvincibleTimer_ > 0) {
        --playerInvincibleTimer_;
    }

    if (playerHp_ <= 0) {
        player_.active = false;
        sceneState_ = SceneState::GameOver;
    }
    if (boss_.active && bossHp_ <= 0) {
        boss_.active = false;
        sceneState_ = SceneState::Clear;
    }
}

void TitleScene::UpdatePlayer()
{
    Input* input = Input::GetInstance();
    Vector2 direction = { 0.0f, 0.0f };

    if (input->IsKeyPressed(DIK_W)) {
        direction.y -= 1.0f;
    }
    if (input->IsKeyPressed(DIK_S)) {
        direction.y += 1.0f;
    }
    if (input->IsKeyPressed(DIK_A)) {
        direction.x -= 1.0f;
    }
    if (input->IsKeyPressed(DIK_D)) {
        direction.x += 1.0f;
    }

    const float length =
        std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 0.0f) {
        direction.x /= length;
        direction.y /= length;
    }

    float speed = 5.0f;
    if (input->IsKeyPressed(DIK_LSHIFT)) {
        speed = 10.0f;
    }

    player_.position.x += direction.x * speed;
    player_.position.y += direction.y * speed;

    if (player_.position.x < 36.0f) {
        player_.position.x = 36.0f;
    }
    if (player_.position.x > kScreenWidth - 36.0f) {
        player_.position.x = kScreenWidth - 36.0f;
    }
    if (player_.position.y < 90.0f) {
        player_.position.y = 90.0f;
    }
    if (player_.position.y > kScreenHeight - 28.0f) {
        player_.position.y = kScreenHeight - 28.0f;
    }

    const bool isChargePressed = input->IsKeyPressed(DIK_E);
    if (isChargePressed && shotCooldownTimer_ <= 0) {
        if (chargeTime_ < kMaxChargeTime) {
            ++chargeTime_;
        }
    }
    if (!isChargePressed && wasChargePressed_) {
        bool isCharged = false;
        if (chargeTime_ >= kMaxChargeTime) {
            isCharged = true;
        }
        if (shotCooldownTimer_ <= 0) {
            FirePlayerBullet(isCharged);
        }
        chargeTime_ = 0;
    }
    wasChargePressed_ = isChargePressed;
}

void TitleScene::FirePlayerBullet(bool charged)
{
    for (int i = 0; i < kPlayerBulletCount; ++i) {
        if (!playerBullets_[i].actor.active) {
            playerBullets_[i].actor.active = true;
            playerBullets_[i].actor.position =
                { player_.position.x + 48.0f, player_.position.y };
            playerBullets_[i].actor.velocity = { 8.0f, 0.0f };
            playerBullets_[i].actor.radius = 10.0f;
            playerBullets_[i].charged = charged;
            if (charged) {
                playerBullets_[i].actor.velocity.x = 16.0f;
                playerBullets_[i].actor.radius = 24.0f;
                chargeShotFlashTimer_ = 15;
            }
            shotCooldownTimer_ = kShotCooldownFrame;
            break;
        }
    }
}

void TitleScene::UpdatePlayerBullets()
{
    for (int i = 0; i < kPlayerBulletCount; ++i) {
        Actor& bullet = playerBullets_[i].actor;
        if (bullet.active) {
            bullet.position.x += bullet.velocity.x;
            bullet.position.y += bullet.velocity.y;
            if (IsOutsideScreen(bullet.position, 80.0f)) {
                bullet.active = false;
            }
        }
    }
}

void TitleScene::UpdateEnemies()
{
    if (nextEnemyIndex_ < kEnemyCount) {
        ++enemySpawnTimer_;
        if (enemySpawnTimer_ >= nextEnemySpawnDelay_) {
            Enemy& enemy = enemies_[nextEnemyIndex_];
            enemy.actor.active = true;
            enemy.actor.position.x = kScreenWidth + 60.0f;
            enemy.basePositionY = RandomFloat(150.0f, 620.0f);
            enemy.actor.position.y = enemy.basePositionY;
            enemy.actor.velocity = { -RandomFloat(2.0f, 4.5f), 0.0f };
            enemy.waveAngle = RandomFloat(0.0f, kPi * 2.0f);
            enemy.waveSpeed = RandomFloat(0.025f, 0.075f);
            enemy.waveAmplitude = RandomFloat(30.0f, 95.0f);
            enemy.shotTimer = RandomInt(0, 90);
            enemy.nextShotDelay = RandomInt(100, 150);
            ++nextEnemyIndex_;
            enemySpawnTimer_ = 0;
            nextEnemySpawnDelay_ = RandomInt(55, 130);
        }
    }

    for (int i = 0; i < kEnemyCount; ++i) {
        Enemy& enemy = enemies_[i];
        if (enemy.explosionTimer > 0) {
            --enemy.explosionTimer;
        }
        if (enemy.actor.active) {
            enemy.actor.position.x += enemy.actor.velocity.x;
            enemy.waveAngle += enemy.waveSpeed;
            enemy.actor.position.y =
                enemy.basePositionY +
                std::sin(enemy.waveAngle) * enemy.waveAmplitude;
            if (enemy.actor.position.y < 110.0f) {
                enemy.actor.position.y = 110.0f;
            }
            if (enemy.actor.position.y > 670.0f) {
                enemy.actor.position.y = 670.0f;
            }
            ++enemy.shotTimer;
            if (enemy.shotTimer >= enemy.nextShotDelay) {
                FireEnemyBullet(enemy.actor.position);
                enemy.shotTimer = 0;
                enemy.nextShotDelay = RandomInt(100, 150);
            }
            if (enemy.actor.position.x < -80.0f) {
                enemy.actor.active = false;
            }
        }
    }
}

void TitleScene::FireEnemyBullet(const Vector2& position)
{
    for (int i = 0; i < kEnemyBulletCount; ++i) {
        if (!enemyBullets_[i].active) {
            enemyBullets_[i].active = true;
            enemyBullets_[i].position = { position.x - 38.0f, position.y };
            Vector2 direction = {
                player_.position.x - position.x,
                player_.position.y - position.y
            };
            const float length =
                std::sqrt(direction.x * direction.x + direction.y * direction.y);
            if (length > 0.0f) {
                direction.x /= length;
                direction.y /= length;
            }
            enemyBullets_[i].velocity =
                { direction.x * 6.0f, direction.y * 6.0f };
            enemyBullets_[i].radius = 8.0f;
            break;
        }
    }
}

void TitleScene::UpdateEnemyBullets()
{
    for (int i = 0; i < kEnemyBulletCount; ++i) {
        Actor& bullet = enemyBullets_[i];
        if (bullet.active) {
            bullet.position.x += bullet.velocity.x;
            bullet.position.y += bullet.velocity.y;
            if (IsOutsideScreen(bullet.position, 40.0f)) {
                bullet.active = false;
            }
        }
    }
}

void TitleScene::UpdateBoss()
{
    if (!boss_.active && gameTimer_ >= kBossAppearFrame && bossHp_ > 0) {
        boss_.active = true;
        boss_.position = { 1100.0f, 360.0f };
        bossShotTimer_ = 0;
        bossPatternTimer_ = 0;
        bossPattern_ = BossPattern::Vertical;
    }

    if (!boss_.active) {
        return;
    }

    ++bossPatternTimer_;
    ++bossShotTimer_;
    if (bossPatternTimer_ >= kBossPatternFrame) {
        bossPatternTimer_ = 0;
        bossShotTimer_ = 0;
        boss_.position.x = 1100.0f;
        if (bossPattern_ == BossPattern::Vertical) {
            bossPattern_ = BossPattern::WaveSpread;
        } else if (bossPattern_ == BossPattern::WaveSpread) {
            bossPattern_ = BossPattern::Tracking;
        } else if (bossPattern_ == BossPattern::Tracking) {
            bossPattern_ = BossPattern::Rush;
        } else {
            bossPattern_ = BossPattern::Vertical;
        }
    }

    if (bossPattern_ == BossPattern::Vertical) {
        boss_.position.y += static_cast<float>(bossMoveDirection_) * 2.5f;
        if (boss_.position.y >= 620.0f) {
            bossMoveDirection_ = -1;
        }
        if (boss_.position.y <= 140.0f) {
            bossMoveDirection_ = 1;
        }
        if (bossShotTimer_ >= 60) {
            FireBossBullet();
            bossShotTimer_ = 0;
        }
    } else if (bossPattern_ == BossPattern::WaveSpread) {
        boss_.position.x = 1080.0f;
        boss_.position.y =
            360.0f + std::sin(static_cast<float>(bossPatternTimer_) * 0.045f) *
            230.0f;
        if (bossShotTimer_ >= 75) {
            FireBossSpread();
            bossShotTimer_ = 0;
        }
    } else if (bossPattern_ == BossPattern::Tracking) {
        boss_.position.x =
            1040.0f + std::sin(static_cast<float>(bossPatternTimer_) * 0.04f) *
            80.0f;
        boss_.position.y += (player_.position.y - boss_.position.y) * 0.025f;
        if (bossShotTimer_ >= 35) {
            FireBossBullet();
            bossShotTimer_ = 0;
        }
    } else {
        if (bossPatternTimer_ < 60) {
            boss_.position.x = 1100.0f -
                static_cast<float>(bossPatternTimer_) * 5.0f;
        } else if (bossPatternTimer_ < 120) {
            boss_.position.x = 800.0f +
                static_cast<float>(bossPatternTimer_ - 60) * 5.0f;
        } else {
            boss_.position.x = 1100.0f;
            boss_.position.y =
                360.0f +
                std::sin(static_cast<float>(bossPatternTimer_) * 0.08f) *
                210.0f;
        }
        if (bossShotTimer_ >= 90) {
            FireBossRadial();
            bossShotTimer_ = 0;
        }
    }
}

void TitleScene::FireBossBullet()
{
    Vector2 direction = {
        player_.position.x - boss_.position.x,
        player_.position.y - boss_.position.y
    };
    const float length =
        std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 0.0f) {
        direction.x /= length;
        direction.y /= length;
    }
    FireBossBulletVelocity({ direction.x * 7.0f, direction.y * 7.0f });
}

void TitleScene::FireBossSpread()
{
    for (int i = -2; i <= 2; ++i) {
        const float angle = kPi + static_cast<float>(i) * 0.22f;
        FireBossBulletVelocity(
            { std::cos(angle) * 6.5f, std::sin(angle) * 6.5f });
    }
}

void TitleScene::FireBossRadial()
{
    for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i) * kPi * 0.25f;
        FireBossBulletVelocity(
            { std::cos(angle) * 5.5f, std::sin(angle) * 5.5f });
    }
}

void TitleScene::FireBossBulletVelocity(const Vector2& velocity)
{
    for (int i = 0; i < kBossBulletCount; ++i) {
        if (!bossBullets_[i].active) {
            bossBullets_[i].active = true;
            bossBullets_[i].position =
                { boss_.position.x - 72.0f, boss_.position.y };
            bossBullets_[i].velocity = velocity;
            bossBullets_[i].radius = 10.0f;
            break;
        }
    }
}

void TitleScene::UpdateBossBullets()
{
    for (int i = 0; i < kBossBulletCount; ++i) {
        Actor& bullet = bossBullets_[i];
        if (bullet.active) {
            bullet.position.x += bullet.velocity.x;
            bullet.position.y += bullet.velocity.y;
            if (IsOutsideScreen(bullet.position, 60.0f)) {
                bullet.active = false;
            }
        }
    }
}

void TitleScene::CheckCollisions()
{
    for (int bulletIndex = 0; bulletIndex < kPlayerBulletCount; ++bulletIndex) {
        PlayerBullet& bullet = playerBullets_[bulletIndex];
        if (!bullet.actor.active) {
            continue;
        }

        for (int enemyIndex = 0; enemyIndex < kEnemyCount; ++enemyIndex) {
            Enemy& enemy = enemies_[enemyIndex];
            if (enemy.actor.active && IsCircleHit(bullet.actor, enemy.actor)) {
                enemy.explosionTimer = 18;
                enemy.actor.active = false;
                bullet.actor.active = false;
                break;
            }
        }

        if (bullet.actor.active && boss_.active &&
            IsCircleHit(bullet.actor, boss_)) {
            int damage = 1;
            if (bullet.charged) {
                damage = 3;
            }
            bossHp_ -= damage;
            if (bossHp_ < 0) {
                bossHp_ = 0;
            }
            bullet.actor.active = false;
        }
    }

    for (int i = 0; i < kEnemyBulletCount; ++i) {
        if (enemyBullets_[i].active && IsCircleHit(enemyBullets_[i], player_)) {
            enemyBullets_[i].active = false;
            DamagePlayer();
        }
    }
    for (int i = 0; i < kBossBulletCount; ++i) {
        if (bossBullets_[i].active && IsCircleHit(bossBullets_[i], player_)) {
            bossBullets_[i].active = false;
            DamagePlayer();
        }
    }
    for (int i = 0; i < kEnemyCount; ++i) {
        if (enemies_[i].actor.active &&
            IsCircleHit(enemies_[i].actor, player_)) {
            enemies_[i].actor.active = false;
            DamagePlayer();
        }
    }
    if (boss_.active && IsCircleHit(boss_, player_)) {
        DamagePlayer();
    }
}

void TitleScene::DamagePlayer()
{
    if (playerInvincibleTimer_ > 0) {
        return;
    }
    --playerHp_;
    playerInvincibleTimer_ = 60;
}

bool TitleScene::IsCircleHit(const Actor& first, const Actor& second) const
{
    const float differenceX = first.position.x - second.position.x;
    const float differenceY = first.position.y - second.position.y;
    const float radiusSum = first.radius + second.radius;
    return differenceX * differenceX + differenceY * differenceY <=
        radiusSum * radiusSum;
}

bool TitleScene::IsOutsideScreen(const Vector2& position, float margin) const
{
    if (position.x < -margin || position.x > kScreenWidth + margin) {
        return true;
    }
    if (position.y < -margin || position.y > kScreenHeight + margin) {
        return true;
    }
    return false;
}

float TitleScene::RandomFloat(float minimum, float maximum)
{
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(randomEngine_);
}

int TitleScene::RandomInt(int minimum, int maximum)
{
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(randomEngine_);
}

std::unique_ptr<Sprite> TitleScene::CreateSprite(
    const Vector2& position,
    const Vector2& size,
    const Vector4& color)
{
    std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
    sprite->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->SetAnchorPoint({ 0.5f, 0.5f });
    sprite->SetColor(color);
    return sprite;
}

std::unique_ptr<Sprite> TitleScene::CreateTexturedSprite(
    const char* texturePath,
    const Vector2& position,
    const Vector2& size)
{
    std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
    sprite->Initialize(SpriteManager::GetInstance(), texturePath);
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->SetAnchorPoint({ 0.5f, 0.5f });
    sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    return sprite;
}

void TitleScene::UpdateSprites()
{
    backgroundSprite_->SetPosition({ backgroundPositionX_, 360.0f });
    backgroundSprite_->Update();
    backgroundSprite2_->SetPosition({ backgroundPositionX2_, 360.0f });
    backgroundSprite2_->Update();
    titlePanelSprite_->Update();
    startSprite_->Update();
    clearSprite_->Update();

    for (int i = 0; i < 4; ++i) {
        playerSprites_[i]->SetPosition(player_.position);
        playerSprites_[i]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        if (playerInvincibleTimer_ > 0 &&
            (playerInvincibleTimer_ / 5) % 2 == 0) {
            playerSprites_[i]->SetColor(kPlayerHitColor);
        }
        playerSprites_[i]->Update();
    }

    float chargeEffectRate = static_cast<float>(chargeTime_) /
        static_cast<float>(kMaxChargeTime);
    if (chargeEffectRate > 1.0f) {
        chargeEffectRate = 1.0f;
    }
    if (chargeTime_ > 0) {
        const float pulse =
            std::sin(static_cast<float>(gameTimer_) * 0.35f) * 7.0f;
        const float effectSize = 24.0f + chargeEffectRate * 72.0f + pulse;
        chargeEffectSprite_->SetPosition(
            { player_.position.x + 55.0f, player_.position.y });
        chargeEffectSprite_->SetSize({ effectSize, effectSize });
        chargeEffectSprite_->SetRotation(
            static_cast<float>(gameTimer_) * 0.12f);
        chargeEffectSprite_->SetColor(
            { 1.0f, 0.85f, 0.15f, 0.45f + chargeEffectRate * 0.45f });
    } else if (chargeShotFlashTimer_ > 0) {
        const float elapsed =
            static_cast<float>(15 - chargeShotFlashTimer_);
        const float effectSize = 80.0f + elapsed * 12.0f;
        const float alpha =
            static_cast<float>(chargeShotFlashTimer_) / 15.0f;
        chargeEffectSprite_->SetPosition(
            { player_.position.x + 62.0f + elapsed * 4.0f, player_.position.y });
        chargeEffectSprite_->SetSize({ effectSize, effectSize });
        chargeEffectSprite_->SetRotation(
            static_cast<float>(gameTimer_) * 0.18f);
        chargeEffectSprite_->SetColor({ 1.0f, 1.0f, 0.3f, alpha });
    } else {
        chargeEffectSprite_->SetPosition({ -100.0f, -100.0f });
        chargeEffectSprite_->SetSize({ 0.0f, 0.0f });
        chargeEffectSprite_->SetRotation(0.0f);
    }
    chargeEffectSprite_->Update();

    for (int i = 0; i < kPlayerBulletCount; ++i) {
        playerBulletSprites_[i]->SetPosition(playerBullets_[i].actor.position);
        playerBulletSprites_[i]->SetSize({ 32.0f, 32.0f });
        playerBulletSprites_[i]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        playerBulletSprites_[i]->SetRotation(0.0f);
        if (playerBullets_[i].charged) {
            const float bulletPulse =
                std::sin(
                    static_cast<float>(gameTimer_ + i * 3) * 0.4f) *
                8.0f;
            playerBulletSprites_[i]->SetSize(
                { 72.0f + bulletPulse, 72.0f + bulletPulse });
            playerBulletSprites_[i]->SetColor(kChargedBulletColor);
            playerBulletSprites_[i]->SetRotation(
                static_cast<float>(gameTimer_) * 0.16f);
        }
        playerBulletSprites_[i]->Update();
    }
    for (int i = 0; i < kEnemyCount; ++i) {
        enemySprites_[i]->SetPosition(enemies_[i].actor.position);
        enemySprites_[i]->Update();
        explosionSprites_[i]->SetPosition(enemies_[i].actor.position);
        explosionSprites_[i]->Update();
    }
    for (int i = 0; i < kEnemyBulletCount; ++i) {
        enemyBulletSprites_[i]->SetPosition(enemyBullets_[i].position);
        enemyBulletSprites_[i]->Update();
    }

    bossSprite_->SetPosition(boss_.position);
    bossSprite_->Update();
    for (int i = 0; i < kBossBulletCount; ++i) {
        bossBulletSprites_[i]->SetPosition(bossBullets_[i].position);
        bossBulletSprites_[i]->Update();
    }

    float playerHpRate = static_cast<float>(playerHp_) / 10.0f;
    if (playerHpRate < 0.0f) {
        playerHpRate = 0.0f;
    }
    playerHpSprite_->SetSize({ 200.0f * playerHpRate, 14.0f });
    playerHpSprite_->Update();
    playerHpBackSprite_->Update();

    float bossHpRate = static_cast<float>(bossHp_) / 20.0f;
    if (bossHpRate < 0.0f) {
        bossHpRate = 0.0f;
    }
    bossHpSprite_->SetSize({ 400.0f * bossHpRate, 14.0f });
    bossHpSprite_->Update();
    bossHpBackSprite_->Update();

    float chargeRate = static_cast<float>(chargeTime_) /
        static_cast<float>(kMaxChargeTime);
    if (chargeRate > 1.0f) {
        chargeRate = 1.0f;
    }
    chargeSprite_->SetColor(kChargeColor);
    if (chargeTime_ <= 0 && shotCooldownTimer_ > 0) {
        chargeRate = 1.0f -
            static_cast<float>(shotCooldownTimer_) /
            static_cast<float>(kShotCooldownFrame);
        chargeSprite_->SetColor({ 1.0f, 0.45f, 0.1f, 1.0f });
    }
    chargeSprite_->SetSize({ 200.0f * chargeRate, 8.0f });
    chargeSprite_->Update();
    chargeBackSprite_->Update();
}

void TitleScene::UpdateTexts()
{
    if (sceneState_ == SceneState::Title) {
        stateText_->SetText("PRESS SPACE TO START");
    } else if (sceneState_ == SceneState::GameOver) {
        titleText_->SetText("GAME OVER");
        stateText_->SetText("PRESS SPACE TO RETRY");
    } else if (sceneState_ == SceneState::Clear) {
        titleText_->SetText("MISSION CLEAR");
        stateText_->SetText("PRESS SPACE TO PLAY AGAIN");
    } else {
        titleText_->SetText("AL SHOOTING");
    }

    titleText_->Update();
    guideText_->Update();
    stateText_->Update();
}

void TitleScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();

    if (sceneState_ == SceneState::Playing) {
        backgroundSprite_->Draw();
        backgroundSprite2_->Draw();
        playerHpBackSprite_->Draw();
        playerHpSprite_->Draw();
        chargeBackSprite_->Draw();
        chargeSprite_->Draw();

        if (boss_.active) {
            bossHpBackSprite_->Draw();
            bossHpSprite_->Draw();
        }

        if (player_.active) {
            int playerAnimationIndex = (gameTimer_ / 8) % 4;
            playerSprites_[playerAnimationIndex]->Draw();
        }
        if (chargeTime_ > 0 || chargeShotFlashTimer_ > 0) {
            chargeEffectSprite_->Draw();
        }
        for (int i = 0; i < kPlayerBulletCount; ++i) {
            if (playerBullets_[i].actor.active) {
                playerBulletSprites_[i]->Draw();
            }
        }
        for (int i = 0; i < kEnemyCount; ++i) {
            if (enemies_[i].actor.active) {
                enemySprites_[i]->Draw();
            }
            if (enemies_[i].explosionTimer > 0) {
                explosionSprites_[i]->Draw();
            }
        }
        for (int i = 0; i < kEnemyBulletCount; ++i) {
            if (enemyBullets_[i].active) {
                enemyBulletSprites_[i]->Draw();
            }
        }
        if (boss_.active) {
            bossSprite_->Draw();
        }
        for (int i = 0; i < kBossBulletCount; ++i) {
            if (bossBullets_[i].active) {
                bossBulletSprites_[i]->Draw();
            }
        }
    } else if (sceneState_ == SceneState::Title) {
        titlePanelSprite_->Draw();
        startSprite_->Draw();

        TextRenderer::GetInstance()->PreDraw();
        guideText_->Draw();
        stateText_->Draw();
    } else {
        backgroundSprite_->Draw();
        if (sceneState_ == SceneState::Clear) {
            clearSprite_->Draw();
        }

        TextRenderer::GetInstance()->PreDraw();
        titleText_->Draw();
        stateText_->Draw();
    }
}

void TitleScene::Draw3D()
{
}

void TitleScene::DrawParticle()
{
}

void TitleScene::DrawImGui()
{
}

void TitleScene::Finalize()
{
}
