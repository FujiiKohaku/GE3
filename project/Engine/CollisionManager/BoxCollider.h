#pragma once
#include "Engine/CollisionManager/CollisionLayer.h"
#include "Engine/math/EngineStruct.h"

class BoxCollider {
public:
    void SetCenter(const Vector3& center) { center_ = center; }
    void SetSize(const Vector3& size) { size_ = size; }
    void SetRotation(const Vector3& rotation) { rotation_ = rotation; }
    void SetLayer(CollisionLayer category, CollisionLayer mask)
    {
        category_ = category;
        mask_ = mask;
    }

    const Vector3& GetCenter() const { return center_; }
    const Vector3& GetSize() const { return size_; }
    const Vector3& GetRotation() const { return rotation_; }
    CollisionLayer GetCategory() const { return category_; }
    CollisionLayer GetMask() const { return mask_; }

private:
    Vector3 center_ = { 0.0f, 0.0f, 0.0f };
    Vector3 size_ = { 1.0f, 1.0f, 1.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    CollisionLayer category_ = 1u;
    CollisionLayer mask_ = kCollisionLayerAll;
};
