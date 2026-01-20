#include "SkinCluster.h"

void SkinCluster::Update(const Skeleton& skeleton)
{
    const size_t jointCount = skeleton.joints.size();
    assert(jointCount == inverseBindPoseMatrices.size());

    for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {

        const Matrix4x4& skeletonSpace = skeleton.joints[jointIndex].skeletonSpaceMatrix;

      mappedPalette[jointIndex].skeletonSpaceMatrix = MatrixMath::Multiply(inverseBindPoseMatrices[jointIndex],skeletonSpace);

        mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = MatrixMath::Transpose(MatrixMath::Inverse(mappedPalette[jointIndex].skeletonSpaceMatrix));
    }
}
