#include "BaseSwarmFormation.h"
#include <array>
#include <cmath>
#include <numbers>

namespace {
constexpr int32_t kSwarmMemberCount = 18;
}
class SpiralSwarmFormation final : public BaseSwarmFormation {
public:
    Vector3 CalculateOffset(const SwarmFormationContext& context) const override
    {
        float slotRate =
            static_cast<float>(context.slotIndex) / static_cast<float>(kSwarmMemberCount);
        float angle = slotRate * std::numbers::pi_v<float> * 2.0f;
        angle += context.time * 2.4f;
    
        Vector3 offset {};
        offset.x = std::cos(angle) * 4.5f;
        offset.y = std::sin(angle) * 8.0f;
        offset.z = std::sin(angle * 2.0f) * 3.0f;
        return offset;
    }
};

class WallSwarmFormation final : public BaseSwarmFormation {
public:
    Vector3 CalculateOffset(const SwarmFormationContext& context) const override
    {
        constexpr int32_t kColumnCount = 6;
        int32_t column = context.slotIndex % kColumnCount;
        int32_t row = context.slotIndex / kColumnCount;
    
        Vector3 offset {};
        offset.x = (static_cast<float>(column) - 2.5f) * 2.5f;
        offset.y = (static_cast<float>(row) - 1.0f) * 6.0f;
        offset.z =
            std::sin(context.time * 3.0f + static_cast<float>(column) * 0.5f) * 1.2f;
        return offset;
    }
};

class GlyphSwarmFormation final : public BaseSwarmFormation {
public:
    Vector3 CalculateOffset(const SwarmFormationContext& context) const override
    {
        constexpr int32_t kLeftBranchCount = 9;
        Vector3 offset {};
        float branchRate = 0.0f;
    
        if (context.slotIndex < kLeftBranchCount) {
            branchRate =
                static_cast<float>(context.slotIndex) /
                static_cast<float>(kLeftBranchCount - 1);
            offset.x = -9.0f + branchRate * 9.0f;
            offset.y = 8.0f - branchRate * 14.0f;
        } else {
            int32_t rightIndex = context.slotIndex - kLeftBranchCount;
            branchRate =
                static_cast<float>(rightIndex) /
                static_cast<float>(kSwarmMemberCount - kLeftBranchCount - 1);
            offset.x = branchRate * 9.0f;
            offset.y = -6.0f + branchRate * 14.0f;
        }
    
        offset.y +=
            std::sin(context.time * 3.2f + static_cast<float>(context.slotIndex) * 0.35f) *
            0.45f;
        offset.z =
            std::cos(context.time * 2.0f + static_cast<float>(context.slotIndex) * 0.22f) *
            1.3f;
        return offset;
    }
};

class DiamondSwarmFormation final : public BaseSwarmFormation {
public:
    Vector3 CalculateOffset(const SwarmFormationContext& context) const override
    {
        int32_t row = 0;
        int32_t indexInRow = context.slotIndex;
        int32_t rowCount = 2;
    
        if (context.slotIndex >= 2 && context.slotIndex < 6) {
            row = 1;
            indexInRow = context.slotIndex - 2;
            rowCount = 4;
        }
        if (context.slotIndex >= 6 && context.slotIndex < 12) {
            row = 2;
            indexInRow = context.slotIndex - 6;
            rowCount = 6;
        }
        if (context.slotIndex >= 12 && context.slotIndex < 16) {
            row = 3;
            indexInRow = context.slotIndex - 12;
            rowCount = 4;
        }
        if (context.slotIndex >= 16) {
            row = 4;
            indexInRow = context.slotIndex - 16;
            rowCount = 2;
        }
    
        Vector3 offset {};
        offset.x =
            (static_cast<float>(indexInRow) -
             static_cast<float>(rowCount - 1) * 0.5f) *
            2.6f;
        offset.y = 8.0f - static_cast<float>(row) * 4.0f;
        offset.z =
            std::sin(context.time * 2.4f + static_cast<float>(row) * 0.6f) *
            0.8f;
        return offset;
    }
};

class WaveSwarmFormation final : public BaseSwarmFormation {
public:
    Vector3 CalculateOffset(const SwarmFormationContext& context) const override
    {
        float slotCenter =
            static_cast<float>(context.slotIndex) -
            static_cast<float>(kSwarmMemberCount - 1) * 0.5f;
        float wavePhase =
            static_cast<float>(context.slotIndex) * 0.72f +
            context.time * 2.8f;
    
        Vector3 offset {};
        offset.x = slotCenter * 1.45f;
        offset.y = std::sin(wavePhase) * 6.5f;
        offset.z = std::cos(wavePhase) * 2.2f;
        return offset;
    }
};

class ArrowSwarmFormation final : public BaseSwarmFormation {
public:
    Vector3 CalculateOffset(const SwarmFormationContext& context) const override
    {
        constexpr int32_t kWingCount = 9;
        int32_t wingIndex = context.slotIndex;
        float verticalDirection = 1.0f;
    
        if (context.slotIndex >= kWingCount) {
            wingIndex = context.slotIndex - kWingCount;
            verticalDirection = -1.0f;
        }
    
        float wingRate =
            static_cast<float>(wingIndex) /
            static_cast<float>(kWingCount - 1);
    
        Vector3 offset {};
        offset.x =
            static_cast<float>(context.travelDirection) *
            (-10.0f + wingRate * 13.0f);
        offset.y = verticalDirection * (8.0f - wingRate * 8.0f);
        offset.y +=
            std::sin(context.time * 3.0f + static_cast<float>(context.slotIndex) * 0.3f) *
            0.35f;
        offset.z =
            std::cos(context.time * 2.2f + static_cast<float>(context.slotIndex) * 0.2f) *
            1.1f;
        return offset;
    }
};

const SwarmFormationDefinition& GetSwarmFormation(SwarmFormationType type)
{
    static const SpiralSwarmFormation spiral;
    static const WallSwarmFormation wall;
    static const GlyphSwarmFormation glyph;
    static const DiamondSwarmFormation diamond;
    static const WaveSwarmFormation wave;
    static const ArrowSwarmFormation arrow;
    static const std::array<SwarmFormationDefinition, 6> formations {{
        { &spiral, { 0.20f, 0.95f, 1.0f, 1.0f } },
        { &wall, { 0.20f, 0.95f, 1.0f, 1.0f } },
        { &glyph, { 1.0f, 0.25f, 0.82f, 1.0f } },
        { &diamond, { 1.0f, 0.82f, 0.18f, 1.0f } },
        { &wave, { 0.35f, 1.0f, 0.30f, 1.0f } },
        { &arrow, { 1.0f, 0.25f, 0.82f, 1.0f } },
    }};
    const auto index = static_cast<size_t>(type);
    return formations[index < formations.size() ? index : 0];
}