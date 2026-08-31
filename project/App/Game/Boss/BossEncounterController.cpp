#include "BossEncounterController.h"

#include "App/Game/Boss/AngerBlockBoss/AngerBlockBoss.h"
#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "App/Game/Boss/StageBoss.h"
#include "Engine/Rail/Rail.h"

BossEncounterController::BossEncounterController() = default;
BossEncounterController::~BossEncounterController() = default;

void BossEncounterController::Initialize(
    const std::string& bossType,
    float spawnDistance,
    const Vector3& spawnPosition,
    Model* fearWormModel,
    Model* angerBlockModel,
    Model* enemyBulletModel,
    Player* player,
    Rail* rail,
    bool enableRailAutoExtension,
    float railExtensionBuffer)
{
    bossType_ = bossType;
    spawnDistance_ = spawnDistance;
    spawnPosition_ = spawnPosition;
    fearWormModel_ = fearWormModel;
    angerBlockModel_ = angerBlockModel;
    enemyBulletModel_ = enemyBulletModel;
    player_ = player;
    rail_ = rail;
    enableRailAutoExtension_ = enableRailAutoExtension;
    railExtensionBuffer_ = railExtensionBuffer;
    activeBoss_.reset();
    isSpawned_ = false;
}

void BossEncounterController::Update(float railDistance)
{
    didSpawnThisFrame_ = false;
    didEnterMadModeThisFrame_ = false;
    isBeamHittingPlayer_ = false;
    didExtendRailThisFrame_ = false;

    if (bossType_ == "None") {
        return;
    }

    if (!isSpawned_ && railDistance >= spawnDistance_) {
        SpawnBoss();
    }

    if (activeBoss_ == nullptr) {
        return;
    }

    const bool wasMadMode = activeBoss_->IsMadModeActive();
    activeBoss_->Update();
    didEnterMadModeThisFrame_ = activeBoss_->IsMadModeActive() && !wasMadMode;
    isBeamHittingPlayer_ = activeBoss_->IsBeamHittingPlayer();

    if (activeBoss_->IsDead()) {
        if (rail_ != nullptr) {
            rail_->StopAutoExtension();
        }
        return;
    }

    if (rail_ != nullptr) {
        didExtendRailThisFrame_ = rail_->UpdateAutoExtension(railDistance);
    }
}

void BossEncounterController::UpdateDeathSequence()
{
    if (activeBoss_ != nullptr && activeBoss_->IsDead()) {
        activeBoss_->Update();
    }
}

StageBoss* BossEncounterController::GetActiveBoss() const
{
    return activeBoss_.get();
}

bool BossEncounterController::IsSpawned() const
{
    return isSpawned_;
}

bool BossEncounterController::DidSpawnThisFrame() const
{
    return didSpawnThisFrame_;
}

bool BossEncounterController::DidEnterMadModeThisFrame() const
{
    return didEnterMadModeThisFrame_;
}

bool BossEncounterController::IsBeamHittingPlayer() const
{
    return isBeamHittingPlayer_;
}

bool BossEncounterController::DidExtendRailThisFrame() const
{
    return didExtendRailThisFrame_;
}

void BossEncounterController::SpawnBoss()
{
    if (bossType_ == "AngerBlock") {
        auto angerBoss = std::make_unique<AngerBlockBoss>();
        angerBoss->Initialize(angerBlockModel_, enemyBulletModel_, player_);
        activeBoss_ = std::move(angerBoss);
    } else {
        auto fearWorm = std::make_unique<FearWormEnemy>();
        fearWorm->Initialize(fearWormModel_, enemyBulletModel_, player_);
        activeBoss_ = std::move(fearWorm);
    }

    activeBoss_->SetPosition(spawnPosition_);
    isSpawned_ = true;
    didSpawnThisFrame_ = true;

    if (enableRailAutoExtension_ && rail_ != nullptr) {
        rail_->StartAutoExtension(railExtensionBuffer_);
    }
}
