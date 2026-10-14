#pragma once
#include "externals/json.hpp"
#include "Engine/math/MathStruct.h"
#include <string>

// 開発UIだけの機能。ゲーム中の敵の振る舞いから調整項目を分離する。
class BaseEnemyDebugView {
public:
    virtual ~BaseEnemyDebugView() = default;
    virtual void DrawInspector() = 0;
    virtual bool HasMovementControls() const { return false; }
    virtual void AppendControls(nlohmann::json&, int) const {}
    virtual nlohmann::json GetState(int) const { return {}; }
    virtual bool Apply(const std::string&, const std::string&) { return false; }
    virtual bool HasBossInspector() const { return false; }
    virtual float GetBossHp() const { return 0.0f; }
    virtual void DrawBossInspector(bool&, float&, const Vector3&) {}
    virtual void DrawCollisionDebug() const {}
};
