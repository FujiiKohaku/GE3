#include "UIAnimationPlayer.h"
#include "Engine/2D/Sprite.h"
#include <cmath>

UIAnimationPlayer::UIAnimationPlayer()
{
}

void UIAnimationPlayer::SetClip(const UIAnimationClip* clip)
{
    clip_ = clip;
    currentFrame_ = 0.0f;
    isPlaying_ = false;
}

const UIAnimationClip* UIAnimationPlayer::GetClip() const
{
    return clip_;
}

void UIAnimationPlayer::Play()
{
    if (clip_ == nullptr) {
        return;
    }

    isPlaying_ = true;
}

void UIAnimationPlayer::Stop()
{
    isPlaying_ = false;
}

bool UIAnimationPlayer::IsPlaying() const
{
    return isPlaying_;
}

void UIAnimationPlayer::SetLoop(bool isLoop)
{
    isLoop_ = isLoop;
}

bool UIAnimationPlayer::IsLoop() const
{
    return isLoop_;
}

void UIAnimationPlayer::SetFramesPerSecond(float framesPerSecond)
{
    framesPerSecond_ = framesPerSecond;

    if (framesPerSecond_ <= 0.0f) {
        framesPerSecond_ = 60.0f;
    }
}

float UIAnimationPlayer::GetFramesPerSecond() const
{
    return framesPerSecond_;
}

void UIAnimationPlayer::SeekFrame(float frame)
{
    currentFrame_ = ClampFrame(frame);
}

float UIAnimationPlayer::GetCurrentFrame() const
{
    return currentFrame_;
}

int UIAnimationPlayer::GetCurrentFrameInt() const
{
    return static_cast<int>(currentFrame_ + 0.5f);
}

void UIAnimationPlayer::Update(float deltaTime)
{
    if (!isPlaying_) {
        return;
    }
    if (clip_ == nullptr) {
        return;
    }

    currentFrame_ += deltaTime * framesPerSecond_;

    float length = static_cast<float>(clip_->GetLength());

    if (currentFrame_ > length) {
        if (isLoop_) {
            currentFrame_ = std::fmod(currentFrame_, length);
        } else {
            currentFrame_ = length;
            isPlaying_ = false;
        }
    }

    if (currentFrame_ < 0.0f) {
        currentFrame_ = 0.0f;
    }
}

UIAnimationSample UIAnimationPlayer::Evaluate(const UIAnimationSample& baseSample) const
{
    return EvaluateAtFrame(currentFrame_, baseSample);
}

UIAnimationSample UIAnimationPlayer::EvaluateAtFrame(float frame, const UIAnimationSample& baseSample) const
{
    UIAnimationSample sample = baseSample;

    if (clip_ == nullptr) {
        return sample;
    }

    sample.position.x = clip_->EvaluateProperty(UIAnimationProperty::PositionX, frame, baseSample.position.x);
    sample.position.y = clip_->EvaluateProperty(UIAnimationProperty::PositionY, frame, baseSample.position.y);
    sample.scale.x = clip_->EvaluateProperty(UIAnimationProperty::ScaleX, frame, baseSample.scale.x);
    sample.scale.y = clip_->EvaluateProperty(UIAnimationProperty::ScaleY, frame, baseSample.scale.y);
    sample.rotation = clip_->EvaluateProperty(UIAnimationProperty::Rotation, frame, baseSample.rotation);
    sample.color.x = clip_->EvaluateProperty(UIAnimationProperty::ColorR, frame, baseSample.color.x);
    sample.color.y = clip_->EvaluateProperty(UIAnimationProperty::ColorG, frame, baseSample.color.y);
    sample.color.z = clip_->EvaluateProperty(UIAnimationProperty::ColorB, frame, baseSample.color.z);
    sample.color.w = clip_->EvaluateProperty(UIAnimationProperty::ColorA, frame, baseSample.color.w);

    return sample;
}

UIAnimationSample UIAnimationPlayer::BuildSampleFromSprite(const Sprite* sprite) const
{
    UIAnimationSample sample;

    if (sprite == nullptr) {
        return sample;
    }

    sample.position = sprite->GetPosition();
    sample.scale = sprite->GetSize();
    sample.rotation = sprite->GetRotation();
    sample.color = sprite->GetColor();

    return sample;
}

void UIAnimationPlayer::ApplyToSprite(Sprite* sprite) const
{
    if (sprite == nullptr) {
        return;
    }

    UIAnimationSample baseSample = BuildSampleFromSprite(sprite);
    ApplyToSprite(sprite, baseSample);
}

void UIAnimationPlayer::ApplyToSprite(Sprite* sprite, const UIAnimationSample& baseSample) const
{
    if (sprite == nullptr) {
        return;
    }

    UIAnimationSample sample = Evaluate(baseSample);

    sprite->SetPosition(sample.position);
    sprite->SetSize(sample.scale);
    sprite->SetRotation(sample.rotation);
    sprite->SetColor(sample.color);
}

float UIAnimationPlayer::ClampFrame(float frame) const
{
    if (frame < 0.0f) {
        return 0.0f;
    }

    if (clip_ == nullptr) {
        return frame;
    }

    float length = static_cast<float>(clip_->GetLength());

    if (frame > length) {
        return length;
    }

    return frame;
}
