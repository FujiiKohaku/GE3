#pragma once

#if defined(ENABLE_DEVELOPMENT_TOOLS)

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class GamePlayScene;
class TestScene1;
class PostEffectManager;

// A small loopback-only HTTP server. Poll runs on the game thread, so scene
// settings are never read or changed concurrently with gameplay.
class DevelopmentWebPanel {
public:
    static DevelopmentWebPanel& GetInstance();

    bool Start();
    void Stop();
    void Poll();
    void SetScene(GamePlayScene* scene);
    void SetTestScene(TestScene1* scene);
    void SetPostEffectManager(PostEffectManager* manager) { postEffectManager_ = manager; }
    void SetLegacyUiVisible(bool visible) { legacyUiVisible_ = visible; }
    bool IsLegacyUiVisible() const { return legacyUiVisible_; }

private:
    struct Client {
        std::uintptr_t socket = ~std::uintptr_t { 0 };
        std::string request;
        std::chrono::steady_clock::time_point acceptedAt;
    };

    void Respond(Client& client);

    std::uintptr_t listener_ = ~std::uintptr_t { 0 };
    std::vector<Client> clients_;
    GamePlayScene* scene_ = nullptr;
    TestScene1* testScene_ = nullptr;
    PostEffectManager* postEffectManager_ = nullptr;
    std::uint16_t port_ = 0;
    std::string token_;
    bool winsockStarted_ = false;
    bool legacyUiVisible_ = false;
    bool browserLaunchPending_ = false;
    std::chrono::steady_clock::time_point browserLaunchAt_ {};
};

#endif
