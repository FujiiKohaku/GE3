#pragma once

#include "App/Game/Enemy/BaseEnemy.h"

class Player;
class Model;

enum class MovePattern {
    LeftRight,
    UpDown,
    ZigZag,
    SineWave,
};

class MoveEnemy : public BaseEnemy {
public:
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    void Update() override;

    void DrawImGui();

    // ゲッター・セッター（ImGuiや外部調整用）
    MovePattern GetMovePattern() const { return movePattern_; }
    void SetMovePattern(MovePattern pattern) { movePattern_ = pattern; }

    float GetMoveSpeed() const { return moveSpeed_; }
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    float GetAmplitude() const { return amplitude_; }
    void SetAmplitude(float amplitude) { amplitude_ = amplitude; }

    float GetFrequency() const { return frequency_; }
    void SetFrequency(float frequency) { frequency_ = frequency; }

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
    MovePattern movePattern_;
    float moveSpeed_;
    float amplitude_;
    float frequency_;
    float moveTime_;
    Vector3 startPosition_;
    bool isStartPositionInitialized_;
};
