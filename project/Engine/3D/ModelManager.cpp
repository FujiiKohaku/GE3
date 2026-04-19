#include "ModelManager.h"
std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;


ModelManager* ModelManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<ModelManager>(ConstructorKey());
    }
    return instance_.get();
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
    instance_.reset();
}

Model* ModelManager::Load(const std::string& filepath)
{
    auto it = models_.find(filepath);
    if (it != models_.end()) {
        return it->second.get();
    }

    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_.get(), "resources", filepath);

    Model* raw = model.get();
    models_.emplace(filepath, std::move(model));
    return raw;
}

Model* ModelManager::FindModel(const std::string& filepath)
{
    auto it = models_.find(filepath);
    if (it != models_.end()) {
        return it->second.get();
    }
    return nullptr;
}

ModelManager::ModelManager(ConstructorKey)
{
}
