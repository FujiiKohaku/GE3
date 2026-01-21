#pragma once
#include <string>
#include <vector>

#include "KeyFrame.h" 
#include "MathStruct.h"
#include "MatrixMath.h"
#include "NodeAnimation.h" 

#include"Animation.h"  
#include"../Skeleton/Skeleton.h"
class PlayAnimation {
public:
    void SetAnimation(const Animation* animation);
    void Update(float deltaTime);
    void SetSkeleton(Skeleton* skeleton) { skeleton_ = skeleton; }
    Matrix4x4 GetLocalMatrix(const std::string& nodeName);
    Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
    Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);
    void ApplyAnimation(Skeleton& skeleton,const Animation& animation,float animationTime);
    const Skeleton* GetSkeleton() const
    {
        return skeleton_;
    }


private:
    const Animation* animation_ = nullptr;
    float animationTime_ = 0.0f;
    Skeleton* skeleton_ = nullptr;
};
