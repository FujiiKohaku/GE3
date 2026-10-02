#pragma once

#include <memory>

class OffscreenRenderer;
class PostEffectManager;
class SceneManager;
class ShadowMapRenderer;
class LocalShadowRenderer;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Initialize();
    void Update();
    void DrawImGui();
    void Draw(SceneManager* sceneManager);
    PostEffectManager* GetPostEffectManager() const { return postEffectManager_.get(); }

private:
    std::unique_ptr<LocalShadowRenderer> localShadowRenderer_;
    std::unique_ptr<ShadowMapRenderer> shadowRenderer_;
    std::unique_ptr<OffscreenRenderer> offscreenRenderer_;
    std::unique_ptr<PostEffectManager> postEffectManager_;
};
