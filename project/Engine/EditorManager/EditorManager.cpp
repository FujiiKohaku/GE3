#include "EditorManager.h"
#include "../3D/Object3d.h"

#include "../Camera/Camera.h"
#include "../math/MatrixMath.h"
#ifdef USE_IMGUI
#include "../../externals/imgui/ImGuizmo.h"
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

        Vector3 translate = selectedObject_->GetTranslate();

        if (ImGui::DragFloat3("Translate", &translate.x, 0.1f)) {

            selectedObject_->SetTranslate(translate);
        }
    }

    ImGui::End();

#endif
}
void EditorManager::DrawGizmo(Camera* camera)
{
#ifdef USE_IMGUI

    if (selectedObject_ == nullptr) {
        return;
    }

    Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(
        selectedObject_->GetScale(),
        selectedObject_->GetRotate(),
        selectedObject_->GetTranslate());

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

    ImGuiIO& io = ImGui::GetIO();

    ImGuizmo::SetRect(
        0.0f,
        0.0f,
        io.DisplaySize.x,
        io.DisplaySize.y);

    const Matrix4x4& viewMatrix = camera->GetViewMatrix();
    const Matrix4x4& projectionMatrix = camera->GetProjectionMatrix();

    ImGuizmo::Manipulate(
        const_cast<float*>(&viewMatrix.m[0][0]),
        const_cast<float*>(&projectionMatrix.m[0][0]),
        ImGuizmo::TRANSLATE,
        ImGuizmo::LOCAL,
        &worldMatrix.m[0][0]);

    ImGui::Begin("Debug");

    ImGui::Text("IsOver : %d", ImGuizmo::IsOver());
    ImGui::Text("IsUsing : %d", ImGuizmo::IsUsing());
    ImGui::Text(
        "Pos : %.2f %.2f %.2f",
        worldMatrix.m[3][0],
        worldMatrix.m[3][1],
        worldMatrix.m[3][2]);
    ImGui::End();

    if (ImGuizmo::IsUsing()) {

        Vector3 translate;

        translate.x = worldMatrix.m[3][0];
        translate.y = worldMatrix.m[3][1];
        translate.z = worldMatrix.m[3][2];

        selectedObject_->SetTranslate(translate);
    }

#endif
}
void EditorManager::SetSelectedObject(Object3d* object)
{
    selectedObject_ = object;
}