#include "BaseBossFactory.h"
#include "App/Game/Boss/AngerBlockBoss/AngerBlockBoss.h"
#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"
#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include <array>

class AngerBlockBossFactory final : public BaseBossFactory {
public:
    std::string_view GetType() const override { return "AngerBlock"; }
    std::unique_ptr<BaseBoss> Create(const BossSpawnContext& context) const override {
        auto boss = std::make_unique<AngerBlockBoss>();
        boss->Initialize(context.angerBlockModel, context.enemyBulletModel, context.player);
        return boss;
    }
};

class JellyfishBossFactory final : public BaseBossFactory {
public:
    std::unique_ptr<BaseBossPresentation> CreatePresentation() const override {
        return CreateJellyfishPresentation();
    }
    std::string_view GetType() const override { return "IceJellyfish"; }
    std::unique_ptr<BaseBoss> Create(const BossSpawnContext& context) const override {
        auto boss = std::make_unique<IceJellyfish>();
        boss->Initialize(context.camera, context.enemyBulletModel, context.player);
        return boss;
    }
};

class FearWormBossFactory final : public BaseBossFactory {
public:
    std::string_view GetType() const override { return "FearWorm"; }
    std::unique_ptr<BaseBoss> Create(const BossSpawnContext& context) const override {
        auto boss = std::make_unique<FearWormEnemy>();
        boss->Initialize(context.fearWormModel, context.enemyBulletModel, context.player);
        return boss;
    }
};

const BaseBossFactory* FindBossFactory(std::string_view type)
{
    if (type == "None") return nullptr;
    static const AngerBlockBossFactory anger;
    static const JellyfishBossFactory jellyfish;
    static const FearWormBossFactory fear;
    static const std::array<const BaseBossFactory*, 3> factories { &anger, &jellyfish, &fear };
    for (const BaseBossFactory* factory : factories) {
        if (factory->GetType() == type) return factory;
    }
    return &fear;
}
