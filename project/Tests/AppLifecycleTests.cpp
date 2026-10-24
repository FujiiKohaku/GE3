#include "App/Scene/Gameplay/GamePlayScene.h"
#include "App/Game/Player/Combat/Bullet/Types/HomingMissileBullet.h"
#include "App/Game/Player/Combat/Weapon/PlayerWeaponController.h"
#include "App/Game/Stage/StageCatalog.h"
#include "Engine/Time/TimeManager.h"
#include "externals/json.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

class LifecycleTestMissile final : public HomingMissileBullet {
public:
    void AdvanceMovement() { Move(); }
};

bool RunHomingInvalidationTests()
{
    PlayerHomingLock lock;
    auto target = std::make_unique<BaseEnemy>();
    lock.SetTargets({ target.get() });
    const auto activeTargets = lock.GetTargets();
    LifecycleTestMissile missile;
    missile.SetTranslate({ 0.0f, 0.0f, 0.0f });
    missile.SetVelocity({ 0.0f, 0.0f, 1.0f });
    missile.SetTarget(target.get(), activeTargets);
    lock.ClearHomingTargets();
    if (!activeTargets->empty() || !lock.GetLockedTargets().empty()) { return false; }
    target.reset();
    // 描画を初期化せず実際の追尾処理を通し、削除済み対象を参照しないことを確認する。
    missile.AdvanceMovement();
    if (!missile.IsAlive()) { return false; }
    target = std::make_unique<BaseEnemy>();
    lock.SetTargets({ target.get() });
    if (activeTargets->size() != 1 || activeTargets->front() != target.get()) { return false; }
    lock.ClearHomingTargets();
    return true;
}

bool RunGameplayLifecycleTests()
{
    SceneManager* manager = SceneManager::GetInstance();
    const std::vector<PostEffectInfo> original {
        { PostEffectType::GaussianFilter, PostEffectStage::AfterParticle, false, 7 },
        { PostEffectType::Dissolve, PostEffectStage::BeforeParticle, true, 3 }
    };
    manager->ApplyPostEffectChain(original);
    GamePlayScene scene;
    scene.SetPaused(true);
    if (!scene.isPaused_ || !manager->IsPauseMenuOpen()) { return false; }
    bool hasActiveGaussian = false;
    bool hasActiveGrayScale = false;
    for (const PostEffectInfo& effect : manager->GetPostEffects()) {
        if (effect.type == PostEffectType::GaussianFilter && effect.enabled) { hasActiveGaussian = true; }
        if (effect.type == PostEffectType::GrayScale && effect.enabled) { hasActiveGrayScale = true; }
    }
    if (!hasActiveGaussian || !hasActiveGrayScale) { return false; }
    // コントローラーの切断通知などで重ねてポーズしても、保存内容を上書きしない。
    scene.SetPaused(true);
    scene.SetPaused(false);
    const auto& restored = manager->GetPostEffects();
    if (scene.isPaused_ || manager->IsPauseMenuOpen() || restored.size() != original.size()) { return false; }
    for (size_t index = 0; index < original.size(); ++index) {
        if (restored[index].type != original[index].type || restored[index].stage != original[index].stage ||
            restored[index].enabled != original[index].enabled || restored[index].priority != original[index].priority) {
            return false;
        }
    }
    // 通常更新や描画に進まず再開できるので、死亡演出中もポーズ効果が残らない。
    manager->ClearPostEffects();
    scene.SetPaused(true);
    scene.SetPaused(false);
    if (!manager->GetPostEffects().empty()) { return false; }
    scene.wasBoostingForSonicBoom_ = true;
    GamePlayScene nextScene;
    if (nextScene.wasBoostingForSonicBoom_) { return false; }
    scene.ResetGameplayPostEffects();
    return !scene.wasBoostingForSonicBoom_;
}

class StageCatalogFixture {
public:
    StageCatalogFixture() {
        const auto uniqueId = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() / ("KohakuStageLifecycleTests-" + std::to_string(uniqueId));
        std::filesystem::create_directory(directory_);
    }
    ~StageCatalogFixture() {
        std::error_code error;
        for (const auto& file : files_) { std::filesystem::remove(file, error); }
        std::filesystem::remove(directory_, error);
    }
    std::string Write(const std::string& name, const nlohmann::json& data) {
        const auto path = directory_ / name;
        std::ofstream stream(path);
        stream << data.dump();
        files_.push_back(path);
        return path.string();
    }
private:
    std::filesystem::path directory_;
    std::vector<std::filesystem::path> files_;
};

bool RunStageCatalogFailureTests()
{
    StageCatalogFixture fixture;
    const std::string layoutPath = fixture.Write("layout.json", nlohmann::json::object());
    const std::string firstPath = fixture.Write("first.json", { { "id", "first" }, { "layout", layoutPath } });
    const std::string secondPath = fixture.Write("second.json", { { "id", "second" }, { "layout", layoutPath } });
    const std::string validPath = fixture.Write("valid.json", {
        { "stages", { { { "settings", firstPath } }, { { "settings", secondPath } } } }
    });
    StageCatalog* catalog = StageCatalog::GetInstance();
    if (!catalog->Load(validPath) || catalog->GetStages().size() != 2) { return false; }
    const std::string missingPath = fixture.Write("missing.json", {
        { "stages", { { { "settings", firstPath } }, { { "settings", secondPath + ".missing" } } } }
    });
    if (catalog->Load(missingPath) || !catalog->GetStages().empty() || catalog->Find("first") != nullptr) { return false; }
    const std::string duplicatePath = fixture.Write("duplicate.json", {
        { "stages", { { { "settings", firstPath } }, { { "settings", firstPath } } } }
    });
    if (catalog->Load(duplicatePath) || !catalog->GetStages().empty()) { return false; }
    const std::string invalidPath = fixture.Write("invalid.json", {
        { "id", "invalid" }, { "layout", layoutPath }, { "rail", { { "length", -1.0f } } }
    });
    const std::string invalidCatalogPath = fixture.Write("invalid-catalog.json", {
        { "stages", { { { "settings", firstPath } }, { { "settings", invalidPath } } } }
    });
    if (catalog->Load(invalidCatalogPath) || !catalog->GetStages().empty()) { return false; }
    const std::string wrongTypePath = fixture.Write("wrong-type.json", {
        { "stages", { { { "settings", firstPath } }, { { "settings", 123 } } } }
    });
    if (catalog->Load(wrongTypePath) || !catalog->GetStages().empty()) { return false; }
    if (!catalog->Load(validPath) || catalog->GetStages().size() != 2) { return false; }
    // 通常の設定を復元し、テスト用パスをシングルトンに残さない。
    return catalog->Load();
}

int main()
{
    TimeManager::GetInstance()->Initialize();
    if (!RunHomingInvalidationTests() || !RunGameplayLifecycleTests() || !RunStageCatalogFailureTests()) {
        std::puts("FAILED: App lifecycle regression tests");
        return 1;
    }
    std::puts("PASS: homing target invalidation, pause effect restoration, per-scene boost state and atomic stage loading");
    return 0;
}
