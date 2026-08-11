#pragma once

#include "Engine/Math/MathStruct.h"
#include <memory>
#include <string>

class Model;
class Player;
class Rail;
class StageBoss;

class BossEncounterController {
public:
    BossEncounterController();
    ~BossEncounterController();

    void Initialize(
        const std::string& bossType,
        float spawnDistance,
        const Vector3& spawnPosition,
        Model* fearWormModel,
        Model* angerBlockModel,
        Model* enemyBulletModel,
        Player* player,
        Rail* rail,
        bool enableRailAutoExtension,
        float railExtensionBuffer);

    void Update(float railDistance);
    void UpdateDeathSequence();

    StageBoss* GetActiveBoss() const;
    bool IsSpawned() const;
    bool DidSpawnThisFrame() const;
    bool DidEnterMadModeThisFrame() const;
    bool IsBeamHittingPlayer() const;
    bool DidExtendRailThisFrame() const;

private:
    void SpawnBoss();

    std::unique_ptr<StageBoss> activeBoss_;
    std::string bossType_;
    Vector3 spawnPosition_ {};
    float spawnDistance_ = 0.0f;
    float railExtensionBuffer_ = 0.0f;
    Model* fearWormModel_ = nullptr;
    Model* angerBlockModel_ = nullptr;
    Model* enemyBulletModel_ = nullptr;
    Player* player_ = nullptr;
    Rail* rail_ = nullptr;
    bool enableRailAutoExtension_ = false;
    bool isSpawned_ = false;
    bool didSpawnThisFrame_ = false;
    bool didEnterMadModeThisFrame_ = false;
    bool isBeamHittingPlayer_ = false;
    bool didExtendRailThisFrame_ = false;
};
