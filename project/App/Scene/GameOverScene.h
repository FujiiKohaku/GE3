#pragma once
#include "BaseScene.h"

#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include "SceneManager.h"

#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include <string>
class GameOverScene : public BaseScene {

public:
    explicit GameOverScene(const std::string& stageId = "stage01")
        : stageId_(stageId) {}
    void Initialize()override;

    void Finalize() override;

    void Update() override;

    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    std::string stageId_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> titleObj_;
    std::unique_ptr<Sprite> titleSprite_;
    std::unique_ptr<Text> retryGuideText_;
    std::unique_ptr<Sprite> creditSprite_;
};
