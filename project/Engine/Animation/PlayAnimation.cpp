#include "PlayAnimation.h"
#include "Animation.h"
#include "NodeAnimation.h"
#include <cassert>
#include <cmath>

void PlayAnimation::ApplyAnimation(
    Skeleton& skeleton,
    const Animation& animation,
    float animationTime)
{
    for (Joint& joint : skeleton.joints) {

        auto it = animation.nodeAnimations.find(joint.name);
        if (it == animation.nodeAnimations.end()) {
            continue;
        }

        const NodeAnimation& nodeAnim = it->second;

        joint.transform.translate = CalculateValue(nodeAnim.translate, animationTime);

        joint.transform.rotate = CalculateValue(nodeAnim.rotation, animationTime);

        joint.transform.scale = CalculateValue(nodeAnim.scale, animationTime);
    }
}

void PlayAnimation::SetAnimation(const Animation* animation, float blendDuration)
{
    // すでに別のアニメーションが設定されており、ブレンド指定がある場合
    if (animation_ && animation_ != animation && blendDuration > 0.0f) {
        prevAnimation_ = animation_;
        prevAnimationTime_ = animationTime_;
        blendTime_ = 0.0f;
        blendDuration_ = blendDuration;
    } else if (animation_ != animation) {
        // ブレンドなし、あるいは同一アニメーションの場合は即時切り替え
        prevAnimation_ = nullptr;
        prevAnimationTime_ = 0.0f;
        blendTime_ = 0.0f;
        blendDuration_ = 0.0f;
    }

    animation_ = animation;
    animationTime_ = 0.0f;
}

void PlayAnimation::Update(float deltaTime) {
    if (!animation_) {
        return;
    }

    animationTime_ += deltaTime;
    if (animationTime_ > animation_->duration) {
        animationTime_ = fmod(animationTime_, animation_->duration);
    }

    // ブレンド更新
    bool isBlending = (prevAnimation_ != nullptr && blendTime_ < blendDuration_);
    if (isBlending) {
        prevAnimationTime_ += deltaTime;
        if (prevAnimationTime_ > prevAnimation_->duration) {
            prevAnimationTime_ = fmod(prevAnimationTime_, prevAnimation_->duration);
        }
        blendTime_ += deltaTime;
        if (blendTime_ >= blendDuration_) {
            // ブレンド終了
            prevAnimation_ = nullptr;
            prevAnimationTime_ = 0.0f;
            blendTime_ = 0.0f;
            blendDuration_ = 0.0f;
            isBlending = false;
        }
    }

    // スキニングがある場合のみ
    if (skeleton_) {
        if (isBlending) {
            float t = blendTime_ / blendDuration_;
            if (t > 1.0f) t = 1.0f;
            ApplyBlendAnimation(*skeleton_, *prevAnimation_, prevAnimationTime_, *animation_, animationTime_, t);
        } else {
            ApplyAnimation(*skeleton_, *animation_, animationTime_);
        }
        skeleton_->UpdateSkeleton();
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


Matrix4x4 PlayAnimation::GetLocalMatrix(const std::string& nodeName) {
    assert(animation_);

    auto it = animation_->nodeAnimations.find(nodeName);

    if (it == animation_->nodeAnimations.end()) {
        if (animation_->nodeAnimations.size() == 1) {
            it = animation_->nodeAnimations.begin();
        }
        else {
            return MatrixMath::MakeIdentity4x4();
        }
    }

    const NodeAnimation& nodeAnim = it->second;

    Vector3 t = CalculateValue(nodeAnim.translate, animationTime_);
    Quaternion r = CalculateValue(nodeAnim.rotation, animationTime_);
    Vector3 s = CalculateValue(nodeAnim.scale, animationTime_);

    return MatrixMath::MakeAffineMatrix(s, r, t);
}

void PlayAnimation::ApplyBlendAnimation(
    Skeleton& skeleton,
    const Animation& prevAnimation,
    float prevTime,
    const Animation& nextAnimation,
    float nextTime,
    float blendRatio)
{
    for (Joint& joint : skeleton.joints) {
        auto itPrev = prevAnimation.nodeAnimations.find(joint.name);
        auto itNext = nextAnimation.nodeAnimations.find(joint.name);

        Vector3 tPrev = joint.transform.translate;
        Quaternion rPrev = joint.transform.rotate;
        Vector3 sPrev = joint.transform.scale;

        Vector3 tNext = joint.transform.translate;
        Quaternion rNext = joint.transform.rotate;
        Vector3 sNext = joint.transform.scale;

        bool hasPrev = (itPrev != prevAnimation.nodeAnimations.end());
        bool hasNext = (itNext != nextAnimation.nodeAnimations.end());

        if (hasPrev) {
            tPrev = CalculateValue(itPrev->second.translate, prevTime);
            rPrev = CalculateValue(itPrev->second.rotation, prevTime);
            sPrev = CalculateValue(itPrev->second.scale, prevTime);
        }
        if (hasNext) {
            tNext = CalculateValue(itNext->second.translate, nextTime);
            rNext = CalculateValue(itNext->second.rotation, nextTime);
            sNext = CalculateValue(itNext->second.scale, nextTime);
        }

        if (hasPrev || hasNext) {
            joint.transform.translate = Lerp(tPrev, tNext, blendRatio);
            joint.transform.rotate = Slerp(rPrev, rNext, blendRatio);
            joint.transform.scale = Lerp(sPrev, sNext, blendRatio);
        }
    }
}




