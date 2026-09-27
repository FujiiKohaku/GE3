#include "App/Game/Audio/GameSfx.h"
#include "Engine/Logger/Logger.h"

#include <string>

namespace {
struct SfxSettings {
    const char* path;
    float volume;
    float minimumIntervalSeconds;
};

constexpr std::array<SfxSettings, static_cast<std::size_t>(GameSfxId::Count)> kSfxSettings = {
    SfxSettings { "resources/Audio/GameSfx/ui_select.wav", 0.22f, 0.08f },
    SfxSettings { "resources/Audio/GameSfx/ui_confirm.wav", 0.32f, 0.20f },
    SfxSettings { "resources/Audio/GameSfx/player_normal_shot.wav", 0.20f, 0.04f },
    SfxSettings { "resources/Audio/GameSfx/player_missile_shot.wav", 0.30f, 0.12f },
    SfxSettings { "resources/Audio/GameSfx/player_minigun_shot.wav", 0.13f, 0.10f },
    SfxSettings { "resources/Audio/GameSfx/enemy_hit.wav", 0.16f, 0.08f },
    SfxSettings { "resources/Audio/GameSfx/armor_deflect.wav", 0.24f, 0.10f },
    SfxSettings { "resources/Audio/GameSfx/player_damage.wav", 0.38f, 0.15f },
    SfxSettings { "resources/Audio/GameSfx/heal_pickup.wav", 0.28f, 0.20f },
    SfxSettings { "resources/Audio/GameSfx/boost_start.wav", 0.22f, 0.30f },
    SfxSettings { "resources/Audio/GameSfx/enemy_destroyed.wav", 0.25f, 0.10f },
    SfxSettings { "resources/Audio/GameSfx/boss_destroyed.wav", 0.42f, 0.50f },
    SfxSettings { "resources/Audio/GameSfx/enemy_shot.wav", 0.10f, 0.12f },
    SfxSettings { "resources/Audio/GameSfx/homing_lock.wav", 0.20f, 0.12f },
    SfxSettings { "resources/Audio/GameSfx/pause_toggle.wav", 0.18f, 0.15f },
    SfxSettings { "resources/Audio/GameSfx/environment_collision.wav", 0.20f, 0.20f },
    SfxSettings { "resources/Audio/GameSfx/low_hp_warning.wav", 0.28f, 0.80f },
    SfxSettings { "resources/Audio/GameSfx/stage_clear.wav", 0.34f, 0.50f },
    SfxSettings { "resources/Audio/GameSfx/game_over.wav", 0.34f, 0.50f }
};
}

GameSfx* GameSfx::GetInstance()
{
    static GameSfx instance;
    return &instance;
}

void GameSfx::Initialize()
{
    if (initialized_) {
        return;
    }

    SoundManager* soundManager = SoundManager::GetInstance();
    std::size_t loadedCount = 0;
    std::size_t loadedBytes = 0;
    for (std::size_t index = 0; index < kSfxSettings.size(); ++index) {
        sounds_[index] = soundManager->SoundLoadFile(kSfxSettings[index].path);
        if (sounds_[index].buffer.empty()) {
            Logger::Warning(std::string("GameSfx load failed: ") + kSfxSettings[index].path);
            continue;
        }
        ++loadedCount;
        loadedBytes += sounds_[index].buffer.size();
    }
    Logger::Log(
        "GameSfx loaded: " + std::to_string(loadedCount) + "/" +
        std::to_string(kSfxSettings.size()) + " clips, " +
        std::to_string(loadedBytes) + " PCM bytes");
    Logger::Flush();
    initialized_ = true;
}

void GameSfx::Finalize()
{
    if (!initialized_) {
        return;
    }

    SoundManager* soundManager = SoundManager::GetInstance();
    for (SoundData& sound : sounds_) {
        soundManager->SoundUnload(&sound);
    }
    initialized_ = false;
}

void GameSfx::Play(GameSfxId id)
{
    if (!initialized_) {
        Initialize();
    }

    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= sounds_.size() || sounds_[index].buffer.empty()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const float elapsedSeconds =
        std::chrono::duration<float>(now - lastPlayed_[index]).count();
    if (elapsedSeconds < kSfxSettings[index].minimumIntervalSeconds) {
        return;
    }

    lastPlayed_[index] = now;
    SoundManager::GetInstance()->SoundPlayWave(
        sounds_[index],
        kSfxSettings[index].volume);
}
