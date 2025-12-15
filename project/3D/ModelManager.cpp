#include "ModelManager.h"

ModelManager* ModelManager::instance = nullptr;

ModelManager* ModelManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new ModelManager();
    }
    return instance;
}



void ModelManager::Initialize(DirectXCommon* dxCommon)
{
    modelCommon_ = new ModelCommon();
    modelCommon_->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
    if (instance) {
        instance->models.clear();

        delete instance->modelCommon_;
        instance->modelCommon_ = nullptr;

        delete instance;
        instance = nullptr;
    }
}

void ModelManager::LoadModel(const std::string& filepath)
{
    if (models.contains(filepath)) {
        return;
    }

    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(modelCommon_, "resources", filepath);

    models.insert(std::make_pair(filepath, std::move(model)));
}

Model* ModelManager::FindModel(const std::string& filePath)
{
    if (models.contains(filePath)) {
        return models[filePath].get();
    }
    return nullptr;
}
