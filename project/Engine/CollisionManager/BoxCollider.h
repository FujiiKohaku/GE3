#pragma once
#include "Engine/math/ValueValidation.h"
#include "Engine/CollisionManager/CollisionLayer.h"
#include "Engine/math/EngineStruct.h"
#include "Engine/CollisionManager/CollisionManager.h"

class BoxCollider {
public:
    void SetCenter(const Vector3& center)
    {
        if (Same(center_, center)) { return; }
        center_ = center;
        // 平行移動では向き・大きさを計算し直す必要がない。
        obb_.center = center;
        aabb_.center = center;
    }
    void SetSize(const Vector3& size)
    {
        if (!ValueValidation::IsFinite(size) || size.x < 0 || size.y < 0 || size.z < 0 || Same(size_, size)) { return; }
        size_ = size;
        boundsDirty_ = true;
    }
    void SetRotation(const Vector3& rotation)
    {
        if (Same(rotation_, rotation)) { return; }
        rotation_ = rotation;
        boundsDirty_ = true;
    }
    void SetLayer(CollisionLayer category, CollisionLayer mask)
    {
        category_ = category;
        mask_ = mask;
    }

    const Vector3& GetCenter() const { return center_; }
    const Vector3& GetSize() const { return size_; }
    const Vector3& GetRotation() const { return rotation_; }
    // 変更後の最初の参照時にだけ更新し、同じ箱を全クエリで共有する。
    const OBB& GetOBB() const { UpdateBounds(); return obb_; }
    const AABB& GetAABB() const { UpdateBounds(); return aabb_; }
    CollisionLayer GetCategory() const { return category_; }
    CollisionLayer GetMask() const { return mask_; }

private:
    static bool Same(const Vector3& a, const Vector3& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    void UpdateBounds() const
    {
        if (!boundsDirty_) { return; }
        obb_ = CollisionManager::MakeOBB(center_, size_, rotation_);
        aabb_.center = center_;
        // 回転したOBB全体を囲む、ワールド軸に平行なAABB。
        aabb_.size = {
            std::abs(obb_.orientation[0].x) * size_.x + std::abs(obb_.orientation[1].x) * size_.y + std::abs(obb_.orientation[2].x) * size_.z,
            std::abs(obb_.orientation[0].y) * size_.x + std::abs(obb_.orientation[1].y) * size_.y + std::abs(obb_.orientation[2].y) * size_.z,
            std::abs(obb_.orientation[0].z) * size_.x + std::abs(obb_.orientation[1].z) * size_.y + std::abs(obb_.orientation[2].z) * size_.z
        };
        boundsDirty_ = false;
    }
    Vector3 center_ = { 0.0f, 0.0f, 0.0f };
    Vector3 size_ = { 1.0f, 1.0f, 1.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    mutable OBB obb_ {};
    mutable AABB aabb_ {};
    mutable bool boundsDirty_ = false;
    CollisionLayer category_ = 1u;
    CollisionLayer mask_ = kCollisionLayerAll;
};
