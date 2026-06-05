#pragma once

class Object3d;
class Camera;
class EditorManager {
public:
    EditorManager() = default;
    ~EditorManager() = default;

    void Initialize();
    void Update();
    void DrawImGui();
    void SetSelectedObject(Object3d* object);
    void DrawGizmo(Camera* camera);

private:
    Object3d* selectedObject_ = nullptr;
};