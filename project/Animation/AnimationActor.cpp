#include "AnimationActor.h"
#include "ModelManager.h"

void AnimationActor::Initialize(const std::string& modelName)
{
    ModelManager::GetInstance()->Load(modelName);

    object_ = std::make_unique<SkinningObject3d>();
    object_->SetModel(ModelManager::GetInstance()->FindModel(modelName));

    skeleton_ = Skeleton::CreateSkeleton(object_->GetRootNode());

    playAnimation_ = std::make_unique<PlayAnimation>();
    animation_ = AnimationLoder::LoadAnimationFile("resources", modelName);

    playAnimation_->SetAnimation(&animation_);
    playAnimation_->SetSkeleton(&skeleton_);

    object_->SetAnimation(playAnimation_.get());
    object_->Initialize(SkinningObject3dManager::GetInstance());
}

void AnimationActor::Update(float deltaTime)
{
    if (playAnimation_) {
        playAnimation_->Update(deltaTime);
    }

    if (object_) {
        object_->Update();
    }
}

void AnimationActor::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

void AnimationActor::SetTranslate(const Vector3& translate)
{
    if (object_) {
        object_->SetTranslate(translate);
    }
}

void AnimationActor::SetRotate(const Vector3& rotate)
{
    if (object_) {
        object_->SetRotate(rotate);
    }
}

void AnimationActor::SetScale(const Vector3& scale)
{
    if (object_) {
        object_->SetScale(scale);
    }
}

SkinningObject3d* AnimationActor::GetObject()
{
    return object_.get();
}

Skeleton* AnimationActor::GetSkeleton()
{
    return &skeleton_;
}

PlayAnimation* AnimationActor::GetPlayAnimation()
{
    return playAnimation_.get();
}