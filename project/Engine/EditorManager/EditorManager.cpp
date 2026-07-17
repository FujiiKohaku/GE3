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

#include "../externals/json.hpp"

#include "../../Engine/SceneObjectManager/SceneObjectManager.h"
#include "../../Engine/LevelEditor/LevelDataLoader.h"
void EditorManager::Initialize()
{
}

void EditorManager::Update(Camera* camera)
{
#ifdef USE_IMGUI

    // 繝槭え繧ｹ繧ｯ繝ｪ繝・け縺ｧ繧ｪ繝悶ず繧ｧ繧ｯ繝磯∈謚・
    if (ImGui::IsMouseClicked(0)) {

        // 繧ｫ繝｡繝ｩ縺九ｉ繝槭え繧ｹ菴咲ｽｮ縺ｸ縺ｮ繝ｬ繧､繧剃ｽ懈・
        Ray ray = CreateMouseRay(camera);

        // 隱ｭ縺ｿ霎ｼ縺ｾ繧後◆繧ｪ繝悶ず繧ｧ繧ｯ繝医・荳ｭ霄ｫ繧弛bjects_縺ｫ蜈･繧後ｋ
        for (const std::unique_ptr<Object3d>& object : sceneObjectManager_->GetObjects()) {

            // object縺ｯstd::unique_ptr縺ｪ縺ｮ縺ｧ縲“et()縺ｧObject3d*繧貞叙蠕・
            Object3d* currentObject = object.get();

            
            Sphere sphere;
            sphere.center = currentObject->GetTranslate();
            sphere.radius = 1.0f; // 逅・・蜊雁ｾ・・莉ｮ縺ｮ蛟､縺ｧ縺吶ゅが繝悶ず繧ｧ繧ｯ繝医・繧ｵ繧､繧ｺ縺ｫ蠢懊§縺ｦ驕ｩ蛻・↑蛟､繧定ｨｭ螳壹＠縺ｦ縺上□縺輔＞縲・

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
        SaveJson("resources/Scenes/TestScene.json");
    }
    if (ImGui::Button("Load")) {
        LoadJson("resources/Scenes/TestScene.json");
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
    // 謫堺ｽ懊Δ繝ｼ繝峨↓蠢懊§縺ｦImGuizmo縺ｮ謫堺ｽ懊ｒ蛻・ｊ譖ｿ縺医ｋ
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

    root["scene"] = nlohmann::json::array();

    for (const std::unique_ptr<Object3d>& object :
        sceneObjectManager_->GetObjects()) {

        Object3d* currentObject = object.get();

        nlohmann::json objectData;

        objectData["name"] = currentObject->GetName();
        objectData["type"] = "MESH";

        if (!currentObject->GetModelFilePath().empty()) {
            objectData["file_name"] = currentObject->GetModelFilePath();
        }

        nlohmann::json transformData;

        transformData["translation"] = {
            currentObject->GetTranslate().x,
            currentObject->GetTranslate().z,
            currentObject->GetTranslate().y
        };

        transformData["rotation"] = {
            -currentObject->GetRotate().x,
            -currentObject->GetRotate().z,
            -currentObject->GetRotate().y
        };

        transformData["scale"] = {
            currentObject->GetScale().x,
            currentObject->GetScale().z,
            currentObject->GetScale().y
        };

        objectData["transform"] = transformData;
        root["scene"].push_back(objectData);
    }

    std::ofstream file(filePath);

    if (file.is_open()) {
        file << root.dump(4);
        file.close();
    }
}
void EditorManager::LoadJson(const std::string& filePath)
{
    if (sceneObjectManager_ == nullptr) {
        return;
    }

    LevelDataLoader loader;
    LevelData levelData = loader.Load(filePath);

    for (const LevelData::ObjectData& objectData : levelData.objects) {
        if (objectData.type != "MESH") {
            continue;
        }

        Object3d* object = sceneObjectManager_->FindObject(objectData.name);
        if (object == nullptr && !objectData.fileName.empty()) {
            object = sceneObjectManager_->CreateObject(objectData.name, objectData.fileName);
        }

        if (object == nullptr) {
            continue;
        }

        object->SetTranslate(objectData.translation);
        object->SetRotate(objectData.rotation);
        object->SetScale(objectData.scale);
    }
}
void EditorManager::SetSceneObjectManager(
    SceneObjectManager* sceneObjectManager)
{
    sceneObjectManager_ = sceneObjectManager;
}
