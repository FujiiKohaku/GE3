#pragma once
#include "MatrixMath.h"
#include <string>
class ParticleEmitter {
public:
    // --- コンストラクタ ---
    ParticleEmitter(); // 既定コンストラクタ
    ParticleEmitter(const std::string& groupName, const Vector3& position);

    // --- 基本関数 ---
    void Initialize();
    void Update();
    void Emit(); // パーティクルを放出

    // --- Setter ---
    void SetGroupName(const std::string& name);
    void SetPosition(const Vector3& pos);

    // --- Getter ---
    const std::string& GetGroupName() const { return groupName_; }
    const Vector3& GetPosition() const { return position_; }

private:
    std::string groupName_; // どのパーティクルグループを使うか
    Vector3 position_ { 0, 0, 0 }; // ★ 初期値を明示的に
};
