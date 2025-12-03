#include "ModelManager.h"

ModelManager* ModelManager::GetInstance()
{
    static ModelManager instance;
    return &instance;
}

void ModelManager::LoadModel(const std::string& filepath)
{
    // 読み込みモデルを検索
    if (models.contains(filepath)) {
        // 読み込み済みなら早期リターン
        return;
    }

    // モデルの製紙絵と読み込み初期化
    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(modelCommon_, "resources", filepath);

    // モデルをmapコンテナに登録
    models.insert(std::make_pair(filepath, std::move(model)));
}

Model* ModelManager::FindModel(const std::string& filePath)
{
    // 読み込み済みモデルを検索
    if (models.contains(filePath)) {
        return models[filePath].get();
    }

    // 見つからなかった場合はnullptrを返す
    return nullptr;
}

ModelManager::~ModelManager()
{
}
ModelManager::ModelManager()
{
}
