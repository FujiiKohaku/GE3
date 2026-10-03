#pragma once

#include <memory>
#include <cstdint>

class OffscreenRenderer;
class PostEffectManager;
class SceneManager;
class ShadowMapRenderer;
class LocalShadowRenderer;
class MotionVectorRenderer;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Initialize();
    void Update();
    void DrawImGui();
    void Draw(SceneManager* sceneManager);
    PostEffectManager* GetPostEffectManager() const { return postEffectManager_.get(); }
    MotionVectorRenderer* GetMotionVectorRenderer() const { return motionVectorRenderer_.get(); }

private:
    std::unique_ptr<MotionVectorRenderer> motionVectorRenderer_;
    uint64_t motionSceneRevision_ = 0;
    std::unique_ptr<LocalShadowRenderer> localShadowRenderer_;
    std::unique_ptr<ShadowMapRenderer> shadowRenderer_;
    std::unique_ptr<OffscreenRenderer> offscreenRenderer_;
    std::unique_ptr<PostEffectManager> postEffectManager_;
};
