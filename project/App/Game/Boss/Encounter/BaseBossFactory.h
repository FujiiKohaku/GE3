#pragma once
#include "App/Game/Boss/Common/BaseBoss.h"
#include "App/Game/Boss/Presentation/BaseBossPresentation.h"
#include <memory>
#include <string_view>

class Model;
class Player;
class Camera;

struct BossSpawnContext {
    Model* fearWormModel = nullptr;
    Model* angerBlockModel = nullptr;
    Model* enemyBulletModel = nullptr;
    Player* player = nullptr;
    Camera* camera = nullptr;
};

class BaseBossFactory {
public:
    virtual ~BaseBossFactory() = default;
    virtual std::string_view GetType() const = 0;
    virtual std::unique_ptr<BaseBoss> Create(const BossSpawnContext& context) const = 0;
    virtual std::unique_ptr<BaseBossPresentation> CreatePresentation() const { return nullptr; }
};

// Noneは生成なし。未登録の名前は従来どおりFearWormへフォールバックする。
const BaseBossFactory* FindBossFactory(std::string_view type);
