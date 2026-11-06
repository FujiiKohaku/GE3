#pragma once
#include "../math/EngineStruct.h"
#include <string>
#include <vector>

struct LevelData {

    struct DashEnemySpawnData {
        float activationDistance = 160.0f;
        float forwardOffset = 100.0f;
        float entrySeconds = 0.5f;
        float trackingSeconds = 0.55f;
        float lockSeconds = 0.45f;
        float dashSpeed = 220.0f;
        float dashDistance = 160.0f;
        float recoverySeconds = 0.65f;
        float exitSeconds = 0.4f;
        float hp = 30.0f;
        int damage = 2;
    };

    struct StrafeEnemySpawnData {
        bool isEnteringFromLeft = false;
        float activationDistance = 120.0f;
        float durationSeconds = 4.0f;
        float forwardOffset = 90.0f;
        float offsetX = 0.0f;
        float offsetY = 4.0f;
        float sideDistance = 80.0f;
        int shotCount = 3;
        float fireStartProgress = 0.25f;
        float fireEndProgress = 0.75f;
    };

    struct PaceEnemySpawnData {
        bool isEnteringFromLeft = false;
        float activationDistance = 120.0f;
        float entryDurationSeconds = 1.5f;
        float holdDurationSeconds = 6.0f;
        float exitDurationSeconds = 1.5f;
        float forwardOffset = 100.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float swayAmplitude = 12.0f;
        float swayPeriodSeconds = 4.0f;
        float sideTravelDistance = 80.0f;
    };

    struct LoopEnemySpawnData {
        int count = 1;
        bool isEnteringFromLeft = false;
        float spawnIntervalSeconds = 0.6f;
        float activationDistance = 120.0f;
    };

    struct ObjectData {

        std::string name;
        std::string type;
        std::string fileName;

        Vector3 translation;
        Vector3 rotation;
        Vector3 scale;

        bool disabled = false;
        int enemyLevel = 2;
        LoopEnemySpawnData loopEnemy;
        PaceEnemySpawnData paceEnemy;
        StrafeEnemySpawnData strafeEnemy;
        DashEnemySpawnData dashEnemy;
        
        struct ScoreItemData {
            bool exists = false;
            int score = 0;
        } scoreItem;

        struct ColliderData {
            bool exists = false;
            std::string type;
            Vector3 center;
            Vector3 size;
            // サイズ・中心は拡大率適用済み。trueなら中心のオフセットもモデルと回転する。
            bool rotateOffset = false;
        } collider;
        // Additional boxes use the same scaled offsets/sizes and rotateOffset option as collider.
        std::vector<ColliderData> colliders;

        struct TriggerData {
            bool exists = false;
            std::string type;
            std::string name;
            Vector3 center;
            Vector3 size;
            Vector3 force;
        } trigger;

        struct HazardData {
            bool exists = false;
            std::string type;
            int damage = 1;
        } hazard;

        struct GimmickData {
            bool exists = false;
            std::string type;
            float speed = 0.0f;
            Vector3 range;
            Vector3 axis = { 0.0f, 1.0f, 0.0f };
        } gimmick;

        struct DestructibleData {
            bool exists = false;
            float hp = 1.0f;
        } destructible;

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
        int enemyLevel = 2;
        LoopEnemySpawnData loopEnemy;
        PaceEnemySpawnData paceEnemy;
        StrafeEnemySpawnData strafeEnemy;
        DashEnemySpawnData dashEnemy;
        Vector3 translation;
        Vector3 rotation;
    };

    std::vector<ObjectData> objects;
    std::vector<PlayerSpawnData> playerSpawns;
    std::vector<EnemySpawnData> enemies;
};
