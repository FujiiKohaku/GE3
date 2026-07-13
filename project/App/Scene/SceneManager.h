#pragma once
#include "BaseScene.h"
#include "Engine/Math/MathStruct.h"
#include <memory>
#include <vector>

#include "Engine/PostEffect/PostEffectType.h"

struct PostEffectInfo {
    PostEffectType type = PostEffectType::Copy;
    bool enabled = true;
    int priority = 0;
};

class SceneManager {
public:
    static SceneManager* GetInstance()
    {
        static SceneManager instance;
        return &instance;
    }

    // unique_ptrで受け取る
    void SetNextScene(std::unique_ptr<BaseScene> nextScene)
    {
        nextScene_ = std::move(nextScene);
    }

    void Update();
    void Finalize();
    void DrawImGui();
    void Draw2D();
    void Draw3D();
    void DrawParticle();
    // PostEffectTypeのセッターとゲッター
    void SetPostEffectType(PostEffectType postEffectType);
    PostEffectType GetPostEffectType() const;
    void AddPostEffect(PostEffectType type);
    void RemovePostEffect(PostEffectType type);
    void ClearPostEffects();
    void SetPostEffectEnabled(PostEffectType type, bool enable);
    const std::vector<PostEffectInfo>& GetPostEffects() const;
    void SetPostEffectCenter(const Vector2& center);
    const Vector2& GetPostEffectCenter() const;
    void SetPostEffectKickStrength(float strength);
    float GetPostEffectKickStrength() const;

private:
    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    PostEffectType postEffectType_ = PostEffectType::Copy;
    std::vector<PostEffectInfo> postEffects_;
    Vector2 postEffectCenter_ = { 0.5f, 0.5f };
    float postEffectKickStrength_ = 0.0f;

private:
    std::unique_ptr<BaseScene> scene_;
    std::unique_ptr<BaseScene> nextScene_;
    std::unique_ptr<BaseScene> retiredScene_;
};
