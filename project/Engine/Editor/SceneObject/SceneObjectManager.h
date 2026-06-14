#pragma once

#include <memory>
#include <string>
#include <vector>

class Object3d;

class SceneObjectManager {
public:
    SceneObjectManager() = default;
    ~SceneObjectManager() = default;

    Object3d* CreateObject(
        const std::string& name,
        const std::string& modelName);

    void DeleteObject(Object3d* object);

    Object3d* FindObject(
        const std::string& name);

    const std::vector<std::unique_ptr<Object3d>>& GetObjects() const;

    void Update();
    void Draw();

private:
    std::vector<std::unique_ptr<Object3d>> objects_;
};