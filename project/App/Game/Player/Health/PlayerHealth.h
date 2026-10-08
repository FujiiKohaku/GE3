#pragma once
#include "Engine/math/MathStruct.h"

class PlayerHealth {
public:
    // 使用クラス：Player（ApplyDamage）。無敵時間・ロール状態を確認し、HPを減らす。
    bool ApplyDamage(int damage, bool isRolling);
    // 使用クラス：Player（Heal）。生存中のHPを上限まで回復する。
    bool Heal(int amount);
    // 使用クラス：Player（Update）。被弾後の無敵時間を経過秒数で進める。
    void UpdateInvincibility(float deltaTimeSeconds = 1.0f / 60.0f);
    // 使用クラス：Player（UpdateDeath）。死亡時の落下・回転を更新する。
    void UpdateDeathAnimation(EulerTransform& transform, float deltaTimeSeconds);
    // 使用クラス：Player、PlayerWeaponController（発射制限）。HPが0以下かを返す。
    bool IsDead() const { return currentHp_ <= 0; }
    // 使用クラス：Player（Update）、本クラス（ShouldDraw）。通常の生存状態かを返す。
    bool IsAlive() const { return deathState_ == DeathState::Alive; }
    // 使用クラス：Player（UpdateDeath）、本クラス（ShouldDraw）。死亡演出の落下中かを返す。
    bool IsFalling() const { return deathState_ == DeathState::Falling; }
    // 使用クラス：Player（外部への公開）。落下演出が終わり、死亡爆発へ進めるかを返す。
    bool IsDeathExplosionReady() const { return deathState_ == DeathState::Exploded; }
    // 使用クラス：Player（Draw・DrawShadow）。無敵中の点滅と死亡状態に応じて描画を判定する。
    bool ShouldDraw() const;
    // 使用クラス：Player（外部への公開・DrawImGui）。現在のHPを返す。
    int GetCurrentHp() const { return currentHp_; }
    // 使用クラス：Player（外部への公開・DrawImGui）。HPの上限を返す。
    int GetMaxHp() const { return maxHp_; }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    // 使用クラス：Player（外部への公開）。開発用の無敵モードを切り替える。
    void SetInvincibleMode(bool isEnabled) { isInvincibleMode_ = isEnabled; }
    // 使用クラス：Player（外部への公開）。開発用の無敵モードが有効かを返す。
    bool IsInvincibleMode() const { return isInvincibleMode_; }
#endif

private:
    enum class DeathState { Alive, Falling, Exploded };
    int maxHp_ = 20;
    int currentHp_ = maxHp_;
    float invincibleRemainingSeconds_ = 0;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    bool isInvincibleMode_ = false;
#endif
    static constexpr float kInvincibleDurationSeconds = 1.0f;
    DeathState deathState_ = DeathState::Alive;
    float deathTimerSeconds_ = 0.0f;
    float deathFallVelocity_ = 0.0f;
    static constexpr float kDeathFallDurationSeconds = 1.2f;
};
