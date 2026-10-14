#pragma once
#include "Engine/math/MathStruct.h"

enum class MovePattern { LeftRight, UpDown, ZigZag, SineWave };

struct EnemyMovementContext {
    Vector3 startPosition;
    float time;
    float speed;
    float amplitude;
    float frequency;
};

class BaseEnemyMovement {
public:
    virtual ~BaseEnemyMovement() = default;
    virtual Vector3 CalculatePosition(const EnemyMovementContext& context) const = 0;
};

const BaseEnemyMovement& GetEnemyMovement(MovePattern pattern);
