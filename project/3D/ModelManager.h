#pragma once
#include "Model.h"
#include <map>
#include <memory>
#include <string>
class ModelManager {
public:
    // 初期化
    void initialize(DirectXCommon* dxCommon)
    {

        modelCommon_ = new ModelCommon();
        modelCommon_->Initialize(dxCommon);
    }
    // インスタンスを取得する関数
    static ModelManager* GetInstance()
    {
        if (instance == nullptr) {
            instance = new ModelManager();
        }
        return instance;
    }

    // 終了時に呼ぶ
    static void Finalize()
    {
        delete instance;
        instance = nullptr;
    }

    void LoadModel(const std::string& filepath);

    Model* FindModel(const std::string& filePath);

private:
    // 唯一のインスタンス（静的）
    static ModelManager* instance;

    // コンストラクタ・デストラクタ
    ModelManager() = default;
    ~ModelManager() = default;

    // 複製禁止
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

    // モデルデータ
    std::map<std::string, std::unique_ptr<Model>> models;

    ModelCommon* modelCommon_ = nullptr;
};
