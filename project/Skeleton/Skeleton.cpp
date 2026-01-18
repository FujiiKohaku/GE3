#include "Skeleton.h"


Skeleton Skeleton::CreateSkeleton(const Node& rootNode)
{
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

    // 名前とindexのマッピングを行いアクセスしやすくする
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap[joint.name] = joint.index;
    }
    return skeleton;
}

void Skeleton::UpdateSkeleton()
{
    for (Joint& joint : joints) {

        joint.localMatrix = MatrixMath::MakeAffineMatrix(
            joint.transform.scale,
            joint.transform.rotate,
            joint.transform.translate);

        if (joint.parent.has_value()) {
            const Joint& parent = joints[joint.parent.value()];

            joint.skeletonSpaceMatrix = MatrixMath::Multiply(
                joint.localMatrix,
                parent.skeletonSpaceMatrix);
        } else {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}
