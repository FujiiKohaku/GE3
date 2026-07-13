#pragma once

#include <memory>

class OffscreenRenderer;
class PostEffectManager;
class SceneManager;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Initialize();
    void Update();
    void DrawImGui();
    void Draw(SceneManager* sceneManager);

private:
    std::unique_ptr<OffscreenRenderer> offscreenRenderer_;
    std::unique_ptr<PostEffectManager> postEffectManager_;
};
