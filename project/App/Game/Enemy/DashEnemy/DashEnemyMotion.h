#pragma once
#include "Engine/LevelEditor/LevelData.h"
#include "Engine/math/EngineStruct.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

enum class DashPhase { Waiting, Entry, Tracking, Locked, Dash, Recovery, Exit, Finished };

struct DashTarget {
    Vector3 position {};
    Vector3 velocity {}; // world units / second; automatic movement only
    Vector3 origin {};
    Vector3 forward { 0, 0, 1 };
    Vector3 right { 1, 0, 0 };
    Vector3 up { 0, 1, 0 };
};

struct DashMotionSegment {
    Vector3 start {}, end {};
    float begin = 0.0f, finish = 1.0f; // fractions of this frame
    bool attacking = false;
};

// CPU-only motion. Records each phase's movement so a frame crossing the launch
// boundary cannot turn the charge into a teleport or an oversized hit interval.
struct DashEnemyMotion {
    LevelData::DashEnemySpawnData settings;
    DashPhase phase = DashPhase::Waiting;
    Vector3 anchor {}, position {}, direction { 0, 0, -1 };
    Vector3 entryStart {}, exitDirection { 1, 0, 0 };
    float offsetX = 0.0f, offsetY = 0.0f, elapsed = 0.0f, frameSeconds = 0.0f;
    std::array<DashMotionSegment, 12> segments {};
    size_t segmentCount = 0;

    void Reset(const Vector3& spawn, const LevelData::DashEnemySpawnData& parameters)
    {
        const auto savedSettings = parameters;
        const Vector3 savedSpawn = spawn;
        *this = {};
        settings = savedSettings;
        anchor = position = savedSpawn;
    }

    bool IsVisible() const { return phase != DashPhase::Waiting && phase != DashPhase::Finished; }
    bool IsWarning() const { return phase == DashPhase::Tracking || phase == DashPhase::Locked; }

    static Vector3 Aim(const Vector3& from, const DashTarget& target, float delay, float speed)
    {
        const Vector3 offset = target.position + target.velocity * delay - from;
        const double a = Dot(target.velocity, target.velocity) - speed * speed;
        const double b = 2.0 * Dot(offset, target.velocity);
        const double c = Dot(offset, offset);
        double time = 0.0;
        if (std::abs(a) < 0.00001) {
            if (b < -0.00001) { time = -c / b; }
        } else {
            const double discriminant = b * b - 4 * a * c;
            if (discriminant >= 0) {
                const double root = std::sqrt(discriminant);
                const double first = (-b - root) / (2 * a), second = (-b + root) / (2 * a);
                double earliest = (std::numeric_limits<double>::max)();
                if (first > 0) { earliest = first; }
                if (second > 0 && second < earliest) { earliest = second; }
                if (earliest != (std::numeric_limits<double>::max)()) { time = earliest; }
            }
        }
        const Vector3 aim = offset + target.velocity * static_cast<float>(time);
        return Vector3Length(aim) > 0.001f ? Normalize(aim) : target.forward * -1.0f;
    }

    float Duration() const
    {
        switch (phase) {
        case DashPhase::Entry: return settings.entrySeconds;
        case DashPhase::Tracking: return settings.trackingSeconds;
        case DashPhase::Locked: return settings.lockSeconds;
        case DashPhase::Dash: return settings.dashDistance / settings.dashSpeed;
        case DashPhase::Recovery: return settings.recoverySeconds;
        case DashPhase::Exit: return settings.exitSeconds;
        default: return 0.0f;
        }
    }

    void Advance(float seconds, const DashTarget& target)
    {
        segmentCount = 0;
        frameSeconds = 0;
        if (!std::isfinite(seconds) || seconds <= 0 || phase == DashPhase::Finished) { return; }
        frameSeconds = seconds;
        if (phase == DashPhase::Waiting) {
            const Vector3 offset = anchor - target.position;
            const float forward = Dot(offset, target.forward);
            if (forward < -60) { phase = DashPhase::Finished; return; }
            if (forward <= 0 || forward > settings.activationDistance ||
                std::abs(Dot(offset, target.up)) > 40.0f || std::abs(Dot(offset, target.right)) > 70.0f) { return; }
            offsetX = std::clamp(Dot(anchor - target.origin, target.right), -40.0f, 40.0f);
            offsetY = std::clamp(Dot(anchor - target.origin, target.up), -24.0f, 24.0f);
            exitDirection = target.right * (offsetX < 0 ? -1.0f : 1.0f);
            entryStart = position = anchor + exitDirection * 35.0f;
            phase = DashPhase::Entry;
        }
        float used = 0.0f;
        while (used < seconds - 0.0000001f && phase != DashPhase::Finished && segmentCount < segments.size()) {
            const float step = (std::min)(seconds - used, (std::max)(0.0f, Duration() - elapsed));
            const Vector3 start = position;
            const bool attacking = phase == DashPhase::Dash;
            elapsed += step;
            DashTarget sample = target;
            sample.position += target.velocity * (used + step);
            sample.origin += target.velocity * (used + step);
            const Vector3 staging = sample.origin + sample.forward * settings.forwardOffset +
                sample.right * offsetX + sample.up * offsetY;
            if (phase == DashPhase::Entry) {
                const float t = std::clamp(elapsed / settings.entrySeconds, 0.0f, 1.0f);
                const float blend = t * t * (3.0f - 2.0f * t);
                position = entryStart + (staging - entryStart) * blend;
            } else if (phase == DashPhase::Tracking) {
                position = staging;
                direction = Aim(position, sample, settings.lockSeconds, settings.dashSpeed);
            } else if (attacking) {
                position += direction * (settings.dashSpeed * step);
            } else if (phase == DashPhase::Exit) {
                position += exitDirection * (45.0f * step);
            }
            if (step > 0) {
                segments[segmentCount++] = { start, position, used / seconds, (used + step) / seconds, attacking };
            }
            used += step;
            if (elapsed + 0.000001f >= Duration()) {
                if (phase == DashPhase::Entry || phase == DashPhase::Tracking) {
                    direction = Aim(position, sample, settings.lockSeconds, settings.dashSpeed);
                }
                phase = static_cast<DashPhase>(static_cast<int>(phase) + 1);
                elapsed = 0;
            }
        }
    }

    void StopAtObstacle(size_t index, float fraction)
    {
        auto& segment = segments[index];
        fraction = std::clamp(fraction, 0.0f, 1.0f);
        position = segment.start + (segment.end - segment.start) * fraction;
        segment.end = position;
        segment.finish = segment.begin + (segment.finish - segment.begin) * fraction;
        segmentCount = index + 1;
        if (segment.finish < 1 && segmentCount < segments.size()) {
            segments[segmentCount++] = { position, position, segment.finish, 1, false };
        }
        phase = DashPhase::Recovery;
        elapsed = (1.0f - segment.finish) * frameSeconds;
    }
};
