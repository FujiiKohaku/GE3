#pragma once

#include "Engine/audio/SoundManager.h"

#include <array>
#include <chrono>
#include <cstddef>

enum class GameSfxId : std::size_t {
    UiSelect,
    UiConfirm,
    NormalShot,
    MissileShot,
    MinigunShot,
    EnemyHit,
    ArmorDeflect,
    PlayerDamage,
    HealPickup,
    BoostStart,
    EnemyDestroyed,
    BossDestroyed,
    EnemyShot,
    HomingLock,
    PauseToggle,
    EnvironmentCollision,
    LowHpWarning,
    StageClear,
    GameOver,
    Count
};

class GameSfx {
public:
    static GameSfx* GetInstance();

    void Initialize();
    void Finalize();
    void Play(GameSfxId id);

private:
    std::array<SoundData, static_cast<std::size_t>(GameSfxId::Count)> sounds_ {};
    std::array<std::chrono::steady_clock::time_point,
        static_cast<std::size_t>(GameSfxId::Count)> lastPlayed_ {};
    bool initialized_ = false;
};
