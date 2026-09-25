#pragma once

#include "BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/OceanSurface.h"
#include "Engine/Camera/Camera.h"
#include <memory>
#include <string>
#include <vector>

class LaunchBaseScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    Object3d* AddObject(
        const std::string& modelPath,
        const Vector3& position,
        const Vector3& scale = { 1.0f, 1.0f, 1.0f },
        const Vector3& rotation = { 0.0f, 0.0f, 0.0f });
    void UpdateAircraft(float deltaTime);
    void UpdateCamera();

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<OceanSurface> oceanSurface_;
    std::unique_ptr<Object3d> aircraft_;
    std::vector<std::unique_ptr<Object3d>> baseObjects_;
    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> controlsText_;
    std::unique_ptr<Text> speedText_;

    Vector3 aircraftPosition_ { 0.0f, 3.0f, -5.0f };
    float aircraftYaw_ = 0.0f;
    float aircraftPitch_ = 0.0f;
    float aircraftBank_ = 0.0f;
    float flightSpeed_ = 0.0f;
};
