#pragma once

#include <cmath>

struct ElectricGateCycleSettings {
    double offDurationSeconds = 3.0;
    double warningDurationSeconds = 0.75;
    double onDurationSeconds = 2.0;
    double initialPhaseSeconds = 0.0;

    bool IsValid() const
    {
        const double cycleDurationSeconds = offDurationSeconds + onDurationSeconds;
        return std::isfinite(offDurationSeconds) && offDurationSeconds > 0.0 &&
            std::isfinite(onDurationSeconds) && onDurationSeconds > 0.0 &&
            std::isfinite(warningDurationSeconds) && warningDurationSeconds >= 0.0 &&
            warningDurationSeconds <= offDurationSeconds &&
            std::isfinite(cycleDurationSeconds) &&
            std::isfinite(initialPhaseSeconds) && initialPhaseSeconds >= 0.0 &&
            initialPhaseSeconds < cycleDurationSeconds;
    }
};

enum class ElectricGateState {
    Off,
    Warning,
    On
};

// 描画に依存しない周期時計。接触した時間区間と放電時間の重なりも求める。
class ElectricGateCycle {
public:
    bool Initialize(const ElectricGateCycleSettings& settings)
    {
        if (!settings.IsValid()) { return false; }
        settings_ = settings;
        phaseSeconds_ = settings.initialPhaseSeconds;
        previousPhaseSeconds_ = phaseSeconds_;
        frameDurationSeconds_ = 0.0;
        return true;
    }

    void Update(double deltaTimeSeconds)
    {
        previousPhaseSeconds_ = phaseSeconds_;
        frameDurationSeconds_ = 0.0;
        if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0) { return; }
        frameDurationSeconds_ = deltaTimeSeconds;
        const double cycleDurationSeconds = GetCycleDurationSeconds();
        phaseSeconds_ = AddPhaseSeconds(phaseSeconds_, std::fmod(deltaTimeSeconds, cycleDurationSeconds));
    }

    ElectricGateState GetState() const
    {
        if (phaseSeconds_ >= settings_.offDurationSeconds) { return ElectricGateState::On; }
        if (settings_.warningDurationSeconds > 0.0 &&
            phaseSeconds_ >= settings_.offDurationSeconds - settings_.warningDurationSeconds) {
            return ElectricGateState::Warning;
        }
        return ElectricGateState::Off;
    }

    float GetWarningProgress() const
    {
        if (GetState() != ElectricGateState::Warning) { return 0.0f; }
        return static_cast<float>((phaseSeconds_ - settings_.offDurationSeconds +
            settings_.warningDurationSeconds) / settings_.warningDurationSeconds);
    }

    bool HasDischargeDuringFrameInterval(double startRatio, double endRatio) const
    {
        if (!std::isfinite(startRatio) || !std::isfinite(endRatio) ||
            startRatio < 0.0 || endRatio > 1.0 || startRatio > endRatio) { return false; }
        const double cycleDurationSeconds = GetCycleDurationSeconds();
        const double startPhaseSeconds = AddPhaseSeconds(previousPhaseSeconds_,
            std::fmod(frameDurationSeconds_ * startRatio, cycleDurationSeconds));
        if (startPhaseSeconds >= settings_.offDurationSeconds) { return true; }
        const double intervalDurationSeconds = frameDurationSeconds_ * (endRatio - startRatio);
        return intervalDurationSeconds >= settings_.offDurationSeconds - startPhaseSeconds;
    }

private:
    double AddPhaseSeconds(double phaseSeconds, double offsetSeconds) const
    {
        const double remainingSeconds = GetCycleDurationSeconds() - phaseSeconds;
        if (offsetSeconds >= remainingSeconds) { return offsetSeconds - remainingSeconds; }
        return phaseSeconds + offsetSeconds;
    }

    double GetCycleDurationSeconds() const
    {
        return settings_.offDurationSeconds + settings_.onDurationSeconds;
    }

    ElectricGateCycleSettings settings_;
    double phaseSeconds_ = 0.0;
    double previousPhaseSeconds_ = 0.0;
    double frameDurationSeconds_ = 0.0;
};
