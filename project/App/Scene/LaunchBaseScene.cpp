#include "LaunchBaseScene.h"

#include "Engine/2D/SpriteManager.h"
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
#include <iterator>
#include <numbers>

namespace {
constexpr const char* kFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kAircraftModel =
    "Characters/Player/AirPlane/AirPlane.obj";
constexpr const char* kLaunchBase = "Environment/LaunchBase/";
constexpr const char* kIslandModel =
    "Environment/Ocean/tropical_island.obj";
constexpr const char* kOceanEnvironment = "Environment/Ocean/";
constexpr const char* kLightBulbModel = "Debug/Sphere/sphere.obj";
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr float kHoursPerSecond = 0.12f;

struct TimeLightingPreset {
    float hour;
    Vector4 sunColor;
    float sunIntensity;
    Vector3 ambientColor;
    float ambientIntensity;
    Vector4 skyColor;
    Vector4 atmosphereTint;
    Vector4 oceanDeepColor;
    Vector4 oceanCrestColor;
    float baseLightIntensity;
};

constexpr TimeLightingPreset kTimeLightingPresets[] = {
    { 0.0f, { 0.24f, 0.34f, 0.62f, 1.0f }, 0.045f,
      { 0.05f, 0.08f, 0.16f }, 0.20f,
      { 0.004f, 0.008f, 0.025f, 1.0f }, { 0.01f, 0.025f, 0.09f, 0.16f },
      { 0.002f, 0.012f, 0.055f, 1.0f }, { 0.012f, 0.10f, 0.18f, 1.0f }, 2.4f },
    { 5.0f, { 0.30f, 0.40f, 0.68f, 1.0f }, 0.08f,
      { 0.07f, 0.11f, 0.22f }, 0.22f,
      { 0.025f, 0.045f, 0.11f, 1.0f }, { 0.03f, 0.04f, 0.10f, 0.12f },
      { 0.003f, 0.018f, 0.065f, 1.0f }, { 0.018f, 0.13f, 0.21f, 1.0f }, 2.2f },
    { 7.5f, { 1.00f, 0.58f, 0.34f, 1.0f }, 1.00f,
      { 0.62f, 0.42f, 0.34f }, 0.30f,
      { 0.48f, 0.30f, 0.22f, 1.0f }, { 0.95f, 0.32f, 0.12f, 0.05f },
      { 0.012f, 0.060f, 0.16f, 1.0f }, { 0.08f, 0.40f, 0.53f, 1.0f }, 0.45f },
    { 12.0f, { 0.78f, 0.88f, 1.00f, 1.0f }, 1.45f,
      { 0.54f, 0.66f, 0.78f }, 0.30f,
      { 0.40f, 0.70f, 1.00f, 1.0f }, { 0.30f, 0.50f, 0.68f, 0.0f },
      { 0.006f, 0.055f, 0.19f, 1.0f }, { 0.025f, 0.43f, 0.64f, 1.0f }, 0.0f },
    { 17.0f, { 0.88f, 0.91f, 1.00f, 1.0f }, 1.25f,
      { 0.52f, 0.62f, 0.72f }, 0.28f,
      { 0.35f, 0.58f, 0.78f, 1.0f }, { 0.35f, 0.48f, 0.62f, 0.01f },
      { 0.008f, 0.050f, 0.17f, 1.0f }, { 0.035f, 0.36f, 0.55f, 1.0f }, 0.0f },
    { 19.5f, { 1.00f, 0.38f, 0.16f, 1.0f }, 0.72f,
      { 0.55f, 0.28f, 0.24f }, 0.25f,
      { 0.28f, 0.09f, 0.07f, 1.0f }, { 0.95f, 0.18f, 0.06f, 0.07f },
      { 0.010f, 0.035f, 0.11f, 1.0f }, { 0.09f, 0.26f, 0.38f, 1.0f }, 1.0f },
    { 21.0f, { 0.26f, 0.36f, 0.64f, 1.0f }, 0.055f,
      { 0.05f, 0.08f, 0.16f }, 0.20f,
      { 0.008f, 0.015f, 0.05f, 1.0f }, { 0.01f, 0.025f, 0.09f, 0.15f },
      { 0.002f, 0.014f, 0.058f, 1.0f }, { 0.014f, 0.11f, 0.19f, 1.0f }, 2.4f },
    { 24.0f, { 0.24f, 0.34f, 0.62f, 1.0f }, 0.045f,
      { 0.05f, 0.08f, 0.16f }, 0.20f,
      { 0.004f, 0.008f, 0.025f, 1.0f }, { 0.01f, 0.025f, 0.09f, 0.16f },
      { 0.002f, 0.012f, 0.055f, 1.0f }, { 0.012f, 0.10f, 0.18f, 1.0f }, 2.4f }
};

float SmoothStep(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

Vector3 BlendVector3(const Vector3& from, const Vector3& to, float amount)
{
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
        from.z + (to.z - from.z) * amount
    };
}

Vector4 BlendVector4(const Vector4& from, const Vector4& to, float amount)
{
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
        from.z + (to.z - from.z) * amount,
        from.w + (to.w - from.w) * amount
    };
}

const char* GetTimePeriodName(float hour)
{
    if (hour >= 5.0f && hour < 9.0f) {
        return "MORNING";
    }
    if (hour >= 9.0f && hour < 17.0f) {
        return "DAY";
    }
    if (hour >= 17.0f && hour < 21.0f) {
        return "EVENING";
    }
    return "NIGHT";
}
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

    atmosphereTint_ = std::make_unique<Sprite>();
    atmosphereTint_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    atmosphereTint_->SetSize({ 1280.0f, 720.0f });

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
        // 支柱全体は周囲の光を受け、灯具部分だけを別の発光体とする。
        leftLight->SetEnableLighting(true);
        rightLight->SetEnableLighting(true);

        for (float x : { -11.5f, 11.5f }) {
            Object3d* bulb = AddObject(
                kLightBulbModel,
                { x, 2.2f, z },
                { 0.28f, 0.28f, 0.28f });
            bulb->SetEnableLighting(false);
            runwayLightBulbs_.push_back(bulb);

            // 灯具の直下だけを照らす短距離ライト。
            baseLightHandles_.push_back(
                LightManager::GetInstance()->AddPointLight(
                    { 0.38f, 0.78f, 1.0f, 1.0f },
                    { x, 2.8f, z },
                    0.0f,
                    12.0f,
                    2.2f));
        }
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

    // 発着場の周囲に近景・中景・遠景のシルエットを作る。
    // 滑走路の正面は開けたままにし、左右の海面に分散させる。
    AddObject(std::string(kOceanEnvironment) + "palm_islet.obj", { -72.0f, -7.0f, 72.0f }, { 1.2f, 1.2f, 1.2f }, { 0.0f, 0.45f, 0.0f });
    AddObject(std::string(kOceanEnvironment) + "rock_arch.obj", { 88.0f, -7.0f, 112.0f }, { 1.35f, 1.35f, 1.35f }, { 0.0f, -0.55f, 0.0f });
    AddObject(std::string(kOceanEnvironment) + "sea_stack.obj", { -92.0f, -7.0f, 225.0f }, { 1.15f, 1.15f, 1.15f }, { 0.0f, 0.2f, 0.0f });
    AddObject(std::string(kOceanEnvironment) + "sea_stack.obj", { 110.0f, -7.0f, 315.0f }, { 1.55f, 1.55f, 1.55f }, { 0.0f, -0.35f, 0.0f });

    // 水面に少し見える岩礁と珊瑚で、拠点近くの密度を上げる。
    AddObject(std::string(kOceanEnvironment) + "reef_cluster.obj", { -43.0f, -10.0f, 38.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.15f, 0.0f });
    AddObject(std::string(kOceanEnvironment) + "reef_cluster.obj", { 48.0f, -10.0f, 66.0f }, { 0.85f, 0.85f, 0.85f }, { 0.0f, -0.7f, 0.0f });
    AddObject(std::string(kOceanEnvironment) + "coral_garden.obj", { -31.0f, -9.0f, 19.0f }, { 0.75f, 0.75f, 0.75f }, { 0.0f, 0.4f, 0.0f });
    AddObject(std::string(kOceanEnvironment) + "coral_fan.obj", { 34.0f, -9.0f, 27.0f }, { 0.8f, 0.8f, 0.8f }, { 0.0f, -0.25f, 0.0f });
    AddObject(std::string(kOceanEnvironment) + "shipwreck.obj", { 64.0f, -10.0f, 188.0f }, { 1.25f, 1.25f, 1.25f }, { 0.0f, 0.7f, 0.0f });

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
    controlsText_->SetText("W/S: MOVE   A/D: TURN   UP/DOWN: ALTITUDE   SHIFT: BOOST   T: TIME +6H   BACKSPACE: TITLE");
    controlsText_->SetPosition({ 42.0f, 650.0f });
    controlsText_->SetFontSize(19.0f);
    controlsText_->SetColor({ 0.88f, 0.94f, 1.0f, 1.0f });
    controlsText_->SetOutlineWidth(1.0f);

    speedText_ = std::make_unique<Text>();
    speedText_->Initialize(kFont);
    speedText_->SetPosition({ 42.0f, 92.0f });
    speedText_->SetFontSize(22.0f);
    speedText_->SetColor({ 1.0f, 0.62f, 0.22f, 1.0f });

    timeText_ = std::make_unique<Text>();
    timeText_->Initialize(kFont);
    timeText_->SetPosition({ 42.0f, 120.0f });
    timeText_->SetFontSize(18.0f);
    timeText_->SetColor({ 0.82f, 0.92f, 1.0f, 1.0f });

    // 日没後に自動点灯する構内照明。明るさは時間帯に応じて更新する。
    LightManager* lightManager = LightManager::GetInstance();
    lightManager->SetPointIntensity(0.0f);
    lightManager->SetSpotLightIntensity(0.0f);

    UpdateAircraft(0.0f);
    UpdateCamera();
    ApplyTimeOfDayLighting();
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
    UpdateTimeOfDay(deltaTime);
    UpdateAircraft(deltaTime);
    UpdateCamera();
    oceanSurface_->Update(deltaTime);

    speedText_->SetText(std::format("SPEED  {:03.0f}", flightSpeed_));
    const int hours = static_cast<int>(timeOfDayHours_);
    const int minutes = static_cast<int>((timeOfDayHours_ - static_cast<float>(hours)) * 60.0f);
    timeText_->SetText(std::format("TIME  {:02}:{:02}  {}", hours, minutes, GetTimePeriodName(timeOfDayHours_)));
    atmosphereTint_->Update();
    titleText_->Update();
    controlsText_->Update();
    speedText_->Update();
    timeText_->Update();
}

void LaunchBaseScene::UpdateTimeOfDay(float deltaTime)
{
    Input* input = Input::GetInstance();
    if (input != nullptr && input->IsKeyTrigger(DIK_T)) {
        timeOfDayHours_ += 6.0f;
    }

    timeOfDayHours_ += deltaTime * kHoursPerSecond;
    if (timeOfDayHours_ >= 24.0f) {
        timeOfDayHours_ = std::fmod(timeOfDayHours_, 24.0f);
    }
    ApplyTimeOfDayLighting();
}

void LaunchBaseScene::ApplyTimeOfDayLighting()
{
    const TimeLightingPreset* from = &kTimeLightingPresets[0];
    const TimeLightingPreset* to = &kTimeLightingPresets[1];
    for (size_t index = 0; index + 1 < std::size(kTimeLightingPresets); ++index) {
        if (timeOfDayHours_ >= kTimeLightingPresets[index].hour &&
            timeOfDayHours_ <= kTimeLightingPresets[index + 1].hour) {
            from = &kTimeLightingPresets[index];
            to = &kTimeLightingPresets[index + 1];
            break;
        }
    }

    const float range = (std::max)(to->hour - from->hour, 0.001f);
    const float blend = SmoothStep((timeOfDayHours_ - from->hour) / range);
    const Vector4 sunColor = BlendVector4(from->sunColor, to->sunColor, blend);
    const float sunIntensity = from->sunIntensity +
        (to->sunIntensity - from->sunIntensity) * blend;
    const Vector3 ambientColor = BlendVector3(from->ambientColor, to->ambientColor, blend);
    const float ambientIntensity = from->ambientIntensity +
        (to->ambientIntensity - from->ambientIntensity) * blend;
    const float baseLightIntensity = from->baseLightIntensity +
        (to->baseLightIntensity - from->baseLightIntensity) * blend;

    const float sunAngle =
        (timeOfDayHours_ - 6.0f) / 24.0f * 2.0f * std::numbers::pi_v<float>;
    const float elevation = std::sin(sunAngle);
    const Vector3 sunDirection = {
        -std::cos(sunAngle),
        -(std::max)(std::abs(elevation), 0.18f),
        0.32f
    };

    LightManager* lightManager = LightManager::GetInstance();
    lightManager->SetDirectional(sunColor, sunDirection, sunIntensity);
    lightManager->SetAmbientColor(ambientColor);
    lightManager->SetAmbientIntensity(ambientIntensity);
    for (uint32_t handle : baseLightHandles_) {
        lightManager->SetPointLightIntensity(handle, baseLightIntensity);
    }
    const float lightActivation = std::clamp(baseLightIntensity / 2.4f, 0.0f, 1.0f);
    const float bulbBrightness = 0.16f + lightActivation * 0.84f;
    for (Object3d* bulb : runwayLightBulbs_) {
        if (bulb != nullptr) {
            bulb->SetColor({
                0.52f * bulbBrightness,
                0.88f * bulbBrightness,
                1.00f * bulbBrightness,
                1.0f });
        }
    }

    if (oceanSurface_) {
        oceanSurface_->SetColors(
            BlendVector4(from->oceanDeepColor, to->oceanDeepColor, blend),
            BlendVector4(from->oceanCrestColor, to->oceanCrestColor, blend));
    }
    // 透明な色板で昼空を隠すのではなく、背景色自体を時間帯で変える。
    const Vector4 skyColor = BlendVector4(from->skyColor, to->skyColor, blend);
    SceneManager::GetInstance()->SetSceneClearColor(skyColor);
    // 遠景を覆う距離フォグも同じ空色にし、夜に昼用の水色が浮かないようにする。
    SceneManager::GetInstance()->SetSceneFogColor(skyColor);
    if (atmosphereTint_) {
        atmosphereTint_->SetColor(
            BlendVector4(from->atmosphereTint, to->atmosphereTint, blend));
    }
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

    if (accelerate && !brake) {
        flightSpeed_ += (boost ? 42.0f : 24.0f) * deltaTime;
    } else if (brake && !accelerate) {
        flightSpeed_ -= 24.0f * deltaTime;
    } else {
        // 入力がないときは前進・後退のどちらからでも穏やかに停止する。
        const float coastDeceleration = 8.0f * deltaTime;
        if (flightSpeed_ > 0.0f) {
            flightSpeed_ = (std::max)(0.0f, flightSpeed_ - coastDeceleration);
        } else if (flightSpeed_ < 0.0f) {
            flightSpeed_ = (std::min)(0.0f, flightSpeed_ + coastDeceleration);
        }
    }
    constexpr float kMaximumReverseSpeed = 16.0f;
    flightSpeed_ = std::clamp(flightSpeed_, -kMaximumReverseSpeed, boost ? 72.0f : 45.0f);

    const float steeringScale = 0.45f + std::clamp(std::abs(flightSpeed_) / 18.0f, 0.0f, 1.0f) * 0.55f;
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
    SpriteManager::GetInstance()->PreDraw();
    atmosphereTint_->Draw();

    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    controlsText_->Draw();
    speedText_->Draw();
    timeText_->Draw();
}

void LaunchBaseScene::Finalize()
{
    LightManager* lightManager = LightManager::GetInstance();
    for (uint32_t handle : baseLightHandles_) {
        lightManager->RemovePointLight(handle);
    }
    baseLightHandles_.clear();
    ShowCursor(TRUE);
    ClipCursor(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void LaunchBaseScene::DrawParticle() {}
void LaunchBaseScene::DrawImGui() {}
