#pragma once
#include "KamataEngine.h"
#include "Math.h"
#include <array>
#include <numbers>

class AlmaDustParticles {
public:
    // 初期化
    void Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera);

    // 発生させる
    void Emit(const KamataEngine::AlmaVector3& position);

    // 着地時に発生させる
    void EmitLand(const KamataEngine::AlmaVector3& position);

    // 更新
    void Update();

    // 描画
    void Draw();

    // 終了判定
    bool IsFinished() const { return isFinished_; }

private:
    static inline const uint32_t kNumParticles = 12; // パーティクルの数
    static inline const float kDuration_ = 0.6f;     // 寿命（秒）
    static inline const float kMinSpeed_ = 0.01f;    // 最小速度
    static inline const float kMaxSpeed_ = 0.05f;    // 最大速度

    KamataEngine::AlmaModel* model_ = nullptr;
    KamataEngine::AlmaCamera* camera_ = nullptr;

    std::array<KamataEngine::AlmaWorldTransform, kNumParticles> worldTransforms_;
    std::array<KamataEngine::AlmaVector3, kNumParticles> velocities_;

    float counter_ = 0.0f;
    bool isFinished_ = true;

    KamataEngine::AlmaObjectColor objectColor_;
    KamataEngine::AlmaVector4 color_;
};
