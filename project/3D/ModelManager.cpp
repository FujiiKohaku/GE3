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

Model* ModelManager::Load(const std::string& filepath)
{
    auto it = models.find(filepath);
    if (it != models.end()) {
        return it->second.get();
    }

    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_, "resources", filepath);

    Model* raw = model.get();
    models.emplace(filepath, std::move(model));
    return raw;
}


Model* ModelManager::FindModel(const std::string& filePath)
{
    if (models.contains(filePath)) {
        return models[filePath].get();
    }
    return nullptr;
}
