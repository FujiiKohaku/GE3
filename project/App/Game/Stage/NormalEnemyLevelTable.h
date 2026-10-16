#pragma once
#include "App/Game/Enemy/Types/NormalEnemySettings.h"
#include "Engine/Logger/Logger.h"
#include "externals/json.hpp"
#include <array>
#include <fstream>
#include <limits>
#include <string>

class NormalEnemyLevelTable {
public:
    bool Load(const std::string& filePath = "resources/Enemies/normal-enemy-levels.json")
    {
        settings_.fill(NormalEnemySettings {});
        std::ifstream file(filePath);
        if (!file) {
            Logger::Warning("Normal enemy levels missing; using legacy settings: " + filePath);
            return false;
        }
        const nlohmann::json data = nlohmann::json::parse(file, nullptr, false);
        if (!data.is_object() || !data.contains("levels") || !data["levels"].is_object()) {
            Logger::Warning("Invalid normal enemy level file; using legacy settings: " + filePath);
            return false;
        }
        bool isValid = true;
        for (size_t index = 0; index < settings_.size(); ++index) {
            const std::string levelKey = std::to_string(index + 1);
            if (!data["levels"].contains(levelKey) ||
                !ReadSettings(data["levels"][levelKey], settings_[index])) {
                Logger::Warning("Invalid normal enemy level " + levelKey +
                    "; using legacy settings: " + filePath);
                isValid = false;
            }
        }
        return isValid;
    }

    NormalEnemySettings GetSettings(int level) const
    {
        if (level < 1 || level > static_cast<int>(settings_.size())) {
            Logger::Warning("Normal enemy level out of range; using legacy settings");
            return {};
        }
        return settings_[static_cast<size_t>(level - 1)];
    }

private:
    static bool ReadSettings(const nlohmann::json& data, NormalEnemySettings& settings)
    {
        if (!data.is_object() || !data.contains("hp") || !data["hp"].is_number() ||
            !data.contains("fireIntervalFrames") || !data["fireIntervalFrames"].is_number_integer() ||
            !data.contains("bulletSpeedPerFrame") || !data["bulletSpeedPerFrame"].is_number() ||
            !data.contains("isRadialShotEnabled") || !data["isRadialShotEnabled"].is_boolean()) {
            return false;
        }
        const double intervalFrames = data["fireIntervalFrames"].get<double>();
        if (intervalFrames < 1.0 || intervalFrames > (std::numeric_limits<int32_t>::max)()) {
            return false;
        }
        NormalEnemySettings candidate;
        candidate.hp = data["hp"].get<float>();
        candidate.fireIntervalFrames = data["fireIntervalFrames"].get<int32_t>();
        candidate.bulletSpeedPerFrame = data["bulletSpeedPerFrame"].get<float>();
        candidate.isRadialShotEnabled = data["isRadialShotEnabled"].get<bool>();
        if (!IsValidNormalEnemySettings(candidate)) {
            return false;
        }
        settings = candidate;
        return true;
    }

    std::array<NormalEnemySettings, 3> settings_ {};
};
