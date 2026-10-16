// 指定した進行距離でボスを生成・所有し、戦闘更新とレール延長を仲介する。
#pragma once

#include "Engine/Math/MathStruct.h"
#include <memory>
#include <string>

class Model;
class Player;
class Rail;
class BaseBoss;
class Camera;

class BossEncounterController {
public:
    BossEncounterController();
    ~BossEncounterController();

    // ボス名と出現条件、外部オブジェクトへの参照を記録し、前回のボスを破棄する。
    void Initialize(const std::string& bossType,
        float spawnDistance,
        const Vector3& spawnPosition,
        Model* fearWormModel,
        Model* enemyBulletModel,
        Player* player,
        Camera* camera,
        Rail* rail,
        bool enableRailAutoExtension,
        float railExtensionBuffer,
        float waterHeight = -30.0f);

    // 出現距離を確認し、所有するボスを更新する。状態変化を記録し、生存中だけレールを延長する。
    void Update(float railDistance);
    // 死亡したボスの更新を続け、戦闘終了後も死亡演出を進める。
    void UpdateDeathSequence();

    // 現在所有しているボスを返す。呼び出し元へ所有権は渡さない。
    BaseBoss* GetActiveBoss() const;
    // ボスの生成が完了しているかを返す。現在の生存状態とは別の情報。
    bool IsSpawned() const;
    // 出現した瞬間だけを通知し、登場演出を重複して開始しないようにする。
    bool DidSpawnThisFrame() const;
    // 激昂へ切り替わったフレームを、効果音や演出の開始条件として返す。
    bool DidEnterMadModeThisFrame() const;
    // このフレームにボスのビームが機体へ当たっているかを返す。
    bool IsBeamHittingPlayer() const;
    // この更新でレールが延びたかを返し、シーン側の追従処理につなげる。
    bool DidExtendRailThisFrame() const;

private:
    void SpawnBoss();

    std::unique_ptr<BaseBoss> activeBoss_;
    std::string bossType_;
    Vector3 spawnPosition_ { };
    float spawnDistance_ = 0.0f;
    float railExtensionBuffer_ = 0.0f;
    float waterHeight_ = -30.0f;
    Model* fearWormModel_ = nullptr;
    Model* enemyBulletModel_ = nullptr;
    Player* player_ = nullptr;
    Camera* camera_ = nullptr;
    Rail* rail_ = nullptr;
    bool enableRailAutoExtension_ = false;
    bool isSpawned_ = false;
    bool didSpawnThisFrame_ = false;
    bool didEnterMadModeThisFrame_ = false;
    bool isBeamHittingPlayer_ = false;
    bool didExtendRailThisFrame_ = false;
};
