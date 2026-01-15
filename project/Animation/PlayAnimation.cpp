#include "PlayAnimation.h"
#include "Animation.h"
#include "NodeAnimation.h"
#include <cassert>
#include <cmath>

void PlayAnimation::SetAnimation(const Animation* animation)
{
    animation_ = animation;
    animationTime_ = 0.0f;
}

void PlayAnimation::Update(float deltaTime)
{
    if (!animation_)
        return;

    animationTime_ += deltaTime;

    if (animationTime_ > animation_->duration) {
        animationTime_ = fmod(animationTime_, animation_->duration);
    }
}
Vector3 PlayAnimation::CalculateValue(const std::vector<KeyframeVector3>& keyframes,float time)
{
    //  NodeMisc 対応
    if (keyframes.empty()) {
        return { 0.0f, 0.0f, 0.0f }; // default translate
    }

    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
        if (keyframes[i].time <= time && time <= keyframes[i + 1].time) {
            float t = (time - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
            return Lerp(keyframes[i].value, keyframes[i + 1].value, t);
        }
    }

    return keyframes.back().value;
}


Quaternion PlayAnimation::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes,float time)
{
    //  NodeMisc 対応
    if (keyframes.empty()) {
        return { 0.0f, 0.0f, 0.0f, 1.0f }; // identity
    }

    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
        if (keyframes[i].time <= time && time <= keyframes[i + 1].time) {
            float t = (time - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
            return Slerp(keyframes[i].value, keyframes[i + 1].value, t);
        }
    }

    return keyframes.back().value;
}


Matrix4x4 PlayAnimation::GetLocalMatrix(const std::string& nodeName)
{
    assert(animation_);

    auto it = animation_->nodeAnimations.find(nodeName);
    if (it == animation_->nodeAnimations.end()) {
        return MatrixMath::MakeIdentity4x4();// アニメが無いノード → identity
    }

    const NodeAnimation& nodeAnim = it->second;

    Vector3 translate = CalculateValue(nodeAnim.translate, animationTime_);
    Quaternion rotate = CalculateValue(nodeAnim.rotation, animationTime_);
    Vector3 scale = CalculateValue(nodeAnim.scale, animationTime_);

    return MatrixMath::MakeAffineMatrix(scale, rotate, translate);
}



