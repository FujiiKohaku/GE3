#include "App/Scene/Gameplay/Pause/BasePauseState.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    int selection = 0;
    int resumes = 0, retries = 0, titles = 0, pitches = 0, toggles = 0;
    float pad = 1.0f, mouse = 1.0f;
    const BasePauseState* state = &GetPauseState(PauseViewState::Menu);
    PauseStateContext context { selection };
    context.changeState = [&](PauseViewState next, int selected) {
        state->Exit(context);
        state = &GetPauseState(next);
        state->Enter(context);
        selection = selected;
    };
    context.menuActions = {
        [&]() { ++resumes; }, [&]() { ++retries; },
        [&]() { context.changeState(PauseViewState::Settings, 0); }, [&]() { ++titles; }
    };
    context.adjustPad = [&](float amount) { pad += amount; };
    context.adjustMouse = [&](float amount) { mouse += amount; };
    context.togglePitch = [&]() { ++pitches; };
    context.toggleControl = [&]() { ++toggles; };
    state->Enter(context);
    PauseStateInput input;
    input.up = true; state->HandleInput(context, input); assert(selection == 3);
    input = {}; input.down = true; state->HandleInput(context, input); assert(selection == 0);
    state->AdjustSensitivity(context, 0.1f); assert(pad == 1.0f && mouse == 1.0f);
    input = {}; input.back = true; state->HandleInput(context, input); assert(resumes == 1);
    input = {}; input.retry = true; state->HandleInput(context, input); assert(retries == 1);
    input = {}; input.title = true; state->HandleInput(context, input); assert(titles == 1);
    input = {}; input.settings = true; state->HandleInput(context, input);
    assert(state->GetType() == PauseViewState::Settings && selection == 0);
    input = {}; input.right = true; state->HandleInput(context, input);
    assert(std::abs(pad - 1.1f) < 0.0001f);
    selection = 2; input = {}; input.left = true; state->HandleInput(context, input);
    assert(std::abs(mouse - 0.9f) < 0.0001f);
    selection = 1; input = {}; input.confirm = true; state->HandleInput(context, input);
    assert(pitches == 1);
    state->HandleControlToggle(context, true); assert(toggles == 1);
    assert(state->HasSensitivityControls(0) && state->HasSensitivityControls(2) && !state->HasSensitivityControls(1));
    const auto settingsDisplay = state->GetDisplay({1, true, true, pad, mouse});
    assert(settingsDisplay.title == "設定" && settingsDisplay.rows[0] == "PAD AIM: 110%" &&
        settingsDisplay.rows[1] == "> PAD PITCH: INVERTED" && settingsDisplay.rows[2] == "MOUSE: 90%");
    input = {}; input.back = true; state->HandleInput(context, input);
    assert(state->GetType() == PauseViewState::Menu && selection == 2);
    state->HandleControlToggle(context, true); assert(toggles == 1);
    state->Activate(context); assert(state->GetType() == PauseViewState::Settings && selection == 0);
    selection = 3; state->Activate(context);
    assert(state->GetType() == PauseViewState::Menu && selection == 2);
    const auto menuDisplay = state->GetDisplay({2, false, false, 1.0f, 1.0f});
    assert(menuDisplay.title == "PAUSE MENU" && menuDisplay.rows[2] == "> 設定 [S / ENTER]");
    context.changeState(PauseViewState::Menu, 0); // ポーズ開始・切断時はメニュー先頭へ戻す。
    assert(selection == 0);
    std::cout << "PauseState tests passed\n";
}
