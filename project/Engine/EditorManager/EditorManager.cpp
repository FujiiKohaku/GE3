#include "EditorManager.h"

#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

void EditorManager::Initialize()
{
}

void EditorManager::Update()
{
}

void EditorManager::DrawImGui()
{
#ifdef USE_IMGUI

    ImGui::Begin("Editor");

    if (selectedObject_ == nullptr) {
        ImGui::Text("No Selection");
    } else {
        ImGui::Text("Object Selected");
    }

    ImGui::End();

#endif
}

void EditorManager::SetSelectedObject(Object3d* object)
{
    selectedObject_ = object;
}