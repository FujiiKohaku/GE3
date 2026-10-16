#pragma once
#include "App/Game/Stage/EnemyAppearanceTable.h"
#include "App/Game/Enemy/NormalEnemy/NormalEnemy.h"
#include "App/Game/Enemy/ArmoredEnemy/ArmoredEnemy.h"
#include "App/Game/Enemy/MoveEnemy/MoveEnemy.h"
#include "App/Game/Enemy/PaintEnemy/PaintShooterEnemy.h"
#include "App/Game/Enemy/SwarmEnemy/SwarmEnemy.h"

inline bool RunEnemyAppearanceTests()
{
    NormalEnemy normal;
    ArmoredEnemy armored;
    MoveEnemy moving;
    PaintShooterEnemy paint;
    SwarmEnemy swarm;
    BaseEnemy ordinary;
    const std::array<const BaseEnemy*, 5> enemies { &normal, &moving, &armored, &paint, &swarm };
    for (size_t i = 0; i < enemies.size(); ++i) {
        const auto id = enemies[i]->GetAppearanceId();
        if (id != kEnemyAppearances[i].appearance ||
            FindEnemyAppearance("stage03", id) != kEnemyAppearances[i].model ||
            FindEnemyAppearance("stage01", id) != kEnemyAppearances[i + 5].model ||
            !FindEnemyAppearance("stage02", id).empty() ||
            !FindEnemyAppearance("tutorial", id).empty()) return false;
    }
    const NormalEnemy* inherited = &armored;
    return inherited->GetAppearanceId() == "ArmoredEnemy" && ordinary.GetAppearanceId().empty() &&
        FindEnemyAppearance("stage03", ordinary.GetAppearanceId()).empty() &&
        FindEnemyAppearance("stage03", "UnknownEnemy").empty();
}
