#pragma once

#include <cstdint>

using CollisionLayer = uint32_t;
constexpr CollisionLayer kCollisionLayerNone = 0;
constexpr CollisionLayer kCollisionLayerAll = 0xffffffffu;

constexpr bool CollisionLayersMatch(
    CollisionLayer firstCategory,
    CollisionLayer firstMask,
    CollisionLayer secondCategory,
    CollisionLayer secondMask)
{
    return (firstCategory & secondMask) != 0 &&
        (secondCategory & firstMask) != 0;
}
