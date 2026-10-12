#pragma once
#include "Joint.h"
#include <map>
class Skeleton {
public:
    static Skeleton CreateSkeleton(const Node& rootNode);
    void UpdateSkeleton();
    int32_t GetRoot() const { return root_; }
    const std::map<std::string, int32_t>& GetJointMap() const { return jointMap_; }
    const std::vector<Joint>& GetJoints() const { return joints_; }
    bool SetJointTransform(int32_t index, const QuaternionTransform& transform);
    bool SetJointRotation(int32_t index, const Quaternion& rotation);
    bool OffsetJointTranslation(int32_t index, const Vector3& offset);
    bool ApplyPose(const Skeleton& pose);
private:
    int32_t CreateJoint(const Node& node, std::optional<int32_t> parent);
    int32_t root_ = -1;
    std::map<std::string, int32_t> jointMap_;
    std::vector<Joint> joints_;
};
