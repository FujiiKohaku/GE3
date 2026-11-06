#include "LevelDataLoader.h"

#include <cassert>
#include <fstream>
#include <cmath>
#include "Engine/Logger/Logger.h"

namespace {
float ReadEnemyNumber(const nlohmann::json& settingsJson, const char* name,
    float defaultValue, float minimum, float maximum, const char* enemyName)
{
    if (!settingsJson.contains(name)) { return defaultValue; }
    const nlohmann::json& valueJson = settingsJson[name];
    if (valueJson.is_number()) {
        const double value = valueJson.get<double>();
        if (std::isfinite(value) && value >= minimum && value <= maximum) {
            return static_cast<float>(value);
        }
    }
    Logger::Warning(std::string("Invalid ") + enemyName + " " + name + "; using default");
    return defaultValue;
}

float ReadPaceEnemyNumber(const nlohmann::json& settingsJson, const char* name,
    float defaultValue, float minimum, float maximum)
{
    return ReadEnemyNumber(settingsJson, name, defaultValue, minimum, maximum, "PaceEnemy");
}

LevelData::StrafeEnemySpawnData ReadStrafeEnemySettings(const nlohmann::json& objectJson)
{
    LevelData::StrafeEnemySpawnData settings;
    if (!objectJson.contains("strafeEnemy")) { return settings; }
    const nlohmann::json& settingsJson = objectJson["strafeEnemy"];
    if (!settingsJson.is_object()) { Logger::Warning("Invalid strafeEnemy settings; using defaults"); return settings; }
    if (settingsJson.contains("entrySide")) {
        const nlohmann::json& sideJson = settingsJson["entrySide"];
        if (sideJson.is_string() && sideJson.get<std::string>() == "left") { settings.isEnteringFromLeft = true; }
        else if (!sideJson.is_string() || sideJson.get<std::string>() != "right") {
            Logger::Warning("Invalid StrafeEnemy entrySide; using right");
        }
    }
    settings.activationDistance = ReadEnemyNumber(settingsJson, "activationDistance", settings.activationDistance, 10.0f, 500.0f, "StrafeEnemy");
    settings.durationSeconds = ReadEnemyNumber(settingsJson, "durationSeconds", settings.durationSeconds, 1.0f, 20.0f, "StrafeEnemy");
    settings.forwardOffset = ReadEnemyNumber(settingsJson, "forwardOffset", settings.forwardOffset, 10.0f, 150.0f, "StrafeEnemy");
    settings.offsetX = ReadEnemyNumber(settingsJson, "offsetX", settings.offsetX, -80.0f, 80.0f, "StrafeEnemy");
    settings.offsetY = ReadEnemyNumber(settingsJson, "offsetY", settings.offsetY, -80.0f, 80.0f, "StrafeEnemy");
    settings.sideDistance = ReadEnemyNumber(settingsJson, "sideDistance", settings.sideDistance, 40.0f, 200.0f, "StrafeEnemy");
    settings.fireStartProgress = ReadEnemyNumber(settingsJson, "fireStartProgress", settings.fireStartProgress, 0.0f, 0.9f, "StrafeEnemy");
    settings.fireEndProgress = ReadEnemyNumber(settingsJson, "fireEndProgress", settings.fireEndProgress, 0.0f, 0.9f, "StrafeEnemy");
    if (settingsJson.contains("shotCount")) {
        const nlohmann::json& countJson = settingsJson["shotCount"];
        if (countJson.is_number_integer() && countJson.get<double>() >= 1.0 && countJson.get<double>() <= 8.0) {
            settings.shotCount = countJson.get<int>();
        } else { Logger::Warning("Invalid StrafeEnemy shotCount; using 3"); }
    }
    if (settings.fireEndProgress < settings.fireStartProgress ||
        (settings.shotCount > 1 && settings.fireEndProgress == settings.fireStartProgress)) {
        Logger::Warning("Invalid StrafeEnemy firing window; using 0.25..0.75");
        settings.fireStartProgress = 0.25f;
        settings.fireEndProgress = 0.75f;
    }
    return settings;
}

LevelData::PaceEnemySpawnData ReadPaceEnemySettings(const nlohmann::json& objectJson)
{
    LevelData::PaceEnemySpawnData settings;
    if (!objectJson.contains("paceEnemy")) { return settings; }
    const nlohmann::json& settingsJson = objectJson["paceEnemy"];
    if (!settingsJson.is_object()) {
        Logger::Warning("Invalid paceEnemy settings; using defaults");
        return settings;
    }
    if (settingsJson.contains("entrySide")) {
        const nlohmann::json& sideJson = settingsJson["entrySide"];
        if (sideJson.is_string() && sideJson.get<std::string>() == "left") {
            settings.isEnteringFromLeft = true;
        } else if (!sideJson.is_string() || sideJson.get<std::string>() != "right") {
            Logger::Warning("Invalid PaceEnemy entrySide; using right");
        }
    }
    settings.activationDistance = ReadPaceEnemyNumber(settingsJson, "activationDistance", settings.activationDistance, 10.0f, 500.0f);
    settings.entryDurationSeconds = ReadPaceEnemyNumber(settingsJson, "entryDurationSeconds", settings.entryDurationSeconds, 0.1f, 30.0f);
    settings.holdDurationSeconds = ReadPaceEnemyNumber(settingsJson, "holdDurationSeconds", settings.holdDurationSeconds, 0.0f, 120.0f);
    settings.exitDurationSeconds = ReadPaceEnemyNumber(settingsJson, "exitDurationSeconds", settings.exitDurationSeconds, 0.1f, 30.0f);
    settings.forwardOffset = ReadPaceEnemyNumber(settingsJson, "forwardOffset", settings.forwardOffset, 10.0f, 150.0f);
    settings.offsetX = ReadPaceEnemyNumber(settingsJson, "offsetX", settings.offsetX, -80.0f, 80.0f);
    settings.offsetY = ReadPaceEnemyNumber(settingsJson, "offsetY", settings.offsetY, -80.0f, 80.0f);
    settings.swayAmplitude = ReadPaceEnemyNumber(settingsJson, "swayAmplitude", settings.swayAmplitude, 0.0f, 40.0f);
    settings.swayPeriodSeconds = ReadPaceEnemyNumber(settingsJson, "swayPeriodSeconds", settings.swayPeriodSeconds, 0.1f, 60.0f);
    settings.sideTravelDistance = ReadPaceEnemyNumber(settingsJson, "sideTravelDistance", settings.sideTravelDistance, 40.0f, 500.0f);
    return settings;
}

LevelData::LoopEnemySpawnData ReadLoopEnemySettings(const nlohmann::json& objectJson)
{
    LevelData::LoopEnemySpawnData settings;
    if (!objectJson.contains("loopEnemy")) { return settings; }
    const nlohmann::json& settingsJson = objectJson["loopEnemy"];
    if (!settingsJson.is_object()) {
        Logger::Warning("Invalid loopEnemy settings; using defaults");
        return settings;
    }
    if (settingsJson.contains("count")) {
        const nlohmann::json& countJson = settingsJson["count"];
        if (countJson.is_number_integer() && countJson.get<double>() >= 1.0 && countJson.get<double>() <= 16.0) {
            settings.count = countJson.get<int>();
        } else { Logger::Warning("Invalid LoopEnemy count; expected integer 1..16, using 1"); }
    }
    if (settingsJson.contains("entrySide")) {
        const nlohmann::json& sideJson = settingsJson["entrySide"];
        if (sideJson.is_string() && sideJson.get<std::string>() == "left") {
            settings.isEnteringFromLeft = true;
        } else if (!sideJson.is_string() || sideJson.get<std::string>() != "right") {
            Logger::Warning("Invalid LoopEnemy entrySide; expected left or right, using right");
        }
    }
    if (settingsJson.contains("spawnIntervalSeconds")) {
        const nlohmann::json& intervalJson = settingsJson["spawnIntervalSeconds"];
        if (intervalJson.is_number() && std::isfinite(intervalJson.get<double>()) &&
            intervalJson.get<double>() >= 0.0 && intervalJson.get<double>() <= 10.0) {
            settings.spawnIntervalSeconds = intervalJson.get<float>();
        } else { Logger::Warning("Invalid LoopEnemy spawnIntervalSeconds; expected 0..10, using 0.6"); }
    }
    if (settingsJson.contains("activationDistance")) {
        const nlohmann::json& distanceJson = settingsJson["activationDistance"];
        if (distanceJson.is_number() && std::isfinite(distanceJson.get<double>()) &&
            distanceJson.get<double>() >= 40.0 && distanceJson.get<double>() <= 150.0) {
            settings.activationDistance = distanceJson.get<float>();
        } else { Logger::Warning("Invalid LoopEnemy activationDistance; expected 40..150, using 120"); }
    }
    return settings;
}

int ReadEnemyLevel(const nlohmann::json& objectJson)
{
    constexpr int kDefaultEnemyLevel = 2;
    if (!objectJson.contains("level")) {
        return kDefaultEnemyLevel;
    }
    const nlohmann::json& level = objectJson["level"];
    if (level.is_number_integer()) {
        const double levelValue = level.get<double>();
        if (levelValue >= 1.0 && levelValue <= 3.0) {
            return level.get<int>();
        }
    }
    Logger::Warning("Invalid enemy level for " + objectJson.value("name", std::string("unnamed")) +
        "; using level 2");
    return kDefaultEnemyLevel;
}
}

LevelData LevelDataLoader::Load(const std::string& filePath)
{
    std::ifstream file(filePath);

    if (file.fail()) {
        assert(false);
    }

    nlohmann::json jsonData;
    file >> jsonData;

    LevelData levelData;

    if (jsonData.contains("scene")) {
        for (const nlohmann::json& objectJson : jsonData["scene"]) {
            LoadObject(objectJson, levelData);
        }
    }

    return levelData;
}

void LevelDataLoader::LoadObject(const nlohmann::json& objectJson, LevelData& levelData)
{
    bool disabled = false;
    if (objectJson.contains("disabled")) {
        disabled = objectJson["disabled"].get<bool>();
    }

    if (disabled) {
        return;
    }

    if (!objectJson.contains("type")) {
        return;
    }

    std::string typeStr = objectJson["type"].get<std::string>();
    int enemyLevel = 2;
    LevelData::LoopEnemySpawnData loopEnemySettings;
    LevelData::PaceEnemySpawnData paceEnemySettings;
    LevelData::StrafeEnemySpawnData strafeEnemySettings;
    if (typeStr == "EnemySpawn") {
        enemyLevel = ReadEnemyLevel(objectJson);
        if (objectJson.value("file_name", std::string()) == "LoopEnemy") {
            loopEnemySettings = ReadLoopEnemySettings(objectJson);
        }
        if (objectJson.value("file_name", std::string()) == "PaceEnemy") {
            paceEnemySettings = ReadPaceEnemySettings(objectJson);
        }
        if (objectJson.value("file_name", std::string()) == "StrafeEnemy") {
            strafeEnemySettings = ReadStrafeEnemySettings(objectJson);
        }
    }
    if (typeStr == "PlayerSpawn") {
        LevelData::PlayerSpawnData spawnData {};

        if (objectJson.contains("transform")) {
            const nlohmann::json& transform = objectJson["transform"];

            if (transform.contains("translation")) {
                spawnData.translation.x = transform["translation"][0].get<float>();
                spawnData.translation.y = transform["translation"][2].get<float>();
                spawnData.translation.z = transform["translation"][1].get<float>();
            }

            if (transform.contains("rotation")) {
                float degreeToRadian = 3.1415926535f / 180.0f;
                spawnData.rotation.x = -transform["rotation"][0].get<float>() * degreeToRadian;
                spawnData.rotation.y = -transform["rotation"][2].get<float>() * degreeToRadian;
                spawnData.rotation.z = -transform["rotation"][1].get<float>() * degreeToRadian;
            }
        }

        levelData.playerSpawns.push_back(spawnData);
        return;
    }
    else if (typeStr == "EnemySpawn") {
        LevelData::EnemySpawnData spawnData {};
        spawnData.enemyLevel = enemyLevel;
        spawnData.loopEnemy = loopEnemySettings;
        spawnData.paceEnemy = paceEnemySettings;
        spawnData.strafeEnemy = strafeEnemySettings;

        if (objectJson.contains("file_name")) {
            spawnData.fileName = objectJson["file_name"].get<std::string>();
        }

        if (objectJson.contains("transform")) {
            const nlohmann::json& transform = objectJson["transform"];

            if (transform.contains("translation")) {
                spawnData.translation.x = transform["translation"][0].get<float>();
                spawnData.translation.y = transform["translation"][2].get<float>();
                spawnData.translation.z = transform["translation"][1].get<float>();
            }

            if (transform.contains("rotation")) {
                float degreeToRadian = 3.1415926535f / 180.0f;
                spawnData.rotation.x = -transform["rotation"][0].get<float>() * degreeToRadian;
                spawnData.rotation.y = -transform["rotation"][2].get<float>() * degreeToRadian;
                spawnData.rotation.z = -transform["rotation"][1].get<float>() * degreeToRadian;
            }
        }

        levelData.enemies.push_back(spawnData);
    }

    LevelData::ObjectData objectData;
    objectData.enemyLevel = enemyLevel;
    objectData.loopEnemy = loopEnemySettings;
    objectData.paceEnemy = paceEnemySettings;
    objectData.strafeEnemy = strafeEnemySettings;

    objectData.type = objectJson["type"].get<std::string>();

    if (objectJson.contains("name")) {
        objectData.name = objectJson["name"].get<std::string>();
    }

    if (objectJson.contains("file_name")) {
        objectData.fileName = objectJson["file_name"].get<std::string>();
    }

    if (objectJson.contains("transform")) {
        const nlohmann::json& transform = objectJson["transform"];

        if (transform.contains("translation")) {
            objectData.translation.x = transform["translation"][0].get<float>();
            objectData.translation.y = transform["translation"][2].get<float>();
            objectData.translation.z = transform["translation"][1].get<float>();
        }

        if (transform.contains("rotation")) {
            objectData.rotation.x = -transform["rotation"][0].get<float>();
            objectData.rotation.y = -transform["rotation"][2].get<float>();
            objectData.rotation.z = -transform["rotation"][1].get<float>();
        }

        if (transform.contains("scale")) {
            objectData.scale.x = transform["scale"][0].get<float>();
            objectData.scale.y = transform["scale"][2].get<float>();
            objectData.scale.z = transform["scale"][1].get<float>();
        }

        if (transform.contains("scaling")) {
            objectData.scale.x = transform["scaling"][0].get<float>();
            objectData.scale.y = transform["scaling"][2].get<float>();
            objectData.scale.z = transform["scaling"][1].get<float>();
        }
    }

    if (objectJson.contains("disabled")) {
        objectData.disabled = objectJson["disabled"].get<bool>();
    }

    if (objectJson.contains("collider")) {
        const nlohmann::json& collider = objectJson["collider"];
        objectData.collider.exists = true;
        if (collider.contains("type")) {
            objectData.collider.type = collider["type"].get<std::string>();
        }
        if (collider.contains("center")) {
            objectData.collider.center.x = collider["center"][0].get<float>();
            objectData.collider.center.y = collider["center"][2].get<float>();
            objectData.collider.center.z = collider["center"][1].get<float>();
        }
        if (collider.contains("size")) {
            objectData.collider.size.x = collider["size"][0].get<float>();
            objectData.collider.size.y = collider["size"][2].get<float>();
            objectData.collider.size.z = collider["size"][1].get<float>();
        }
    }

    if (objectJson.contains("colliders")) {
        for (const nlohmann::json& colliderJson : objectJson["colliders"]) {
            LevelData::ObjectData::ColliderData colliderData {};
            colliderData.exists = true;
            colliderData.type = colliderJson.at("type").get<std::string>();
            colliderData.center = {
                colliderJson.at("center")[0].get<float>(),
                colliderJson.at("center")[2].get<float>(),
                colliderJson.at("center")[1].get<float>()
            };
            colliderData.size = {
                colliderJson.at("size")[0].get<float>(),
                colliderJson.at("size")[2].get<float>(),
                colliderJson.at("size")[1].get<float>()
            };
            objectData.colliders.push_back(colliderData);
        }
    }

    if (objectJson.contains("trigger")) {
        const nlohmann::json& trigger = objectJson["trigger"];
        objectData.trigger.exists = true;
        if (trigger.contains("type")) {
            objectData.trigger.type = trigger["type"].get<std::string>();
        }
        if (trigger.contains("name")) {
            objectData.trigger.name = trigger["name"].get<std::string>();
        }
        if (trigger.contains("center")) {
            objectData.trigger.center.x = trigger["center"][0].get<float>();
            objectData.trigger.center.y = trigger["center"][2].get<float>();
            objectData.trigger.center.z = trigger["center"][1].get<float>();
        }
        if (trigger.contains("size")) {
            objectData.trigger.size.x = trigger["size"][0].get<float>();
            objectData.trigger.size.y = trigger["size"][2].get<float>();
            objectData.trigger.size.z = trigger["size"][1].get<float>();
        }
        if (trigger.contains("force")) {
            objectData.trigger.force.x = trigger["force"][0].get<float>();
            objectData.trigger.force.y = trigger["force"][2].get<float>();
            objectData.trigger.force.z = trigger["force"][1].get<float>();
        }
    }

    if (objectJson.contains("hazard")) {
        const nlohmann::json& hazard = objectJson["hazard"];
        objectData.hazard.exists = true;
        if (hazard.contains("type")) {
            objectData.hazard.type = hazard["type"].get<std::string>();
        }
        if (hazard.contains("damage")) {
            objectData.hazard.damage = hazard["damage"].get<int>();
        }
    }

    if (objectJson.contains("gimmick")) {
        const nlohmann::json& gimmick = objectJson["gimmick"];
        objectData.gimmick.exists = true;
        if (gimmick.contains("type")) {
            objectData.gimmick.type = gimmick["type"].get<std::string>();
        }
        if (gimmick.contains("speed")) {
            objectData.gimmick.speed = gimmick["speed"].get<float>();
        }
        if (gimmick.contains("range")) {
            objectData.gimmick.range.x = gimmick["range"][0].get<float>();
            objectData.gimmick.range.y = gimmick["range"][2].get<float>();
            objectData.gimmick.range.z = gimmick["range"][1].get<float>();
        }
        if (gimmick.contains("axis")) {
            objectData.gimmick.axis.x = gimmick["axis"][0].get<float>();
            objectData.gimmick.axis.y = gimmick["axis"][2].get<float>();
            objectData.gimmick.axis.z = gimmick["axis"][1].get<float>();
        }
    }

    if (objectJson.contains("destructible")) {
        const nlohmann::json& destructible = objectJson["destructible"];
        objectData.destructible.exists = true;
        if (destructible.contains("hp")) {
            objectData.destructible.hp = destructible["hp"].get<float>();
        }
    }

    if (objectJson.contains("camera_point")) {
        const nlohmann::json& cam = objectJson["camera_point"];
        objectData.cameraPoint.exists = true;
        if (cam.contains("name")) {
            objectData.cameraPoint.name = cam["name"].get<std::string>();
        }
        if (cam.contains("target")) {
            objectData.cameraPoint.target.x = cam["target"][0].get<float>();
            objectData.cameraPoint.target.y = cam["target"][2].get<float>();
            objectData.cameraPoint.target.z = cam["target"][1].get<float>();
        }
        if (cam.contains("move_time")) {
            objectData.cameraPoint.moveTime = cam["move_time"].get<float>();
        }
    }

    if (objectJson.contains("camera_fov_point")) {
        const nlohmann::json& fovPoint = objectJson["camera_fov_point"];
        objectData.cameraFovPoint.exists = true;
        if (fovPoint.contains("fov")) {
            objectData.cameraFovPoint.fov = fovPoint["fov"].get<float>();
        }
        if (fovPoint.contains("time")) {
            objectData.cameraFovPoint.time = fovPoint["time"].get<float>();
        }
    }

    if (objectJson.contains("patrol_route")) {
        const nlohmann::json& patrol = objectJson["patrol_route"];
        objectData.patrolRoute.exists = true;
        if (patrol.contains("waypoints")) {
            for (const auto& wpJson : patrol["waypoints"]) {
                Vector3 wp;
                wp.x = wpJson[0].get<float>();
                wp.y = wpJson[2].get<float>();
                wp.z = wpJson[1].get<float>();
                objectData.patrolRoute.waypoints.push_back(wp);
            }
        }
    }

    if (objectJson.contains("terrain")) {
        const nlohmann::json& terr = objectJson["terrain"];
        objectData.terrain.exists = true;
        if (terr.contains("file")) {
            objectData.terrain.file = terr["file"].get<std::string>();
        }
        if (terr.contains("width")) {
            objectData.terrain.width = terr["width"].get<float>();
        }
        if (terr.contains("height")) {
            objectData.terrain.height = terr["height"].get<float>();
        }
    }

    if (objectJson.contains("mesh_sync")) {
        objectData.meshSync = objectJson["mesh_sync"].get<bool>();
    }

    levelData.objects.push_back(objectData);

    if (objectJson.contains("children")) {
        for (const nlohmann::json& childJson : objectJson["children"]) {
            LoadObject(childJson, levelData);
        }
    }
}
