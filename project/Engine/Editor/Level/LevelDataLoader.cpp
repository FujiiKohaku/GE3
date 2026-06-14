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
    if (!objectJson.contains("type")) {
        return;
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

    levelData.objects.push_back(objectData);

    if (objectJson.contains("children")) {
        for (const nlohmann::json& childJson : objectJson["children"]) {
            LoadObject(childJson, levelData);
        }
    }
}