#include "EditorManager.h"
#include "Engine/3D/Object3d.h"

#include "Engine/Camera/Camera.h"
#include "Engine/Math/MatrixMath.h"
#ifdef USE_IMGUI
#include "externals/imgui/ImGuizmo.h"
#include "externals/imgui/imgui.h"
#endif
#include "Engine/Math/Collision.h"
#include "Engine/Math/Sphere.h"

#include "externals/json.hpp"

#include "Engine/Editor/SceneObject/SceneObjectManager.h"
void EditorManager::Initialize()
{
}

void EditorManager::Update(Camera* camera)
{
#ifdef USE_IMGUI

    // マウスクリックでオブジェクト選択
    if (ImGui::IsMouseClicked(0)) {

        // カメラからマウス位置へのレイを作成
        Ray ray = CreateMouseRay(camera);

        // 読み込まれたオブジェクトの中身をobjects_に入れる
        for (const std::unique_ptr<Object3d>& object : sceneObjectManager_->GetObjects()) {

            // objectはstd::unique_ptrなので、get()でObject3d*を取得 
            Object3d* currentObject = object.get();

            
            Sphere sphere;
            sphere.center = currentObject->GetTranslate();
            sphere.radius = 1.0f; // 球の半径は仮の値です。オブジェクトのサイズに応じて適切な値を設定してください。

            if (RaySphereIntersect(ray, sphere)) {

                SetSelectedObject(currentObject);
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

    // =====================================
    // Hierarchy
    // =====================================

    if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {

        for (const std::unique_ptr<Object3d>& object : sceneObjectManager_->GetObjects()) {

            Object3d* currentObject = object.get();

            bool isSelected = false;

            if (currentObject == selectedObject_) {
                isSelected = true;
            }

            if (ImGui::Selectable(currentObject->GetName().c_str(), isSelected)) {

                selectedObject_ = currentObject;
            }
        }
    }

    ImGui::Separator();

    // =====================================
    // Inspector
    // =====================================

    if (ImGui::CollapsingHeader("Inspector", ImGuiTreeNodeFlags_DefaultOpen)) {

        if (selectedObject_ == nullptr) {

            ImGui::Text("No Selection");

        } else {

            ImGui::Text(
                "Selected : %s",
                selectedObject_->GetName().c_str());

            ImGui::Separator();

            Vector3 translate = selectedObject_->GetTranslate();

            if (ImGui::DragFloat3(
                    "Position",
                    &translate.x,
                    0.1f)) {

                selectedObject_->SetTranslate(translate);
            }

            Vector3 rotate = selectedObject_->GetRotate();

            if (ImGui::DragFloat3(
                    "Rotation",
                    &rotate.x,
                    0.01f)) {

                selectedObject_->SetRotate(rotate);
            }

            Vector3 scale = selectedObject_->GetScale();

            if (ImGui::DragFloat3(
                    "Scale",
                    &scale.x,
                    0.01f)) {

                selectedObject_->SetScale(scale);
            }
        }
    }

    ImGui::Separator();

    // =====================================
    // Gizmo
    // =====================================

    if (ImGui::CollapsingHeader("Gizmo", ImGuiTreeNodeFlags_DefaultOpen)) {

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
    }
    if (ImGui::Button("Save")) {
        SaveJson("Resources/Scene/TestScene.json");
    }
    if (ImGui::Button("Load")) {
        LoadJson("Resources/Scene/TestScene.json");
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

    ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

    const Matrix4x4& viewMatrix = camera->GetViewMatrix();
    const Matrix4x4& projectionMatrix = camera->GetProjectionMatrix();
    // 操作モードに応じてImGuizmoの操作を切り替える
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

void EditorManager::SaveJson(const std::string& filePath)
{
    nlohmann::json root;

    root["Objects"] = nlohmann::json::array();

    for (const std::unique_ptr<Object3d>& object :
        sceneObjectManager_->GetObjects()) {

        Object3d* currentObject = object.get();

        nlohmann::json objectData;

        objectData["Name"] = currentObject->GetName();

        objectData["Translate"] = {
            currentObject->GetTranslate().x,
            currentObject->GetTranslate().y,
            currentObject->GetTranslate().z
        };

        objectData["Rotate"] = {
            currentObject->GetRotate().x,
            currentObject->GetRotate().y,
            currentObject->GetRotate().z
        };

        objectData["Scale"] = {
            currentObject->GetScale().x,
            currentObject->GetScale().y,
            currentObject->GetScale().z
        };

        root["Objects"].push_back(objectData);
    }

    std::ofstream file(filePath);

    if (file.is_open()) {
        file << root.dump(4);
        file.close();
    }
}
void EditorManager::LoadJson(const std::string& filePath)
{
    std::ifstream file(filePath);

    if (!file.is_open()) {
        return;
    }

    nlohmann::json root;
    file >> root;

    for (const auto& objectData : root["Objects"]) {

        std::string objectName = objectData["Name"];

        Vector3 translate;

        translate.x = objectData["Translate"][0];

        translate.y = objectData["Translate"][1];

        translate.z = objectData["Translate"][2];

        OutputDebugStringA((objectName + "\n").c_str());
    }
}
void EditorManager::SetSceneObjectManager(SceneObjectManager* sceneObjectManager)
{
    sceneObjectManager_ = sceneObjectManager;
}