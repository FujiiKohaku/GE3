#include "SceneObjectManager.h"

#include "../3D/ModelManager.h"
#include "../3D/Object3d.h"
#include "../3D/Object3dManager.h"

Object3d* SceneObjectManager::CreateObject(
    const std::string& name,
    const std::string& modelName)
{
    std::unique_ptr<Object3d> object = std::make_unique<Object3d>();

    object->Initialize(Object3dManager::GetInstance());

    ModelManager::GetInstance()->Load(modelName);

    object->SetModel(ModelManager::GetInstance()->FindModel(modelName));

    object->SetName(name);

    Object3d* createdObject = object.get();

    objects_.push_back(std::move(object));

    return createdObject;
}

void SceneObjectManager::DeleteObject(
    Object3d* object)
{
    for (std::vector<std::unique_ptr<Object3d>>::iterator iterator = objects_.begin();
        iterator != objects_.end();
        ++iterator) {

        if (iterator->get() == object) {

            objects_.erase(iterator);
            return;
        }
    }
}

Object3d* SceneObjectManager::FindObject(
    const std::string& name)
{
    for (const std::unique_ptr<Object3d>& object : objects_) {

        if (object->GetName() == name) {
            return object.get();
        }
    }

    return nullptr;
}

const std::vector<std::unique_ptr<Object3d>>&
SceneObjectManager::GetObjects() const
{
    return objects_;
}

void SceneObjectManager::Update()
{
    for (std::unique_ptr<Object3d>& object : objects_) {
        object->Update();
    }
}

void SceneObjectManager::Draw()
{
    for (std::unique_ptr<Object3d>& object : objects_) {
        object->Draw();
    }
}