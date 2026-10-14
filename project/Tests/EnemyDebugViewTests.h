#pragma once
#include "App/Game/Enemy/MoveEnemy/MoveEnemy.h"
#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"

inline bool RunEnemyDebugViewTests()
{
    MoveEnemy enemy;
    BaseEnemy* base = &enemy;
    auto view = base->CreateDebugView();
    if (!view || !view->HasMovementControls() || view->HasBossInspector()) return false;
    if (!view->Apply("pattern", "2") || !view->Apply("speed", "4") ||
        !view->Apply("amplitude", "6") || !view->Apply("frequency", "3")) return false;
    const auto state = view->GetState(2);
    if (state["index"] != 2 || state["pattern"] != 2 || state["speed"] != 4.0f ||
        state["amplitude"] != 6.0f || state["frequency"] != 3.0f) return false;
    if (view->Apply("speed", "NaN") || view->Apply("speed", "4x") || view->Apply("invalid", "1")) return false;
    if (!view->Apply("speed", "20") || enemy.GetMoveSpeed() != 10.0f) return false;
    if (!view->Apply("pattern", "-1") || enemy.GetMovePattern() != MovePattern::LeftRight) return false;
    auto controls = nlohmann::json::array();
    view->AppendControls(controls, 2);
    if (controls.size() != 5 || controls[0]["key"] != "enemy.2.pattern" ||
        controls[4]["key"] != "enemy.2.reset") return false;
    IceJellyfish boss;
    base = &boss;
    auto bossView = base->CreateDebugView();
    if (!bossView || !bossView->HasBossInspector() || bossView->HasMovementControls() ||
        bossView->GetBossHp() != boss.GetHp()) return false;
    BaseEnemy ordinary;
    return ordinary.CreateDebugView() == nullptr;
}
