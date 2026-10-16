#include "App/Game/Tutorial/BaseTutorialStep.h"
#include "Engine/2D/Text/Utf8Utility.h"
#include <cassert>
#include <iostream>
#include <unordered_set>

void VerifyGlyphCoverage(const std::unordered_set<uint32_t>& preparedGlyphs, const std::string& text)
{
    for (uint32_t codepoint : Utf8Utility::Decode(text)) {
        assert(preparedGlyphs.contains(codepoint));
    }
}

void VerifyTutorialGlyphCoverage()
{
    const auto codepoints = Utf8Utility::Decode(GetTutorialGlyphText());
    std::unordered_set<uint32_t> preparedGlyphs(codepoints.begin(), codepoints.end());
    // フォント読込時に登録済みのASCIIも含め、アトラスの1024セルに収まることを確認する。
    for (uint32_t codepoint = 32; codepoint <= 126; ++codepoint) {
        preparedGlyphs.insert(codepoint);
    }
    assert(preparedGlyphs.size() <= 1024);
    auto steps = CreateTutorialSteps();
    for (auto& step : steps) {
        const TutorialStepData& data = step->GetData();
        VerifyGlyphCoverage(preparedGlyphs, data.keyboardInstruction);
        VerifyGlyphCoverage(preparedGlyphs, data.gamepadInstruction);
        for (int deviceIndex = 0; deviceIndex < 2; ++deviceIndex) {
            TutorialStepInput input;
            input.gamepadConnected = deviceIndex == 1;
            input.usingGamepad = input.gamepadConnected;
            input.invertedPitch = true;
            input.weapon = data.practiceWeapon;
            step->Enter(input);
            VerifyGlyphCoverage(preparedGlyphs, step->GetOperation(input));
            for (int sampleIndex = 0; sampleIndex < 3; ++sampleIndex) {
                input.heat = 0.5f;
                if (sampleIndex == 2) input.heat = 0.0f;
                input.defeatedTargets = data.requiredKills;
                input.moveX = 1.0f;
                input.bank = 1.0f;
                input.rolling = true;
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

int main()
{
    VerifyTutorialGlyphCoverage();
    auto steps = CreateTutorialSteps();
    TutorialStepInput input;
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
    input.usingGamepad = true;
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
    assert(steps[3]->GetData().movingTargets && steps[13]->GetData().movingTargets);

    input.heat = 0.0f;
    steps[2]->Update(input, 0.1f);
    assert(!steps[2]->IsAchieved());
    input.heat = 0.3f; steps[2]->Update(input, 0.1f);
    assert(steps[2]->GetButtonHints().size() == 2);
    input.heat = 0.05f; input.overheated = true; steps[2]->Update(input, 0.1f);
    assert(!steps[2]->IsAchieved());
    input.overheated = false; steps[2]->Update(input, 0.1f);
    assert(steps[2]->IsAchieved());

    for (int index : {4, 5}) {
        input.boosting = true; input.speedMultiplier = 0.5f;
        steps[index]->Update(input, 1.0f);
        assert(!steps[index]->IsAchieved());
        input.boosting = false; input.speedMultiplier = 1.0f;
        steps[index]->Update(input, 0.1f); // 途中で離すと継続時間をリセット。
        input.boosting = true; input.speedMultiplier = 0.5f;
        steps[index]->Update(input, 1.0f);
        assert(!steps[index]->IsAchieved());
        steps[index]->Update(input, 0.5f);
        assert(steps[index]->IsAchieved());
    }
    input.gamepadConnected = true;
    input.bank = 0.9f; steps[6]->Update(input, 0.35f);
    assert(!steps[6]->IsAchieved());
    input.bank = -0.9f; steps[6]->Update(input, 0.35f);
    assert(steps[6]->IsAchieved());
    input.rolling = true; steps[7]->Enter(input);
    steps[7]->Update(input, 0.1f);
    assert(!steps[7]->IsAchieved()); // 入場前からのロールは達成にしない。
    input.rolling = false; steps[7]->Update(input, 0.1f);
    input.rolling = true; steps[7]->Update(input, 0.1f);
    assert(steps[7]->IsAchieved());
    input.gamepadConnected = false; input.spaceTriggered = true;
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
    std::cout << "BaseTutorialStep tests passed (14 steps)\n";
}
