#include "Skeleton.h"
#include "Engine/math/ValueValidation.h"
#include "Engine/math/MatrixMath.h"
#include <stdexcept>
int32_t Skeleton::CreateJoint(const Node& node, std::optional<int32_t> parent)
{
    if (!ValueValidation::IsValidTransform(node.transform)) throw std::invalid_argument("Invalid joint transform");
    Joint joint;
    joint.name_ = node.name;
    joint.transform_ = node.transform;
    joint.index_ = static_cast<int32_t>(joints_.size());
    joint.parent_ = parent;
    const int32_t index = joint.index_;
    joints_.push_back(joint);
    for (const auto& child : node.children) {
        const int32_t childIndex = CreateJoint(child, index);
        joints_[index].children_.push_back(childIndex);
    }
    return index;
}
Skeleton Skeleton::CreateSkeleton(const Node& rootNode)
{
    Skeleton skeleton;
    skeleton.root_ = skeleton.CreateJoint(rootNode, {});
    for (const auto& joint : skeleton.joints_) skeleton.jointMap_[joint.name_] = joint.index_;
    skeleton.UpdateSkeleton();
    return skeleton;
}
bool Skeleton::SetJointTransform(int32_t index, const QuaternionTransform& transform)
{
    if (index < 0 || static_cast<size_t>(index) >= joints_.size() || !ValueValidation::IsValidTransform(transform)) return false;
    joints_[index].transform_ = transform;
    UpdateSkeleton();
    return true;
}
bool Skeleton::SetJointRotation(int32_t index, const Quaternion& rotation)
{
    if (index < 0 || static_cast<size_t>(index) >= joints_.size()) return false;
    auto transform = joints_[index].transform_;
    transform.rotate = rotation;
    return SetJointTransform(index, transform);
}
bool Skeleton::OffsetJointTranslation(int32_t index, const Vector3& offset)
{
    if (index < 0 || static_cast<size_t>(index) >= joints_.size() || !ValueValidation::IsFinite(offset)) return false;
    auto transform = joints_[index].transform_;
    transform.translate = transform.translate + offset;
    return SetJointTransform(index, transform);
}
bool Skeleton::ApplyPose(const Skeleton& pose)
{
    if (joints_.size() != pose.joints_.size() || root_ != pose.root_ || jointMap_ != pose.jointMap_) return false;
    for (size_t i = 0; i < joints_.size(); ++i) {
        if (joints_[i].parent_ != pose.joints_[i].parent_ || joints_[i].children_ != pose.joints_[i].children_ ||
            joints_[i].name_ != pose.joints_[i].name_ || !ValueValidation::IsValidTransform(pose.joints_[i].transform_)) return false;
    }
    for (size_t i = 0; i < joints_.size(); ++i) joints_[i].transform_ = pose.joints_[i].transform_;
    UpdateSkeleton();
    return true;
}
void Skeleton::UpdateSkeleton()
{
    for (auto& joint : joints_) {
        joint.localMatrix_ = MatrixMath::MakeAffineMatrix(joint.transform_.scale, joint.transform_.rotate, joint.transform_.translate);
        joint.skeletonSpaceMatrix_ = joint.parent_ ? MatrixMath::Multiply(joint.localMatrix_, joints_[*joint.parent_].skeletonSpaceMatrix_) : joint.localMatrix_;
    }
}
