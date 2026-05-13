#pragma once
#include "BaseScene.h"
#include <memory>

#include "Engine/PostEffect/PostEffectType.h"
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
    // PostEffectTypeのセッターとゲッター
    void SetPostEffectType(PostEffectType postEffectType);
    PostEffectType GetPostEffectType() const;

private:
    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    PostEffectType postEffectType_ = PostEffectType::Copy;

private:
    std::unique_ptr<BaseScene> scene_;
    std::unique_ptr<BaseScene> nextScene_;
};
