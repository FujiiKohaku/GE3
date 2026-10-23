#include "IceJellyfishPoseController.h"
#include "../IceJellyfish.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace {
constexpr float kBodyAlpha = 0.95f;
// 調整用の設定値。意味が異なる値は、数値が同じでも個別に保持する。
constexpr float kMinimumWaveSpeed = 0.95f;
constexpr float kMaximumWaveSpeed = 1.35f;
constexpr float kMinimumWaveAmplitude = 0.82f;
constexpr float kMaximumWaveAmplitude = 1.25f;
constexpr float kMaximumPoseDeltaSeconds = 0.1f;
constexpr float kFloatFrequency = 1.10f;
constexpr float kBellLiftAmplitude = 0.8f;
constexpr float kRadialContraction = 0.10f;
constexpr float kBellHeightContraction = 0.20f;
constexpr float kShadowPaddingBase = 1.2f;
constexpr float kShadowPaddingRadiusFactor = 0.020f;
constexpr float kTentacleShadowPaddingBase = 1.2f;
constexpr float kTentacleShadowPaddingRadiusFactor = 0.020f;
constexpr float kTentacleAzimuthOffset = 0.25f;
constexpr float kWaveSpeedVariationWeight = 0.15f;
constexpr float kWavePhaseVariationWeight = 0.35f;
constexpr float kTentacleRootRadius = 10.5f;
constexpr float kTentacleRootHeight = -1.2f;
constexpr float kContractionFrequency = 0.72f;
constexpr float kRootBend = 0.30f;
constexpr float kRootBendAmplitude = 0.44f;
constexpr float kRootContractionBend = 0.42f;
constexpr float kWaveDelayPerSegment = 0.68f;
constexpr float kWaveDelayAcceleration = 0.045f;
constexpr float kBendWaveFrequency = 1.15f;
constexpr float kTipCurlFrequency = 1.30f;
constexpr float kTipCurlDelay = 0.90f;
constexpr float kTipCurlAmplitude = 0.36f;
constexpr float kBendPerSegment = 0.045f;
constexpr float kBendAzimuthPhaseWeight = 0.35f;
constexpr float kBendWaveAmplitude = 0.095f;
constexpr float kBendWaveAmplitudePerSegment = 0.017f;
constexpr float kContractionBend = 0.025f;
constexpr float kContractionBendPerSegment = 0.010f;
constexpr float kSwayFrequency = 0.95f;
constexpr float kSwayDelayPerSegment = 0.58f;
constexpr float kSwayAmplitude = 0.025f;
constexpr float kSwayAmplitudePerSegment = 0.011f;
constexpr float kSwayTipAmplitude = 0.14f;
constexpr float kRootFollowRate = 9.0f;
constexpr float kFollowRateTipReduction = 6.0f;
constexpr float kSegmentBaseLength = 5.4f;
constexpr float kSegmentLengthReduction = 0.32f;
constexpr float kSegmentLengthScale = 1.5f;
constexpr float kStretchAmplitude = 0.025f;
constexpr float kStretchTipAmplitude = 0.035f;
constexpr float kSegmentBaseWidth = 6.6f;
constexpr float kSegmentWidthReduction = 0.88f;
constexpr float kTipEndRatio = 0.025f;
constexpr float kJointSpacingRatio = 0.94f;
constexpr float kPillarModelHeightOffset = 0.35f;
constexpr float kPillarBaseHeightVariation = 0.88f;
constexpr float kPillarHeightVariationStep = 0.08f;
constexpr float kPillarModelWidth = 4.0f;
constexpr float kPillarModelHeight = 11.0f;
constexpr float kPillarRotationCenterIndex = 2.5f;
constexpr float kPillarRotationStep = 0.08f;
constexpr Vector4 kBellColor = { 0.65f, 0.86f, 1.0f, kBodyAlpha };
constexpr Vector4 kCoreHitColor = { 4.0f, 3.4f, 2.8f, 1.0f };
constexpr Vector4 kExposedCoreColor = { 3.6f, 1.8f, 0.35f, 1.0f };
constexpr Vector4 kCoreColor = { 2.8f, 1.25f, 0.25f, 1.0f };
constexpr Vector4 kTentacleColor = { 0.55f, 0.80f, 1.0f, kBodyAlpha };
constexpr Vector3 kCoreOffset = { 0.0f, -2.0f, 0.0f };

constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
constexpr float kBellWaveAmplitude = 1.5f;
constexpr float kTentacleWaveAmplitude = 0.12f;
}

Vector4 IceJellyfishPoseController::GetBellAnimationParameters(float time)
{
    return { time, kBellWaveAmplitude, 0.0f, 0.0f };
}

void IceJellyfishPoseController::Reset()
{
    softPoseTime_ = 0.0f;
    softPoseInitialized_ = false;
    softDirections_ = {};
    std::mt19937 randomEngine(std::random_device {}());
    std::uniform_real_distribution<float> phaseDistribution(0.0f, kTwoPi);
    std::uniform_real_distribution<float> speedDistribution(kMinimumWaveSpeed, kMaximumWaveSpeed);
    std::uniform_real_distribution<float> amplitudeDistribution(kMinimumWaveAmplitude, kMaximumWaveAmplitude);
    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        tentacleWavePhase_[tentacle] = phaseDistribution(randomEngine);
        tentacleWaveSpeed_[tentacle] = speedDistribution(randomEngine);
        tentacleWaveAmplitude_[tentacle] = amplitudeDistribution(randomEngine);
    }
}

// 攻撃の姿勢と揺れを反映して、頭や触手の位置・回転・大きさを更新する。
void IceJellyfishPoseController::Update(IceJellyfish& boss)
{
    if (boss.bell_ == nullptr) return;
    const float poseDelta = std::clamp(boss.animationTime_ - softPoseTime_, 0.0f, kMaximumPoseDeltaSeconds);
    softPoseTime_ = boss.animationTime_;

    const float floatPhase = boss.animationTime_ * kFloatFrequency;
    const float contractionAmount = 0.5f - 0.5f * std::sin(floatPhase);
    const Vector3 rotation = {};
    const Matrix4x4 bodyMatrix = MatrixMath::MakeAffineMatrix(
        { 1.0f, 1.0f, 1.0f }, rotation, boss.bodyPosition_);
    const float openLift = std::sin(floatPhase) * kBellLiftAmplitude;
    const Matrix4x4 bellLocal = MatrixMath::MakeAffineMatrix(
        Vector3 {
            1.0f - contractionAmount * kRadialContraction,
            1.0f - contractionAmount * kBellHeightContraction,
            1.0f - contractionAmount * kRadialContraction },
        Vector3 {}, Vector3 { 0.0f, openLift, 0.0f });
    boss.bell_->SetCustomWorldMatrix(MatrixMath::Multiply(bellLocal, bodyMatrix));
    boss.bell_->SetColor(kBellColor);
    boss.bell_->SetVertexShaderParameters(GetBellAnimationParameters(boss.animationTime_));
    boss.bell_->SetShadowBoundsPadding(kBellWaveAmplitude *
        (kShadowPaddingBase + boss.bell_->GetModelBoundsRadius() * kShadowPaddingRadiusFactor));
    boss.bell_->Update();

    const float coreScale = 1.0f - contractionAmount * kRadialContraction;
    const Matrix4x4 coreLocal = MatrixMath::MakeAffineMatrix(
        Vector3 { coreScale, coreScale, coreScale },
        Vector3 {}, kCoreOffset);
    boss.core_->SetCustomWorldMatrix(MatrixMath::Multiply(coreLocal, bodyMatrix));
    Vector4 coreColor = kCoreColor;
    if (boss.hitFlashTime_ > 0.0f) {
        coreColor = kCoreHitColor;
    } else if (boss.coreExposed_) {
        coreColor = kExposedCoreColor;
    }
    boss.core_->SetColor(coreColor);
    boss.core_->Update();


    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        const float azimuth = kTwoPi * static_cast<float>(tentacle) /
            static_cast<float>(kTentacleCount) + kTentacleAzimuthOffset;
        const float tentaclePhase =
            floatPhase * (1.0f + (tentacleWaveSpeed_[tentacle] - 1.0f) * kWaveSpeedVariationWeight) + tentacleWavePhase_[tentacle] * kWavePhaseVariationWeight;
        const float tentacleAmplitude = tentacleWaveAmplitude_[tentacle];
        const float radialX = std::cos(azimuth);
        const float radialZ = std::sin(azimuth);
        const float rootRadius = kTentacleRootRadius * (1.0f - contractionAmount * kRadialContraction);
        Vector3 joint = { radialX * rootRadius, kTentacleRootHeight + openLift, radialZ * rootRadius };
        const float contraction = std::sin(tentaclePhase * kContractionFrequency);
        float bend = kRootBend + contraction * kRootBendAmplitude * tentacleAmplitude -
            contractionAmount * kRootContractionBend;

        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            const float index = static_cast<float>(segment);
            const float tipWeight = index / static_cast<float>(kSegmentsPerTentacle - 1);
            const float tipInfluence = tipWeight * tipWeight;
            const float waveDelay = index * kWaveDelayPerSegment + index * index * kWaveDelayAcceleration;
            const float delayedPhase = tentaclePhase * kBendWaveFrequency - waveDelay;
            const float tipCurl = std::sin(tentaclePhase * kTipCurlFrequency - index * kTipCurlDelay) *
                tipInfluence * kTipCurlAmplitude * tentacleAmplitude;
            bend += kBendPerSegment + std::sin(delayedPhase + azimuth * kBendAzimuthPhaseWeight) *
                (kBendWaveAmplitude + index * kBendWaveAmplitudePerSegment) * tentacleAmplitude + tipCurl -
                contractionAmount * (kContractionBend + index * kContractionBendPerSegment);
            const float sway = std::sin(tentaclePhase * kSwayFrequency - index * kSwayDelayPerSegment + azimuth) *
                (kSwayAmplitude + index * kSwayAmplitudePerSegment + tipInfluence * kSwayTipAmplitude) * tentacleAmplitude;
            Vector3 down = {
                radialX * std::sin(bend) - radialZ * sway,
                -std::cos(bend),
                radialZ * std::sin(bend) + radialX * sway
            };

            const IceJellyfishAttackMotionContext attackMotionContext {
                tentacle, segment, tipWeight, joint, radialX, radialZ
            };
            down = boss.battleState_.ApplyAttackTentacleDirection(boss, attackMotionContext, down);

            down = Normalize(down);
            if (!softPoseInitialized_) {
                softDirections_[tentacle][segment] = down;
            } else if (poseDelta > 0.0f) {
                const float follow = 1.0f - std::exp(-(kRootFollowRate - tipWeight * kFollowRateTipReduction) * poseDelta);
                softDirections_[tentacle][segment] = Normalize(
                    softDirections_[tentacle][segment] * (1.0f - follow) + down * follow);
            }
            down = softDirections_[tentacle][segment];
            const Vector3 up = down * -1.0f;
            const Vector3 tangent = { -radialZ, 0.0f, radialX };
            Vector3 right = Normalize(Cross(up, tangent));
            const Vector3 forward = Cross(right, up);
            float length = (kSegmentBaseLength - index * kSegmentLengthReduction) * kSegmentLengthScale;
            const float stretch = 1.0f + std::sin(tentaclePhase - waveDelay) * (kStretchAmplitude + tipWeight * kStretchTipAmplitude);
            length *= stretch;
            const float width = (kSegmentBaseWidth - index * kSegmentWidthReduction) / std::sqrt(stretch);
            float endRatio = (kSegmentBaseWidth - (index + 1.0f) * kSegmentWidthReduction) / (kSegmentBaseWidth - index * kSegmentWidthReduction);
            if (segment + 1 == kSegmentsPerTentacle) { endRatio = kTipEndRatio; }
            Matrix4x4 local = MatrixMath::MakeIdentity4x4();
            local.m[0][0] = right.x * width;
            local.m[0][1] = right.y * width;
            local.m[0][2] = right.z * width;
            local.m[1][0] = up.x * length;
            local.m[1][1] = up.y * length;
            local.m[1][2] = up.z * length;
            local.m[2][0] = forward.x * width;
            local.m[2][1] = forward.y * width;
            local.m[2][2] = forward.z * width;
            local.m[3][0] = joint.x;
            local.m[3][1] = joint.y;
            local.m[3][2] = joint.z;
            Object3d& object = *boss.tentacles_[tentacle][segment];
            object.SetCustomWorldMatrix(MatrixMath::Multiply(local, bodyMatrix));
            const Vector4 baseTentacleColor = kTentacleColor;
            const Vector4 tentacleColor = boss.battleState_.ApplyAttackTentacleColor(
                boss, attackMotionContext, baseTentacleColor);
            object.SetColor(tentacleColor);
            object.SetVertexShaderParameters({ boss.animationTime_, kTentacleWaveAmplitude,
                tentacleWavePhase_[tentacle] - waveDelay, 1.0f + endRatio });
            object.SetShadowBoundsPadding(kTentacleWaveAmplitude *
                (kTentacleShadowPaddingBase + object.GetModelBoundsRadius() * kTentacleShadowPaddingRadiusFactor));
            object.Update();
            joint = joint + down * (length * kJointSpacingRatio);
        }
    }
    softPoseInitialized_ = true;
    UpdateIcePillarModelTransforms(boss);
}

// 氷柱の出現状態を、表示位置と大きさへ反映する。
void IceJellyfishPoseController::UpdateIcePillarModelTransforms(IceJellyfish& boss)
{
    if (!boss.battleState_.HasIcePillarModels()) return;

    const float scaleWeight = boss.battleState_.GetIcePillarModelScaleWeight();
    for (size_t index = 0; index < boss.battleState_.GetIcePillarModelPositions().size(); ++index) {
        Vector3 position = boss.battleState_.GetIcePillarModelPositions()[index];
        position.y += kPillarModelHeightOffset;
        const float heightVariation = kPillarBaseHeightVariation + static_cast<float>(index % 3) * kPillarHeightVariationStep;
        const Matrix4x4 matrix = MatrixMath::MakeAffineMatrix(
            Vector3 { kPillarModelWidth * scaleWeight, kPillarModelHeight * heightVariation * scaleWeight,
                kPillarModelWidth * scaleWeight },
            Vector3 { 0.0f, (static_cast<float>(index) - kPillarRotationCenterIndex) * kPillarRotationStep, 0.0f },
            position);
        boss.icePillarModels_[index]->SetCustomWorldMatrix(matrix);
        boss.icePillarModels_[index]->Update();
    }
}

