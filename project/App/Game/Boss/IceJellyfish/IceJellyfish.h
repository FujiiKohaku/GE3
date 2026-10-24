// 頭・触手・氷柱で構成するボス。攻撃パターン、部位ごとのHPと当たり判定を扱う。
#pragma once

#include "App/Game/Boss/Common/BaseBoss.h"
#include "PoseController/IceJellyfishPoseController.h"
#include "Wait/IceJellyfishWaitState.h"
#include "Entry/IceJellyfishEntryState.h"
#include "Battle/IceJellyfishBattleState.h"
#include "Dead/IceJellyfishDeadState.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Math/MatrixMath.h"
#include <array>
#include <memory>
#include <vector>

class Camera;
class Model;
class Player;

class IceJellyfish final : public BaseBoss {
public:
    enum class BossState { Wait, Entry, Battle, Dead };
    IceJellyfish();
    BossState GetBossState() const { return state_; }
    static constexpr float kMaxHp = 120.0f;
    static constexpr size_t kTentacleCount = IceJellyfishPoseController::kTentacleCount;
    static constexpr size_t kSegmentsPerTentacle = IceJellyfishPoseController::kSegmentsPerTentacle;
    static constexpr size_t kIcePillarModelCount = IceJellyfishBattleState::kIcePillarModelCount;

    struct PresentationPositions {
        Vector3 spotLightPosition;
        Vector3 pointLightPosition;
        std::array<Vector3, 2> fogCenters;
    };

    // 配置を計算する。ライト・霧の所有と反映はシーンが担当する。
    PresentationPositions GetPresentationPositions() const;

    // 頭・触手と氷柱を用意し、攻撃と部位HPを初期状態にする。
    void Initialize(Camera* camera, Model* bulletModel, Player* player);
    // 生存状態を確認し、攻撃の進行と部位の姿勢を更新する。
    void Update() override;
    void Draw() override;
    void SubmitRaytracing(DxrRenderer& renderer) override;
    void DrawShadow(ShadowMapRenderer& renderer) override;
    // 配置の基準位置を変更し、部位の位置も更新する。
    void SetPosition(const Vector3& position) override;
    Vector3 GetPosition() const override { return basePosition_; }

    // 弾の移動区間と各部位を調べ、最も手前で当たった部位を返す。
    SweepHit SweepBullet(
        const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const override;
    // 命中した部位へダメージを渡し、命中位置を使う演出につなげる。
    void OnBulletHit(int32_t partIndex, float damage, const Vector3& position) override;
    // 照準が頭や触手を狙えるように、各部位をレイ判定へ登録する。
    void RegisterRaycastTargets(CollisionObjectId& nextObjectId) const override;
    bool RequiresBulletOcclusionCheck() const override { return true; }
    // 部位や特殊攻撃の当たり判定を、開発用の線で可視化する。
    void DrawCollisionDebug() const;
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
    std::unique_ptr<BaseEnemyDebugView> CreateDebugView() override;
#endif

    // 現在の部位配置を、接触判定に使う球の一覧へ変換する。
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;
    // 指定部位が現在ダメージを受けられるかを確認する。
    bool IsCollisionPartDamageable(int32_t partIndex) const override;
    // 頭や触手など、命中先の部位に対応するHPを減らす。
    void ApplyDamageToPart(int32_t partIndex, float damage) override;
    // 防御中の部位に命中したことを、エフェクトで伝える。
    void OnCollisionPartGuarded(int32_t partIndex, const Vector3& position) override;

    bool IsDeathSequenceFinished() const override { return deadState_.IsDeathSequenceFinished(); }
    bool IsMadModeActive() const override { return GetHealth() <= kMaxHp * kMadModeHealthThreshold; }
    bool IsBeamHittingPlayer() const override { return false; }
    // 頭の残りHPをHUD用の割合として返す。
    float GetHeadHpFraction() const override;
    // 触手などの残りHPをHUD用の割合として返す。
    float GetBodyHpFraction() const override;
    float GetHp() const { return GetHealth(); }

    // 攻撃用の読み取りと操作。モデル配列やプレイヤー自体は公開しない。
    bool HasAttackTarget() const { return player_ != nullptr; }
    Vector3 GetAttackTargetPosition() const;
    Vector3 GetAttackBodyPosition() const { return bodyPosition_; }
    Vector3 GetAttackHoverCenter() const { return { hoverCenterX_, hoverCenterY_, 0.0f }; }
    Model* GetAttackBulletModel() const { return bulletModel_; }
    Vector3 GetAttackSegmentPosition(size_t tentacle, size_t segment) const;
    bool ApplyAttackDamageToPlayer(int damage);
    void SpawnAttackBullet(std::unique_ptr<EnemyBullet> bullet);
    bool IsSegmentAlive(size_t tentacle, size_t segment) const;

private:
    friend class IceJellyfishPoseController;
    friend class IceJellyfishWaitState;
    friend class IceJellyfishEntryState;
    friend class IceJellyfishBattleState;
    friend class IceJellyfishDeadState;
    friend bool RunIceJellyfishStateTests();
    static constexpr int32_t kMaxStateUpdatesPerFrame = 3;
    void ChangeState(BossState state);
    void ResetStates();
    void UpdateState();
    void OnDeath() override;
    friend bool RunJellyfishAttackTests();
    friend int RunIceJellyfishCollisionTests();

    static OBB MakeCollisionBox(const Matrix4x4& world, const Vector3& center, const Vector3& size);
    static void BuildBellColliders(const Matrix4x4& bellWorld, float pulse, std::vector<OBB>& boxes, const Vector4& animation = {});
    static constexpr float kMadModeHealthThreshold = 0.35f;
    static constexpr float kHoverDistance = 125.0f;
    static constexpr float kSegmentHp = 9.0f;
    static constexpr float kTentacleMaxHp = kSegmentHp * static_cast<float>(kSegmentsPerTentacle);
    static constexpr int32_t kCorePart = 0;
    static constexpr int32_t kFirstSegmentPart = 1;
    static constexpr int32_t kBellPart = 100;

    // モデルと描画設定から、ボスの一部として使うオブジェクトを生成する。
    std::unique_ptr<Object3d> CreatePart(
        Camera* camera, const char* modelPath, const Vector4& color, bool ice);

    // 攻撃の姿勢と揺れを反映して、頭や触手の位置・回転・大きさを更新する。
    void UpdatePartTransforms();
    // 指定した触手の節が破壊されずに残っているかを調べる。
    // すべての触手の破壊状態を確認する。
    bool AreAllTentaclesDestroyed() const;
    // 触手番号と節番号を、当たり判定で共通に使う部位番号へ変換する。
    int32_t SegmentPartIndex(size_t tentacle, size_t segment) const;
    // 今回の命中が現在の候補より手前なら、命中結果と部位番号を置き換える。
    static void ConsiderNearestHit(SweepHit& nearest, int32_t& partIndex,
        const SweepHit& hit, int32_t candidatePart);

    std::unique_ptr<Object3d> bell_;
    std::unique_ptr<Object3d> core_;
    std::array<std::array<std::unique_ptr<Object3d>, kSegmentsPerTentacle>,
        kTentacleCount> tentacles_;
    std::array<std::unique_ptr<Object3d>, kIcePillarModelCount> icePillarModels_;
    std::array<float, kTentacleCount> tentacleHp_ {};
    IceJellyfishPoseController poseController_;

    Camera* camera_ = nullptr;
    Model* bulletModel_ = nullptr;
    Player* player_ = nullptr;
    Vector3 basePosition_ {};
    Vector3 bodyPosition_ {};
    float hoverCenterX_ = 0.0f;
    float hoverCenterY_ = 0.0f;
    float animationTime_ = 0.0f;
    float hitFlashTime_ = 0.0f;
    bool coreExposed_ = false;

    std::vector<OBB> bellColliders_;
    std::vector<OBB> tentacleColliders_;
    std::vector<int32_t> tentacleColliderParts_;
    Sphere coreCollider_ {};

    IceJellyfishWaitState waitState_;
    IceJellyfishEntryState entryState_;
    IceJellyfishBattleState battleState_;
    IceJellyfishDeadState deadState_;
    BossState state_ = BossState::Wait;
    IceJellyfishState* currentState_ = nullptr;
};
