#pragma once

#include "Engine/Math/Ray.h"
#include <vector>

#include <string>
#ifdef USE_IMGUI
// #include "externals/imgui/ImGuizmo.h"
#endif
class Object3d;
class Camera;
class SceneObjectManager;
enum class GizmoMode {
    Translate,
    Rotate,
    Scale
};
class EditorManager {
public:
    EditorManager() = default;
    ~EditorManager() = default;

    void Initialize();
    void Update(Camera* camera);
    void DrawImGui();
    void SaveJson(const std::string& filePath);
    void LoadJson(const std::string& filePath);

    void SetSelectedObject(Object3d* object);
    void DrawGizmo(Camera* camera);

    //void AddObject(Object3d* object);
    void SetSceneObjectManager(SceneObjectManager* sceneObjectManager);

private:
    Ray CreateMouseRay(Camera* camera);

private:
    GizmoMode gizmoMode_ = GizmoMode::Translate;
   // std::vector<Object3d*> objects_;
    Object3d* selectedObject_ = nullptr;
    SceneObjectManager* sceneObjectManager_ = nullptr;
};