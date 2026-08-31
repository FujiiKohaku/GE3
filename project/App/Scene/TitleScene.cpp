#include "TitleScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Light/LightManager.h"
#include "Engine/Time/TimeManager.h"
#include "StageSelectScene.h"
#include "SceneManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kPlayerModelPath = "fish/fish.obj";
constexpr const char* kObstacleModelPath = "Environment/Block/block.obj";
constexpr float kCourseLapDuration = 18.0f;

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
    camera_->SetFovY(0.55f);
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    Model* playerModel =
        ModelManager::GetInstance()->Load(kPlayerModelPath);
    playerObject_ = std::make_unique<Object3d>();
    playerObject_->Initialize(Object3dManager::GetInstance());
    playerObject_->SetModel(playerModel);
    playerObject_->SetCamera(camera_.get());
    playerObject_->SetScale({ 1.45f, 1.45f, 1.45f });
    playerObject_->SetEnableLighting(true);

    showcaseRail_ = std::make_unique<Rail>();
    showcaseRail_->Initialize();
    constexpr int kRailPointCount = 12;
    for (int pointIndex = 0; pointIndex < kRailPointCount; ++pointIndex) {
        const float angle =
            static_cast<float>(pointIndex) /
            static_cast<float>(kRailPointCount) *
            2.0f * std::numbers::pi_v<float>;
        showcaseRail_->AddPoint({
            std::cos(angle) * 28.0f + std::sin(angle * 3.0f) * 5.0f,
            2.0f + std::sin(angle * 2.0f) * 1.8f,
            65.0f + std::sin(angle) * 34.0f,
        });
    }
    showcaseRail_->SetLooping(true);

    Model* obstacleModel =
        ModelManager::GetInstance()->Load(kObstacleModelPath);
    auto addObstacle = [&](const Vector3& position, const Vector3& scale,
                           float rotationY) {
        auto obstacle = std::make_unique<Object3d>();
        obstacle->Initialize(Object3dManager::GetInstance());
        obstacle->SetModel(obstacleModel);
        obstacle->SetCamera(camera_.get());
        obstacle->SetTranslate(position);
        obstacle->SetScale(scale);
        obstacle->SetRotate({ 0.0f, rotationY, 0.08f * rotationY });
        obstacle->SetEnableLighting(true);
        obstacle->Update();
        obstacleObjects_.push_back(std::move(obstacle));
    };

    // 周回ラインの内側に回避対象を置く。最短でも機体中心から約10以上離れる。
    addObstacle({ 12.0f, -2.5f, 65.0f }, { 3.0f, 9.0f, 3.0f }, 0.35f);
    addObstacle({ 0.0f, -3.5f, 84.0f }, { 3.5f, 8.0f, 3.5f }, -0.25f);
    addObstacle({ -12.0f, -2.0f, 65.0f }, { 3.0f, 10.0f, 3.0f }, 0.50f);
    addObstacle({ 0.0f, -3.0f, 46.0f }, { 3.2f, 8.5f, 3.2f }, -0.40f);

    // コースの内外に置く遠景の島。飛行ラインから十分に離してある。
    addObstacle({ 0.0f, -5.0f, 65.0f }, { 9.0f, 5.0f, 9.0f }, 0.15f);
    addObstacle({ -48.0f, -5.0f, 82.0f }, { 13.0f, 5.0f, 12.0f }, 0.22f);
    addObstacle({ 50.0f, -5.5f, 45.0f }, { 14.0f, 4.5f, 13.0f }, -0.12f);
    addObstacle({ -38.0f, -4.5f, 18.0f }, { 10.0f, 6.0f, 9.0f }, 0.32f);
    addObstacle({ 40.0f, -4.0f, 112.0f }, { 11.0f, 6.0f, 10.0f }, -0.28f);

    oceanSurface_ = std::make_unique<OceanSurface>();
    oceanSurface_->Initialize(camera_.get(), 1600.0f, 1600.0f, -7.0f);
    oceanSurface_->SetWaveAmplitude(0.85f);
    oceanSurface_->SetWaveFrequency(0.095f);

    LightManager::GetInstance()->SetDirectional(
        { 0.65f, 0.85f, 1.0f, 1.0f },
        { -0.35f, -0.75f, 0.55f },
        1.7f);

    UpdatePlayerShowcase(0.0f);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    backgroundSprite_->SetSize({ 1280.0f, 720.0f });
    // 3Dの機体を残しつつ、タイトル文字が読みやすくなる薄い色味。
    backgroundSprite_->SetColor({ 0.015f, 0.025f, 0.055f, 0.28f });

    // 正式なロゴ画像が完成するまで使用する仮タイトルロゴ。
    logoText_ = std::make_unique<Text>();
    logoText_->Initialize(kDefaultFont);
    logoText_->SetText("KOHAKU ENGINE");
    logoText_->SetPosition({ 640.0f, 275.0f });
    logoText_->SetAnchorPoint({ 0.5f, 0.5f });
    logoText_->SetFontSize(78.0f);
    logoText_->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f });
    logoText_->SetOutlineWidth(2.0f);

    pushToStartText_ = std::make_unique<Text>();
    pushToStartText_->Initialize(kDefaultFont);
    pushToStartText_->SetText("PUSH TO START");
    pushToStartText_->SetPosition({ 640.0f, 475.0f });
    pushToStartText_->SetAnchorPoint({ 0.5f, 0.5f });
    pushToStartText_->SetFontSize(30.0f);
    pushToStartText_->SetOutlineWidth(1.0f);

    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::NeonGlow);
}

void TitleScene::Update()
{
    Input* input = Input::GetInstance();
    if (input != nullptr &&
        (input->IsKeyTrigger(DIK_RETURN) ||
         input->IsKeyTrigger(DIK_SPACE))) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<StageSelectScene>());
        return;
    }

    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    promptAnimationTime_ += deltaTime;
    UpdatePlayerShowcase(deltaTime);
    oceanSurface_->Update(deltaTime);
    const float alpha =
        0.55f + 0.45f * std::sin(promptAnimationTime_ * 3.0f);
    pushToStartText_->SetColor({ 0.75f, 0.92f, 1.0f, alpha });

    backgroundSprite_->Update();
    logoText_->Update();
    pushToStartText_->Update();
    for (const std::unique_ptr<Object3d>& obstacle : obstacleObjects_) {
        obstacle->Update();
    }
}

void TitleScene::UpdatePlayerShowcase(float deltaTime)
{
    if (camera_ == nullptr || playerObject_ == nullptr) {
        return;
    }

    showcaseTime_ += deltaTime;

    if (showcaseRail_ == nullptr || showcaseRail_->GetTotalLength() <= 0.0f) {
        return;
    }

    const float lapProgress =
        std::fmod(showcaseTime_, kCourseLapDuration) / kCourseLapDuration;
    const float railDistance = showcaseRail_->GetTotalLength() * lapProgress;
    const Vector3 playerPosition =
        showcaseRail_->GetPositionByDistance(railDistance);
    const Vector3 lookAheadPosition =
        showcaseRail_->GetPositionByDistance(railDistance + 1.5f);

    Vector3 forward = lookAheadPosition - playerPosition;
    const float forwardLength = std::sqrt(
        forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (forwardLength > 0.0001f) {
        forward.x /= forwardLength;
        forward.y /= forwardLength;
        forward.z /= forwardLength;
    }
    // レールの進行方向を基準にした4種類の撮影位置。
    constexpr Vector3 kCameraAngles[] = {
        { 0.0f, 6.5f, -17.0f },   // 後方
        { -18.0f, 5.0f, -4.0f },  // 左側面
        { 12.0f, 7.0f, 15.0f },   // 正面右上
        { 16.0f, -2.0f, -8.0f },  // 右下
    };
    constexpr int kCameraAngleCount = 4;

    const float cameraSequence = lapProgress * kCameraAngleCount;
    const int cameraAngleIndex =
        static_cast<int>(cameraSequence) % kCameraAngleCount;
    const int nextCameraAngleIndex =
        (cameraAngleIndex + 1) % kCameraAngleCount;
    const float cameraAngleProgress =
        cameraSequence - std::floor(cameraSequence);

    // 各アングルを少し保持し、後半45%で次の位置へ滑らかに移る。
    float cameraBlend = std::clamp(
        (cameraAngleProgress - 0.55f) / 0.45f, 0.0f, 1.0f);
    cameraBlend =
        cameraBlend * cameraBlend * (3.0f - 2.0f * cameraBlend);
    const Vector3 currentCameraOffset = kCameraAngles[cameraAngleIndex];
    const Vector3 nextCameraOffset = kCameraAngles[nextCameraAngleIndex];
    const Vector3 cameraOffset = {
        currentCameraOffset.x +
            (nextCameraOffset.x - currentCameraOffset.x) * cameraBlend,
        currentCameraOffset.y +
            (nextCameraOffset.y - currentCameraOffset.y) * cameraBlend,
        currentCameraOffset.z +
            (nextCameraOffset.z - currentCameraOffset.z) * cameraBlend,
    };

    Vector3 cameraRight = Normalize(Cross(
        Vector3 { 0.0f, 1.0f, 0.0f }, forward));
    if (cameraRight.x == 0.0f &&
        cameraRight.y == 0.0f &&
        cameraRight.z == 0.0f) {
        cameraRight = { 1.0f, 0.0f, 0.0f };
    }
    Vector3 cameraUp = Normalize(Cross(forward, cameraRight));

    const Vector3 cameraPosition = {
        playerPosition.x +
            cameraRight.x * cameraOffset.x +
            cameraUp.x * cameraOffset.y +
            forward.x * cameraOffset.z,
        playerPosition.y +
            cameraRight.y * cameraOffset.x +
            cameraUp.y * cameraOffset.y +
            forward.y * cameraOffset.z,
        playerPosition.z +
            cameraRight.z * cameraOffset.x +
            cameraUp.z * cameraOffset.y +
            forward.z * cameraOffset.z,
    };
    const Vector3 lookTarget = {
        playerPosition.x + forward.x * 2.5f,
        playerPosition.y + forward.y * 2.5f,
        playerPosition.z + forward.z * 2.5f,
    };

    // GamePlayScene::UpdatePlayerTransformと同じ方法でレール方向へ向ける。
    const float horizontalLength =
        std::sqrt(forward.x * forward.x + forward.z * forward.z);
    const float pitch = -std::atan2(forward.y, horizontalLength);
    const float yaw = -std::atan2(forward.x, forward.z);

    playerObject_->SetTranslate(playerPosition);
    playerObject_->SetRotate({ pitch, yaw, 0.0f });
    playerObject_->Update();

    camera_->LookAt(cameraPosition, lookTarget);
    camera_->Update();
}

void TitleScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();

    TextRenderer::GetInstance()->PreDraw();
    logoText_->Draw();
    pushToStartText_->Draw();
}

void TitleScene::Finalize()
{
    ShowCursor(TRUE);
    ClipCursor(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void TitleScene::Draw3D()
{
    if (playerObject_ == nullptr) {
        return;
    }
    if (oceanSurface_ != nullptr) {
        oceanSurface_->Draw();
    }
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(
        DirectXCommon::GetInstance()->GetCommandList());
    for (const std::unique_ptr<Object3d>& obstacle : obstacleObjects_) {
        obstacle->Draw();
    }
    playerObject_->Draw();
}
void TitleScene::DrawParticle() {}
void TitleScene::DrawImGui() {}
