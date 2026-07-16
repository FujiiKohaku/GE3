#pragma once
#include "../math/EngineStruct.h"
#include <string>
#include <vector>

struct LevelData {

    struct ObjectData {

        std::string name;
        std::string type;
        std::string fileName;

        Vector3 translation;
        Vector3 rotation;
        Vector3 scale;

        bool disabled = false;
        
        struct ScoreItemData {
            bool exists = false;
            int score = 0;
        } scoreItem;

        struct ColliderData {
            bool exists = false;
            std::string type;
            Vector3 center;
            Vector3 size;
        } collider;

        struct TriggerData {
            bool exists = false;
            std::string type;
            std::string name;
            Vector3 center;
            Vector3 size;
        } trigger;

        struct GimmickData {
            bool exists = false;
            std::string type;
            float speed = 0.0f;
            Vector3 range;
        } gimmick;

        struct CameraPointData {
            bool exists = false;
            std::string name;
            Vector3 target;
            float moveTime = 0.0f;
        } cameraPoint;

        struct CameraFovPointData {
            bool exists = false;
            float fov = 0.0f;
            float time = 0.0f;
        } cameraFovPoint;

        struct PatrolRouteData {
            bool exists = false;
            std::vector<Vector3> waypoints;
        } patrolRoute;

        struct TerrainData {
            bool exists = false;
            std::string file;
            float width = 0.0f;
            float height = 0.0f;
        } terrain;

        bool meshSync = false;
    };

    struct PlayerSpawnData {
        Vector3 translation;
        Vector3 rotation;
    };

    struct EnemySpawnData {
        std::string fileName;
        Vector3 translation;
        Vector3 rotation;
    };

    std::vector<ObjectData> objects;
    std::vector<PlayerSpawnData> playerSpawns;
    std::vector<EnemySpawnData> enemies;
};