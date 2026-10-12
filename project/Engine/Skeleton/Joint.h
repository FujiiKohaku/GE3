#pragma once
#include "Engine/math/Object3DStruct.h"
#include <optional>
#include <string>
#include <vector>
class Skeleton;
class Joint {
public:
    const QuaternionTransform& GetTransform() const { return transform_; }
    const Matrix4x4& GetLocalMatrix() const { return localMatrix_; }
    const Matrix4x4& GetSkeletonSpaceMatrix() const { return skeletonSpaceMatrix_; }
    const std::string& GetName() const { return name_; }
    const std::vector<int32_t>& GetChildren() const { return children_; }
    int32_t GetIndex() const { return index_; }
    const std::optional<int32_t>& GetParent() const { return parent_; }
private:
    // Skeleton owns topology and derived matrices.
    friend class Skeleton;
    QuaternionTransform transform_ {};
    Matrix4x4 localMatrix_ {};
    Matrix4x4 skeletonSpaceMatrix_ {};
    std::string name_;
    std::vector<int32_t> children_;
    int32_t index_ = -1;
    std::optional<int32_t> parent_;
};
