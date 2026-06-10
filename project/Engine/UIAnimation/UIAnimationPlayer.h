#pragma once

#include "Engine/math/MathStruct.h"
#include "UIAnimationClip.h"

class Sprite;

// Snapshot of UI values evaluated at one frame.
struct UIAnimationSample {
    Vector2 position = { 0.0f, 0.0f };
    Vector2 scale = { 1.0f, 1.0f };
    float rotation = 0.0f;
    Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

// Runtime player that advances frames and applies evaluated values to Sprite.
class UIAnimationPlayer {
public:
    UIAnimationPlayer();

    void SetClip(const UIAnimationClip* clip);
    const UIAnimationClip* GetClip() const;

    void Play();
    void Stop();
    bool IsPlaying() const;

    void SetLoop(bool isLoop);
    bool IsLoop() const;

    void SetFramesPerSecond(float framesPerSecond);
    float GetFramesPerSecond() const;

    void SeekFrame(float frame);
    float GetCurrentFrame() const;
    int GetCurrentFrameInt() const;

    void Update(float deltaTime);

    UIAnimationSample Evaluate(const UIAnimationSample& baseSample) const;
    UIAnimationSample EvaluateAtFrame(float frame, const UIAnimationSample& baseSample) const;

    UIAnimationSample BuildSampleFromSprite(const Sprite* sprite) const;
    void ApplyToSprite(Sprite* sprite) const;
    void ApplyToSprite(Sprite* sprite, const UIAnimationSample& baseSample) const;

private:
    float ClampFrame(float frame) const;

private:
    const UIAnimationClip* clip_ = nullptr;
    float currentFrame_ = 0.0f;
    float framesPerSecond_ = 60.0f;
    bool isPlaying_ = false;
    bool isLoop_ = false;
};
