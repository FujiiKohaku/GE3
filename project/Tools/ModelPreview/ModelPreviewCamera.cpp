#include "ModelPreviewApp.h"
#include "Engine/Camera/Camera.h"
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <cmath>

Matrix4x4 ModelPreviewApp::ItemTransform(const PreviewItem& item) const
{
    Matrix4x4 transform = MatrixMath::MakeAffineMatrix(item.axisScale * item.scale,
        item.rotation, item.position);
    if (item.stageTransform) { return transform; }
    Matrix4x4 recenter = MatrixMath::MakeTranslateMatrix(item.center * -1.0f);
    return MatrixMath::Multiply(recenter, transform);
}

Vector3 ModelPreviewApp::ItemCenter(const PreviewItem& item) const
{
    return MatrixMath::Transform(item.center, ItemTransform(item));
}

void ModelPreviewApp::Fit()
{
    target_ = {};
    sceneRadius_ = 1.0f;
    for (const auto& item : items_) { target_ = target_ + ItemCenter(*item); }
    if (!items_.empty()) { target_ = target_ * (1.0f / static_cast<float>(items_.size())); }
    for (const auto& item : items_) {
        Vector3 delta = ItemCenter(*item) - target_;
        float offset = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        float largestScale = (std::max)({ std::abs(item->axisScale.x),
            std::abs(item->axisScale.y), std::abs(item->axisScale.z) });
        sceneRadius_ = (std::max)(sceneRadius_, item->radius * item->scale * largestScale + offset);
    }
    // Fit projected vertices instead of a max-scale sphere. The latter places
    // wide, shallow ice islands unnecessarily far into the game's distance fog.
    float tangentY = std::tan(camera_->GetFovY() * 0.5f);
    float tangentX = tangentY * camera_->GetAspectRatio();
    Vector3 right { std::cos(orbitYaw_), 0.0f, std::sin(orbitYaw_) };
    Vector3 up { -std::sin(orbitYaw_) * std::sin(orbitPitch_), std::cos(orbitPitch_),
        std::cos(orbitYaw_) * std::sin(orbitPitch_) };
    Vector3 back { std::sin(orbitYaw_) * std::cos(orbitPitch_), std::sin(orbitPitch_),
        -std::cos(orbitYaw_) * std::cos(orbitPitch_) };
    distance_ = 1.0f;
    for (const auto& item : items_) {
        Matrix4x4 world = ItemTransform(*item);
        for (const auto& primitive : item->model->GetModelData().primitives) {
            for (const auto& vertex : primitive.vertices) {
                Vector3 local { vertex.position.x, vertex.position.y, vertex.position.z };
                Vector3 delta = MatrixMath::Transform(local, world) - target_;
                float x = delta.x * right.x + delta.y * right.y + delta.z * right.z;
                float y = delta.x * up.x + delta.y * up.y + delta.z * up.z;
                float z = delta.x * back.x + delta.y * back.y + delta.z * back.z;
                distance_ = (std::max)(distance_, std::abs(x) / tangentX + z);
                distance_ = (std::max)(distance_, std::abs(y) / tangentY + z);
            }
        }
    }
    distance_ = distance_ * 1.10f + sceneRadius_ * 0.05f;
}
