// 敵のHP・移動・攻撃・外見と部位の当たり判定をまとめる基底クラス。
#pragma once

#include "Engine/3D/Object3d.h"
#include <memory>
#include <vector>
#include <string_view>

#include "Engine/math/EngineStruct.h"
#include "Engine/CollisionManager/CollisionManager.h"

#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"
#include "App/Game/Enemy/EnemyAppearanceMotion.h"
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
#include "Debug/BaseEnemyDebugView.h"
#endif
class Camera;
class Model;
class EnemyBulletManager;

// 敵の一部位を当たり判定に渡すための位置・半径・識別情報。
struct EnemyCollisionPart {
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    float radius = 3.0f;
    int32_t partIndex = 0;
};

class BaseEnemy {
public:
    virtual ~BaseEnemy() = default;
    // ステージの外見対応表に渡す識別名。未対応の敵は外見を変更しない。
    virtual std::string_view GetAppearanceId() const { return {}; }
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
    virtual std::unique_ptr<BaseEnemyDebugView> CreateDebugView() { return nullptr; }
#endif

    // モデルと材質を設定し、敵として共通の描画オブジェクトを作る。
    virtual void Initialize(Model* model);
    // ステージ固有のモデルへ差し替え、テクスチャ本来の色で表示する。
    virtual void SetAppearance(Model* model);
    // 本体と追加パーツに適用する材質を変更する。
    void SetAppearanceMaterial(const std::string& materialPath);
    // 目や脚などの別モデルを追加し、担当する動きと基準位置を記録する。
    void AddAppearancePart(Model* model, EnemyAppearancePart motion);
    void SetAppearanceTarget(const Vector3& position) { appearanceTarget_ = position; }
    // 生存中だけ移動・攻撃・アニメーションを進め、外見パーツを追従させる。
    virtual void Update();
    virtual void Draw();
    virtual void SubmitRaytracing(DxrRenderer& renderer);
    virtual void DrawShadow(ShadowMapRenderer& renderer);

    virtual void Move();
    // 標準では射撃しない。具体的な敵の派生クラスで攻撃を実装する。
    virtual void Attack();

    bool IsDead() const;

    virtual Vector3 GetPosition() const;
    // 生成した敵弾の引き渡し先を設定する。管理クラスの所有権は受け取らない。
    static void SetBulletManager(EnemyBulletManager* bulletManager);
    virtual void SetPosition(const Vector3& position);
    virtual void SetRotate(const Vector3& rotate);
    void SetPatrolWaypoints(const std::vector<Vector3>& waypoints)
    {
        waypoints_ = waypoints;
        currentWaypointIndex_ = 0;
    }
    void SetEnableLighting(bool enable);
    // 死亡状態の変更を反映し、撃破されたタイミングで死亡処理を呼ぶ。
    void SetDead(bool isDead);
    // ダメージをHPへ反映し、被弾時やHPが尽きたときの処理につなげる。
    void ApplyDamage(float damage);
    // 部位別HPを持たない敵では、命中部位を分けずに本体へダメージを渡す。
    virtual void ApplyDamageToPart(int32_t partIndex, float damage);
    // 標準では敵本体を一つの球として、衝突判定用の一覧へ書き出す。
    virtual void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const;
    // 弾の前フレームからの移動区間を調べ、高速の弾でも当たりをすり抜けないようにする。
    virtual SweepHit SweepBullet(const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const;
    // 命中位置と部位番号を受け取り、部位へのダメージ処理へ渡す。
    virtual void OnBulletHit(int32_t partIndex, float damage, const Vector3& position);
    // 照準が敵を狙えるように、部位の球をレイ判定へ登録する。
    virtual void RegisterRaycastTargets(CollisionObjectId& nextObjectId) const;
    virtual bool RequiresBulletOcclusionCheck() const { return false; }
    // 標準の敵がダメージを受けられる状態かを返す。
    virtual bool IsCollisionPartDamageable(int32_t partIndex) const;
    // 防御時の演出を派生クラスで定義するための窓口。標準では何も行わない。
    virtual void OnCollisionPartGuarded(int32_t partIndex, const Vector3& position);

protected:
    // 生成した弾の所有権を敵弾管理へ渡し、必要なら発射音も鳴らす。
    void AddEnemyBullet(std::unique_ptr<EnemyBullet> bullet, bool shouldPlayShotSound = true);
    // 標準の追加アニメーション処理は空にし、必要な敵だけが上書きする。
    virtual void UpdateAnimation();
    // 被弾時の追加反応を派生クラスで定義するための窓口。標準では何も行わない。
    virtual void OnDamage(float damage);
    // 標準では撃破時に爆発を再生し、専用の演出は派生クラスで上書きする。
    virtual void OnDeath();

    std::unique_ptr<Object3d> object_;

    EulerTransform transform_;

    // 敵の最大HPと現在HPを設定する。
    void SetHealth(float hp);
    float GetHealth() const { return hp_; }
    // HPの初期値と死亡状態を初期化する。
    void InitializeHealth(float hp);

private:
    struct AppearancePart {
        std::unique_ptr<Object3d> object_;
        EnemyAppearancePart motion_;
        float maxTranslate_ = 0.0f;
    };
    // 視線・揺れ・反動による移動を計算し、本体に追従するパーツの姿勢へ反映する。
    void UpdateAppearanceParts();
    std::vector<AppearancePart> appearanceParts_;
    std::string appearanceMaterialPath_;
    Vector3 appearanceTarget_ {};
    Vector3 appearanceEyeOffset_ {};
    Vector3 appearanceEyeTravel_ {};
    Vector3 appearanceEyeAnchor_ {};
    float appearanceTimeSeconds_ = 0.0f;
    float appearancePhaseRadians_ = 0.0f;
    float appearanceRecoilSeconds_ = 0.0f;
    bool isDead_ = false;

    float hp_ = 1.0f;

protected:
    float moveSpeed_ = 0.1f;

    static EnemyBulletManager* bulletManager_;

    std::vector<Vector3> waypoints_;
    size_t currentWaypointIndex_ = 0;
};
