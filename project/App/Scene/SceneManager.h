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

    // ロード画面を挟んでシーン遷移するテンプレート関数
    template <typename TLoadingScene, typename TTargetScene, typename... Args>
    void SetNextSceneWithLoading(Args&&... args)
    {
        auto targetScene = std::make_unique<TTargetScene>(std::forward<Args>(args)...);
        auto loadingScene = std::make_unique<TLoadingScene>(std::move(targetScene));
        SetNextScene(std::move(loadingScene));
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
    void SetPaintProgress(float progress) { paintProgress_ = progress; }
    float GetPaintProgress() const { return paintProgress_; }
    void SetPaintIntensity(float intensity) { paintIntensity_ = intensity; }
    float GetPaintIntensity() const { return paintIntensity_; }
    void SetPaintSeed(float seed) { paintSeed_ = seed; }
    float GetPaintSeed() const { return paintSeed_; }
    void SetPaintPatternType(int type) { paintPatternType_ = type; }
    int GetPaintPatternType() const { return paintPatternType_; }
    void SetPaintColor(const Vector3& color) { paintColor_ = color; }
    const Vector3& GetPaintColor() const { return paintColor_; }

private:
    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    PostEffectType postEffectType_ = PostEffectType::Copy;
    std::vector<PostEffectInfo> postEffects_;
    Vector2 postEffectCenter_ = { 0.5f, 0.5f };
    float postEffectKickStrength_ = 0.0f;
    float paintProgress_ = 0.0f;
    float paintIntensity_ = 0.0f;
    float paintSeed_ = 0.0f;
    int paintPatternType_ = 0;
    Vector3 paintColor_ = { 0.95f, 0.10f, 0.58f };

private:
    std::unique_ptr<BaseScene> scene_;
    std::unique_ptr<BaseScene> nextScene_;
    std::unique_ptr<BaseScene> retiredScene_;
};
