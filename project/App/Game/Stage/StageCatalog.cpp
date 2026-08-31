#include "StageCatalog.h"

#include "externals/json.hpp"
#include <filesystem>
#include <fstream>

namespace {
Vector3 ReadVector3(const nlohmann::json& value, const Vector3& fallback)
{
    if (!value.is_array() || value.size() < 3) {
        return fallback;
    }
    return {
        value[0].get<float>(),
        value[1].get<float>(),
        value[2].get<float>()
    };
}
}

StageCatalog* StageCatalog::GetInstance()
{
    static StageCatalog instance;
    return &instance;
}

bool StageCatalog::Load(const std::string& catalogPath)
{
    stages_.clear();
    lastError_.clear();

    std::ifstream file(catalogPath);
    if (!file) {
        lastError_ = "Stage catalog not found: " + catalogPath;
        return false;
    }

    try {
        nlohmann::json catalog;
        file >> catalog;
        if (!catalog.contains("stages") || !catalog["stages"].is_array()) {
            lastError_ = "Stage catalog has no stages array: " + catalogPath;
            return false;
        }

        for (const nlohmann::json& entry : catalog["stages"]) {
            if (!entry.contains("settings")) {
                continue;
            }
            StageSettings settings;
            const std::string path = entry["settings"].get<std::string>();
            if (!LoadStageSettings(path, settings)) {
                return false;
            }
            for (const StageSettings& existing : stages_) {
                if (existing.id == settings.id) {
                    lastError_ = "Duplicate stage id: " + settings.id;
                    return false;
                }
            }
            stages_.push_back(std::move(settings));
        }
    } catch (const std::exception& exception) {
        lastError_ = "Failed to load stage catalog: ";
        lastError_ += exception.what();
        stages_.clear();
        return false;
    }

    if (stages_.empty()) {
        lastError_ = "Stage catalog is empty: " + catalogPath;
        return false;
    }
    return true;
}

const StageSettings* StageCatalog::Find(const std::string& id) const
{
    for (const StageSettings& stage : stages_) {
        if (stage.id == id) {
            return &stage;
        }
    }
    return nullptr;
}

bool StageCatalog::LoadStageSettings(
    const std::string& settingsPath,
    StageSettings& settings)
{
    std::ifstream file(settingsPath);
    if (!file) {
        lastError_ = "Stage settings not found: " + settingsPath;
        return false;
    }

    nlohmann::json json;
    file >> json;
    settings.settingsFile = settingsPath;
    settings.id = json.value("id", "");
    settings.name = json.value("name", settings.id);
    settings.description = json.value("description", "");
    settings.layoutFile = json.value("layout", "");

    if (json.contains("rail")) {
        const nlohmann::json& rail = json["rail"];
        settings.railLength = rail.value("length", settings.railLength);
        settings.railPointInterval =
            rail.value("point_interval", settings.railPointInterval);
        settings.railSpeed = rail.value("speed", settings.railSpeed);
        if (rail.contains("control_points") &&
            rail["control_points"].is_array()) {
            for (const nlohmann::json& point : rail["control_points"]) {
                settings.railControlPoints.push_back(ReadVector3(point, {}));
            }
        }
    }
    if (json.contains("boss")) {
        const nlohmann::json& boss = json["boss"];
        settings.bossType = boss.value("type", settings.bossType);
        settings.bossSpawnDistance =
            boss.value("spawn_distance", settings.bossSpawnDistance);
        settings.bossRailAutoExtension =
            boss.value("rail_auto_extension", settings.bossRailAutoExtension);
        settings.bossRailExtensionBuffer =
            boss.value("rail_extension_buffer", settings.bossRailExtensionBuffer);
        if (boss.contains("position")) {
            settings.bossPosition =
                ReadVector3(boss["position"], settings.bossPosition);
        }
    }
    if (json.contains("environment")) {
        const nlohmann::json& environment = json["environment"];
        settings.skybox = environment.value("skybox", settings.skybox);
        settings.bgm = environment.value("bgm", settings.bgm);
        settings.floorTexture =
            environment.value("floor_texture", settings.floorTexture);
        settings.floorEnabled =
            environment.value("floor_enabled", settings.floorEnabled);
        settings.floorHeight =
            environment.value("floor_height", settings.floorHeight);
    }
    if (json.contains("swarm_wave_distances")) {
        settings.swarmWaveDistances =
            json["swarm_wave_distances"].get<std::vector<float>>();
    }
    if (json.contains("recovery_items")) {
        for (const nlohmann::json& position : json["recovery_items"]) {
            settings.recoveryItemPositions.push_back(
                ReadVector3(position, {}));
        }
    }
    if (json.contains("recovery_item_distances")) {
        settings.recoveryItemDistances =
            json["recovery_item_distances"].get<std::vector<float>>();
    }
    if (json.contains("paint_enemy_distances")) {
        settings.paintEnemyDistances =
            json["paint_enemy_distances"].get<std::vector<float>>();
    }

    if (settings.id.empty() || settings.layoutFile.empty()) {
        lastError_ = "Stage id or layout is missing: " + settingsPath;
        return false;
    }
    if (settings.railLength <= 0.0f || settings.railPointInterval <= 0.0f) {
        lastError_ = "Invalid rail settings: " + settings.id;
        return false;
    }
    if (!settings.railControlPoints.empty() &&
        settings.railControlPoints.size() < 4) {
        lastError_ = "Rail requires at least four control points: " + settings.id;
        return false;
    }
    if (settings.bossSpawnDistance > settings.railLength) {
        lastError_ = "Boss distance exceeds rail length: " + settings.id;
        return false;
    }
    if (settings.bossRailExtensionBuffer < 0.0f) {
        lastError_ = "Boss rail extension buffer is negative: " + settings.id;
        return false;
    }
    if (!std::filesystem::exists(settings.layoutFile)) {
        lastError_ = "Stage layout not found: " + settings.layoutFile;
        return false;
    }
    return true;
}
