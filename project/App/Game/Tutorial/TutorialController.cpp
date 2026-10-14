#include "TutorialController.h"
#include "App/Game/Player/Player.h"
#include "Engine/Input/Input.h"
#include "Engine/audio/SoundManager.h"
#include <algorithm>
#include <cmath>

void TutorialController::Initialize(const Player& player)
{
    stepIndex_ = 0;
    EnterStep(player);
}

void TutorialController::EnterStep(const Player& player)
{
    defeatedTargets_ = 0;
    achieved_ = false;
    stepTime_ = successTime_ = 0.0f;
    currentWeapon_ = player.GetCurrentWeaponDisplayName();
    if (auto* step = GetCurrentStep()) {
        TutorialStepInput initial;
        initial.weapon = currentWeapon_;
        initial.rolling = player.IsRolling();
        step->Enter(initial);
    }
}

TutorialStepInput TutorialController::ReadStepInput(const Input& input, const Player& player) const
{
    TutorialStepInput state;
    state.gamepadConnected = input.IsGamepadConnected();
    state.usingGamepad = input.IsUsingGamepad();
    state.invertedPitch = input.IsGamepadPitchInverted();
    state.moveX = state.usingGamepad ? input.GetGamepadLeftStickX() : static_cast<float>(input.GetMouseDeltaX());
    state.moveY = state.usingGamepad ? input.GetGamepadLeftStickY() : -static_cast<float>(input.GetMouseDeltaY());
    state.spaceTriggered = input.IsKeyTrigger(DIK_SPACE);
    state.boosting = player.IsBoosting();
    state.speedMultiplier = player.GetForwardSpeedMultiplier();
    state.bank = player.GetManualBank();
    state.rolling = player.IsRolling();
    state.heat = player.GetHeatRatio();
    state.overheated = player.IsOverheated();
    state.weapon = player.GetCurrentWeaponDisplayName();
    state.defeatedTargets = defeatedTargets_;
    return state;
}

int TutorialController::GetTargetCount() const
{
    const auto* step = GetCurrentStep();
    if (!step || successTime_ > 0.0f) return 0;
    const auto& data = step->GetData();
    if (!data.practiceWeapon.empty() && currentWeapon_ != data.practiceWeapon) return 0;
    return data.requiredKills;
}

bool TutorialController::HasMovingTargets() const
{
    const auto* step = GetCurrentStep();
    return step && step->GetData().movingTargets;
}

void TutorialController::Update(const Input& input, const Player& player, float deltaTime)
{
    if (IsComplete() || player.IsDead()) return;
    currentWeapon_ = player.GetCurrentWeaponDisplayName();
    const float dt = std::isfinite(deltaTime) ? (std::max)(deltaTime, 0.0f) : 0.0f;
    if (dt <= 0.0f) return;
    stepTime_ += dt;
    if (successTime_ > 0.0f) {
        successTime_ -= dt;
        if (successTime_ <= 0.0f) {
            ++stepIndex_;
            EnterStep(player);
        }
        return;
    }
    BaseTutorialStep* step = GetCurrentStep();
    step->Update(ReadStepInput(input, player), dt);
    achieved_ = achieved_ || step->IsAchieved();
    if (achieved_ && stepTime_ >= 6.0f) {
        successTime_ = 3.0f;
        SoundManager::GetInstance()->Play("TutorialConfirm");
    }
}

void TutorialController::NotifyManualPauseOpened()
{
    if (auto* step = GetCurrentStep(); step && successTime_ <= 0.0f) {
        step->NotifyManualPauseOpened();
    }
}

int TutorialController::GetCompletedCount() const
{
    return (std::min)(kStepCount, stepIndex_ + (successTime_ > 0.0f ? 1 : 0));
}

float TutorialController::LimitRailDistance(float distance) const
{
    const auto* step = GetCurrentStep();
    if (!step) return distance;
    if (GetTargetCount() > 0 || step->GetData().holdRail) return 0.0f;
    return (std::min)(distance, step->GetData().checkpoint);
}

std::string TutorialController::GetTitle() const
{
    const auto* step = GetCurrentStep();
    if (!step) return "TRAINING COMPLETE // GATE OPEN";
    return "TRAINING " + std::to_string(stepIndex_ + 1) + " / " +
        std::to_string(kStepCount) + " // " + step->GetData().title;
}

std::string TutorialController::GetInstructionText(const Input& input) const
{
    const auto* step = GetCurrentStep();
    if (!step) return "訓練完了。出撃ゲートを開きました。\n前方の緑色のゲートへ進んでください。";
    if (successTime_ > 0.0f) return "できました！ この練習は完了です。";
    const auto& data = step->GetData();
    return input.IsGamepadConnected() ? data.gamepadInstruction : data.keyboardInstruction;
}

std::string TutorialController::GetOperation(const Input& input, const Player& player) const
{
    const auto* step = GetCurrentStep();
    if (!step) return "前方のゲートへ進む";
    if (successTime_ > 0.0f) return "練習クリア";
    return step->GetOperation(ReadStepInput(input, player));
}

std::vector<std::string> TutorialController::GetButtonHints(const Input& input) const
{
    const auto* step = GetCurrentStep();
    if (!input.IsGamepadConnected() || !step || successTime_ > 0.0f) return {};
    return step->GetButtonHints();
}