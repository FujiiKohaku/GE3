#pragma once
#include "BaseScene.h"
#include <memory>

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

private:
    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

private:
    std::unique_ptr<BaseScene> scene_;
    std::unique_ptr<BaseScene> nextScene_;
};
