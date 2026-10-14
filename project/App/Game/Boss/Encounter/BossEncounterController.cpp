#include "BossEncounterController.h"

#include "BaseBossFactory.h"
#include "App/Game/Boss/Common/BaseBoss.h"
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
    Camera* camera,
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
    camera_ = camera;
    rail_ = rail;
    enableRailAutoExtension_ = enableRailAutoExtension;
    railExtensionBuffer_ = railExtensionBuffer;
    activeBoss_.reset();
    presentation_.reset();
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

void BossEncounterController::UpdatePresentation(const BossPresentationContext& context)
{
    if (activeBoss_ && presentation_) presentation_->Update(*activeBoss_, context);
}

BaseBoss* BossEncounterController::GetActiveBoss() const
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
    const BaseBossFactory* factory = FindBossFactory(bossType_);
    if (!factory) return;
    const BossSpawnContext context {
        fearWormModel_, angerBlockModel_, enemyBulletModel_, player_, camera_ };
    activeBoss_ = factory->Create(context);
    presentation_ = factory->CreatePresentation();

    activeBoss_->SetPosition(spawnPosition_);
    isSpawned_ = true;
    didSpawnThisFrame_ = true;

    if (enableRailAutoExtension_ && rail_ != nullptr) {
        rail_->StartAutoExtension(railExtensionBuffer_);
    }
}
