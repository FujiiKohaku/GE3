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
Vector3 PlayAnimation::CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time)
{
    assert(!keyframes.empty());

    // キーが1個 or 最初より前
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    // キーフレーム間を探す
    for (size_t index = 0; index + 1 < keyframes.size(); ++index) {
        size_t nextIndex = index + 1;

        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {

            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);

            return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }

    // 最後より後
    return keyframes.back().value;
}

Quaternion PlayAnimation::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) 
{
    assert(!keyframes.empty());

    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t index = 0; index + 1 < keyframes.size(); ++index) {
        size_t nextIndex = index + 1;

        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {

            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);

            return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }

    return keyframes.back().value;
}

Matrix4x4 PlayAnimation::GetLocalMatrix(const std::string& nodeName) 
{
    assert(animation_);

    const NodeAnimation& nodeAnim = animation_->nodeAnimations.at(nodeName);

    Vector3 translate = CalculateValue(nodeAnim.translate, animationTime_);

    Quaternion rotate = CalculateValue(nodeAnim.rotation, animationTime_);

    Vector3 scale = CalculateValue(nodeAnim.scale, animationTime_);

    return MatrixMath::MakeAffineMatrix(scale, rotate, translate);
}


