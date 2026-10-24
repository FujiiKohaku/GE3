// ミサイルの描画・威力と航跡エフェクトを扱う。消滅時には航跡も停止する。
#pragma once

#include "Engine/Effect/EffectManager.h"
#include "App/Game/Player/Combat/Bullet/Common/PlayerBullet.h"

#include <memory>

class MissileBullet : public PlayerBullet {
public:
    // ミサイルの見た目と威力を設定し、追従する航跡エフェクトを開始する。
    void Initialize(Model* model) override;
    // 移動と寿命を更新し、航跡を追従させる。消滅した場合は航跡を停止する。
    void Update() override;
    // 本体の位置を設定し、航跡の追従先も同じ位置へ更新する。
    void SetTranslate(const Vector3& translate) override;
    // 弾を消滅扱いにし、その場に航跡だけが残り続けないよう停止する。
    void SetDead() override;
   // void Move() override;
    float GetSpeed() const
    {
        return speed_;
    }
    // 命中位置にミサイル用の演出を出す。
    void OnHitEnemy(const Vector3& position) override;

private:
    // ミサイルの位置を追いかけるループエフェクトを生成する。
    void CreateTrailEffect();
    // 弾の位置に航跡の発生位置を追従させる。
    void UpdateTrailPosition();
    // 再生中の航跡を停止し、エフェクトのハンドルを無効化する。
    void StopTrailEffect();

private:
    float speed_ = 180.0f;
    float scale_ = 2.0f;

    EffectHandle trailEffectHandle_ = kInvalidEffectHandle;

    std::shared_ptr<Vector3> trailPosition_;
};
