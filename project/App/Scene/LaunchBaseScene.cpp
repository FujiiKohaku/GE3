#include "LaunchBaseScene.h"

#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Light/LightManager.h"
#include "Engine/Time/TimeManager.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace {
constexpr const char* kFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kAircraftModel =
    "Characters/Player/AirPlane/AirPlane.obj";
constexpr const char* kLaunchBase = "Environment/LaunchBase/";
constexpr const char* kIslandModel =
    "Environment/Ocean/tropical_island.obj";
}

void LaunchBaseScene::Initialize()
{
    ShowCursor(FALSE);
    ClipCursor(nullptr);

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    Object3dManager::GetInstance()->Initialize(dxCommon);
    LightManager::GetInstance()->Initialize(dxCommon);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->SetFovY(0.62f);
    camera_->SetFarClip(3000.0f);
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    oceanSurface_ = std::make_unique<OceanSurface>();
    oceanSurface_->Initialize(camera_.get(), 1800.0f, 1800.0f, -8.0f);
    oceanSurface_->SetWaveAmplitude(0.65f);
    oceanSurface_->SetWaveFrequency(0.085f);

    // 格納庫前の作業デッキ。
    for (int xIndex = -2; xIndex <= 2; ++xIndex) {
        for (int zIndex = -2; zIndex <= 1; ++zIndex) {
            AddObject(
                std::string(kLaunchBase) + "deck_tile.obj",
                { static_cast<float>(xIndex) * 20.0f, -0.5f,
                  static_cast<float>(zIndex) * 20.0f - 20.0f });
        }
    }

    AddObject(std::string(kLaunchBase) + "hangar_frame.obj", { 0.0f, 0.0f, -30.0f });
    AddObject(std::string(kLaunchBase) + "launch_pad.obj", { 0.0f, 0.0f, -5.0f });

    // 海へ伸びる滑走路。
    for (int index = 0; index < 7; ++index) {
        const float segmentZ = 30.0f + static_cast<float>(index) * 40.0f;
        AddObject(
            std::string(kLaunchBase) + "runway_segment.obj",
            { 0.0f, 0.0f, segmentZ });

        // 滑走路下面の縦梁と設備配管。
        AddObject(std::string(kLaunchBase) + "underdeck_beam.obj", { -5.8f, 0.0f, segmentZ });
        AddObject(std::string(kLaunchBase) + "underdeck_beam.obj", { 5.8f, 0.0f, segmentZ });
        AddObject(std::string(kLaunchBase) + "pipe_module.obj", { 8.7f, -2.4f, segmentZ });

        // 1区画おきに海中支柱とX字補強を入れ、空中に浮いて見えないようにする。
        if (index % 2 == 0) {
            AddObject(std::string(kLaunchBase) + "support_pylon.obj", { -7.4f, 0.0f, segmentZ });
            AddObject(std::string(kLaunchBase) + "support_pylon.obj", { 7.4f, 0.0f, segmentZ });
            AddObject(std::string(kLaunchBase) + "cross_brace.obj", { 0.0f, 0.0f, segmentZ });
            AddObject(std::string(kLaunchBase) + "maintenance_catwalk.obj", { -10.4f, -3.0f, segmentZ });
            AddObject(std::string(kLaunchBase) + "service_ladder.obj", { 7.4f, -0.2f, segmentZ - 1.9f });
        }
    }

    // 格納庫デッキ側の支持構造。
    for (float x : { -24.0f, 24.0f }) {
        for (float z : { -35.0f, -5.0f }) {
            AddObject(std::string(kLaunchBase) + "support_pylon.obj", { x, 0.0f, z });
        }
    }
    AddObject(std::string(kLaunchBase) + "underdeck_beam.obj", { -19.0f, 0.0f, -20.0f });
    AddObject(std::string(kLaunchBase) + "underdeck_beam.obj", { 19.0f, 0.0f, -20.0f });
    AddObject(std::string(kLaunchBase) + "cross_brace.obj", { 0.0f, 0.0f, -35.0f }, { 1.65f, 1.0f, 1.0f });
    AddObject(std::string(kLaunchBase) + "cross_brace.obj", { 0.0f, 0.0f, -5.0f }, { 1.65f, 1.0f, 1.0f });

    AddObject(std::string(kLaunchBase) + "control_tower.obj", { -43.0f, 0.0f, -18.0f });
    AddObject(std::string(kLaunchBase) + "cargo_container.obj", { -35.0f, 0.0f, 5.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.35f, 0.0f });
    AddObject(std::string(kLaunchBase) + "cargo_container.obj", { -39.0f, 0.0f, 11.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, -0.15f, 0.0f });
    AddObject(std::string(kLaunchBase) + "cargo_container.obj", { 37.0f, 0.0f, -8.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, -0.55f, 0.0f });

    // 格納庫左側は給油・部品保管、右側は整備・安全設備としてまとめる。
    AddObject(std::string(kLaunchBase) + "fuel_station.obj", { -42.0f, 0.0f, -8.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.35f, 0.0f });
    AddObject(std::string(kLaunchBase) + "tool_crate.obj", { -36.0f, 0.0f, -1.5f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, -0.2f, 0.0f });
    AddObject(std::string(kLaunchBase) + "tool_crate.obj", { -40.0f, 0.0f, 2.0f }, { 0.85f, 0.85f, 0.85f }, { 0.0f, 0.45f, 0.0f });
    AddObject(std::string(kLaunchBase) + "maintenance_tug.obj", { 31.5f, 0.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, -0.3f, 0.0f });
    AddObject(std::string(kLaunchBase) + "tool_crate.obj", { 39.0f, 0.0f, 2.5f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.25f, 0.0f });
    AddObject(std::string(kLaunchBase) + "fire_station.obj", { 43.0f, 0.0f, -5.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, -0.5f, 0.0f });
    AddObject(std::string(kLaunchBase) + "windsock.obj", { -47.0f, 0.0f, -3.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.3f, 0.0f });

    // 発進経路の両側だけに誘導表示を置き、中央の飛行経路を空ける。
    Object3d* leftGuidance = AddObject(std::string(kLaunchBase) + "guidance_sign.obj", { -15.0f, 0.0f, 9.0f });
    Object3d* rightGuidance = AddObject(std::string(kLaunchBase) + "guidance_sign.obj", { 15.0f, 0.0f, 9.0f });
    leftGuidance->SetEnableLighting(false);
    rightGuidance->SetEnableLighting(false);

    for (int index = 0; index < 10; ++index) {
        const float z = 18.0f + static_cast<float>(index) * 28.0f;
        Object3d* leftLight = AddObject(std::string(kLaunchBase) + "runway_light.obj", { -11.5f, 0.0f, z });
        Object3d* rightLight = AddObject(std::string(kLaunchBase) + "runway_light.obj", { 11.5f, 0.0f, z });
        leftLight->SetEnableLighting(false);
        rightLight->SetEnableLighting(false);
    }

    for (int index = 0; index < 5; ++index) {
        const float z = -35.0f + static_cast<float>(index) * 10.0f;
        AddObject(std::string(kLaunchBase) + "safety_rail.obj", { -31.0f, 0.0f, z }, { 1.0f, 1.0f, 1.0f }, { 0.0f, std::numbers::pi_v<float> * 0.5f, 0.0f });
        AddObject(std::string(kLaunchBase) + "safety_rail.obj", { 31.0f, 0.0f, z }, { 1.0f, 1.0f, 1.0f }, { 0.0f, std::numbers::pi_v<float> * 0.5f, 0.0f });
    }

    // Stage01で使用している島を遠景として再利用。
    AddObject(kIslandModel, { -120.0f, -8.0f, 155.0f }, { 1.8f, 1.8f, 1.8f }, { 0.0f, 0.5f, 0.0f });
    AddObject(kIslandModel, { 145.0f, -8.0f, 245.0f }, { 2.1f, 2.1f, 2.1f }, { 0.0f, -0.4f, 0.0f });
    AddObject(kIslandModel, { -170.0f, -8.0f, 360.0f }, { 2.5f, 2.5f, 2.5f }, { 0.0f, 0.2f, 0.0f });

    Model* aircraftModel = ModelManager::GetInstance()->Load(kAircraftModel);
    aircraft_ = std::make_unique<Object3d>();
    aircraft_->Initialize(Object3dManager::GetInstance());
    aircraft_->SetModel(aircraftModel);
    aircraft_->SetCamera(camera_.get());
    aircraft_->SetScale({ 1.4f, 1.4f, 1.4f });
    aircraft_->SetEnableLighting(true);

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kFont);
    titleText_->SetText("LAUNCH BASE  /  FLIGHT TEST");
    titleText_->SetPosition({ 42.0f, 42.0f });
    titleText_->SetFontSize(32.0f);
    titleText_->SetColor({ 0.65f, 0.95f, 1.0f, 1.0f });
    titleText_->SetOutlineWidth(1.0f);

    controlsText_ = std::make_unique<Text>();
    controlsText_->Initialize(kFont);
    controlsText_->SetText("W/S: SPEED   A/D: TURN   UP/DOWN: ALTITUDE   SHIFT: BOOST   BACKSPACE: TITLE");
    controlsText_->SetPosition({ 42.0f, 650.0f });
    controlsText_->SetFontSize(19.0f);
    controlsText_->SetColor({ 0.88f, 0.94f, 1.0f, 1.0f });
    controlsText_->SetOutlineWidth(1.0f);

    speedText_ = std::make_unique<Text>();
    speedText_->Initialize(kFont);
    speedText_->SetPosition({ 42.0f, 92.0f });
    speedText_->SetFontSize(22.0f);
    speedText_->SetColor({ 1.0f, 0.62f, 0.22f, 1.0f });

    LightManager::GetInstance()->SetDirectional(
        { 0.75f, 0.88f, 1.0f, 1.0f },
        { -0.32f, -0.82f, 0.48f },
        1.6f);

    UpdateAircraft(0.0f);
    UpdateCamera();
    // NeonGlowは画面下側の輪郭をマゼンタへ着色するため、
    // 発着場では素材色とライティングをそのまま表示する。
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
}

Object3d* LaunchBaseScene::AddObject(
    const std::string& modelPath,
    const Vector3& position,
    const Vector3& scale,
    const Vector3& rotation)
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(Object3dManager::GetInstance());
    object->SetModel(ModelManager::GetInstance()->Load(modelPath));
    object->SetCamera(camera_.get());
    object->SetTranslate(position);
    object->SetScale(scale);
    object->SetRotate(rotation);
    object->SetEnableLighting(true);
    object->Update();
    Object3d* result = object.get();
    baseObjects_.push_back(std::move(object));
    return result;
}

void LaunchBaseScene::Update()
{
    Input* input = Input::GetInstance();
    if (input != nullptr && input->IsKeyTrigger(DIK_BACK)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
        return;
    }

    const float deltaTime = (std::min)(TimeManager::GetInstance()->GetDeltaTime(), 1.0f / 20.0f);
    UpdateAircraft(deltaTime);
    UpdateCamera();
    oceanSurface_->Update(deltaTime);

    speedText_->SetText(std::format("SPEED  {:03.0f}", flightSpeed_));
    titleText_->Update();
    controlsText_->Update();
    speedText_->Update();
}

void LaunchBaseScene::UpdateAircraft(float deltaTime)
{
    Input* input = Input::GetInstance();
    if (input == nullptr || aircraft_ == nullptr) {
        return;
    }

    const float turnInput =
        (input->IsKeyPressed(DIK_D) ? 1.0f : 0.0f) -
        (input->IsKeyPressed(DIK_A) ? 1.0f : 0.0f);
    const float altitudeInput =
        (input->IsKeyPressed(DIK_UP) ? 1.0f : 0.0f) -
        (input->IsKeyPressed(DIK_DOWN) ? 1.0f : 0.0f);
    const bool accelerate = input->IsKeyPressed(DIK_W);
    const bool brake = input->IsKeyPressed(DIK_S);
    const bool boost = input->IsKeyPressed(DIK_LSHIFT) || input->IsKeyPressed(DIK_RSHIFT);

    if (accelerate) {
        flightSpeed_ += (boost ? 42.0f : 24.0f) * deltaTime;
    } else if (brake) {
        flightSpeed_ -= 34.0f * deltaTime;
    } else {
        flightSpeed_ -= 8.0f * deltaTime;
    }
    flightSpeed_ = std::clamp(flightSpeed_, 0.0f, boost ? 72.0f : 45.0f);

    const float steeringScale = 0.45f + std::clamp(flightSpeed_ / 18.0f, 0.0f, 1.0f) * 0.55f;
    // GamePlaySceneと同じ座標系では、右方向へ機首を向ける回転Yは負方向。
    aircraftYaw_ -= turnInput * 1.15f * steeringScale * deltaTime;
    aircraftPosition_.y = std::clamp(
        aircraftPosition_.y + altitudeInput * 22.0f * deltaTime,
        2.0f,
        100.0f);

    const Vector3 forward = {
        -std::sin(aircraftYaw_),
        0.0f,
        std::cos(aircraftYaw_)
    };
    aircraftPosition_.x += forward.x * flightSpeed_ * deltaTime;
    aircraftPosition_.z += forward.z * flightSpeed_ * deltaTime;
    aircraftPosition_.x = std::clamp(aircraftPosition_.x, -500.0f, 500.0f);
    aircraftPosition_.z = std::clamp(aircraftPosition_.z, -80.0f, 900.0f);

    const float bankTarget = -turnInput * 0.48f;
    const float pitchTarget = -altitudeInput * 0.20f;
    const float blend = 1.0f - std::exp(-6.0f * deltaTime);
    aircraftBank_ += (bankTarget - aircraftBank_) * blend;
    aircraftPitch_ += (pitchTarget - aircraftPitch_) * blend;

    aircraft_->SetTranslate(aircraftPosition_);
    aircraft_->SetRotate({ aircraftPitch_, aircraftYaw_, aircraftBank_ });
    aircraft_->Update();
}

void LaunchBaseScene::UpdateCamera()
{
    if (camera_ == nullptr) {
        return;
    }
    const Vector3 forward = {
        -std::sin(aircraftYaw_),
        0.0f,
        std::cos(aircraftYaw_)
    };
    const Vector3 eye = {
        aircraftPosition_.x - forward.x * 18.0f,
        aircraftPosition_.y + 7.0f,
        aircraftPosition_.z - forward.z * 18.0f
    };
    const Vector3 target = {
        aircraftPosition_.x + forward.x * 7.0f,
        aircraftPosition_.y + 1.5f,
        aircraftPosition_.z + forward.z * 7.0f
    };
    camera_->LookAt(eye, target);
    camera_->Update();
}

void LaunchBaseScene::Draw3D()
{
    if (oceanSurface_) {
        oceanSurface_->Draw();
    }
    Object3dManager::GetInstance()->PreDraw();
    for (const auto& object : baseObjects_) {
        object->Draw();
    }
    if (aircraft_) {
        aircraft_->Draw();
    }
}

void LaunchBaseScene::Draw2D()
{
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    controlsText_->Draw();
    speedText_->Draw();
}

void LaunchBaseScene::Finalize()
{
    ShowCursor(TRUE);
    ClipCursor(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void LaunchBaseScene::DrawParticle() {}
void LaunchBaseScene::DrawImGui() {}
