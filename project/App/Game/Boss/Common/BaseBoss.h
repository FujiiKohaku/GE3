// 敵の共通機能に、ボスの激昂・ビーム命中・部位HP・死亡演出の問い合わせを加える。
#pragma once

#include "App/Game/Enemy/BaseEnemy.h"

class BaseBoss : public BaseEnemy {
public:
    ~BaseBoss() override = default;

    // 撃破されたかとは別に、死亡演出が最後まで終わったかを問い合わせる。
    virtual bool IsDeathSequenceFinished() const = 0;
    // 攻撃や移動が強化される激昂状態かを問い合わせる。
    virtual bool IsMadModeActive() const = 0;
    // ビームがプレイヤーへ命中しているかを、画面演出などのために問い合わせる。
    virtual bool IsBeamHittingPlayer() const = 0;
    // 頭の残りHPを割合で取得し、ボスのHP表示に使う。
    virtual float GetHeadHpFraction() const = 0;
    // 胴体の残りHPを割合で取得し、ボスのHP表示に使う。
    virtual float GetBodyHpFraction() const = 0;
};
