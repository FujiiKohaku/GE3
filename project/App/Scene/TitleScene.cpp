#include "TitleScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Light/LightManager.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Audio/GameSfx.h"
#include "App/Game/Stage/StageCatalog.h"
#include "GamePlayScene.h"
#include "LoadingScene.h"
#include "SceneManager.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <numbers>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kLaunchBase = "Environment/LaunchBase/";
constexpr const char* kOceanEnvironment = "Environment/Ocean/";
constexpr const char* kIceEnvironment = "Environment/Ice/";
constexpr const char* kAircraftModel =
    "Characters/Player/AirPlane/AirPlane.obj";

constexpr float kStageTransitionDuration = 3.20f;
constexpr float kStageReturnDuration = 2.20f;
constexpr float kStageConfirmDuration = 0.82f;
constexpr float kSettingsTransitionDuration = 0.46f;
constexpr float kStageRoomTransitionDuration = 0.72f;

const Vector3 kOverviewEye = { 92.0f, 82.0f, -118.0f };
const Vector3 kOverviewTarget = { 0.0f, 8.0f, -30.0f };
// 俯瞰位置は格納庫の背面側にあるため、右外周を大きく回って
// 滑走路側の正面入口へ向かう。
const Vector3 kHangarOrbitFirstControlA = { 170.0f, 70.0f, -90.0f };
const Vector3 kHangarOrbitFirstControlB = { 170.0f, 38.0f, 10.0f };
const Vector3 kHangarOrbitMidEye = { 110.0f, 24.0f, 48.0f };
const Vector3 kHangarOrbitSecondControlA = { 80.0f, 17.0f, 67.0f };
const Vector3 kHangarOrbitSecondControlB = { 0.0f, 12.0f, 26.0f };
const Vector3 kHangarOpeningEye = { 0.0f, 10.0f, 18.0f };
const Vector3 kHangarFocusTarget = { 0.0f, 8.0f, -30.0f };
const Vector3 kStageEntryEye = { 0.0f, 7.5f, -10.0f };
const Vector3 kStageEntryTarget = { 0.0f, 7.0f, -45.0f };
const Vector3 kStageConfirmEye = { 0.0f, 7.0f, -27.0f };
const Vector3 kStageConfirmTarget = { 0.0f, 7.0f, -48.0f };
const Vector3 kSettingsEye = { -78.0f, 34.0f, -58.0f };
const Vector3 kSettingsTarget = { -24.0f, 3.0f, -4.0f };

Vector3 CubicBezier(
    const Vector3& p0,
    const Vector3& p1,
    const Vector3& p2,
    const Vector3& p3,
    float time)
{
    const float inverse = 1.0f - time;
    const float weight0 = inverse * inverse * inverse;
    const float weight1 = 3.0f * inverse * inverse * time;
    const float weight2 = 3.0f * inverse * time * time;
    const float weight3 = time * time * time;
    return {
        p0.x * weight0 + p1.x * weight1 +
            p2.x * weight2 + p3.x * weight3,
        p0.y * weight0 + p1.y * weight1 +
            p2.y * weight2 + p3.y * weight3,
        p0.z * weight0 + p1.z * weight1 +
            p2.z * weight2 + p3.z * weight3,
    };
}

Vector3 EvaluateHangarEyePath(float pathTime)
{
    pathTime = std::clamp(pathTime, 0.0f, 1.0f);
    if (pathTime < 0.52f) {
        const float localTime = pathTime / 0.52f;
        return CubicBezier(
            kOverviewEye,
            kHangarOrbitFirstControlA,
            kHangarOrbitFirstControlB,
            kHangarOrbitMidEye,
            localTime);
    }
    if (pathTime < 0.78f) {
        const float localTime = (pathTime - 0.52f) / 0.26f;
        return CubicBezier(
            kHangarOrbitMidEye,
            kHangarOrbitSecondControlA,
            kHangarOrbitSecondControlB,
            kHangarOpeningEye,
            localTime);
    }

    const float localTime = (pathTime - 0.78f) / 0.22f;
    return CubicBezier(
        kHangarOpeningEye,
        { 0.0f, 8.31f, 11.23f },
        { 0.0f, 7.9f, -8.0f },
        kStageEntryEye,
        localTime);
}

Vector3 EvaluateHangarTargetPath(float pathTime)
{
    // 外周では格納庫中央を見続け、正面に揃ってから奥へ視線を送る。
    if (pathTime <= 0.78f) {
        return kHangarFocusTarget;
    }
    float localTime = std::clamp(
        (pathTime - 0.78f) / 0.22f, 0.0f, 1.0f);
    localTime = localTime * localTime * (3.0f - 2.0f * localTime);
    return Lerp(kHangarFocusTarget, kStageEntryTarget, localTime);
}

float FindHangarPathTimeByDistance(float distanceProgress)
{
    constexpr int kSampleCount = 128;
    distanceProgress = std::clamp(distanceProgress, 0.0f, 1.0f);

    float totalLength = 0.0f;
    Vector3 previous = EvaluateHangarEyePath(0.0f);
    for (int index = 1; index <= kSampleCount; ++index) {
        const float time = static_cast<float>(index) / kSampleCount;
        const Vector3 current = EvaluateHangarEyePath(time);
        const Vector3 delta = current - previous;
        totalLength += std::sqrt(
            delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        previous = current;
    }

    const float targetLength = totalLength * distanceProgress;
    float accumulated = 0.0f;
    previous = EvaluateHangarEyePath(0.0f);
    for (int index = 1; index <= kSampleCount; ++index) {
        const float currentTime = static_cast<float>(index) / kSampleCount;
        const Vector3 current = EvaluateHangarEyePath(currentTime);
        const Vector3 delta = current - previous;
        const float segmentLength = std::sqrt(
            delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (accumulated + segmentLength >= targetLength) {
            const float segmentProgress = segmentLength > 0.0001f
                ? (targetLength - accumulated) / segmentLength
                : 0.0f;
            const float previousTime =
                static_cast<float>(index - 1) / kSampleCount;
            return previousTime +
                (currentTime - previousTime) * segmentProgress;
        }
        accumulated += segmentLength;
        previous = current;
    }
    return 1.0f;
}
}

void TitleScene::Initialize()
{
    ShowCursor(TRUE);
    ClipCursor(nullptr);

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    Object3dManager::GetInstance()->Initialize(dxCommon);
    LightManager::GetInstance()->Initialize(dxCommon);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->SetFovY(0.64f);
    camera_->SetFarClip(3000.0f);
    camera_->LookAt(kOverviewEye, kOverviewTarget);
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    InitializeLaunchBase();
    InitializeInterface();
    InitializeStageSelection();
    UpdateInterface(0.0f);

    LightManager::GetInstance()->SetDirectional(
        { 1.0f, 0.68f, 0.46f, 1.0f },
        { -0.35f, -0.78f, 0.42f },
        1.15f);
    LightManager::GetInstance()->SetAmbientColor({ 0.34f, 0.42f, 0.56f });
    LightManager::GetInstance()->SetAmbientIntensity(0.32f);

    SceneManager::GetInstance()->SetSceneClearColor(
        { 0.16f, 0.29f, 0.46f, 1.0f });
    SceneManager::GetInstance()->SetSceneFogColor(
        { 0.22f, 0.38f, 0.54f, 1.0f });
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
}

Object3d* TitleScene::AddBaseObject(
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

Object3d* TitleScene::AddStageRoomObject(
    std::vector<StageRoomObject>& roomObjects,
    const std::string& modelPath,
    const Vector3& position,
    const Vector3& scale,
    const Vector3& rotation,
    float motionScale,
    float delay)
{
    constexpr float kRoomObjectScale = 0.82f;
    constexpr float kRoomPositionSpread = 0.86f;
    constexpr float kRoomCenterZ = -35.0f;
    const Vector3 fittedPosition = {
        position.x * kRoomPositionSpread,
        position.y * 0.90f,
        kRoomCenterZ + (position.z - kRoomCenterZ) * kRoomPositionSpread,
    };
    const Vector3 fittedScale = {
        scale.x * kRoomObjectScale,
        scale.y * kRoomObjectScale,
        scale.z * kRoomObjectScale,
    };

    StageRoomObject roomObject;
    roomObject.object = std::make_unique<Object3d>();
    roomObject.object->Initialize(Object3dManager::GetInstance());
    roomObject.object->SetModel(ModelManager::GetInstance()->Load(modelPath));
    roomObject.object->SetCamera(camera_.get());
    roomObject.object->SetTranslate(fittedPosition);
    roomObject.object->SetScale(fittedScale);
    roomObject.object->SetRotate(rotation);
    roomObject.object->SetEnableLighting(true);
    roomObject.object->Update();
    roomObject.basePosition = fittedPosition;
    roomObject.baseRotation = rotation;
    roomObject.motionScale = motionScale;
    roomObject.delay = delay;
    Object3d* result = roomObject.object.get();
    roomObjects.push_back(std::move(roomObject));
    return result;
}

void TitleScene::InitializeLaunchBase()
{
    oceanSurface_ = std::make_unique<OceanSurface>();
    oceanSurface_->Initialize(camera_.get(), 3200.0f, 3200.0f, -8.0f);
    // OceanSurfaceは既定でZ=0から前方へ伸びるため、タイトルでは
    // 格納庫背後も覆うよう海面をマイナスZ側へ移す。
    oceanSurface_->SetStartZ(-1600.0f);
    oceanSurface_->SetWaveAmplitude(0.55f);
    oceanSurface_->SetWaveFrequency(0.08f);
    oceanSurface_->SetColors(
        { 0.006f, 0.05f, 0.16f, 1.0f },
        { 0.05f, 0.36f, 0.55f, 1.0f });

    for (int xIndex = -2; xIndex <= 2; ++xIndex) {
        for (int zIndex = -2; zIndex <= 1; ++zIndex) {
            AddBaseObject(
                std::string(kLaunchBase) + "deck_tile.obj",
                { static_cast<float>(xIndex) * 20.0f, -0.5f,
                  static_cast<float>(zIndex) * 20.0f - 20.0f });
        }
    }

    AddBaseObject(
        std::string(kLaunchBase) + "hangar_frame.obj",
        { 0.0f, 0.0f, -30.0f });
    AddBaseObject(
        std::string(kLaunchBase) + "launch_pad.obj",
        { 0.0f, 0.0f, -5.0f });

    for (int index = 0; index < 7; ++index) {
        const float z = 30.0f + static_cast<float>(index) * 40.0f;
        AddBaseObject(
            std::string(kLaunchBase) + "runway_segment.obj",
            { 0.0f, 0.0f, z });
        AddBaseObject(
            std::string(kLaunchBase) + "underdeck_beam.obj",
            { -5.8f, 0.0f, z });
        AddBaseObject(
            std::string(kLaunchBase) + "underdeck_beam.obj",
            { 5.8f, 0.0f, z });

        if (index % 2 == 0) {
            AddBaseObject(
                std::string(kLaunchBase) + "support_pylon.obj",
                { -7.4f, 0.0f, z });
            AddBaseObject(
                std::string(kLaunchBase) + "support_pylon.obj",
                { 7.4f, 0.0f, z });
            AddBaseObject(
                std::string(kLaunchBase) + "cross_brace.obj",
                { 0.0f, 0.0f, z });
        }

        for (float x : { -11.5f, 11.5f }) {
            Object3d* light = AddBaseObject(
                std::string(kLaunchBase) + "runway_light.obj",
                { x, 0.0f, z });
            light->SetEnableLighting(false);
        }
    }

    AddBaseObject(
        std::string(kLaunchBase) + "control_tower.obj",
        { -43.0f, 0.0f, -18.0f });
    AddBaseObject(
        std::string(kLaunchBase) + "fuel_station.obj",
        { -42.0f, 0.0f, -8.0f },
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.35f, 0.0f });
    AddBaseObject(
        std::string(kLaunchBase) + "cargo_container.obj",
        { -36.0f, 0.0f, 5.0f },
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.28f, 0.0f });
    AddBaseObject(
        std::string(kLaunchBase) + "cargo_container.obj",
        { 38.0f, 0.0f, -7.0f },
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, -0.45f, 0.0f });
    AddBaseObject(
        std::string(kLaunchBase) + "fire_station.obj",
        { 43.0f, 0.0f, -4.0f });
    AddBaseObject(
        std::string(kLaunchBase) + "windsock.obj",
        { -48.0f, 0.0f, 1.0f });

    // タイトル俯瞰で見える格納庫の背後。カメラ開始位置の周囲は空け、
    // 海面に近い小物で格納庫から海へつながる距離感を作る。
    AddBaseObject(
        std::string(kOceanEnvironment) + "palm_islet.obj",
        { -88.0f, -7.0f, -108.0f },
        { 1.25f, 1.25f, 1.25f },
        { 0.0f, 0.38f, 0.0f });
    AddBaseObject(
        std::string(kOceanEnvironment) + "reef_cluster.obj",
        { -64.0f, -9.0f, -76.0f },
        { 1.15f, 1.15f, 1.15f },
        { 0.0f, -0.25f, 0.0f });
    AddBaseObject(
        std::string(kOceanEnvironment) + "coral_garden.obj",
        { 68.0f, -9.0f, -82.0f },
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.52f, 0.0f });

    // 背後の中景。旋回経路のさらに外側へ置き、横方向の視差を出す。
    AddBaseObject(
        std::string(kOceanEnvironment) + "rock_arch.obj",
        { 238.0f, -7.0f, -165.0f },
        { 1.55f, 1.55f, 1.55f },
        { 0.0f, -0.42f, 0.0f });
    AddBaseObject(
        std::string(kOceanEnvironment) + "sea_stack.obj",
        { -205.0f, -7.0f, -178.0f },
        { 1.35f, 1.35f, 1.35f },
        { 0.0f, 0.31f, 0.0f });
    AddBaseObject(
        std::string(kOceanEnvironment) + "shipwreck.obj",
        { 158.0f, -10.0f, -248.0f },
        { 1.25f, 1.25f, 1.25f },
        { 0.0f, -0.72f, 0.0f });
    AddBaseObject(
        std::string(kOceanEnvironment) + "rock_arch.obj",
        { -268.0f, -7.0f, -275.0f },
        { 1.35f, 1.35f, 1.35f },
        { 0.0f, 0.65f, 0.0f });

    // 格納庫背後の遠景。水平線が空になりすぎないようシルエットを置く。
    AddBaseObject(
        std::string(kOceanEnvironment) + "tropical_island.obj",
        { -185.0f, -8.0f, -345.0f },
        { 2.15f, 2.15f, 2.15f },
        { 0.0f, 0.22f, 0.0f });
    AddBaseObject(
        std::string(kOceanEnvironment) + "tropical_island.obj",
        { 310.0f, -8.0f, -315.0f },
        { 2.75f, 2.75f, 2.75f },
        { 0.0f, -0.48f, 0.0f });
    AddBaseObject(
        std::string(kOceanEnvironment) + "tropical_island.obj",
        { -355.0f, -8.0f, -535.0f },
        { 3.2f, 3.2f, 3.2f },
        { 0.0f, 0.74f, 0.0f });
    AddBaseObject(
        std::string(kOceanEnvironment) + "sea_stack.obj",
        { 270.0f, -7.0f, -475.0f },
        { 1.75f, 1.75f, 1.75f },
        { 0.0f, -0.16f, 0.0f });
    AddBaseObject(
        std::string(kOceanEnvironment) + "sea_stack.obj",
        { -245.0f, -7.0f, -440.0f },
        { 1.5f, 1.5f, 1.5f },
        { 0.0f, 0.44f, 0.0f });

    aircraft_ = std::make_unique<Object3d>();
    aircraft_->Initialize(Object3dManager::GetInstance());
    aircraft_->SetModel(ModelManager::GetInstance()->Load(kAircraftModel));
    aircraft_->SetCamera(camera_.get());
    aircraft_->SetTranslate({ 0.0f, 3.0f, -5.0f });
    aircraft_->SetScale({ 1.45f, 1.45f, 1.45f });
    aircraft_->SetEnableLighting(true);
    aircraft_->Update();

    InitializeStageRoomSets();
}

void TitleScene::InitializeStageRoomSets()
{
    firstFlightRoomObjects_.clear();
    frozenPassageRoomObjects_.clear();

    // ショップ切り替え時のキャラクター移動に相当する前景機体。
    stageSwitchAircraft_ = std::make_unique<Object3d>();
    stageSwitchAircraft_->Initialize(Object3dManager::GetInstance());
    stageSwitchAircraft_->SetModel(
        ModelManager::GetInstance()->Load(kAircraftModel));
    stageSwitchAircraft_->SetCamera(camera_.get());
    stageSwitchAircraft_->SetTranslate({ 0.0f, -40.0f, -15.0f });
    stageSwitchAircraft_->SetScale({ 1.7f, 1.7f, 1.7f });
    stageSwitchAircraft_->SetEnableLighting(true);
    stageSwitchAircraft_->Update();

    // FIRST FLIGHT: 中央の訓練機と、その周囲を囲む整備・誘導設備。
    AddStageRoomObject(
        firstFlightRoomObjects_, kAircraftModel,
        { 0.0f, 3.0f, -37.0f },
        { 0.90f, 0.90f, 0.90f },
        { 0.0f, std::numbers::pi_v<float>, 0.0f }, 0.72f, 0.00f);
    AddStageRoomObject(
        firstFlightRoomObjects_,
        std::string(kLaunchBase) + "fuel_station.obj",
        { 23.0f, 0.0f, -43.0f },
        { 0.72f, 0.72f, 0.72f },
        { 0.0f, -0.50f, 0.0f }, 0.45f, 0.04f);
    AddStageRoomObject(
        firstFlightRoomObjects_,
        std::string(kLaunchBase) + "service_ladder.obj",
        { -13.0f, 7.0f, -37.0f },
        { 0.90f, 0.90f, 0.90f },
        { 0.0f, 0.12f, 0.0f }, 0.88f, 0.08f);

    const Vector3 cratePositions[] = {
        { -23.0f, 0.0f, -27.0f },
        { 21.0f, 0.0f, -29.0f },
        { -20.0f, 0.0f, -46.0f },
    };
    for (size_t index = 0; index < std::size(cratePositions); ++index) {
        AddStageRoomObject(
            firstFlightRoomObjects_,
            std::string(kLaunchBase) + "tool_crate.obj",
            cratePositions[index],
            index == 2
                ? Vector3 { 0.78f, 0.78f, 0.78f }
                : Vector3 { 1.0f, 1.0f, 1.0f },
            { 0.0f, -0.35f + static_cast<float>(index) * 0.42f, 0.0f },
            1.05f, 0.05f * static_cast<float>(index));
    }

    for (float x : { -23.0f, 23.0f }) {
        Object3d* sign = AddStageRoomObject(
            firstFlightRoomObjects_,
            std::string(kLaunchBase) + "guidance_sign.obj",
            { x, 0.0f, -22.0f },
            { 0.90f, 0.90f, 0.90f },
            { 0.0f, x < 0.0f ? 0.20f : -0.20f, 0.0f }, 1.12f, 0.10f);
        sign->SetEnableLighting(false);
    }

    // 安全柵を組み合わせた訓練用ゲート。
    AddStageRoomObject(
        firstFlightRoomObjects_,
        std::string(kLaunchBase) + "safety_rail.obj",
        { 0.0f, 12.5f, -48.0f },
        { 2.30f, 1.0f, 1.0f }, {}, 0.56f, 0.02f);
    AddStageRoomObject(
        firstFlightRoomObjects_,
        std::string(kLaunchBase) + "safety_rail.obj",
        { -21.0f, 0.0f, -48.0f },
        { 1.0f, 6.0f, 1.0f }, {}, 0.56f, 0.06f);
    AddStageRoomObject(
        firstFlightRoomObjects_,
        std::string(kLaunchBase) + "safety_rail.obj",
        { 21.0f, 0.0f, -48.0f },
        { 1.0f, 6.0f, 1.0f }, {}, 0.56f, 0.10f);

    for (float z : { -23.0f, -32.0f, -41.0f, -49.0f }) {
        for (float x : { -10.5f, 10.5f }) {
            Object3d* light = AddStageRoomObject(
                firstFlightRoomObjects_,
                std::string(kLaunchBase) + "runway_light.obj",
                { x, 0.0f, z },
                { 0.72f, 0.72f, 0.72f }, {}, 1.25f,
                0.015f * static_cast<float>(firstFlightRoomObjects_.size()));
            light->SetEnableLighting(false);
        }
    }

    // FROZEN PASSAGE: 氷塊・冷却設備・青い誘導灯で格納庫全体を冷却区画化。
    const struct {
        const char* model;
        Vector3 position;
        Vector3 scale;
        Vector3 rotation;
    } iceProps[] = {
        { "ice_arch.obj", { 0.0f, 0.0f, -47.0f },
          { 20.0f, 14.0f, 8.0f }, { 0.0f, 0.0f, 0.0f } },
        { "ice_boulder.obj", { -22.0f, 0.0f, -29.0f },
          { 10.0f, 8.0f, 9.0f }, { 0.0f, 0.35f, 0.0f } },
        { "ice_boulder.obj", { 23.0f, 0.0f, -39.0f },
          { 9.0f, 11.0f, 8.0f }, { 0.0f, -0.52f, 0.0f } },
        { "ice_slab.obj", { -17.0f, 0.0f, -45.0f },
          { 12.0f, 4.5f, 10.0f }, { 0.0f, -0.25f, 0.0f } },
        { "ice_slab.obj", { 18.0f, 0.0f, -25.0f },
          { 10.0f, 3.5f, 8.0f }, { 0.0f, 0.48f, 0.0f } },
        { "crystal.obj", { -28.0f, 0.0f, -39.0f },
          { 5.0f, 13.0f, 5.0f }, { 0.0f, 0.20f, -0.08f } },
        { "crystal.obj", { 27.0f, 0.0f, -29.0f },
          { 4.0f, 10.0f, 4.0f }, { 0.0f, -0.40f, 0.06f } },
    };
    for (size_t index = 0; index < std::size(iceProps); ++index) {
        Object3d* ice = AddStageRoomObject(
            frozenPassageRoomObjects_,
            std::string(kIceEnvironment) + iceProps[index].model,
            iceProps[index].position,
            iceProps[index].scale,
            iceProps[index].rotation,
            0.72f + static_cast<float>(index % 3) * 0.20f,
            static_cast<float>(index) * 0.025f);
        ice->SetColor({ 0.68f, 0.88f, 1.0f, 1.0f });
        ice->SetShadingMode(MaterialShadingMode::Ice);
    }

    for (float x : { -24.0f, 18.0f }) {
        Object3d* cooler = AddStageRoomObject(
            frozenPassageRoomObjects_,
            std::string(kLaunchBase) + "fuel_station.obj",
            { x, 0.0f, -44.0f },
            { 0.68f, 0.68f, 0.68f },
            { 0.0f, x < 0.0f ? 0.35f : -0.55f, 0.0f }, 0.48f, 0.10f);
        cooler->SetColor({ 0.58f, 0.82f, 1.0f, 1.0f });
    }
    for (float z : { -24.0f, -35.0f, -46.0f }) {
        for (float x : { -11.0f, 11.0f }) {
            Object3d* light = AddStageRoomObject(
                frozenPassageRoomObjects_,
                std::string(kLaunchBase) + "runway_light.obj",
                { x, 0.0f, z },
                { 0.82f, 0.82f, 0.82f }, {}, 1.20f,
                0.02f * static_cast<float>(frozenPassageRoomObjects_.size()));
            light->SetEnableLighting(false);
            light->SetColor({ 0.18f, 0.72f, 1.0f, 1.0f });
        }
    }
}

void TitleScene::InitializeInterface()
{
    atmosphereTint_ = std::make_unique<Sprite>();
    atmosphereTint_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    atmosphereTint_->SetSize({ 1280.0f, 720.0f });
    atmosphereTint_->SetColor({ 0.02f, 0.055f, 0.12f, 0.18f });

    logoText_ = std::make_unique<Text>();
    logoText_->Initialize(kDefaultFont);
    logoText_->SetText("KOHAKU ENGINE");
    logoText_->SetPosition({ 84.0f, 92.0f });
    logoText_->SetFontSize(70.0f);
    logoText_->SetLetterSpacing(2.0f);
    logoText_->SetColor({ 0.88f, 0.97f, 1.0f, 1.0f });
    logoText_->SetOutlineWidth(2.0f);
    logoText_->SetShadowOffset({ 8.0f, 8.0f });

    pushToStartText_ = std::make_unique<Text>();
    pushToStartText_->Initialize(kDefaultFont);
    pushToStartText_->SetText("PRESS SPACE  /  MISSION SELECT");
    pushToStartText_->SetPosition({ 88.0f, 550.0f });
    pushToStartText_->SetFontSize(26.0f);
    pushToStartText_->SetLetterSpacing(1.0f);
    pushToStartText_->SetOutlineWidth(1.0f);

    settingsShortcutText_ = std::make_unique<Text>();
    settingsShortcutText_->Initialize(kDefaultFont);
    settingsShortcutText_->SetText("S  SETTINGS");
    settingsShortcutText_->SetPosition({ 90.0f, 602.0f });
    settingsShortcutText_->SetFontSize(20.0f);
    settingsShortcutText_->SetColor({ 0.72f, 0.86f, 0.94f, 1.0f });
    settingsShortcutText_->SetOutlineWidth(1.0f);

    settingsBackdrop_ = std::make_unique<Sprite>();
    settingsBackdrop_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    settingsBackdrop_->SetAnchorPoint({ 0.5f, 0.5f });
    settingsBackdrop_->SetSize({ 930.0f, 820.0f });
    settingsBackdrop_->SetRotation(-0.075f);
    settingsBackdrop_->SetColor({ 0.015f, 0.035f, 0.09f, 0.0f });

    settingsAccent_ = std::make_unique<Sprite>();
    settingsAccent_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    settingsAccent_->SetAnchorPoint({ 0.5f, 0.5f });
    settingsAccent_->SetSize({ 84.0f, 900.0f });
    settingsAccent_->SetRotation(-0.18f);
    settingsAccent_->SetColor({ 0.10f, 0.86f, 1.0f, 0.0f });

    settingsTitleText_ = std::make_unique<Text>();
    settingsTitleText_->Initialize(kDefaultFont);
    settingsTitleText_->SetText("SETTINGS");
    settingsTitleText_->SetFontSize(68.0f);
    settingsTitleText_->SetRotation(-0.045f);
    settingsTitleText_->SetColor({ 1.0f, 0.78f, 0.22f, 0.0f });
    settingsTitleText_->SetOutlineWidth(2.0f);

    settingsItemsText_ = std::make_unique<Text>();
    settingsItemsText_->Initialize(kDefaultFont);
    settingsItemsText_->SetText(
        "DISPLAY\n\nAUDIO\n\nCONTROLS\n\nACCESSIBILITY");
    settingsItemsText_->SetFontSize(29.0f);
    settingsItemsText_->SetLineSpacing(8.0f);
    settingsItemsText_->SetColor({ 0.90f, 0.96f, 1.0f, 0.0f });

    settingsBackText_ = std::make_unique<Text>();
    settingsBackText_->Initialize(kDefaultFont);
    settingsBackText_->SetText("BACKSPACE / S  BACK");
    settingsBackText_->SetFontSize(19.0f);
    settingsBackText_->SetColor({ 0.62f, 0.84f, 0.94f, 0.0f });

}

void TitleScene::InitializeStageSelection()
{
    stages_.clear();
    StageCatalog* catalog = StageCatalog::GetInstance();
    if (catalog->Load()) {
        for (const StageSettings& stage : catalog->GetStages()) {
            if (stage.id == "gimmick_test") {
                continue;
            }
            stages_.push_back({ stage.id, stage.name, stage.description });
        }
    }

    stagePanel_ = std::make_unique<Sprite>();
    stagePanel_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    stagePanel_->SetAnchorPoint({ 0.5f, 0.5f });
    stagePanel_->SetSize({ 720.0f, 240.0f });
    stagePanel_->SetColor({ 0.008f, 0.026f, 0.055f, 0.0f });

    frozenFogOverlay_ = std::make_unique<Sprite>();
    frozenFogOverlay_->Initialize(
        SpriteManager::GetInstance(), kWhiteTexture);
    frozenFogOverlay_->SetSize({ 1280.0f, 720.0f });
    frozenFogOverlay_->SetColor({ 0.72f, 0.90f, 1.0f, 0.0f });

    stageSwitchBlackout_ = std::make_unique<Sprite>();
    stageSwitchBlackout_->Initialize(
        SpriteManager::GetInstance(), kWhiteTexture);
    stageSwitchBlackout_->SetSize({ 1280.0f, 720.0f });
    stageSwitchBlackout_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });

    stageHeaderText_ = std::make_unique<Text>();
    stageHeaderText_->Initialize(kDefaultFont);
    stageHeaderText_->SetText("MISSION HANGAR");
    stageHeaderText_->SetAnchorPoint({ 0.5f, 0.0f });
    stageHeaderText_->SetFontSize(22.0f);
    stageHeaderText_->SetLetterSpacing(2.0f);
    stageHeaderText_->SetColor({ 0.20f, 0.90f, 1.0f, 0.0f });

    stageNameText_ = std::make_unique<Text>();
    stageNameText_->Initialize(kDefaultFont);
    stageNameText_->SetAnchorPoint({ 0.5f, 0.0f });
    stageNameText_->SetFontSize(36.0f);
    stageNameText_->SetMaxWidth(640.0f);
    stageNameText_->SetHorizontalAlignment(TextHorizontalAlignment::Center);
    stageNameText_->SetColor({ 1.0f, 0.78f, 0.22f, 0.0f });
    stageNameText_->SetOutlineWidth(1.0f);

    stageDescriptionText_ = std::make_unique<Text>();
    stageDescriptionText_->Initialize(kDefaultFont);
    stageDescriptionText_->SetAnchorPoint({ 0.5f, 0.0f });
    stageDescriptionText_->SetFontSize(16.0f);
    stageDescriptionText_->SetMaxWidth(610.0f);
    stageDescriptionText_->SetHorizontalAlignment(TextHorizontalAlignment::Center);
    stageDescriptionText_->SetLineSpacing(4.0f);
    stageDescriptionText_->SetColor({ 0.82f, 0.92f, 1.0f, 0.0f });

    stagePageText_ = std::make_unique<Text>();
    stagePageText_->Initialize(kDefaultFont);
    stagePageText_->SetAnchorPoint({ 0.5f, 0.0f });
    stagePageText_->SetFontSize(20.0f);
    stagePageText_->SetColor({ 0.36f, 0.88f, 1.0f, 0.0f });

    stageControlsText_ = std::make_unique<Text>();
    stageControlsText_->Initialize(kDefaultFont);
    stageControlsText_->SetText(
        "A / D  SELECT     SPACE  DEPLOY     BACKSPACE  OVERVIEW");
    stageControlsText_->SetAnchorPoint({ 0.5f, 0.0f });
    stageControlsText_->SetFontSize(17.0f);
    stageControlsText_->SetColor({ 0.70f, 0.88f, 0.96f, 0.0f });

    currentStageIndex_ = 0;
    RefreshStageSelectionText();
}

void TitleScene::RefreshStageSelectionText()
{
    if (stages_.empty()) {
        stageNameText_->SetText("NO MISSION DATA");
        stageDescriptionText_->SetText(
            StageCatalog::GetInstance()->GetLastError());
        stagePageText_->SetText("-- / --");
        return;
    }

    const StagePanelData& stage = stages_[currentStageIndex_];
    stageNameText_->SetText(stage.name);
    stageDescriptionText_->SetText(stage.description);
    stagePageText_->SetText(std::format(
        "A  <   {} / {}   >  D",
        currentStageIndex_ + 1,
        stages_.size()));
}

std::vector<TitleScene::StageRoomObject>*
TitleScene::FindStageRoomObjects(const std::string& stageId)
{
    if (stageId == "stage01") {
        return &firstFlightRoomObjects_;
    }
    if (stageId == "stage03") {
        return &frozenPassageRoomObjects_;
    }
    return nullptr;
}

const std::vector<TitleScene::StageRoomObject>*
TitleScene::FindStageRoomObjects(const std::string& stageId) const
{
    if (stageId == "stage01") {
        return &firstFlightRoomObjects_;
    }
    if (stageId == "stage03") {
        return &frozenPassageRoomObjects_;
    }
    return nullptr;
}

void TitleScene::StartStageRoomTransition(int direction)
{
    if (stages_.empty() || stageRoomTransitionActive_) {
        return;
    }

    previousStageIndex_ = currentStageIndex_;
    stageRoomTransitionDirection_ = direction >= 0 ? 1 : -1;
    if (stageRoomTransitionDirection_ > 0) {
        nextStageIndex_ = (currentStageIndex_ + 1) % stages_.size();
    } else {
        nextStageIndex_ =
            (currentStageIndex_ + stages_.size() - 1) % stages_.size();
    }

    stageRoomTransitionTime_ = 0.0f;
    stageRoomTransitionActive_ = true;
    stageRoomSelectionSwapped_ = false;
    GameSfx::GetInstance()->Play(GameSfxId::UiSelect);
}

void TitleScene::UpdateStageRoomObjectGroup(
    std::vector<StageRoomObject>& roomObjects,
    float offsetX,
    float settleAmount,
    float settleDirection)
{
    for (StageRoomObject& roomObject : roomObjects) {
        const float delayedSettle = Clamp01(
            (settleAmount - roomObject.delay) /
            (std::max)(0.01f, 1.0f - roomObject.delay));
        const float spring =
            std::sin(delayedSettle * 5.0f * std::numbers::pi_v<float>) *
            std::exp(-4.2f * delayedSettle);

        Vector3 position = roomObject.basePosition;
        position.x += offsetX * roomObject.motionScale;
        position.x += settleDirection * spring * 0.75f * roomObject.motionScale;
        position.y += std::abs(spring) * 0.42f * roomObject.motionScale;

        Vector3 rotation = roomObject.baseRotation;
        rotation.z += settleDirection * spring * 0.055f * roomObject.motionScale;
        roomObject.object->SetTranslate(position);
        roomObject.object->SetRotate(rotation);
        roomObject.object->Update();
    }
}

void TitleScene::UpdateStageRoomTransition(float deltaTime)
{
    if (stages_.empty()) {
        return;
    }

    if (!stageRoomTransitionActive_) {
        UpdateStageRoomObjectGroup(firstFlightRoomObjects_, 0.0f, 1.0f, 0.0f);
        UpdateStageRoomObjectGroup(frozenPassageRoomObjects_, 0.0f, 1.0f, 0.0f);
        frozenRoomAmount_ = stages_[currentStageIndex_].id == "stage03"
            ? 1.0f
            : 0.0f;
        if (stageSwitchAircraft_) {
            stageSwitchAircraft_->SetTranslate({ 0.0f, -40.0f, -15.0f });
            stageSwitchAircraft_->Update();
        }
        return;
    }

    stageRoomTransitionTime_ += deltaTime;
    const float progress = Clamp01(
        stageRoomTransitionTime_ / kStageRoomTransitionDuration);
    const float outgoingProgress = SmoothStep(progress / 0.52f);
    const float incomingProgress = Clamp01((progress - 0.38f) / 0.62f);
    const float incomingEase = EaseOutBack(incomingProgress);
    const float direction = static_cast<float>(stageRoomTransitionDirection_);

    // 前回の遷移で残った座標を先に戻し、今回描く2セットだけ動かす。
    UpdateStageRoomObjectGroup(firstFlightRoomObjects_, 0.0f, 1.0f, 0.0f);
    UpdateStageRoomObjectGroup(frozenPassageRoomObjects_, 0.0f, 1.0f, 0.0f);

    if (std::vector<StageRoomObject>* outgoing =
            FindStageRoomObjects(stages_[previousStageIndex_].id)) {
        UpdateStageRoomObjectGroup(
            *outgoing,
            -direction * 11.0f * outgoingProgress,
            0.0f,
            -direction);
    }
    if (std::vector<StageRoomObject>* incoming =
            FindStageRoomObjects(stages_[nextStageIndex_].id)) {
        UpdateStageRoomObjectGroup(
            *incoming,
            direction * 13.0f * (1.0f - incomingEase),
            incomingProgress,
            direction);
    }

    // 機体が画面中央を横切る瞬間に、選択ステージとUIを交換する。
    if (stageSwitchAircraft_) {
        const float flightEase = SmoothStep(progress);
        const float x = -direction * 27.0f + direction * 54.0f * flightEase;
        const float jump = std::sin(progress * std::numbers::pi_v<float>);
        const float landingWobble = progress > 0.72f
            ? std::sin((progress - 0.72f) * 9.0f * std::numbers::pi_v<float>) *
                std::exp(-(progress - 0.72f) * 9.0f)
            : 0.0f;
        stageSwitchAircraft_->SetTranslate({
            x,
            6.4f + jump * 3.8f + std::abs(landingWobble) * 0.35f,
            -15.5f,
        });
        stageSwitchAircraft_->SetRotate({
            -0.08f + jump * 0.10f,
            direction > 0.0f
                ? -std::numbers::pi_v<float> * 0.5f
                : std::numbers::pi_v<float> * 0.5f,
            -direction * (0.22f * jump + 0.12f * landingWobble),
        });
        stageSwitchAircraft_->Update();
    }

    if (!stageRoomSelectionSwapped_ && progress >= 0.46f) {
        currentStageIndex_ = nextStageIndex_;
        stageRoomSelectionSwapped_ = true;
        RefreshStageSelectionText();
    }

    const bool leavingFrozen =
        stages_[previousStageIndex_].id == "stage03";
    const bool enteringFrozen =
        stages_[nextStageIndex_].id == "stage03";
    if (enteringFrozen) {
        frozenRoomAmount_ = SmoothStep((progress - 0.18f) / 0.62f);
    } else if (leavingFrozen) {
        frozenRoomAmount_ = 1.0f - SmoothStep(progress / 0.52f);
    } else {
        frozenRoomAmount_ = 0.0f;
    }

    if (progress >= 1.0f) {
        currentStageIndex_ = nextStageIndex_;
        stageRoomTransitionActive_ = false;
        stageRoomTransitionTime_ = 0.0f;
        if (std::vector<StageRoomObject>* current =
                FindStageRoomObjects(stages_[currentStageIndex_].id)) {
            UpdateStageRoomObjectGroup(*current, 0.0f, 1.0f, 0.0f);
        }
    }
}

void TitleScene::Update()
{
    const float deltaTime =
        (std::min)(TimeManager::GetInstance()->GetDeltaTime(), 1.0f / 20.0f);
    idleTime_ += deltaTime;

    Input* input = Input::GetInstance();
    if (input != nullptr) {
        if (viewState_ == ViewState::Title) {
            if (input->IsKeyTrigger(DIK_SPACE) ||
                input->IsKeyTrigger(DIK_RETURN)) {
                StartStageSelectTransition();
            } else if (input->IsKeyTrigger(DIK_S)) {
                StartSettingsTransition(true);
            }
        } else if (viewState_ == ViewState::StageSelect) {
            if (input->IsKeyTrigger(DIK_BACK)) {
                StartTitleReturn();
            } else if (!stages_.empty() &&
                       !stageRoomTransitionActive_ &&
                       (input->IsKeyTrigger(DIK_D) ||
                        input->IsKeyTrigger(DIK_RIGHT))) {
                StartStageRoomTransition(1);
            } else if (!stages_.empty() &&
                       !stageRoomTransitionActive_ &&
                       (input->IsKeyTrigger(DIK_A) ||
                        input->IsKeyTrigger(DIK_LEFT))) {
                StartStageRoomTransition(-1);
            } else if (!stageRoomTransitionActive_ &&
                       (input->IsKeyTrigger(DIK_SPACE) ||
                        input->IsKeyTrigger(DIK_RETURN))) {
                ConfirmStage();
            }
        } else if (viewState_ == ViewState::Settings &&
                   (input->IsKeyTrigger(DIK_BACK) ||
                    input->IsKeyTrigger(DIK_S))) {
            StartSettingsTransition(false);
        }
    }

    switch (viewState_) {
    case ViewState::Title:
        UpdateTitleCamera(deltaTime);
        break;
    case ViewState::EnteringStageSelect:
        UpdateStageSelectTransition(deltaTime);
        break;
    case ViewState::StageSelect:
        UpdateStageSelection(deltaTime);
        break;
    case ViewState::LeavingStageSelect:
        UpdateStageSelectTransition(deltaTime);
        break;
    case ViewState::ConfirmingStage:
        UpdateStageConfirmation(deltaTime);
        break;
    case ViewState::EnteringSettings:
    case ViewState::LeavingSettings:
        UpdateSettingsTransition(deltaTime);
        break;
    case ViewState::Settings:
        camera_->LookAt(kSettingsEye, kSettingsTarget);
        camera_->Update();
        break;
    }

    UpdateStageRoomTransition(deltaTime);

    oceanSurface_->Update(deltaTime);
    for (const auto& object : baseObjects_) {
        object->Update();
    }
    aircraft_->Update();
    UpdateInterface(deltaTime);
}

void TitleScene::StartStageSelectTransition()
{
    if (viewState_ != ViewState::Title) {
        return;
    }
    animationTime_ = 0.0f;
    viewState_ = ViewState::EnteringStageSelect;
}

void TitleScene::StartTitleReturn()
{
    if (viewState_ != ViewState::StageSelect) {
        return;
    }
    GameSfx::GetInstance()->Play(GameSfxId::UiSelect);
    animationTime_ = 0.0f;
    viewState_ = ViewState::LeavingStageSelect;
}

void TitleScene::ConfirmStage()
{
    if (viewState_ != ViewState::StageSelect || stages_.empty()) {
        return;
    }
    GameSfx::GetInstance()->Play(GameSfxId::UiConfirm);
    animationTime_ = 0.0f;
    viewState_ = ViewState::ConfirmingStage;
}

void TitleScene::StartSettingsTransition(bool opening)
{
    if (opening && viewState_ != ViewState::Title) {
        return;
    }
    if (!opening && viewState_ != ViewState::Settings) {
        return;
    }
    animationTime_ = 0.0f;
    viewState_ = opening
        ? ViewState::EnteringSettings
        : ViewState::LeavingSettings;
}

void TitleScene::UpdateTitleCamera(float deltaTime)
{
    (void)deltaTime;
    // 遷移開始時に位置が飛ばないよう、待機中のカメラは固定する。
    camera_->LookAt(kOverviewEye, kOverviewTarget);
    camera_->Update();
}

void TitleScene::UpdateStageSelectTransition(float deltaTime)
{
    animationTime_ += deltaTime;
    const bool entering = viewState_ == ViewState::EnteringStageSelect;
    const float duration = entering
        ? kStageTransitionDuration
        : kStageReturnDuration;
    const float progress = Clamp01(animationTime_ / duration);

    const float distanceProgress = entering
        ? SmoothStep((progress - 0.04f) / 0.96f)
        : 1.0f - SmoothStep(progress);
    // ベジェ曲線の時刻ではなく移動距離を基準に進め、速度むらをなくす。
    const float pathTime = FindHangarPathTimeByDistance(distanceProgress);
    const Vector3 eye = EvaluateHangarEyePath(pathTime);
    const Vector3 target = EvaluateHangarTargetPath(pathTime);
    camera_->LookAt(eye, target);
    // 移動中にズーム感を混ぜず、純粋なカメラ移動として見せる。
    camera_->SetFovY(0.64f);
    camera_->Update();

    if (progress >= 1.0f) {
        animationTime_ = 0.0f;
        viewState_ = entering ? ViewState::StageSelect : ViewState::Title;
        if (!entering) {
            camera_->SetFovY(0.64f);
        }
    }
}

void TitleScene::UpdateStageSelection(float deltaTime)
{
    (void)deltaTime;
    const float breathe = std::sin(idleTime_ * 1.3f) * 0.08f;
    Vector3 eye = {
        kStageEntryEye.x,
        kStageEntryEye.y + breathe,
        kStageEntryEye.z,
    };
    Vector3 target = kStageEntryTarget;

    if (stageRoomTransitionActive_) {
        const float progress = Clamp01(
            stageRoomTransitionTime_ / kStageRoomTransitionDuration);
        const float envelope = std::sin(progress * std::numbers::pi_v<float>);
        const float horizontalShake =
            std::sin(stageRoomTransitionTime_ * 49.0f) * 0.24f * envelope;
        const float verticalShake =
            std::sin(stageRoomTransitionTime_ * 67.0f + 0.8f) *
            0.11f * envelope;
        eye.x += horizontalShake;
        eye.y += verticalShake;
        target.x -= horizontalShake * 0.32f;
        target.y -= verticalShake * 0.18f;
    }

    camera_->LookAt(eye, target);
    camera_->Update();
}

void TitleScene::UpdateStageConfirmation(float deltaTime)
{
    animationTime_ += deltaTime;
    const float progress = Clamp01(animationTime_ / kStageConfirmDuration);
    const float eased = SmoothStep(progress);
    camera_->LookAt(
        Lerp(kStageEntryEye, kStageConfirmEye, eased),
        Lerp(kStageEntryTarget, kStageConfirmTarget, eased));
    camera_->SetFovY(0.64f - 0.08f * eased);
    camera_->Update();

    if (progress >= 1.0f && !stageTransitionQueued_) {
        stageTransitionQueued_ = true;
        SceneManager::GetInstance()->SetNextSceneWithLoading<
            LoadingScene, GamePlayScene>(stages_[currentStageIndex_].id);
    }
}

void TitleScene::UpdateSettingsTransition(float deltaTime)
{
    animationTime_ += deltaTime;
    const float progress = Clamp01(animationTime_ / kSettingsTransitionDuration);
    const bool opening = viewState_ == ViewState::EnteringSettings;
    const float eased = opening
        ? EaseOutBack(progress)
        : SmoothStep(progress);
    const float settingsAmount = opening ? eased : 1.0f - eased;

    // 中盤で大きく横へ振ることで、部屋が切り替わるような勢いを出す。
    const float whip = std::sin(progress * std::numbers::pi_v<float>);
    Vector3 eye = Lerp(kOverviewEye, kSettingsEye, settingsAmount);
    Vector3 target = Lerp(kOverviewTarget, kSettingsTarget, settingsAmount);
    eye.x += (opening ? -1.0f : 1.0f) * whip * 10.0f;
    target.x += (opening ? -1.0f : 1.0f) * whip * 6.0f;
    camera_->LookAt(eye, target);
    camera_->Update();

    if (progress >= 1.0f) {
        animationTime_ = 0.0f;
        viewState_ = opening ? ViewState::Settings : ViewState::Title;
        if (!opening) {
            camera_->SetFovY(0.64f);
        }
    }
}

void TitleScene::UpdateInterface(float deltaTime)
{
    (void)deltaTime;
    float stageAmount = 0.0f;
    float stageTitleExit = 0.0f;
    if (viewState_ == ViewState::StageSelect) {
        stageAmount = 1.0f;
        stageTitleExit = 1.0f;
    } else if (viewState_ == ViewState::EnteringStageSelect) {
        const float progress =
            Clamp01(animationTime_ / kStageTransitionDuration);
        stageAmount = EaseOutBack((progress - 0.74f) / 0.26f);
        stageTitleExit = SmoothStep(progress / 0.22f);
    } else if (viewState_ == ViewState::LeavingStageSelect) {
        const float progress =
            Clamp01(animationTime_ / kStageReturnDuration);
        stageAmount = 1.0f - SmoothStep(progress / 0.35f);
        stageTitleExit = 1.0f - SmoothStep((progress - 0.55f) / 0.45f);
    } else if (viewState_ == ViewState::ConfirmingStage) {
        stageAmount = 1.0f - SmoothStep(
            Clamp01(animationTime_ / kStageConfirmDuration));
        stageTitleExit = 1.0f;
    }

    float settingsAmount = 0.0f;
    if (viewState_ == ViewState::Settings) {
        settingsAmount = 1.0f;
    } else if (viewState_ == ViewState::EnteringSettings) {
        settingsAmount = EaseOutBack(
            Clamp01(animationTime_ / kSettingsTransitionDuration));
    } else if (viewState_ == ViewState::LeavingSettings) {
        settingsAmount = 1.0f - SmoothStep(
            Clamp01(animationTime_ / kSettingsTransitionDuration));
    }

    const float titleExit =
        Clamp01((std::max)(stageTitleExit, settingsAmount));
    const float titleOffset = -520.0f * titleExit;
    const float titleAlpha = 1.0f - Clamp01(titleExit);
    logoText_->SetPosition({ 84.0f + titleOffset, 92.0f });
    logoText_->SetRotation(-0.035f * titleExit);
    logoText_->SetColor({ 0.88f, 0.97f, 1.0f, titleAlpha });

    const float promptAlpha =
        (0.58f + 0.42f * std::sin(idleTime_ * 3.2f)) * titleAlpha;
    pushToStartText_->SetPosition({ 88.0f + titleOffset * 1.18f, 550.0f });
    pushToStartText_->SetColor({ 0.78f, 0.94f, 1.0f, promptAlpha });
    settingsShortcutText_->SetPosition(
        { 90.0f + titleOffset * 1.34f, 602.0f });
    settingsShortcutText_->SetColor(
        { 0.72f, 0.86f, 0.94f, titleAlpha });

    const float panelX = 1480.0f + (790.0f - 1480.0f) * settingsAmount;
    settingsBackdrop_->SetPosition({ panelX, 360.0f });
    settingsAccent_->SetPosition({ panelX - 470.0f, 360.0f });
    settingsBackdrop_->SetColor(
        { 0.015f, 0.035f, 0.09f, 0.94f * Clamp01(settingsAmount) });
    settingsAccent_->SetColor(
        { 0.10f, 0.86f, 1.0f, 0.95f * Clamp01(settingsAmount) });

    const float textX = 1420.0f + (560.0f - 1420.0f) * settingsAmount;
    settingsTitleText_->SetPosition({ textX, 104.0f });
    settingsTitleText_->SetColor(
        { 1.0f, 0.78f, 0.22f, Clamp01(settingsAmount) });
    settingsItemsText_->SetPosition({ textX + 48.0f, 248.0f });
    settingsItemsText_->SetColor(
        { 0.90f, 0.96f, 1.0f, Clamp01(settingsAmount) });
    settingsBackText_->SetPosition({ textX + 48.0f, 626.0f });
    settingsBackText_->SetColor(
        { 0.62f, 0.84f, 0.94f, Clamp01(settingsAmount) });

    // 格納庫内の端末表示。カメラが入口を越えてから起動する。
    const float stageAlpha = Clamp01(stageAmount);
    const float fogPulse = 0.045f + 0.012f * std::sin(idleTime_ * 0.85f);
    frozenFogOverlay_->SetColor({
        0.72f, 0.90f, 1.0f,
        fogPulse * frozenRoomAmount_ * stageAlpha });
    const float stagePanelY =
        790.0f + (558.0f - 790.0f) * stageAmount;
    stagePanel_->SetPosition({ 640.0f, stagePanelY });
    float roomUiAlpha = 1.0f;
    float roomUiOffset = 0.0f;
    float panelKick = 0.0f;
    float blackoutAmount = 0.0f;
    if (stageRoomTransitionActive_) {
        const float progress = Clamp01(
            stageRoomTransitionTime_ / kStageRoomTransitionDuration);
        const float direction =
            static_cast<float>(stageRoomTransitionDirection_);
        panelKick = direction *
            std::sin(progress * std::numbers::pi_v<float>) * 0.012f;
        const float fadeToBlack = SmoothStep((progress - 0.24f) / 0.20f);
        const float fadeFromBlack =
            1.0f - SmoothStep((progress - 0.50f) / 0.20f);
        blackoutAmount = (std::min)(fadeToBlack, fadeFromBlack);
        if (progress < 0.46f) {
            const float exit = SmoothStep(progress / 0.46f);
            roomUiAlpha = 1.0f - exit;
            roomUiOffset = -direction * 260.0f * exit;
        } else {
            const float enter = Clamp01((progress - 0.46f) / 0.54f);
            roomUiAlpha = SmoothStep(enter / 0.58f);
            roomUiOffset =
                direction * 260.0f * (1.0f - EaseOutBack(enter));
        }
    }
    stagePanel_->SetRotation(
        (1.0f - stageAlpha) * 0.025f + panelKick);
    stagePanel_->SetColor({ 0.008f, 0.026f, 0.055f, 0.80f * stageAlpha });
    stageSwitchBlackout_->SetColor(
        { 0.0f, 0.0f, 0.0f, blackoutAmount });

    const float stageTextY = stagePanelY - 112.0f;
    stageHeaderText_->SetPosition({ 640.0f, stageTextY + 10.0f });
    const float blackoutVisibility = 1.0f - blackoutAmount;
    stageHeaderText_->SetColor(
        { 0.20f, 0.90f, 1.0f, stageAlpha * blackoutVisibility });
    const float changingTextAlpha =
        stageAlpha * roomUiAlpha * blackoutVisibility;
    stageNameText_->SetPosition(
        { 640.0f + roomUiOffset, stageTextY + 40.0f });
    stageNameText_->SetColor(
        { 1.0f, 0.78f, 0.22f, changingTextAlpha });
    stageDescriptionText_->SetPosition(
        { 640.0f + roomUiOffset * 1.08f, stageTextY + 88.0f });
    stageDescriptionText_->SetColor(
        { 0.82f, 0.92f, 1.0f, changingTextAlpha });
    stagePageText_->SetPosition(
        { 640.0f + roomUiOffset * 0.84f, stageTextY + 154.0f });
    stagePageText_->SetColor(
        { 0.36f, 0.88f, 1.0f, changingTextAlpha });
    stageControlsText_->SetPosition({ 640.0f, stageTextY + 194.0f });
    stageControlsText_->SetColor(
        { 0.70f, 0.88f, 0.96f, stageAlpha * blackoutVisibility });

    atmosphereTint_->Update();
    frozenFogOverlay_->Update();
    stagePanel_->Update();
    stageSwitchBlackout_->Update();
    settingsBackdrop_->Update();
    settingsAccent_->Update();
    logoText_->Update();
    pushToStartText_->Update();
    settingsShortcutText_->Update();
    settingsTitleText_->Update();
    settingsItemsText_->Update();
    settingsBackText_->Update();
    stageHeaderText_->Update();
    stageNameText_->Update();
    stageDescriptionText_->Update();
    stagePageText_->Update();
    stageControlsText_->Update();
}

float TitleScene::Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float TitleScene::SmoothStep(float value)
{
    const float clamped = Clamp01(value);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

float TitleScene::EaseOutBack(float value)
{
    const float clamped = Clamp01(value);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float shifted = clamped - 1.0f;
    return 1.0f + c3 * shifted * shifted * shifted +
        c1 * shifted * shifted;
}

void TitleScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    atmosphereTint_->Draw();
    frozenFogOverlay_->Draw();
    stagePanel_->Draw();
    settingsBackdrop_->Draw();
    settingsAccent_->Draw();
    stageSwitchBlackout_->Draw();

    TextRenderer::GetInstance()->PreDraw();
    logoText_->Draw();
    pushToStartText_->Draw();
    settingsShortcutText_->Draw();
    settingsTitleText_->Draw();
    settingsItemsText_->Draw();
    settingsBackText_->Draw();
    stageHeaderText_->Draw();
    stageNameText_->Draw();
    stageDescriptionText_->Draw();
    stagePageText_->Draw();
    stageControlsText_->Draw();
}

void TitleScene::Draw3D()
{
    if (oceanSurface_ != nullptr) {
        oceanSurface_->Draw();
    }
    Object3dManager::GetInstance()->PreDraw();
    for (const auto& object : baseObjects_) {
        object->Draw();
    }
    if (aircraft_ != nullptr) {
        aircraft_->Draw();
    }

    auto drawRoomGroup = [](const std::vector<StageRoomObject>* roomObjects) {
        if (roomObjects == nullptr) {
            return;
        }
        for (const StageRoomObject& roomObject : *roomObjects) {
            roomObject.object->Draw();
        }
    };

    if (!stages_.empty()) {
        if (stageRoomTransitionActive_) {
            const float progress = Clamp01(
                stageRoomTransitionTime_ / kStageRoomTransitionDuration);
            if (progress < 0.60f) {
                drawRoomGroup(
                    FindStageRoomObjects(stages_[previousStageIndex_].id));
            }
            if (progress >= 0.32f) {
                drawRoomGroup(
                    FindStageRoomObjects(stages_[nextStageIndex_].id));
            }
            if (stageSwitchAircraft_) {
                stageSwitchAircraft_->Draw();
            }
        } else {
            drawRoomGroup(
                FindStageRoomObjects(stages_[currentStageIndex_].id));
        }
    }
}

void TitleScene::Finalize()
{
    ShowCursor(TRUE);
    ClipCursor(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void TitleScene::DrawParticle() {}
void TitleScene::DrawImGui() {}
