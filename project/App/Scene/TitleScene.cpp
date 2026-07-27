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
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";

constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;
constexpr int kMaxChargeTime = 120;
constexpr int kBossAppearFrame = 25 * 60;

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
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    backgroundSprite_ = CreateSprite(
        { kScreenWidth * 0.5f, kScreenHeight * 0.5f },
        { kScreenWidth, kScreenHeight },
        kBackgroundColor);
    titlePanelSprite_ = CreateSprite(
        { kScreenWidth * 0.5f, kScreenHeight * 0.5f },
        { 760.0f, 300.0f },
        kPanelColor);
    playerSprite_ = CreateSprite({ 120.0f, 360.0f }, { 72.0f, 48.0f }, kPlayerColor);

    for (int i = 0; i < kPlayerBulletCount; ++i) {
        playerBulletSprites_[i] =
            CreateSprite({ -100.0f, -100.0f }, { 24.0f, 12.0f }, kBulletColor);
    }
    for (int i = 0; i < kEnemyCount; ++i) {
        enemySprites_[i] =
            CreateSprite({ -100.0f, -100.0f }, { 64.0f, 64.0f }, kEnemyColor);
    }
    for (int i = 0; i < kEnemyBulletCount; ++i) {
        enemyBulletSprites_[i] =
            CreateSprite({ -100.0f, -100.0f }, { 16.0f, 16.0f }, kEnemyBulletColor);
    }

    bossSprite_ =
        CreateSprite({ -200.0f, -200.0f }, { 128.0f, 128.0f }, kBossColor);
    for (int i = 0; i < kBossBulletCount; ++i) {
        bossBulletSprites_[i] =
            CreateSprite({ -100.0f, -100.0f }, { 20.0f, 20.0f }, kBossBulletColor);
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

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetPosition({ 640.0f, 250.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(64.0f);
    titleText_->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f });
    titleText_->SetText("AL SHOOTING");

    guideText_ = std::make_unique<Text>();
    guideText_->Initialize(kDefaultFont);
    guideText_->SetPosition({ 640.0f, 360.0f });
    guideText_->SetAnchorPoint({ 0.5f, 0.5f });
    guideText_->SetFontSize(25.0f);
    guideText_->SetColor({ 0.82f, 0.9f, 1.0f, 1.0f });
    guideText_->SetText("WASD: MOVE   SHIFT: BOOST   HOLD E: CHARGE SHOT");

    stateText_ = std::make_unique<Text>();
    stateText_->Initialize(kDefaultFont);
    stateText_->SetPosition({ 640.0f, 455.0f });
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

    for (int i = 0; i < kPlayerBulletCount; ++i) {
        playerBullets_[i].actor = Actor();
        playerBullets_[i].charged = false;
    }
    for (int i = 0; i < kEnemyCount; ++i) {
        enemies_[i].actor = Actor();
        enemies_[i].actor.radius = 30.0f;
        enemies_[i].shotTimer = 0;
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

    gameTimer_ = 0;
    enemySpawnTimer_ = 0;
    nextEnemyIndex_ = 0;
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
    if (isChargePressed) {
        if (chargeTime_ < kMaxChargeTime) {
            ++chargeTime_;
        }
    }
    if (!isChargePressed && wasChargePressed_) {
        bool isCharged = false;
        if (chargeTime_ >= kMaxChargeTime) {
            isCharged = true;
        }
        FirePlayerBullet(isCharged);
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
            }
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
        if (enemySpawnTimer_ >= 120) {
            Enemy& enemy = enemies_[nextEnemyIndex_];
            enemy.actor.active = true;
            enemy.actor.position.x = kScreenWidth + 60.0f;
            enemy.actor.position.y =
                130.0f + static_cast<float>(nextEnemyIndex_ % 5) * 110.0f;
            enemy.actor.velocity = { -2.5f, 0.0f };
            enemy.shotTimer = 30 + (nextEnemyIndex_ % 3) * 30;
            ++nextEnemyIndex_;
            enemySpawnTimer_ = 0;
        }
    }

    for (int i = 0; i < kEnemyCount; ++i) {
        Enemy& enemy = enemies_[i];
        if (enemy.actor.active) {
            enemy.actor.position.x += enemy.actor.velocity.x;
            ++enemy.shotTimer;
            if (enemy.shotTimer >= 120) {
                FireEnemyBullet(enemy.actor.position);
                enemy.shotTimer = 0;
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
            enemyBullets_[i].velocity = { -7.0f, 0.0f };
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
    }

    if (!boss_.active) {
        return;
    }

    boss_.position.y += static_cast<float>(bossMoveDirection_) * 2.0f;
    if (boss_.position.y >= 620.0f) {
        bossMoveDirection_ = -1;
    }
    if (boss_.position.y <= 140.0f) {
        bossMoveDirection_ = 1;
    }

    ++bossShotTimer_;
    if (bossShotTimer_ >= 60) {
        FireBossBullet();
        bossShotTimer_ = 0;
    }
}

void TitleScene::FireBossBullet()
{
    for (int i = 0; i < kBossBulletCount; ++i) {
        if (!bossBullets_[i].active) {
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

            bossBullets_[i].active = true;
            bossBullets_[i].position = { boss_.position.x - 72.0f, boss_.position.y };
            bossBullets_[i].velocity =
                { direction.x * 7.0f, direction.y * 7.0f };
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

void TitleScene::UpdateSprites()
{
    backgroundSprite_->Update();
    titlePanelSprite_->Update();

    playerSprite_->SetPosition(player_.position);
    playerSprite_->SetColor(kPlayerColor);
    if (playerInvincibleTimer_ > 0 && (playerInvincibleTimer_ / 5) % 2 == 0) {
        playerSprite_->SetColor(kPlayerHitColor);
    }
    playerSprite_->Update();

    for (int i = 0; i < kPlayerBulletCount; ++i) {
        playerBulletSprites_[i]->SetPosition(playerBullets_[i].actor.position);
        playerBulletSprites_[i]->SetSize({ 24.0f, 12.0f });
        playerBulletSprites_[i]->SetColor(kBulletColor);
        if (playerBullets_[i].charged) {
            playerBulletSprites_[i]->SetSize({ 54.0f, 34.0f });
            playerBulletSprites_[i]->SetColor(kChargedBulletColor);
        }
        playerBulletSprites_[i]->Update();
    }
    for (int i = 0; i < kEnemyCount; ++i) {
        enemySprites_[i]->SetPosition(enemies_[i].actor.position);
        enemySprites_[i]->Update();
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
    backgroundSprite_->Draw();

    if (sceneState_ == SceneState::Playing) {
        playerHpBackSprite_->Draw();
        playerHpSprite_->Draw();
        chargeBackSprite_->Draw();
        chargeSprite_->Draw();

        if (boss_.active) {
            bossHpBackSprite_->Draw();
            bossHpSprite_->Draw();
        }

        if (player_.active) {
            playerSprite_->Draw();
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
    } else {
        titlePanelSprite_->Draw();

        TextRenderer::GetInstance()->PreDraw();
        titleText_->Draw();
        guideText_->Draw();
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
