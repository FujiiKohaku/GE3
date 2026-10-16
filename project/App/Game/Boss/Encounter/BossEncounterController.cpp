// 指定した進行距離でボスを生成・所有し、戦闘更新とレール延長を仲介する。
#include "BossEncounterController.h"

#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"
#include <utility>
#include "App/Game/Boss/Common/BaseBoss.h"
#include "Engine/Rail/Rail.h"
#include "Engine/Logger/Logger.h"
BossEncounterController::BossEncounterController() = default;
BossEncounterController::~BossEncounterController() = default;

// ボス名と出現条件、外部オブジェクトへの参照を記録し、前回のボスを破棄する。
void BossEncounterController::Initialize(
    const std::string& bossType,
    float spawnDistance,
    const Vector3& spawnPosition,
    Model* fearWormModel,
    Model* enemyBulletModel,
    Player* player,
    Camera* camera,
    Rail* rail,
    bool enableRailAutoExtension,
    float railExtensionBuffer,
    float waterHeight)
{
   
    bossType_ = bossType;
    // 未登録のボスタイプが指定された場合はNoneに置き換える
    if (bossType_ != "None"
        && bossType_ != "IceJellyfish"
        && bossType_ != "FearWorm") {
        Logger::Log("Unknown boss type: " + bossType_);
        bossType_ = "None";
    }
    spawnDistance_ = spawnDistance;
    spawnPosition_ = spawnPosition;
    fearWormModel_ = fearWormModel;
    enemyBulletModel_ = enemyBulletModel;
    player_ = player;
    camera_ = camera;
    rail_ = rail;
    enableRailAutoExtension_ = enableRailAutoExtension;
    railExtensionBuffer_ = railExtensionBuffer;
    waterHeight_ = waterHeight;
    activeBoss_.reset();
    isSpawned_ = false;
}

// 出現距離を確認し、所有するボスを更新する。状態変化を記録し、生存中だけレールを延長する。
void BossEncounterController::Update(float railDistance)
{
    didSpawnThisFrame_ = false;
    didEnterMadModeThisFrame_ = false;
    isBeamHittingPlayer_ = false;
    didExtendRailThisFrame_ = false;

    // ボスが存在しない場合は何もしない
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

// 死亡したボスの更新を続け、戦闘終了後も死亡演出を進める。
void BossEncounterController::UpdateDeathSequence()
{
    if (activeBoss_ != nullptr && activeBoss_->IsDead()) {
        activeBoss_->Update();
    }
}

// 現在所有しているボスを返す。呼び出し元へ所有権は渡さない。
BaseBoss* BossEncounterController::GetActiveBoss() const
{
    return activeBoss_.get();
}

// ボスの生成が完了しているかを返す。現在の生存状態とは別の情報。
bool BossEncounterController::IsSpawned() const
{
    return isSpawned_;
}

// 出現した瞬間だけを通知し、登場演出を重複して開始しないようにする。
bool BossEncounterController::DidSpawnThisFrame() const
{
    return didSpawnThisFrame_;
}

// 激昂へ切り替わったフレームを、効果音や演出の開始条件として返す。
bool BossEncounterController::DidEnterMadModeThisFrame() const
{
    return didEnterMadModeThisFrame_;
}

// このフレームにボスのビームが機体へ当たっているかを返す。
bool BossEncounterController::IsBeamHittingPlayer() const
{
    return isBeamHittingPlayer_;
}

// この更新でレールが延びたかを返し、シーン側の追従処理につなげる。
bool BossEncounterController::DidExtendRailThisFrame() const
{
    return didExtendRailThisFrame_;
}



// ボスの種類が少なく、生成箇所もここだけなのでif文で分岐する。
// 生成処理が複雑化・重複した場合はFactoryへの分離を検討する。
void BossEncounterController::SpawnBoss()
{
    
    if (bossType_ == "None") { 
        return;
    } 
    if (bossType_ == "IceJellyfish") {
        auto boss = std::make_unique<IceJellyfish>();
        boss->Initialize(camera_, enemyBulletModel_, player_);
        activeBoss_ = std::move(boss);
    } else if(bossType_ == "FearWorm") {
        auto boss = std::make_unique<FearWormEnemy>();
        boss->Initialize(fearWormModel_, enemyBulletModel_, player_);
        boss->SetEntryWaterHeight(waterHeight_);
        activeBoss_ = std::move(boss);
    } else {
        return; // 未登録なら生成しない
    }

    activeBoss_->SetPosition(spawnPosition_);
    isSpawned_ = true;
    didSpawnThisFrame_ = true;

    if (enableRailAutoExtension_ && rail_ != nullptr) {
        rail_->StartAutoExtension(railExtensionBuffer_);
    }
}
