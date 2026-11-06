#include "SavePoint.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kBoundaryHalfWidth = 3.75f;
constexpr float kBoundaryHeight = 2.75f;
constexpr float kBoundaryCenterHeight = 1.42f;
constexpr float kBoundaryOpacity = 0.225f;
constexpr float kBoundaryOpacityAmplitude = 0.025f;
constexpr float kArrivalDurationSeconds = 2.5f;
constexpr float kBoundaryFadeSeconds = 0.65f;
constexpr float kPreviewDistance = 1800.0f;
constexpr float kBehindDistance = 140.0f;
}

SavePoint::SavePoint() = default;
SavePoint::~SavePoint() = default;

void SavePoint::Initialize(const SavePointSettings& settings, const Vector3& playerPosition)
{
    settings_ = settings;
    previousPlayerPosition_ = playerPosition;
    hasActivated_ = false;
    animationTimeSeconds_ = 0.0f;
    arrivalRemainingSeconds_ = 0.0f;
    body_ = std::make_unique<Object3d>();
    body_->Initialize(Object3dManager::GetInstance());
    body_->SetModel(ModelManager::GetInstance()->Load("Environment/SavePoint/SavePointBody.obj"));
    body_->SetScale(settings_.scale);
    body_->SetTranslate(settings_.position);
    body_->SetRotate({ 0.0f, settings_.yawRadians, 0.0f });
    body_->SetEnableLighting(true);
    body_->SetSurfaceProperties(0.4f, 0.4f, 0.5f);
    body_->Update();
    glow_ = std::make_unique<Object3d>();
    glow_->Initialize(Object3dManager::GetInstance());
    glow_->SetModel(ModelManager::GetInstance()->Load("Environment/SavePoint/SavePointGlow.obj"));
    glow_->SetScale(settings_.scale);
    glow_->SetTranslate(settings_.position);
    glow_->SetRotate({ 0.0f, settings_.yawRadians, 0.0f });
    glow_->SetEnableLighting(false);
    glow_->Update();
    boundary_ = std::make_unique<Object3d>();
    boundary_->Initialize(Object3dManager::GetInstance());
    boundary_->SetModel(ModelManager::GetInstance()->CreatePlane("resources/Textures/white.png"));
    boundary_->SetTranslate({ settings_.position.x,
        settings_.position.y + kBoundaryCenterHeight * settings_.scale.y, settings_.position.z });
    boundary_->SetRotate({ 0.0f, settings_.yawRadians, 0.0f });
    boundary_->SetScale({ kBoundaryHalfWidth * 2.0f * settings_.scale.x,
        kBoundaryHeight * settings_.scale.y, 1.0f });
    boundary_->SetColor({ 0.08f, 0.8f, 1.0f, kBoundaryOpacity });
    boundary_->SetEnableLighting(false);
    boundary_->SetTransparent(true);
    boundary_->Update();
    arrivalText_ = std::make_unique<Text>();
    arrivalText_->Initialize("resources/Fonts/KurobaraGothic/kurobara-gothic-heavy.ttf");
    arrivalText_->SetText("チェックポイント到達");
    arrivalText_->SetAnchorPoint({ 0.5f, 0.5f });
    arrivalText_->SetFontSize(30.0f);
    arrivalText_->SetOutlineWidth(2.0f);
    arrivalText_->Update();
}

bool SavePoint::IsPassageCrossed(const SavePointSettings& settings,
    const Vector3& previousPosition, const Vector3& currentPosition)
{
    if (settings.scale.x <= 0.0f || settings.scale.y <= 0.0f) { return false; }
    const float forwardX = std::sin(settings.yawRadians);
    const float forwardZ = std::cos(settings.yawRadians);
    const float previousDistance = (previousPosition.x - settings.position.x) * forwardX
        + (previousPosition.z - settings.position.z) * forwardZ;
    const float currentDistance = (currentPosition.x - settings.position.x) * forwardX
        + (currentPosition.z - settings.position.z) * forwardZ;
    if (previousDistance > 0.0f || currentDistance <= 0.0f) { return false; }
    const float crossingRatio = -previousDistance / (currentDistance - previousDistance);
    const float crossingX = previousPosition.x
        + (currentPosition.x - previousPosition.x) * crossingRatio - settings.position.x;
    const float crossingZ = previousPosition.z
        + (currentPosition.z - previousPosition.z) * crossingRatio - settings.position.z;
    const float localX = crossingX * forwardZ - crossingZ * forwardX;
    const float localY = previousPosition.y
        + (currentPosition.y - previousPosition.y) * crossingRatio - settings.position.y;
    const float halfHeight = kBoundaryHeight * 0.5f;
    return std::abs(localX) <= kBoundaryHalfWidth * settings.scale.x
        && localY >= (kBoundaryCenterHeight - halfHeight) * settings.scale.y
        && localY <= (kBoundaryCenterHeight + halfHeight) * settings.scale.y;
}

bool SavePoint::Update(const Vector3& playerPosition, float deltaTimeSeconds)
{
    const float elapsedSeconds = (std::max)(deltaTimeSeconds, 0.0f);
    animationTimeSeconds_ += elapsedSeconds;
    arrivalRemainingSeconds_ = (std::max)(0.0f, arrivalRemainingSeconds_ - elapsedSeconds);
    bool hasJustActivated = false;
    if (!hasActivated_ && IsPassageCrossed(settings_, previousPlayerPosition_, playerPosition)) {
        hasActivated_ = true;
        hasJustActivated = true;
        arrivalRemainingSeconds_ = kArrivalDurationSeconds;
    }
    previousPlayerPosition_ = playerPosition;
    float boundaryAlpha = kBoundaryOpacity
        + kBoundaryOpacityAmplitude * std::sin(animationTimeSeconds_ * 2.0f);
    float glowStrength = 1.4f + 0.2f * std::sin(animationTimeSeconds_ * 2.0f);
    if (hasActivated_) {
        const float activationSeconds = kArrivalDurationSeconds - arrivalRemainingSeconds_;
        boundaryAlpha = 0.28f * (std::max)(0.0f, 1.0f - activationSeconds / kBoundaryFadeSeconds);
        glowStrength = 1.3f + 2.0f * (std::max)(0.0f, 1.0f - activationSeconds / kBoundaryFadeSeconds);
    }
    glow_->SetColor({ glowStrength, glowStrength, glowStrength, 1.0f });
    boundary_->SetColor({ 0.08f, 0.8f, 1.0f, boundaryAlpha });
    if (arrivalRemainingSeconds_ > 0.0f) {
        const float textAlpha = (std::min)(1.0f, arrivalRemainingSeconds_ / 0.4f);
        arrivalText_->SetPosition({ static_cast<float>(WinApp::GetInstance()->GetClientWidth()) * 0.5f,
            static_cast<float>(WinApp::GetInstance()->GetClientHeight()) * 0.23f });
        arrivalText_->SetColor({ 0.6f, 1.0f, 1.0f, textAlpha });
        arrivalText_->Update();
    }
    return hasJustActivated;
}

void SavePoint::Draw3D()
{
    const float forwardDistance = (settings_.position.x - previousPlayerPosition_.x) * std::sin(settings_.yawRadians)
        + (settings_.position.z - previousPlayerPosition_.z) * std::cos(settings_.yawRadians);
    if (forwardDistance < -kBehindDistance || forwardDistance > kPreviewDistance) { return; }
    body_->Draw();
    glow_->Draw();
    if (!hasActivated_ || arrivalRemainingSeconds_ > kArrivalDurationSeconds - kBoundaryFadeSeconds) {
        boundary_->Draw();
    }
}

void SavePoint::Draw2D()
{
    if (arrivalRemainingSeconds_ > 0.0f) { arrivalText_->Draw(); }
}
