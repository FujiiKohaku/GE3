#pragma once

class Object3d;

class EditorManager {
public:
    EditorManager() = default;
    ~EditorManager() = default;

    void Initialize();
    void Update();
    void DrawImGui();
    void SetSelectedObject(Object3d* object);

private:
    Object3d* selectedObject_ = nullptr;
};