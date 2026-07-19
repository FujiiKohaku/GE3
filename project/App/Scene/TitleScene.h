#pragma once
#include "BaseScene.h"

#include "SceneManager.h"

#include "Engine/2D/Sprite.h"

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

    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> headerLineSprite_;
    std::unique_ptr<Sprite> gamePlayButtonSprite_;
    std::unique_ptr<Sprite> gamePlayImageSprite_;
    std::unique_ptr<Sprite> testButtonSprite_;
    std::unique_ptr<Sprite> testImageSprite_;
};
