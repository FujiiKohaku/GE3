#pragma once
#include <cstdint>

namespace EffectParticleLimits {
inline constexpr uint32_t kMaxParticleCapacity = 4096;
inline constexpr uint32_t kMaxEmitCount = kMaxParticleCapacity;
inline constexpr uint32_t kEmitThreadsPerGroup = 256;

inline bool IsValidEmitCount(uint32_t emitCount)
{
    return emitCount <= kMaxEmitCount;
}
}
