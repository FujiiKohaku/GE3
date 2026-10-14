#include "BaseEnemyMovement.h"
#include <array>
#include <cmath>

class LeftRightMovement final : public BaseEnemyMovement {
public:
    Vector3 CalculatePosition(const EnemyMovementContext& context) const override {
        Vector3 position = context.startPosition;
        position.x += std::sin(context.time * context.speed) * context.amplitude;
        return position;
    }
};

class UpDownMovement final : public BaseEnemyMovement {
public:
    Vector3 CalculatePosition(const EnemyMovementContext& context) const override {
        Vector3 position = context.startPosition;
        position.y += std::sin(context.time * context.speed) * context.amplitude;
        return position;
    }
};

class ZigZagMovement final : public BaseEnemyMovement {
public:
    Vector3 CalculatePosition(const EnemyMovementContext& context) const override {
        Vector3 position = context.startPosition;
        position.x += std::sin(context.time * context.speed) * context.amplitude;
        position.y += std::sin(context.time * context.speed * 2.0f) * context.amplitude * 0.5f;
        return position;
    }
};

class ForwardWaveMovement final : public BaseEnemyMovement {
public:
    Vector3 CalculatePosition(const EnemyMovementContext& context) const override {
        Vector3 position = context.startPosition;
        position.y += std::sin(context.time * context.frequency) * context.amplitude;
        position.z -= context.time * context.speed * 1.5f;
        return position;
    }
};

const BaseEnemyMovement& GetEnemyMovement(MovePattern pattern)
{
    static const LeftRightMovement horizontal;
    static const UpDownMovement vertical;
    static const ZigZagMovement zigzag;
    static const ForwardWaveMovement wave;
    static const std::array<const BaseEnemyMovement*, 4> movements { &horizontal, &vertical, &zigzag, &wave };
    const auto index = static_cast<size_t>(pattern);
    return *movements[index < movements.size() ? index : 0];
}
