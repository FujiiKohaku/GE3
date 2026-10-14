#pragma once
#include "App/Scene/Common/BaseScene.h"

#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include "App/Scene/Common/SceneManager.h"

#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
class ClearScene : public BaseScene {
public:
    void Initialize() override;

    void Finalize() override;

    void Update() override;

    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;
    private:
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> titleObj_;
    std::unique_ptr<Sprite> titleSprite_;
    std::unique_ptr<Sprite> creditSprite_;
    std::unique_ptr<Text> returnGuideText_;
};
