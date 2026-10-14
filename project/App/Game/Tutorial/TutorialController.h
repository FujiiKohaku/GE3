#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

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
    bool IsComplete() const { return step_ == Step::Complete; }
    bool IsCompact() const { return false; }
    int GetCompletedCount() const;
    static constexpr int kStepCount = 14;
    float LimitRailDistance(float distance) const;
    std::string GetTitle() const;
    std::string GetInstructionText(const Input& input) const;
    std::string GetOperation(const Input& input, const Player& player) const;
    std::vector<std::string> GetButtonHints(const Input& input) const;

private:
    enum class Step { Move, Fire, Heat, FireMoving, Boost, Brake, Bank, Roll, Weapon, Missile, Homing, Minigun, Pause, Combat, Complete };
    const char* GetPracticeWeapon() const;
    std::string currentWeapon_;
    bool heatRaised_ = false;
    int defeatedTargets_ = 0;
    bool achieved_ = false;
    void EnterStep(const Player& player);
    int CountDirections() const;
    Step step_ = Step::Move;
    float stepTime_ = 0.0f;
    float successTime_ = 0.0f;
    float holdTime_ = 0.0f;
    std::array<float, 4> directionTime_ {};
    unsigned directions_ = 0;
    unsigned bankDirections_ = 0;
    bool wasRolling_ = false;
    bool pauseOpened_ = false;
};
