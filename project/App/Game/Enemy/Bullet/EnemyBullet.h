#pragma once

#include "App/Game/Common/Bullet/BaseBullet.h"

class EnemyBullet : public BaseBullet {
public:
    virtual ~EnemyBullet() = default;

    void Initialize(Model* model) override;

    virtual void OnHitPlayer(const Vector3& position);

    static void SetTimeScale(float timeScale);
    static float GetCurrentTimeScale();

    void MarkJustDodgeCandidate();
    bool ResolveJustDodge();

protected:
    float GetTimeScale() const override;

private:
    static float timeScale_;
    bool justDodgeCandidate_ = false;
    bool justDodgeResolved_ = false;
};
