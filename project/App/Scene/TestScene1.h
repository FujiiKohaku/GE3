#pragma once
#include "BaseScene.h"
#include "SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/3D/Object3d.h"
#include <memory>

class TestScene1 : public BaseScene {
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
    std::unique_ptr<Object3d> floorObj_;
};
