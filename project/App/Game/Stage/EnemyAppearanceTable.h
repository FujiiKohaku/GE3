#pragma once
#include <array>
#include <string_view>

struct EnemyAppearanceEntry {
    std::string_view stage;
    std::string_view appearance;
    std::string_view model;
};

// 敵はステージやモデルパスを知らず、外見の識別名だけを返す。
inline constexpr std::array<EnemyAppearanceEntry, 10> kEnemyAppearances {{
    { "stage03", "NormalEnemy", "Enemy/Frozen/NormalEnemy.obj" },
    { "stage03", "MoveEnemy", "Enemy/Frozen/MoveEnemy.obj" },
    { "stage03", "ArmoredEnemy", "Enemy/Frozen/ArmoredEnemy.obj" },
    { "stage03", "PaintShooterEnemy", "Enemy/Frozen/PaintShooterEnemy.obj" },
    { "stage03", "SwarmEnemy", "Enemy/Frozen/SwarmEnemy.obj" },
    { "stage01", "NormalEnemy", "Enemy/Ruins/NormalEnemy_Body.obj" },
    { "stage01", "MoveEnemy", "Enemy/Ruins/MoveEnemy_Body.obj" },
    { "stage01", "ArmoredEnemy", "Enemy/Ruins/ArmoredEnemy_Body.obj" },
    { "stage01", "PaintShooterEnemy", "Enemy/Ruins/PaintShooterEnemy_Body.obj" },
    { "stage01", "SwarmEnemy", "Enemy/Ruins/SwarmEnemy_Body.obj" },
}};

inline constexpr std::string_view FindEnemyAppearance(std::string_view stage, std::string_view appearance)
{
    for (const auto& entry : kEnemyAppearances) {
        if (entry.stage == stage && entry.appearance == appearance) return entry.model;
    }
    return {};
}
