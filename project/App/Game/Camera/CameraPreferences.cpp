#include "CameraPreferences.h"
#include "externals/json.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numbers>

namespace {
constexpr float kDistances[] = {35.0f, 27.0f, 22.0f};
constexpr float kFovs[] = {25.783f, 32.0f, 28.0f};
}
void CameraPreferences::CyclePreset(int direction) {
    if (preset < 0) preset = direction < 0 ? 2 : 0;
    else preset = (preset + (direction < 0 ? 2 : 1)) % 3;
    distance = kDistances[preset]; verticalFovDegrees = kFovs[preset];
}
void CameraPreferences::AdjustDistance(float amount) {
    if (!std::isfinite(amount)) return;
    distance = std::clamp(distance + amount, 18.0f, 60.0f); preset = -1;
}
void CameraPreferences::AdjustFov(float degrees) {
    if (!std::isfinite(degrees)) return;
    verticalFovDegrees = std::clamp(verticalFovDegrees + degrees, 20.0f, 60.0f); preset = -1;
}
float CameraPreferences::FovRadians() const { return verticalFovDegrees * std::numbers::pi_v<float> / 180.0f; }
float CameraPreferences::BoostFovRadians() const { return (std::min)(verticalFovDegrees * (4.0f / 3.0f), 75.0f) * std::numbers::pi_v<float> / 180.0f; }
const char* CameraPreferences::PresetName() const {
    switch (preset) { case 0: return "標準"; case 1: return "低空・速度感"; case 2: return "機体重視"; default: return "カスタム"; }
}
bool CameraPreferences::Load(const std::filesystem::path& path) {
    *this = CameraPreferences{};
    try {
        std::ifstream stream(path); if (!stream) return false;
        nlohmann::json data; stream >> data;
        const float d = data.at("distance").get<float>(), f = data.at("vertical_fov_degrees").get<float>();
        if (!std::isfinite(d) || !std::isfinite(f) || d < 18 || d > 60 || f < 20 || f > 60) return false;
        distance = d; verticalFovDegrees = f; preset = -1;
        for (int i = 0; i < 3; ++i) if (std::abs(d - kDistances[i]) < 0.001f && std::abs(f - kFovs[i]) < 0.001f) preset = i;
        return true;
    } catch (const std::exception&) { return false; }
}
bool CameraPreferences::Save(const std::filesystem::path& path) const {
    try {
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path);
        if (!stream) return false;
        stream << nlohmann::json{{"distance",distance},{"vertical_fov_degrees",verticalFovDegrees}}.dump(2) << '\n';
        stream.flush(); return bool(stream);
    } catch (const std::exception&) { return false; }
}
