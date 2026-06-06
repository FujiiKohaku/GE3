#include "EditorManager.h"
#include "../3D/Object3d.h"

#include "../Camera/Camera.h"
#include "../math/MatrixMath.h"
#ifdef USE_IMGUI
#include "../../externals/imgui/ImGuizmo.h"
#include "../externals/imgui/imgui.h"
#endif
#include "../Math/Collision.h"
#include "../Math/Sphere.h"
void EditorManager::Initialize()
{
}

void EditorManager::Update(Camera* camera)
{
#ifdef USE_IMGUI

    if (ImGui::IsMouseClicked(0)) {

        Ray ray = CreateMouseRay(camera);

        for (Object3d* object : objects_) {

            Sphere sphere;
            sphere.center = object->GetTranslate();
            sphere.radius = 1.0f;

            if (RaySphereIntersect(ray, sphere)) {

                SetSelectedObject(object);
                break;
            }
        }
    }

#endif
}
Ray EditorManager::CreateMouseRay(Camera* camera)
{
    Ray ray {};

#ifdef USE_IMGUI

    ImVec2 mousePosition = ImGui::GetMousePos();

    float mouseX = mousePosition.x;
    float mouseY = mousePosition.y;

    ImGuiIO& io = ImGui::GetIO();

    float ndcX = (2.0f * mouseX / io.DisplaySize.x) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / io.DisplaySize.y);

    Matrix4x4 inverseProjection = MatrixMath::Inverse(camera->GetProjectionMatrix());

    Matrix4x4 inverseView = MatrixMath::Inverse(camera->GetViewMatrix());

    Vector3 nearPoint = {
        ndcX,
        ndcY,
        0.0f
    };

    Vector3 farPoint = {
        ndcX,
        ndcY,
        1.0f
    };

    nearPoint = MatrixMath::Transform(nearPoint, inverseProjection);
    farPoint = MatrixMath::Transform(farPoint, inverseProjection);

    nearPoint = MatrixMath::Transform(nearPoint, inverseView);
    farPoint = MatrixMath::Transform(farPoint, inverseView);

    ray.origin = nearPoint;
    ray.direction = Normalize(farPoint - nearPoint);

#endif

    return ray;
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
    ImGui::Separator();
    ImGui::Text("Gizmo");

    if (ImGui::Button("Move")) {
        gizmoMode_ = GizmoMode::Translate;
    }

    ImGui::SameLine();

    if (ImGui::Button("Rotate")) {
        gizmoMode_ = GizmoMode::Rotate;
    }

    ImGui::SameLine();

    if (ImGui::Button("Scale")) {
        gizmoMode_ = GizmoMode::Scale;
    }
    ImGui::End();

#endif
}
void EditorManager::DrawGizmo(Camera* camera)
{
#ifdef USE_IMGUI
    ImGuizmo::BeginFrame();
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
    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;

    if (gizmoMode_ == GizmoMode::Rotate) {
        operation = ImGuizmo::ROTATE;
    }

    if (gizmoMode_ == GizmoMode::Scale) {
        operation = ImGuizmo::SCALE;
    }
    ImGuizmo::Manipulate(
        const_cast<float*>(&viewMatrix.m[0][0]),
        const_cast<float*>(&projectionMatrix.m[0][0]),
        operation,
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

        float translation[3];
        float rotation[3];
        float scale[3];

        ImGuizmo::DecomposeMatrixToComponents(
            &worldMatrix.m[0][0],
            translation,
            rotation,
            scale);

        selectedObject_->SetTranslate({ translation[0],
            translation[1],
            translation[2] });

        selectedObject_->SetScale({ scale[0],
            scale[1],
            scale[2] });
        float degreeToRadian = 3.1415926535f / 180.0f;

        Vector3 rotate;

        rotate.x = rotation[0] * degreeToRadian;
        rotate.y = rotation[1] * degreeToRadian;
        rotate.z = rotation[2] * degreeToRadian;

      //  selectedObject_->SetRotate(rotate);
    }

#endif
}
void EditorManager::SetSelectedObject(Object3d* object)
{
    selectedObject_ = object;
}
void EditorManager::AddObject(Object3d* object)
{
    objects_.push_back(object);
}