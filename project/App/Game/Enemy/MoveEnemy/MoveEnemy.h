// 移動パターンに沿って位置を変えながら、プレイヤーに向けて射撃する敵を扱う。
#pragma once

#include "App/Game/Enemy/BaseEnemy.h"
#include <cmath>

enum class MovePattern { LeftRight, UpDown, ZigZag, SineWave };

class Player;
class Model;

class MoveEnemy : public BaseEnemy {
public:
    std::string_view GetAppearanceId() const override { return "MoveEnemy"; }
    // 移動の開始位置とパターン、射撃用のモデルやプレイヤー参照を準備する。
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    // 基底クラスを通じて、この敵の移動・射撃・見た目を更新する。
    void Update() override;

#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
    std::unique_ptr<BaseEnemyDebugView> CreateDebugView() override;
#endif

    // ゲッター・セッター（ImGuiや外部調整用）
    MovePattern GetMovePattern() const { return movePattern_; }
    void SetMovePattern(MovePattern pattern) {
        if (pattern >= MovePattern::LeftRight && pattern <= MovePattern::SineWave) {
            movePattern_ = pattern;
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
    friend bool RunEnemyMovementCalculationTests();
    friend bool RunAppTimingTests();
    // 選択された移動パターンに応じて、開始位置からの変位を求める。
    static Vector3 CalculateMovementPosition(MovePattern pattern, const Vector3& startPosition,
        float elapsedSeconds, float speed, float amplitude, float frequency);
    // 経過時間を進め、選択した軌道上の位置へ本体を移動する。
    void Move() override;
    // プレイヤーとの距離や向きを確認し、射撃可能な場合だけ発射タイマーを進める。
    void Attack() override;
    // 現在位置からプレイヤーを狙う弾を生成する。
    void FireBullet();

private:
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;

    float fireElapsedSeconds_ = 0.0f;
    static constexpr float kFireIntervalSeconds = 1.0f;
    float bulletSpeed_ = 0.8f;

    // 移動制御用メンバ変数 (プロンプトの指定通り、名前は省略しない)
    MovePattern movePattern_ = MovePattern::LeftRight;
    float moveSpeed_;
    float amplitude_;
    float frequency_;
    float moveTime_;
    Vector3 startPosition_;
    bool isStartPositionInitialized_;
};
