#pragma once

#include <memory>
#include <string>

#include "Animation.h"
#include "AnimationLoder.h"
#include "PlayAnimation.h"

#include "SkinningObject3d.h"
#include "SkinningObject3dManager.h"

#include "../Skeleton/Skeleton.h"

class AnimationActor {
public:
    void Initialize(const std::string& modelName);

    void Update(float deltaTime);
    void Draw();

    void SetTranslate(const Vector3& translate);
    void SetRotate(const Vector3& rotate);
    void SetScale(const Vector3& scale);

    SkinningObject3d* GetObject();
    Skeleton* GetSkeleton();
    PlayAnimation* GetPlayAnimation();

private:
    std::unique_ptr<SkinningObject3d> object_;
    std::unique_ptr<PlayAnimation> playAnimation_;
    Animation animation_;
    Skeleton skeleton_;
};