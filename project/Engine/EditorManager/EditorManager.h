#pragma once

#include "Engine/Math/Ray.h"
#include <vector>
#ifdef USE_IMGUI
//#include "../../externals/imgui/ImGuizmo.h"
#endif
class Object3d;
class Camera;
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

    void SetSelectedObject(Object3d* object);
    void DrawGizmo(Camera* camera);

    void AddObject(Object3d* object);

private:
    Ray CreateMouseRay(Camera* camera);

private:
    GizmoMode gizmoMode_ = GizmoMode::Translate;
    std::vector<Object3d*> objects_;
    Object3d* selectedObject_ = nullptr;
};