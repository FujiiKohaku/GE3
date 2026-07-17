#pragma once

#include <memory>
#include <string>

#include "Animation.h"
#include "AnimationLoder.h"
#include "PlayAnimation.h"

#include "Engine/3D/SkinningObject3d.h"
#include "Engine/3D/SkinningObject3dManager.h"

#include "../Skeleton/Skeleton.h"

class AnimationActor {
public:
    void Initialize(const std::string& modelName);

    void Update(float deltaTime);
    void Draw();

    void SetTranslate(const Vector3& translate);
    void SetRotate(const Vector3& rotate);
    void SetScale(const Vector3& scale);
    void SetSkeletonDebugVisible(bool visible);

    SkinningObject3d* GetObject();
    Skeleton* GetSkeleton();
    PlayAnimation* GetPlayAnimation();

private:
    void DrawSkeletonDebug();

    std::unique_ptr<SkinningObject3d> object_;
    std::unique_ptr<PlayAnimation> playAnimation_;
    Animation animation_;
    Skeleton skeleton_;
    bool isSkeletonDebugVisible_ = true;
};
