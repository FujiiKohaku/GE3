#include "SceneManager.h"
#include <cassert>



void SceneManager::Update()
{
    if (nextScene_) {

        if (scene_) {
            scene_->Finalize();
        }

        scene_ = std::move(nextScene_);

        scene_->Initialize();
    }

    if (scene_) {
        scene_->Update();
    }
}

void SceneManager::Finalize()
{
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
    AddPostEffect(postEffectType);
    AddPostEffect(PostEffectType::Fog);
    postEffectType_ = postEffectType;
}

PostEffectType SceneManager::GetPostEffectType() const
{
    return postEffectType_;
}

void SceneManager::AddPostEffect(PostEffectType type)
{
    for (const PostEffectInfo& postEffect : postEffects_) {
        if (postEffect.type == type) {
            return;
        }
    }

    PostEffectInfo postEffect;
    postEffect.type = type;
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

void SceneManager::SetPostEffectKickStrength(float strength)
{
    postEffectKickStrength_ = strength;
    if (postEffectKickStrength_ < 0.0f) {
        postEffectKickStrength_ = 0.0f;
    }

    if (postEffectKickStrength_ > 1.0f) {
        postEffectKickStrength_ = 1.0f;
    }
}

float SceneManager::GetPostEffectKickStrength() const
{
    return postEffectKickStrength_;
}
