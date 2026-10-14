#pragma once
#include "Engine/math/MathStruct.h"
#include <cstdint>

enum class SwarmFormationType { Spiral, Wall, Glyph, Diamond, Wave, Arrow };

struct SwarmFormationContext {
    int32_t slotIndex;
    int32_t travelDirection;
    float time;
};

class BaseSwarmFormation {
public:
    virtual ~BaseSwarmFormation() = default;
    virtual Vector3 CalculateOffset(const SwarmFormationContext& context) const = 0;
};

struct SwarmFormationDefinition {
    const BaseSwarmFormation* strategy;
    Vector4 color;
};

// 配置の振る舞いと、隊形ごとの色設定を一緒に選択する。
const SwarmFormationDefinition& GetSwarmFormation(SwarmFormationType type);
