#include "App/Scene/Common/SceneManager.h"
#include <cassert>
#include <cmath>
#include "Engine/Light/LightManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/PostEffect/Volumetric/VolumetricLightRenderer.h"

namespace {
bool ChangeScene(
    std::unique_ptr<BaseScene>& scene,
    std::unique_ptr<BaseScene>& nextScene,
    std::unique_ptr<BaseScene>& retiredScene)
{
    if (!nextScene) {
        return false;
    }

    if (scene) {
        retiredScene = std::move(scene);
    }

    scene = std::move(nextScene);
    scene->Initialize();
    return true;
}
}


void SceneManager::Update()
{
    if (retiredScene_) {
        retiredScene_->Finalize();
        retiredScene_.reset();
    }

    if (nextScene_) {
        auto* lights = LightManager::GetInstance();
        if (lights->IsInitialized()) {
            lights->ClearDynamicPointLights();
            lights->ClearDynamicSpotLights();
            lights->SetPointIntensity(0.0f);
            lights->SetSpotLightIntensity(0.0f);
            lights->SetPointLightShadowEnabled(0, false);
            lights->SetSpotLightShadowEnabled(0, false);
        }
        SetSceneExposure(1.0f);
        if (volumetricLightRenderer_ != nullptr) { volumetricLightRenderer_->ResetLocalFog(); }
        RemovePostEffect(PostEffectType::ArchiveAtmosphere);
        radialBlurSettings_ = {};
        effectParameters_.clear();
        // 時間帯を持たないシーンに、前シーンの空色を残さない。
        sceneClearColor_ = { 0.4f, 0.7f, 1.0f, 1.0f };
        sceneFogColor_ = { 0.58f, 0.80f, 0.96f, 1.0f };
        SetSceneDistanceFog({ 380.0f, 720.0f, 1.0f, 1.2f });
    }
    if (ChangeScene(scene_, nextScene_, retiredScene_)) {
        pageReveal_.InitializeIfRequested();
    }
    pageReveal_.Update(TimeManager::GetInstance()->GetDeltaTime());

    if (scene_) {
        scene_->Update();
    }

}

void SceneManager::Finalize()
{
    if (retiredScene_) {
        retiredScene_->Finalize();
        retiredScene_.reset();
    }

    if (scene_) {
        scene_->Finalize();
        scene_.reset();
    }
}

void SceneManager::Draw2D()
{
    // 実行中シーンの2D描画
    if (scene_) {
        scene_->Draw2D();
    }
    pageReveal_.Draw();
}
void SceneManager::Draw3D()
{
    // 実行中シーンの3D描画
    if (scene_) {
        scene_->Draw3D();
    }
}

void SceneManager::DrawParticle()
{
    if (scene_) {
        scene_->DrawParticle();
    }
}

void SceneManager::DrawImGui()
{
    // 実行中シーンのImGui描画
    if (scene_) {
        scene_->DrawImGui();
    }
}

void SceneManager::SetPostEffectType(PostEffectType postEffectType)
{
    ClearPostEffects();
    AddPostEffect(
        postEffectType,
        PostEffectStage::BeforeParticle);
    AddPostEffect(
        PostEffectType::Fog,
        PostEffectStage::BeforeParticle);
    postEffectType_ = postEffectType;
}

PostEffectType SceneManager::GetPostEffectType() const
{
    return postEffectType_;
}

void SceneManager::AddPostEffect(
    PostEffectType type,
    PostEffectStage stage)
{
    for (const PostEffectInfo& postEffect : postEffects_) {
        if (postEffect.type == type) {
            return;
        }
    }

    PostEffectInfo postEffect;
    postEffect.type = type;
    postEffect.stage = stage;
    postEffect.enabled = true;
    postEffect.priority = 0;
    postEffects_.push_back(postEffect);

    if (postEffects_.size() == 1) {
        postEffectType_ = type;
    }
}

void SceneManager::RemovePostEffect(PostEffectType type)
{
    for (std::vector<PostEffectInfo>::iterator iterator = postEffects_.begin(); iterator != postEffects_.end(); ++iterator) {
        if (iterator->type == type) {
            postEffects_.erase(iterator);
            break;
        }
    }

    if (postEffects_.empty()) {
        postEffectType_ = PostEffectType::Copy;
        return;
    }

    postEffectType_ = postEffects_.front().type;
}

void SceneManager::ClearPostEffects()
{
    postEffects_.clear();
    postEffectType_ = PostEffectType::Copy;
}
const Vector3* SceneManager::FindPostEffectParameters(PostEffectType type) const
{
    const auto parameters = effectParameters_.find(type);
    if (parameters == effectParameters_.end()) { return nullptr; }
    return &parameters->second;
}
bool SceneManager::SetRadialBlurSettings(const RadialBlurSettings& settings)
{
    if (!std::isfinite(settings.width) || !std::isfinite(settings.impulseStrength) ||
        settings.width < 0.0f || settings.width > 1.0f || settings.impulseStrength < 0.0f ||
        settings.impulseStrength > 1.0f || settings.sampleCount < 8 || settings.sampleCount > 128) { return false; }
    radialBlurSettings_ = settings;
    return true;
}
void SceneManager::ApplyAtmospherePreset(const AtmospherePreset& preset)
{
    SetSceneExposure(preset.exposure);
    SetSceneDistanceFog(preset.distanceFog);
    if (preset.shouldApplyFogColor) { SetSceneFogColor(preset.fogColor); }
}
void SceneManager::ApplyPostEffectChain(const std::vector<PostEffectInfo>& effects)
{
    postEffects_ = effects;
    postEffectType_ = PostEffectType::Copy;
    if (!postEffects_.empty()) { postEffectType_ = postEffects_.front().type; }
}

void SceneManager::SetPostEffectEnabled(PostEffectType type, bool enable)
{
    for (PostEffectInfo& postEffect : postEffects_) {
        if (postEffect.type == type) {
            postEffect.enabled = enable;
            return;
        }
    }
}

const std::vector<PostEffectInfo>& SceneManager::GetPostEffects() const
{
    return postEffects_;
}

void SceneManager::SetPostEffectCenter(const Vector2& center)
{
    postEffectCenter_ = center;
}

const Vector2& SceneManager::GetPostEffectCenter() const
{
    return postEffectCenter_;
}

void SceneManager::SetCameraShakeStrength(float strength)
{
    cameraShakeStrength_ = strength;
    if (cameraShakeStrength_ < 0.0f) {
        cameraShakeStrength_ = 0.0f;
    }

    if (cameraShakeStrength_ > 0.05f) {
        cameraShakeStrength_ = 0.05f;
    }
}

float SceneManager::GetCameraShakeStrength() const
{
    return cameraShakeStrength_;
}

void SceneManager::DrawShadow(ShadowMapRenderer& renderer)
{
    if (scene_) { scene_->DrawShadow(renderer); }
}
ShadowSettings SceneManager::GetShadowSettings() const
{
    if (scene_) { return scene_->GetShadowSettings(); }
    return {};
}

void SceneManager::SetSceneExposure(float exposure) {
    if (!std::isfinite(exposure) || exposure < 0.1f || exposure > 4.0f) { return; }
    sceneExposure_ = exposure;
    ++sceneExposureRevision_;
}
