#pragma once

#include "App/Game/Enemy/BaseEnemy.h"
#include "BaseEnemyMovement.h"
#include <cmath>

class Player;
class Model;

class MoveEnemy : public BaseEnemy {
public:
    std::string_view GetAppearanceId() const override { return "MoveEnemy"; }
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    void Update() override;

#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
    std::unique_ptr<BaseEnemyDebugView> CreateDebugView() override;
#endif

    // ゲッター・セッター（ImGuiや外部調整用）
    MovePattern GetMovePattern() const { return movePattern_; }
    void SetMovePattern(MovePattern pattern) {
        if (pattern >= MovePattern::LeftRight && pattern <= MovePattern::SineWave) {
            movePattern_ = pattern;
            movement_ = &GetEnemyMovement(pattern);
        }
    }

    float GetMoveSpeed() const { return moveSpeed_; }
    void SetMoveSpeed(float speed) { if (std::isfinite(speed) && speed >= 0.0f) { moveSpeed_ = speed; } }

    float GetAmplitude() const { return amplitude_; }
    void SetAmplitude(float amplitude) { if (std::isfinite(amplitude) && amplitude >= 0.0f) { amplitude_ = amplitude; } }

    float GetFrequency() const { return frequency_; }
    void SetFrequency(float frequency) { if (std::isfinite(frequency) && frequency >= 0.0f) { frequency_ = frequency; } }
    void ResetPosition() { moveTime_ = 0.0f; transform_.translate = startPosition_; }

private:
    void Move() override;
    void Attack() override;
    void FireBullet();

private:
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;

    int32_t fireTimer_ = 0;
    int32_t fireInterval_ = 60;
    float bulletSpeed_ = 0.8f;

    // 移動制御用メンバ変数 (プロンプトの指定通り、名前は省略しない)
    MovePattern movePattern_ = MovePattern::LeftRight;
    const BaseEnemyMovement* movement_ = &GetEnemyMovement(MovePattern::LeftRight);
    float moveSpeed_;
    float amplitude_;
    float frequency_;
    float moveTime_;
    Vector3 startPosition_;
    bool isStartPositionInitialized_;
};
