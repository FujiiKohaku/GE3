#pragma once
#include "App/Game/Enemy/EnemyAppearanceMotion.h"
#include <array>
#include <string_view>

struct EnemyAppearancePartEntry {
    std::string_view appearance;
    std::string_view model;
    EnemyAppearancePart motion;
};

inline constexpr std::array<EnemyAppearancePartEntry, 17> kEnemyAppearanceParts {{
    { "NormalEnemy", "Enemy/Ruins/NormalEnemy_Eye.obj", EnemyAppearancePart::Eye },
    { "NormalEnemy", "Enemy/Ruins/NormalEnemy_Left.obj", EnemyAppearancePart::Left },
    { "NormalEnemy", "Enemy/Ruins/NormalEnemy_Right.obj", EnemyAppearancePart::Right },
    { "MoveEnemy", "Enemy/Ruins/MoveEnemy_Eye.obj", EnemyAppearancePart::Eye },
    { "MoveEnemy", "Enemy/Ruins/MoveEnemy_Left.obj", EnemyAppearancePart::Left },
    { "MoveEnemy", "Enemy/Ruins/MoveEnemy_Right.obj", EnemyAppearancePart::Right },
    { "MoveEnemy", "Enemy/Ruins/MoveEnemy_Cannon.obj", EnemyAppearancePart::Cannon },
    { "ArmoredEnemy", "Enemy/Ruins/ArmoredEnemy_Eye.obj", EnemyAppearancePart::Eye },
    { "ArmoredEnemy", "Enemy/Ruins/ArmoredEnemy_Left.obj", EnemyAppearancePart::Left },
    { "ArmoredEnemy", "Enemy/Ruins/ArmoredEnemy_Right.obj", EnemyAppearancePart::Right },
    { "ArmoredEnemy", "Enemy/Ruins/ArmoredEnemy_Cannon.obj", EnemyAppearancePart::Cannon },
    { "SwarmEnemy", "Enemy/Ruins/SwarmEnemy_Tail.obj", EnemyAppearancePart::Tail },
    { "PaintShooterEnemy", "Enemy/Ruins/PaintShooterEnemy_Eye.obj", EnemyAppearancePart::Eye },
    { "PaintShooterEnemy", "Enemy/Ruins/PaintShooterEnemy_Left.obj", EnemyAppearancePart::Left },
    { "PaintShooterEnemy", "Enemy/Ruins/PaintShooterEnemy_Right.obj", EnemyAppearancePart::Right },
    { "PaintShooterEnemy", "Enemy/Ruins/PaintShooterEnemy_Cannon.obj", EnemyAppearancePart::Cannon },
    { "PaintShooterEnemy", "Enemy/Ruins/PaintShooterEnemy_Tank.obj", EnemyAppearancePart::Tank },
}};
