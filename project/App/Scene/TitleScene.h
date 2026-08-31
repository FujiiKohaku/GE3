#pragma once

#include "BaseScene.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/OceanSurface.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Rail/Rail.h"
#include <memory>
#include <vector>

class TitleScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    void UpdatePlayerShowcase(float deltaTime);

    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Text> logoText_;
    std::unique_ptr<Text> pushToStartText_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> playerObject_;
    std::unique_ptr<OceanSurface> oceanSurface_;
    std::unique_ptr<Rail> showcaseRail_;
    std::vector<std::unique_ptr<Object3d>> obstacleObjects_;
    float promptAnimationTime_ = 0.0f;
    float showcaseTime_ = 0.0f;
};
