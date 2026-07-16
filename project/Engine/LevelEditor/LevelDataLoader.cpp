#include "LevelDataLoader.h"

#include <cassert>
#include <fstream>

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