#pragma once
#include "BaseScene.h"

class SceneManager {
public:
    // --------- Singleton取得 ---------
    static SceneManager* GetInstance()
    {
        static SceneManager instance;
        return &instance;
    }

    // --------- 次シーン予約 ---------
    void SetNextScene(BaseScene* nextScene)
    {
        nextScene_ = nextScene;
    }

    // --------- 実行 ---------
    void Update();
    void Draw();
    void Finalize();

private:
    // --------- Singleton基本処理 ---------
    SceneManager() = default;
    ~SceneManager();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

private:
    BaseScene* scene_ = nullptr;
    BaseScene* nextScene_ = nullptr;
};
