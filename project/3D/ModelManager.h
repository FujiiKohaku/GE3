#pragma once
#include "Model.h"
#include <map>
#include <memory>
#include <string>

class ModelManager {
public:
    // 追加：他Managerと揃える用
    void Initialize(DirectXCommon* dxCommon);

    // インスタンス取得
    static ModelManager* GetInstance();

    // 終了処理
    static void Finalize();

    Model* Load(const std::string& filepath);

    Model* FindModel(const std::string& filePath);

private:
    static ModelManager* instance;

    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    std::unordered_map<std::string, std::unique_ptr<Model>> models;
    ModelCommon* modelCommon_ = nullptr;
};
