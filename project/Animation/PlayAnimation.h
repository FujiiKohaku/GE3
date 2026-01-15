#pragma once
#include <string>
#include <vector>

#include "KeyFrame.h" 
#include "MathStruct.h"
#include "MatrixMath.h"
#include "NodeAnimation.h" 

#include"Animation.h"  

class PlayAnimation {
public:
    void SetAnimation(const Animation* animation);
    void Update(float deltaTime);

    Matrix4x4 GetLocalMatrix(const std::string& nodeName);
    Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
    Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

private:
    const Animation* animation_ = nullptr;
    float animationTime_ = 0.0f;
};
