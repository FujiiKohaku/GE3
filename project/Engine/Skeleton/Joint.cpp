#include "Joint.h"
int32_t CreateJoint(const Node& node,const std::optional<int32_t>& parent,std::vector<Joint>& joints)
{
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = MatrixMath::MakeIdentity4x4();
    joint.transform = node.transform;
    joint.index = static_cast<int32_t>(joints.size());
    joint.parent = parent;

    joints.push_back(joint);

    for (const Node& child : node.children) {
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[joint.index].children.push_back(childIndex);
    }

    return joint.index;
}
