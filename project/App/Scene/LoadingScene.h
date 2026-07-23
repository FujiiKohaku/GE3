#pragma once

#include "BaseScene.h"
#include "Engine/2D/Sprite.h"

#include <memory>

class LoadingScene : public BaseScene {
public:
    LoadingScene(std::unique_ptr<BaseScene> nextScene)
        : nextScene_(std::move(nextScene)) {}

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    std::unique_ptr<BaseScene> nextScene_;
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> progressBackSprite_;
    std::unique_ptr<Sprite> progressSprite_;
    bool hasDrawnFirstFrame_ = false;
    bool gameResourcesInitialized_ = false;
};
