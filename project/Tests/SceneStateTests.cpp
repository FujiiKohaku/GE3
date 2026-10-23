#include "App/Scene/Common/SceneManager.h"
#include "Engine/Time/TimeManager.h"
#include "TitleStateTests.h"
#include <array>
#include <cstdio>
#include <memory>
#include <vector>

struct SceneLifecycleProbe {
    int sharedOwner = 0;
    std::array<int, 4> finalizeCounts {};
    std::array<int, 4> destructionCounts {};
    std::vector<int> events;
};

class LifecycleTestScene final : public BaseScene {
public:
    LifecycleTestScene(int sceneId, SceneLifecycleProbe& probe)
        : sceneId_(sceneId), probe_(probe) {}
    ~LifecycleTestScene() override {
        probe_.destructionCounts[sceneId_]++;
        probe_.events.push_back(sceneId_ * 10 + 4);
    }
    void Initialize() override {
        probe_.events.push_back(sceneId_ * 10 + 1);
        probe_.sharedOwner = sceneId_;
        SceneManager::GetInstance()->SetPostEffectParameters(PostEffectType::ToneMap,
            { static_cast<float>(sceneId_), 0.0f, 0.0f });
    }
    void Finalize() override {
        probe_.events.push_back(sceneId_ * 10 + 3);
        probe_.finalizeCounts[sceneId_]++;
        probe_.sharedOwner = 0;
        SceneManager::GetInstance()->SetPostEffectParameters(PostEffectType::ToneMap, {});
    }
    void Update() override { probe_.events.push_back(sceneId_ * 10 + 2); }
    void Draw2D() override {}
    void Draw3D() override {}
    void DrawParticle() override {}
    void DrawImGui() override {}

private:
    int sceneId_;
    SceneLifecycleProbe& probe_;
};

bool RunSceneLifecycleTests()
{
    SceneManager* manager = SceneManager::GetInstance();
    SceneLifecycleProbe probe;
    bool isValid = true;
    manager->SetNextScene(std::make_unique<LifecycleTestScene>(1, probe));
    manager->Update();
    if (probe.events != std::vector<int> { 11, 12 } || probe.sharedOwner != 1) {
        isValid = false;
    }
    manager->SetNextScene(std::make_unique<LifecycleTestScene>(2, probe));
    manager->Update();
    // 終了処理は次の初期化より前。本体はこのフレーム中、まだ生存する。
    if (probe.events != std::vector<int> { 11, 12, 13, 21, 22 }
        || probe.sharedOwner != 2 || probe.finalizeCounts[1] != 1
        || probe.destructionCounts[1] != 0) {
        isValid = false;
    }
    manager->Update();
    const Vector3* parameters = manager->FindPostEffectParameters(PostEffectType::ToneMap);
    if (probe.events != std::vector<int> { 11, 12, 13, 21, 22, 14, 22 }
        || probe.sharedOwner != 2 || probe.finalizeCounts[1] != 1
        || parameters == nullptr || parameters->x != 2.0f) {
        isValid = false;
    }
    manager->SetNextScene(std::make_unique<LifecycleTestScene>(3, probe));
    manager->Update();
    if (probe.sharedOwner != 3 || probe.finalizeCounts[2] != 1 || probe.destructionCounts[2] != 0) {
        isValid = false;
    }
    // 遅延破棄を待つシーンがあっても、終了処理を二重に呼ばない。
    manager->Finalize();
    manager->Finalize();
    for (int sceneId = 1; sceneId <= 3; sceneId++) {
        if (probe.finalizeCounts[sceneId] != 1 || probe.destructionCounts[sceneId] != 1) {
            isValid = false;
        }
    }
    if (probe.sharedOwner != 0) {
        isValid = false;
    }
    return isValid;
}

int main()
{
    TimeManager::GetInstance()->Initialize();
    if (!RunSceneLifecycleTests()) {
        std::puts("Scene lifecycle regression tests failed.");
        return 1;
    }
    if (!RunTitleStateTests()) {
        std::puts("Title State regression tests failed.");
        return 1;
    }
    std::puts("Scene lifecycle: finalize-before-initialize, deferred destruction, shared settings and shutdown passed.");
    std::puts("Title State: dispatch, transitions and per-scene reset passed.");
    return 0;
}
