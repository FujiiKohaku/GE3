#include "App/Game/Tutorial/TutorialController.h"
#include "Engine/2D/Text/Utf8Utility.h"
#include "App/Game/Player/Player.h"
#include "Engine/Input/Input.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <unordered_set>

#include <memory>

// 統合前の各項目の検証を、Playerや描画を起動せずに続けるための窓口。
struct TutorialTestAccess {
    using StepInput = TutorialController::StepInput;
    struct Step {
        explicit Step(int stepIndex) { controller_.stepIndex_ = stepIndex; }
        void Enter(const StepInput& input) { controller_.ResetStep(input); }
        void Update(const StepInput& input, float deltaTimeSeconds) { controller_.UpdateStep(input, deltaTimeSeconds); }
        void NotifyManualPauseOpened() { controller_.NotifyManualPauseOpened(); }
        bool IsAchieved() const { return controller_.hasAchievedStep_; }
        const TutorialController::StepData& GetData() const { return *controller_.GetCurrentStep(); }
        std::string GetOperation(const StepInput& input) const { return controller_.GetStepOperation(input); }
        std::vector<std::string> GetButtonHints() const { return controller_.GetStepButtonHints(); }
    private:
        TutorialController controller_;
    };
    static std::array<std::unique_ptr<Step>, TutorialController::kStepCount> CreateSteps()
    {
        std::array<std::unique_ptr<Step>, TutorialController::kStepCount> steps;
        for (int stepIndex = 0; stepIndex < TutorialController::kStepCount; ++stepIndex) {
            steps[stepIndex] = std::make_unique<Step>(stepIndex);
        }
        return steps;
    }
    static void SelectStep(TutorialController& controller, int stepIndex, const Player& player)
    {
        controller.stepIndex_ = stepIndex;
        controller.EnterStep(player);
    }
};
void VerifyGlyphCoverage(const std::unordered_set<uint32_t>& preparedGlyphs, const std::string& text)
{
    for (uint32_t codepoint : Utf8Utility::Decode(text)) {
        assert(preparedGlyphs.contains(codepoint));
    }
}

void VerifyTutorialGlyphCoverage()
{
    const auto codepoints = Utf8Utility::Decode(TutorialController::GetGlyphText());
    std::unordered_set<uint32_t> preparedGlyphs(codepoints.begin(), codepoints.end());
    // フォント読込時に登録済みのASCIIも含め、アトラスの1024セルに収まることを確認する。
    for (uint32_t codepoint = 32; codepoint <= 126; ++codepoint) {
        preparedGlyphs.insert(codepoint);
    }
    assert(preparedGlyphs.size() <= 1024);
    auto steps = TutorialTestAccess::CreateSteps();
    for (auto& step : steps) {
        const auto& data = step->GetData();
        VerifyGlyphCoverage(preparedGlyphs, data.keyboardInstruction);
        VerifyGlyphCoverage(preparedGlyphs, data.gamepadInstruction);
        for (int deviceIndex = 0; deviceIndex < 2; ++deviceIndex) {
            TutorialTestAccess::StepInput input;
            input.isGamepadConnected = deviceIndex == 1;
            input.isUsingGamepad = input.isGamepadConnected;
            input.isPitchInverted = true;
            input.weapon = data.practiceWeapon;
            step->Enter(input);
            VerifyGlyphCoverage(preparedGlyphs, step->GetOperation(input));
            for (int sampleIndex = 0; sampleIndex < 3; ++sampleIndex) {
                input.heat = 0.5f;
                if (sampleIndex == 2) input.heat = 0.0f;
                input.defeatedTargets = data.requiredKills;
                input.moveX = 1.0f;
                input.bank = 1.0f;
                input.isRolling = true;
                step->Update(input, 0.5f);
                VerifyGlyphCoverage(preparedGlyphs, step->GetOperation(input));
                for (const std::string& button : step->GetButtonHints()) {
                    VerifyGlyphCoverage(preparedGlyphs, button);
                }
            }
        }
    }
    std::cout << "Tutorial glyph coverage passed (" << preparedGlyphs.size() << " codepoints)\n";
}

void VerifyTutorialProgression()
{
    Player player;
    Input& input = *Input::GetInstance();
    TutorialController tutorial;
    tutorial.Initialize(player);
    assert(tutorial.GetCompletedCount() == 0);
    assert(tutorial.LimitRailDistance(1000.0f) == 180.0f);

    TutorialTestAccess::SelectStep(tutorial, 1, player);
    assert(tutorial.GetTargetCount() == 3);
    assert(tutorial.LimitRailDistance(1000.0f) == 0.0f);
    tutorial.SetDefeatedTargets(3);
    tutorial.Update(input, player, std::numeric_limits<float>::quiet_NaN());
    tutorial.Update(input, player, -1.0f);
    assert(tutorial.GetCompletedCount() == 1);
    tutorial.Update(input, player, 5.0f);
    assert(tutorial.GetCompletedCount() == 1);
    tutorial.Update(input, player, 1.0f);
    assert(tutorial.GetCompletedCount() == 2);
    assert(tutorial.GetTargetCount() == 0);
    tutorial.Update(input, player, 2.0f);
    assert(tutorial.GetTitle().find("FIRE") != std::string::npos);
    tutorial.Update(input, player, 1.0f);
    assert(tutorial.GetTitle().find("HEAT") != std::string::npos);
    assert(tutorial.LimitRailDistance(1000.0f) == 0.0f);

    TutorialTestAccess::SelectStep(tutorial, 13, player);
    assert(tutorial.HasMovingTargets());
    tutorial.SetDefeatedTargets(5);
    tutorial.Update(input, player, 6.0f);
    assert(!tutorial.IsComplete());
    tutorial.Update(input, player, 3.0f);
    assert(tutorial.IsComplete());
    assert(tutorial.GetCompletedCount() == TutorialController::kStepCount);
    assert(tutorial.GetTargetCount() == 0);
    assert(!tutorial.HasMovingTargets());
    assert(tutorial.LimitRailDistance(1000.0f) == 1000.0f);
    tutorial.Update(input, player, 1.0f);
    assert(tutorial.GetOperation(input, player) == "前方のゲートへ進む");
    tutorial.Initialize(player);
    assert(!tutorial.IsComplete());
    assert(tutorial.GetCompletedCount() == 0);
    assert(tutorial.GetTargetCount() == 0);
}

int main()
{
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    VerifyTutorialProgression();
    VerifyTutorialGlyphCoverage();
    auto steps = TutorialTestAccess::CreateSteps();
    TutorialTestAccess::StepInput input;
    for (const auto& step : steps) {
        assert(step && !step->GetData().title.empty());
        assert(!step->GetData().keyboardInstruction.empty());
        assert(!step->GetData().gamepadInstruction.empty());
        step->Enter(input);
        assert(!step->IsAchieved());
    }
    // マウスの4方向。再入場で方向の記録を消去する。
    for (const auto& direction : std::array<std::array<float, 2>, 4> {{ {-30, 0}, {30, 0}, {0, 30}, {0, -30} }}) {
        input.moveX = direction[0]; input.moveY = direction[1];
        steps[0]->Update(input, 0.016f);
    }
    assert(steps[0]->IsAchieved());
    steps[0]->Enter(input);
    assert(!steps[0]->IsAchieved());
    input.isUsingGamepad = true;
    for (const auto& direction : std::array<std::array<float, 2>, 4> {{ {-1, 0}, {1, 0}, {0, 1}, {0, -1} }}) {
        input.moveX = direction[0]; input.moveY = direction[1];
        steps[0]->Update(input, 0.15f);
    }
    assert(steps[0]->IsAchieved());

    for (int index : {1, 3, 13}) {
        input.defeatedTargets = steps[index]->GetData().requiredKills - 1;
        steps[index]->Update(input, 0.1f);
        assert(!steps[index]->IsAchieved());
        ++input.defeatedTargets;
        steps[index]->Update(input, 0.1f);
        assert(steps[index]->IsAchieved());
    }
    assert(steps[13]->GetData().requiredKills == 5);
    assert(steps[3]->GetData().hasMovingTargets && steps[13]->GetData().hasMovingTargets);

    input.heat = 0.0f;
    steps[2]->Update(input, 0.1f);
    assert(!steps[2]->IsAchieved());
    input.heat = 0.3f; steps[2]->Update(input, 0.1f);
    assert(steps[2]->GetButtonHints().size() == 2);
    input.heat = 0.05f; input.isOverheated = true; steps[2]->Update(input, 0.1f);
    assert(!steps[2]->IsAchieved());
    input.isOverheated = false; steps[2]->Update(input, 0.1f);
    assert(steps[2]->IsAchieved());

    for (int index : {4, 5}) {
        input.isBoosting = true; input.speedMultiplier = 0.5f;
        steps[index]->Update(input, 1.0f);
        assert(!steps[index]->IsAchieved());
        input.isBoosting = false; input.speedMultiplier = 1.0f;
        steps[index]->Update(input, 0.1f); // 途中で離すと継続時間をリセット。
        input.isBoosting = true; input.speedMultiplier = 0.5f;
        steps[index]->Update(input, 1.0f);
        assert(!steps[index]->IsAchieved());
        steps[index]->Update(input, 0.5f);
        assert(steps[index]->IsAchieved());
    }
    input.isGamepadConnected = true;
    input.bank = 0.9f; steps[6]->Update(input, 0.35f);
    assert(!steps[6]->IsAchieved());
    input.bank = -0.9f; steps[6]->Update(input, 0.35f);
    assert(steps[6]->IsAchieved());
    input.isRolling = true; steps[7]->Enter(input);
    steps[7]->Update(input, 0.1f);
    assert(!steps[7]->IsAchieved()); // 入場前からのロールは達成にしない。
    input.isRolling = false; steps[7]->Update(input, 0.1f);
    input.isRolling = true; steps[7]->Update(input, 0.1f);
    assert(steps[7]->IsAchieved());
    input.isGamepadConnected = false; input.isSpaceTriggered = true;
    for (int index : {6, 7}) {
        steps[index]->Enter(input); steps[index]->Update(input, 0.1f);
        assert(steps[index]->IsAchieved());
    }

    for (int index = 8; index <= 11; ++index) {
        input.weapon = "WrongWeapon"; input.defeatedTargets = 3;
        steps[index]->Update(input, 0.1f);
        assert(!steps[index]->IsAchieved());
        input.weapon = steps[index]->GetData().practiceWeapon;
        steps[index]->Update(input, 0.1f);
        assert(steps[index]->IsAchieved());
        steps[index]->Enter(input);
        assert(!steps[index]->IsAchieved());
    }
    steps[12]->Update(input, 0.1f); assert(!steps[12]->IsAchieved());
    steps[12]->NotifyManualPauseOpened(); steps[12]->Update(input, 0.1f);
    assert(steps[12]->IsAchieved());
    steps[12]->Enter(input); steps[12]->Update(input, 0.1f);
    assert(!steps[12]->IsAchieved());
    std::cout << "TutorialController tests passed (14 steps)\n";
}
