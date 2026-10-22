#include "Engine/3D/OceanInteraction.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        bool isImpactArmed = false;
        if (!OceanInteraction::ShouldEmitSphereImpact(isImpactArmed, -5.0f, 0.1f, 2.0f)) {
            throw std::runtime_error("A submerged-to-air crossing must emit one impact");
        }
        for (int crossingIndex = 0; crossingIndex < 20; ++crossingIndex) {
            if (OceanInteraction::ShouldEmitSphereImpact(isImpactArmed, 0.1f, -0.1f, 2.0f)
                || OceanInteraction::ShouldEmitSphereImpact(isImpactArmed, -0.1f, 0.1f, 2.0f)) {
                throw std::runtime_error("Surface jitter must not rearm impact rings");
            }
        }
        if (OceanInteraction::ShouldEmitSphereImpact(isImpactArmed, 0.1f, 5.0f, 2.0f)
            || !OceanInteraction::ShouldEmitSphereImpact(isImpactArmed, 5.0f, -5.0f, 2.0f)) {
            throw std::runtime_error("An impact must rearm after clearing the surface");
        }
        float lowRateStrength = 0.0f;
        float highRateStrength = 0.0f;
        for (int frameIndex = 0; frameIndex < 30; ++frameIndex) {
            lowRateStrength = OceanInteraction::FollowWakeValue(lowRateStrength, 1.0f, 1.0f / 30.0f);
        }
        for (int frameIndex = 0; frameIndex < 120; ++frameIndex) {
            highRateStrength = OceanInteraction::FollowWakeValue(highRateStrength, 1.0f, 1.0f / 120.0f);
        }
        if (std::abs(lowRateStrength - highRateStrength) > 0.00001f
            || OceanInteraction::FollowWakeValue(0.5f, 1.0f, 0.0f) != 0.5f
            || OceanInteraction::FollowWakeValue(0.0f, 1.0f, 1.0f / 60.0f) >= 0.2f) {
            throw std::runtime_error("Wake smoothing must bound spikes and be frame-rate independent");
        }
        const auto delayedBirth = OceanInteraction::EvaluateOceanWakeProfile(6.0f, 0.0f, 0.5f, 1.0f, 2.0f, 0.5f, 0.0f, 1.0f);
        const auto delayedVisible = OceanInteraction::EvaluateOceanWakeProfile(6.0f, 0.0f, 0.6f, 1.0f, 2.0f, 0.5f, 0.0f, 1.0f);
        const auto samePhysicalAge = OceanInteraction::OceanRadialWakeProfile(6.0f, 0.0f, 0.6f, 1.0f, 2.0f);
        if (delayedBirth.y != 0.0f || std::abs(delayedVisible.y - samePhysicalAge.y) > 0.00001f) {
            throw std::runtime_error("Queued rings must reveal smoothly without restarting their propagation");
        }
        using OceanInteraction::FindSphereContactFraction;
        if (FindSphereContactFraction(8.0f, 7.0f, 2.0f) >= 0.0f
            || FindSphereContactFraction(-8.0f, -7.0f, 2.0f) >= 0.0f
            || FindSphereContactFraction(0.0f, 0.0f, 0.0f) >= 0.0f
            || FindSphereContactFraction(3.0f, 2.0f, 2.0f) != 1.0f
            || FindSphereContactFraction(-3.0f, -2.0f, 2.0f) != 1.0f
            || std::abs(FindSphereContactFraction(-6.0f, 4.0f, 2.0f) - 0.6f) > 0.00001f
            || std::abs(FindSphereContactFraction(4.0f, -6.0f, 2.0f) - 0.4f) > 0.00001f) {
            throw std::runtime_error("Sphere contact must detect entry and swept crossings, excluding deep water and air");
        }
        for (float ageSeconds : {0.05f, 0.4f, 1.5f, 2.49f}) {
            for (float scale : {0.8f, 2.0f, 3.5f}) {
                for (float positionX : {0.0f, 0.5f, 3.0f, 8.0f, 20.0f}) {
                    for (float positionZ : {0.0f, -2.0f, 4.0f, 15.0f}) {
                        const auto wave = OceanInteraction::OceanRadialWakeProfile(positionX, positionZ, ageSeconds, 1.2f, scale);
                        const float kDerivativeStep = 0.002f;
                        const float derivativeX = (OceanInteraction::OceanRadialWakeProfile(positionX + kDerivativeStep, positionZ, ageSeconds, 1.2f, scale).y
                            - OceanInteraction::OceanRadialWakeProfile(positionX - kDerivativeStep, positionZ, ageSeconds, 1.2f, scale).y) / (2.0f * kDerivativeStep);
                        const float derivativeZ = (OceanInteraction::OceanRadialWakeProfile(positionX, positionZ + kDerivativeStep, ageSeconds, 1.2f, scale).y
                            - OceanInteraction::OceanRadialWakeProfile(positionX, positionZ - kDerivativeStep, ageSeconds, 1.2f, scale).y) / (2.0f * kDerivativeStep);
                        const auto selected = OceanInteraction::EvaluateOceanWakeProfile(positionX, positionZ, ageSeconds, 1.2f, scale, 0.0f, 1.0f, 1.0f);
                        if (!std::isfinite(wave.y) || std::abs(wave.x - derivativeX) > 0.002f
                            || std::abs(wave.z - derivativeZ) > 0.002f || selected.y != wave.y) {
                            throw std::runtime_error("Radial wake gradients and CPU/VS profile selection must agree");
                        }
                    }
                }
            }
        }
        for (float ageSeconds : {-0.1f, 0.0f, 2.5f, 3.0f}) {
            if (OceanInteraction::OceanRadialWakeProfile(3.0f, 0.0f, ageSeconds, 1.0f, 2.0f).y != 0.0f) {
                throw std::runtime_error("Radial wakes must start at zero and expire");
            }
        }
        // Check the shared analytic tangents, including envelope and phase warp.
        for (float timeSeconds : {0.0f, 17.0f, 300.0f}) {
            for (float positionX : {-140.0f, 0.0f, 35.0f, 900.0f}) {
                for (float positionZ : {-80.0f, 4.0f, 7200.0f}) {
                    const float kDerivativeStep = 0.02f;
#define OCEAN_WAVE(dx, dz, f, a, speed, steepness, phase) \
                    { \
                        const auto wave = OceanInteraction::EvaluateOceanWave(positionX, positionZ, timeSeconds, 1.55f, 0.075f, dx, dz, f, a, speed, steepness, phase); \
                        const auto beforeX = OceanInteraction::EvaluateOceanWave(positionX - kDerivativeStep, positionZ, timeSeconds, 1.55f, 0.075f, dx, dz, f, a, speed, steepness, phase); \
                        const auto afterX = OceanInteraction::EvaluateOceanWave(positionX + kDerivativeStep, positionZ, timeSeconds, 1.55f, 0.075f, dx, dz, f, a, speed, steepness, phase); \
                        const auto beforeZ = OceanInteraction::EvaluateOceanWave(positionX, positionZ - kDerivativeStep, timeSeconds, 1.55f, 0.075f, dx, dz, f, a, speed, steepness, phase); \
                        const auto afterZ = OceanInteraction::EvaluateOceanWave(positionX, positionZ + kDerivativeStep, timeSeconds, 1.55f, 0.075f, dx, dz, f, a, speed, steepness, phase); \
                        const Vector3 numericalX{(afterX.displacement.x - beforeX.displacement.x) / (2 * kDerivativeStep), (afterX.displacement.y - beforeX.displacement.y) / (2 * kDerivativeStep), (afterX.displacement.z - beforeX.displacement.z) / (2 * kDerivativeStep)}; \
                        const Vector3 numericalZ{(afterZ.displacement.x - beforeZ.displacement.x) / (2 * kDerivativeStep), (afterZ.displacement.y - beforeZ.displacement.y) / (2 * kDerivativeStep), (afterZ.displacement.z - beforeZ.displacement.z) / (2 * kDerivativeStep)}; \
                        if (std::abs(numericalX.x - wave.derivativeX.x) > 0.003f || std::abs(numericalX.y - wave.derivativeX.y) > 0.003f || std::abs(numericalX.z - wave.derivativeX.z) > 0.003f \
                            || std::abs(numericalZ.x - wave.derivativeZ.x) > 0.003f || std::abs(numericalZ.y - wave.derivativeZ.y) > 0.003f || std::abs(numericalZ.z - wave.derivativeZ.z) > 0.003f) \
                            throw std::runtime_error("Ocean wave tangents must match displacement derivatives"); \
                    }
#include "../resources/Shaders/Ocean/OceanWaveLayers.hlsli"
#undef OCEAN_WAVE
                }
            }
        }
        float worstError = 0;
        int samples = 0;
        for (float amplitude : {0.0f, 0.65f, 1.55f}) {
            for (float time : {0.0f, 0.4f, 17.0f, 80.0f, 300.0f}) {
                for (int x = -50; x <= 50; x += 5) for (int z = 0; z <= 7200; z += 73) {
                    const auto d = OceanInteraction::Displacement(float(x), float(z), time, amplitude, 0.075f);
                    const float recovered = OceanInteraction::Height(x + d.x, z + d.z, time, -30.0f, amplitude, 0.075f);
                    const float error = std::abs(recovered - (-30.0f + d.y));
                    worstError = (std::max)(worstError, error);
                    if (!std::isfinite(recovered) || error > 0.002f) throw std::runtime_error("Wave inverse height mismatch");
                    ++samples;
                }
            }
        }
        if (OceanInteraction::Strength(7, 30) != 0 || OceanInteraction::Strength(0, 0) != 0)
            throw std::runtime_error("High or stationary player must not disturb water");
        if (OceanInteraction::Strength(0.5f, 30) != 1 || OceanInteraction::Strength(2, 15) >= OceanInteraction::Strength(2, 30))
            throw std::runtime_error("Close/fast player must produce stronger interaction");
        float previous = 1;
        for (int i = 0; i <= 120; ++i) {
            const float strength = OceanInteraction::Strength(i * 0.05f, 30);
            if (strength < 0 || strength > previous + 0.00001f) throw std::runtime_error("Height response must fade monotonically");
            previous = strength;
        }
        const Vector3 heading{0, 0, 1};
        const auto firstTurn = OceanInteraction::FollowWakeDirection(heading, {30, 0, 30}, 1.0f / 60);
        if (firstTurn.x <= 0 || firstTurn.x >= 0.1f || firstTurn.z < 0.99f)
            throw std::runtime_error("Lateral motion must turn the wake gradually");
        const auto stopped = OceanInteraction::FollowWakeDirection(firstTurn, {0.01f, 0, 0.01f}, 1.0f / 60);
        if (stopped.x != firstTurn.x || stopped.z != firstTurn.z)
            throw std::runtime_error("Near-stationary movement must not jitter the heading");
        const auto paused = OceanInteraction::FollowWakeDirection(heading, {30, 0, 0}, 0);
        if (paused.x != 0 || paused.z != 1) throw std::runtime_error("Paused heading must hold");
        const auto opposite = OceanInteraction::FollowWakeDirection(heading, {0, 0, -30}, 1.0f / 60);
        if (!std::isfinite(opposite.x) || opposite.z < 0.99f)
            throw std::runtime_error("Reversal must not snap or produce an invalid heading");
        Vector3 lowRate = heading, highRate = heading;
        for (int i = 0; i < 30; ++i) lowRate = OceanInteraction::FollowWakeDirection(lowRate, {30, 0, 10}, 1.0f / 30);
        for (int i = 0; i < 120; ++i) highRate = OceanInteraction::FollowWakeDirection(highRate, {30, 0, 10}, 1.0f / 120);
        if (std::abs(lowRate.x - highRate.x) > 0.01f || std::abs(lowRate.z - highRate.z) > 0.01f
            || std::abs(highRate.x * highRate.x + highRate.z * highRate.z - 1) > 0.00001f)
            throw std::runtime_error("Heading follow must remain normalized and consistent across frame rates");
        const Vector3 origin{10, -27, 20};
        const auto source = OceanInteraction::SourcePosition(origin, {0, 0, 1}, 30);
        if (source.x != 10 || source.z != 29 || source.y != origin.y)
            throw std::runtime_error("Wake source must lead forward by 9 units at normal speed");
        const auto reverse = OceanInteraction::SourcePosition(origin, {0, 0, -1}, 1000);
        const auto vertical = OceanInteraction::SourcePosition(origin, {0, 1, 0}, 0);
        if (reverse.z != 4 || vertical.z != 29) throw std::runtime_error("Lead clamp/direction fallback");
        using OceanInteraction::OceanWakeProfile;
        const auto wake = [](float x, float z, float age, float strength = 1.0f, float scale = 1.0f) {
            return OceanWakeProfile(x, z, age, strength, scale, 0, 1);
        };
        if (wake(0, 0, -0.1f).y != 0 || wake(0, 0, 0).y != 0
            || wake(0, 0, 2.5f).y != 0 || wake(100, 0, 0.5f).y != 0 || wake(0, 2, 0.2f).y != 0)
            throw std::runtime_error("Wake birth, expiry, forward cutoff and spatial bounds");
        for (float age : {0.1f, 0.4f, 1.0f, 2.0f}) {
            const float behind = 1.0f + age * 1.6f;
            const float side = 1.8f + age * 3.2f + behind * 0.42f;
            const auto crest = wake(side, -behind, age);
            const auto middle = wake(0, -behind, age);
            if (crest.y <= 0 || middle.y > crest.y * 0.05f || wake(side + 1.5f, -behind, age).y >= 0)
                throw std::runtime_error("Side crests require troughs and a flat center");
            const auto mirrored = wake(-side, -behind, age);
            const auto rotated = OceanWakeProfile(-behind, -side, age, 1, 1, 1, 0);
            if (std::abs(mirrored.y - crest.y) > 0.0001f || std::abs(rotated.y - crest.y) > 0.0001f)
                throw std::runtime_error("Wake must follow heading and remain symmetric");
            float integral = 0;
            for (int i = -4000; i <= 4000; ++i) integral += wake(i * 0.01f, -behind, age).y * 0.01f;
            if (std::abs(integral) > 0.001f) throw std::runtime_error("Wake must not raise the whole cross-section");
            for (float scale : {1.0f, 1.45f}) for (float x : {-10.0f, -2.0f, 0.0f, 1.0f, 4.0f, 10.0f}) {
                const auto wave = wake(x, -0.7f, age, 0.8f, scale);
                const float derivativeX = (wake(x + 0.001f, -0.7f, age, 0.8f, scale).y
                    - wake(x - 0.001f, -0.7f, age, 0.8f, scale).y) / 0.002f;
                const float derivativeZ = (wake(x, -0.699f, age, 0.8f, scale).y
                    - wake(x, -0.701f, age, 0.8f, scale).y) / 0.002f;
                if (!std::isfinite(wave.y) || std::abs(wave.x - derivativeX) > 0.002f || std::abs(wave.z - derivativeZ) > 0.002f)
                    throw std::runtime_error("Directional wake normals must match height derivatives");
            }
        }
        const auto saturated = OceanInteraction::LimitOceanWake({10, 100, 10});
        if (saturated.y >= 1.90f || !std::isfinite(saturated.x)) throw std::runtime_error("Overlapping wakes must stay bounded");
        // The focused mesh retains the original bounds and never folds, including near ocean edges.
        for (float focus : {-1000.0f, 0.0f, 3600.0f, 8000.0f}) {
            float last = -1;
            for (int i = 0; i <= 512; ++i) {
                const float coordinate = OceanInteraction::OceanFocusedCoordinate(i / 512.0f, 0, 7200, focus);
                if (coordinate < last || coordinate < 0 || coordinate > 7200) throw std::runtime_error("Focused ocean mesh folds");
                last = coordinate;
            }
            if (last != 7200) throw std::runtime_error("Focused ocean must retain far boundary");
        }
        std::cout << "PASS: " << samples << " displaced wave queries; max Y error=" << worstError
                  << "; proximity, smooth movement heading, forward source, directional crests/troughs, balanced cross-section, height/normal, bounded overlap and focused mesh\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
