#include "Engine/audio/SoundManager.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

void Require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}

int main()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    try {
        SoundManager* sounds = SoundManager::GetInstance();
        SoundRegistrationResult unavailableResult {};
        Require(!sounds->Register("not-ready", "unused", 1.0f, 0.0f, &unavailableResult) &&
            unavailableResult == SoundRegistrationResult::AudioUnavailable, "Uninitialized audio must fail safely");
        sounds->Initialize();
        sounds->Initialize();
        Require(!sounds->Play("missing"), "Missing ID must be rejected");
        SoundRegistrationResult registrationResult {};
        Require(!sounds->Register("", "unused", 1.0f, 0.0f, &registrationResult) &&
            registrationResult == SoundRegistrationResult::InvalidId, "Invalid ID reason missing");
        Require(!sounds->Register("file-error", "resources/Audio/missing.wav", 1.0f, 0.0f, &registrationResult) &&
            registrationResult == SoundRegistrationResult::LoadFailed, "Load failure reason missing");
        Require(!sounds->SoundPlayWave(SoundData {}), "Empty playback must fail");
        SoundData invalidFormat;
        invalidFormat.buffer.resize(16);
        Require(!sounds->SoundPlayWave(invalidFormat), "Invalid format must fail safely");
        const char* path = "resources/Audio/GameSfx/ui_select.wav";
        Require(!sounds->Register("", path), "Empty ID must be rejected");
        Require(!sounds->Register("bad-file", "resources/Audio/missing.wav"), "Missing file must be rejected");
        Require(!sounds->Register("bad-volume", path, std::numeric_limits<float>::quiet_NaN(), 0.0f, &registrationResult) &&
            registrationResult == SoundRegistrationResult::InvalidSettings, "Invalid settings reason missing");
        Require(!sounds->Register("bad-interval", path, 0.0f, std::numeric_limits<float>::infinity()), "Infinite interval must be rejected");
        Require(sounds->Register("interval", path, 0.0f, 0.1f), "Sound registration failed");
        Require(!sounds->Register("interval", path, 1.0f, 0.0f, &registrationResult) &&
            registrationResult == SoundRegistrationResult::DuplicateId, "Duplicate ID reason missing");
        Require(sounds->Play("interval"), "First playback failed");
        Require(!sounds->Play("interval"), "Immediate repeat must be suppressed");
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        Require(sounds->Play("interval"), "Playback after interval failed");
        Require(sounds->Register("no-interval", path, -1.0f, -1.0f), "Clamped registration failed");
        Require(sounds->Play("no-interval") && sounds->Play("no-interval"), "Zero interval must allow repeats");
        Require(sounds->RegisterSounds({ { "scene-sound", path, 0.0f, 60.0f } }), "Scene sound registration failed");
        Require(sounds->Play("scene-sound"), "Scene playback failed");
        Require(sounds->RegisterSounds({ { "scene-sound", "missing.wav", 1.0f, 0.0f } }), "Scene revisit must reuse registered sound");
        Require(!sounds->Play("scene-sound"), "Scene revisit must preserve playback interval");
        Require(!sounds->RegisterSounds({ { "missing-scene-file", "missing.wav" } }), "Scene file failure must be reported");
        Require(!sounds->RegisterSounds({ { nullptr, path } }), "Null scene ID must fail safely");
        sounds->Update();
        sounds->Finalize();
        sounds = SoundManager::GetInstance();
        sounds->Initialize();
        Require(!sounds->Play("interval"), "Finalization must release registered sounds");
        Require(sounds->Register("interval", path, 0.0f), "Reinitialization failed");
        Require(sounds->Play("interval"), "Reinitialized playback failed");
        sounds->Finalize();
        std::cout << "SoundManager tests passed: failure reasons, unavailable audio, invalid format, registration, interval, scene registration and revisit, shutdown, reinitialization\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        SoundManager::GetInstance()->Finalize();
        CoUninitialize();
        return 1;
    }
    CoUninitialize();
    return 0;
}
