// フォント・文字配置・スクロールなど、テキスト表示の挙動を確認するための開発用シーン。
#pragma once

#include "App/Scene/Common/BaseScene.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include <memory>

class TextTestScene : public BaseScene {
public:
    // 表示を比較するフォントや文章を準備し、テキストの検証用配置を作る。
    void Initialize() override;
    void Finalize() override;
    // 入力や経過時間に応じて、検証用テキストの表示状態を更新する。
    void Update() override;
    // 検証用の文字を描画し、フォントや配置の違いを画面で確認する。
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
