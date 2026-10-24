#pragma once
#include <filesystem>

class CameraPreferences {
public:
    float distance = 35.0f;
    float verticalFovDegrees = 25.783f;
    int preset = 0; // -1 means custom.
    void CyclePreset(int direction);
    void AdjustDistance(float amount);
    void AdjustFov(float degrees);
    float FovRadians() const;
    float BoostFovRadians() const;
    const char* PresetName() const;
    bool Load(const std::filesystem::path& path = "generated/settings/camera.json");
    bool Save(const std::filesystem::path& path = "generated/settings/camera.json") const;
};
