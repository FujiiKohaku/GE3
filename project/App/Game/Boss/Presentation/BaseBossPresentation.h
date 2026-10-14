#pragma once
#include "Engine/math/MathStruct.h"
#include <functional>
#include <memory>
#include <cstdint>

class BaseBoss;

// ライトや霧の所有者はシーン。演出クラスは位置の更新だけを要求する。
struct BossPresentationContext {
    bool followFog = false;
    std::function<void(const Vector3&)> setSpotPosition;
    std::function<void(const Vector3&)> setPointPosition;
    std::function<void(uint32_t, const Vector3&)> setFogCenter;
};

class BaseBossPresentation {
public:
    virtual ~BaseBossPresentation() = default;
    virtual void Update(const BaseBoss& boss, const BossPresentationContext& context) = 0;
};

std::unique_ptr<BaseBossPresentation> CreateJellyfishPresentation();
