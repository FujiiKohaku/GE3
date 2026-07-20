#pragma once

#include "BaseScene.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include <memory>

class TextTestScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> japaneseText_;
    std::unique_ptr<Text> wrappingText_;
    std::unique_ptr<Text> leftText_;
    std::unique_ptr<Text> centerText_;
    std::unique_ptr<Text> rightText_;
    std::unique_ptr<Text> smallText_;
    std::unique_ptr<Text> mediumText_;
    std::unique_ptr<Text> largeText_;
    std::unique_ptr<Text> dynamicText_;
    std::unique_ptr<Text> instructionText_;
    uint64_t frameCount_ = 0;
};
