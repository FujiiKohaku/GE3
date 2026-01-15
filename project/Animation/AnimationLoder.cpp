#include "AnimationLoder.h"

Animation AnimationLoder::LoadAnimationFile(const std::string& directoryPath,
    const std::string& filename)
{
    Animation animation;
    Assimp::Importer importer;

    std::string filePath = directoryPath + "/" + filename;

    const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
    assert(scene);
    assert(scene->mNumAnimations > 0);

    aiAnimation* animAssimp = scene->mAnimations[0];

    animation.duration = static_cast<float>(animAssimp->mDuration / animAssimp->mTicksPerSecond);

    for (uint32_t channelIndex = 0; channelIndex < animAssimp->mNumChannels; ++channelIndex) {

        aiNodeAnim* nodeAnimAssimp = animAssimp->mChannels[channelIndex];

        NodeAnimation& nodeAnim = animation.nodeAnimations[nodeAnimAssimp->mNodeName.C_Str()];

        // --------------------
        // Translate
        // --------------------
        for (uint32_t i = 0; i < nodeAnimAssimp->mNumPositionKeys; ++i) {
            aiVectorKey& key = nodeAnimAssimp->mPositionKeys[i];

            KeyframeVector3 kf;
            kf.time = static_cast<float>(key.mTime / animAssimp->mTicksPerSecond);
            kf.value = {
                -key.mValue.x,
                key.mValue.y,
                key.mValue.z
            };

            nodeAnim.translate.push_back(kf);
        }

        // --------------------
        // Rotate (Quaternion)
        // --------------------
        for (uint32_t i = 0; i < nodeAnimAssimp->mNumRotationKeys; ++i) {
            aiQuatKey& key = nodeAnimAssimp->mRotationKeys[i];

            KeyframeQuaternion kf;
            kf.time = static_cast<float>(key.mTime / animAssimp->mTicksPerSecond);
            kf.value = {
                -key.mValue.x,
                key.mValue.y,
                key.mValue.z,
                -key.mValue.w
            };

            nodeAnim.rotation.push_back(kf);
        }

        // --------------------
        // Scale
        // --------------------
        for (uint32_t i = 0; i < nodeAnimAssimp->mNumScalingKeys; ++i) {
            aiVectorKey& key = nodeAnimAssimp->mScalingKeys[i];

            KeyframeVector3 kf;
            kf.time = static_cast<float>(key.mTime / animAssimp->mTicksPerSecond);
            kf.value = {
                key.mValue.x,
                key.mValue.y,
                key.mValue.z
            };

            nodeAnim.scale.push_back(kf);
        }
    }

    return animation;
}
