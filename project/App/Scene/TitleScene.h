#pragma once

#include "BaseScene.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include "SceneManager.h"
#include <array>
#include <memory>

class TitleScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    bool IsMouseOver(float left, float top, float width, float height) const;
    void SetMouseCursorVisible(bool visible) const;
    void UpdateButtonVisual(
        Sprite* buttonSprite,
        Text* buttonText,
        float baseLeft,
        float baseTop,
        float baseWidth,
        float baseHeight,
        bool isHovered,
        bool isPressed,
        std::size_t animationIndex);

private:
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> flagSprite_;
    std::unique_ptr<Sprite> headerLineSprite_;
    std::unique_ptr<Sprite> gamePlayButtonSprite_;
    std::unique_ptr<Text> gamePlayButtonText_;
    std::unique_ptr<Sprite> testButtonSprite_;
    std::unique_ptr<Text> testButtonText_;
    std::unique_ptr<Sprite> spriteTestButtonSprite_;
    std::unique_ptr<Text> spriteTestButtonText_;
    std::unique_ptr<Sprite> textTestButtonSprite_;
    std::unique_ptr<Text> textTestButtonText_;

    std::array<float, 4> buttonScales_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<float, 4> buttonOffsetsY_ = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> buttonGlowStrengths_ = { 0.0f, 0.0f, 0.0f, 0.0f };
    int pressedButtonIndex_ = -1;
    bool wasMousePressed_ = false;
};
