// 現在と次のシーンを管理し、シーン切替と描画・ポストエフェクト設定の受け渡しを行う。
#include "App/Scene/Common/SceneManager.h"
#include <cassert>
#include <cmath>
#include "Engine/Light/LightManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/PostEffect/Volumetric/VolumetricLightRenderer.h"

// 予約されたシーンへの切替と、現在のシーンの更新を行う。
void SceneManager::Update()
{
    if (retiredScene_) {
        retiredScene_.reset();
    }

    if (nextScene_) {
        if (scene_) {
            // 共有設定の解除は次シーンの初期化前に済ませ、本体の破棄だけを遅延する。
            scene_->Finalize();
            retiredScene_ = std::move(scene_);
        }
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
        scene_ = std::move(nextScene_);
        scene_->Initialize();
        ++sceneRevision_;
        pageReveal_.InitializeIfRequested();
    }
    pageReveal_.Update(TimeManager::GetInstance()->GetDeltaTime());

    if (scene_) {
        scene_->Update();
    }

}

// 保持しているシーンを終了し、シーン固有のリソースを解放する。
void SceneManager::Finalize()
{
    if (retiredScene_) {
        retiredScene_.reset();
    }

    if (scene_) {
        scene_->Finalize();
        scene_.reset();
    }
}

// 画面に重ねるスプライトや文字を描画する。
void SceneManager::Draw2D()
{
    // 実行中シーンの2D描画
    if (scene_) {
        scene_->Draw2D();
    }
    pageReveal_.Draw();
}
// 現在表示するモデルや追加パーツを描画する。
void SceneManager::Draw3D()
{
    // 実行中シーンの3D描画
    if (scene_) {
        scene_->Draw3D();
    }
}

// 描画対象をレイトレーシング用のシーンへ登録する。
bool SceneManager::SubmitRaytracingScene(DxrRenderer& renderer) {
    if (!scene_) { return false; }
    return scene_->SubmitRaytracingScene(renderer);
}

// このシーンで使用するパーティクルを描画する。
void SceneManager::DrawParticle()
{
    if (scene_) {
        scene_->DrawParticle();
    }
}

// 開発用の画面に内部状態や調整項目を表示する。
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

// 画面効果をチェーンへ追加し、同じ種類がある場合の扱いを調整する。
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

// 指定した種類の画面効果を適用対象から外す。
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
// 画面効果を探し、その設定値を参照する。見つからない場合はnullptrを返す。
const Vector3* SceneManager::FindPostEffectParameters(PostEffectType type) const
{
    const auto parameters = effectParameters_.find(type);
    if (parameters == effectParameters_.end()) { return nullptr; }
    return &parameters->second;
}
// 放射状ブラーの設定を検証し、適用できる場合だけ変更する。
bool SceneManager::SetRadialBlurSettings(const RadialBlurSettings& settings)
{
    if (!std::isfinite(settings.width) || !std::isfinite(settings.impulseStrength) ||
        settings.width < 0.0f || settings.width > 1.0f || settings.impulseStrength < 0.0f ||
        settings.impulseStrength > 1.0f || settings.sampleCount < 8 || settings.sampleCount > 128) { return false; }
    radialBlurSettings_ = settings;
    return true;
}
// シーン全体の霧や空気感に関する設定をまとめて適用する。
void SceneManager::ApplyAtmospherePreset(const AtmospherePreset& preset)
{
    SetSceneExposure(preset.exposure);
    SetSceneDistanceFog(preset.distanceFog);
    if (preset.shouldApplyFogColor) { SetSceneFogColor(preset.fogColor); }
}
// 渡された一覧で画面効果の適用順と設定を更新する。
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

// カメラ揺れの強さを設定し、許可された値に収める。
void SceneManager::SetCameraShakeStrength(float strength)
{
    if (!std::isfinite(strength)) { return; }
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

// 現在のシーンに、影を落とす物体の描画を依頼する。
void SceneManager::DrawShadow(ShadowMapRenderer& renderer)
{
    if (scene_) { scene_->DrawShadow(renderer); }
}
ShadowSettings SceneManager::GetShadowSettings() const
{
    if (scene_) { return scene_->GetShadowSettings(); }
    return {};
}

// 現在のシーンを表示するときの露出を設定する。
void SceneManager::SetSceneExposure(float exposure) {
    if (!std::isfinite(exposure) || exposure < 0.1f || exposure > 4.0f) { return; }
    sceneExposure_ = exposure;
    ++sceneExposureRevision_;
}
