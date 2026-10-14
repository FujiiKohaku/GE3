#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "BaseTutorialStep.h"

class Input;
class Player;

// 通信パネルとは独立した訓練の進行・達成判定。
class TutorialController {
public:
    void Initialize(const Player& player);
    void Update(const Input& input, const Player& player, float deltaTime);
    void NotifyManualPauseOpened();
    int GetTargetCount() const;
    bool HasMovingTargets() const;
    void SetDefeatedTargets(int count) { defeatedTargets_ = count; }
    bool IsComplete() const { return stepIndex_ >= kStepCount; }
    bool IsCompact() const { return false; }
    int GetCompletedCount() const;
    static constexpr int kStepCount = kTutorialStepCount;
    float LimitRailDistance(float distance) const;
    std::string GetTitle() const;
    std::string GetInstructionText(const Input& input) const;
    std::string GetOperation(const Input& input, const Player& player) const;
    std::vector<std::string> GetButtonHints(const Input& input) const;

private:
    TutorialStepInput ReadStepInput(const Input& input, const Player& player) const;
    BaseTutorialStep* GetCurrentStep() const { return IsComplete() ? nullptr : steps_[stepIndex_].get(); }
    std::string currentWeapon_;
    int defeatedTargets_ = 0;
    bool achieved_ = false;
    void EnterStep(const Player& player);
    std::array<std::unique_ptr<BaseTutorialStep>, kStepCount> steps_ = CreateTutorialSteps();
    int stepIndex_ = 0;
    float stepTime_ = 0.0f;
    float successTime_ = 0.0f;
};
