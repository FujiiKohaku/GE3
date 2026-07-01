#pragma once

#include <memory>

class CopyImageRenderer;
class FogManager;
class FogRenderer;
class OffscreenRenderer;
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
    std::unique_ptr<CopyImageRenderer> copyImageRenderer_;
    std::unique_ptr<FogManager> fogManager_;
    std::unique_ptr<FogRenderer> fogRenderer_;
};
