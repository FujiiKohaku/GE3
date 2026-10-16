// チュートリアルのメッセージ・操作ボタン・達成状況をパネルとして描画する。
#pragma once
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include <memory>
#include <string>
#include <vector>

// 表示だけを担当する。課題の達成判定は呼び出し元が行う。
class TutorialCommPanel {
public:
    // パネルの文字と図形を準備し、表示を開始できる状態にする。
    void Initialize();
    // 説明文・操作文・ボタン一覧を受け取り、現在の案内表示を更新する。
    void ShowMessage(const std::string& message, const std::string& operation,
        const std::vector<std::string>& buttons = {});
    void SetCompact(bool compact) { compact_ = compact; }
    void SetProgress(int completed, int total = 5);
    void Hide() { visible_ = false; }
    // 表示の透明度や経過時間を進め、パネルの位置と文字・図形の配置を更新する。
    void Update(float deltaTime);
    // 現在使用しているパネル部品と文字を描画する。
    void Draw();

private:
    // パネル内で共通して使う設定のテキストを生成する。
    std::unique_ptr<Text> CreateText() const;
    // 描画用の矩形を用意し、位置・大きさ・色を設定する。
    void AddRectangle(float centerX, float centerY, float width, float height,
        Vector4 color, float rotation = 0.0f);
    // 始点と終点から長さと角度を求め、細い矩形で線を表す。
    void AddLine(float startX, float startY, float endX, float endY, Vector4 color);
    // 矩形と角の部品を組み合わせ、角を丸めたパネル形状を作る。
    void AddRoundedRectangle(float centerX, float centerY, float width, float height,
        float radius, Vector4 color);
    // 文字の表示位置・サイズ・色を現在のパネル配置に揃える。
    void UpdateText(Text& text, float x, float y, float fontSize, Vector4 color);

    std::vector<std::unique_ptr<Sprite>> parts_;
    size_t activePartCount_ = 0;
    Vector2 panelPosition_ {};
    float panelScale_ = 1.0f;
    std::unique_ptr<Text> header_;
    std::unique_ptr<Text> status_;
    std::unique_ptr<Text> message_;
    std::unique_ptr<Text> operation_;
    std::vector<std::unique_ptr<Text>> buttonLabels_;
    std::vector<std::string> buttons_;
    bool success_ = false;
    bool visible_ = false;
    bool compact_ = false;
    int completed_ = 0;
    int total_ = 5;
    float alpha_ = 0.0f;
    float elapsed_ = 0.0f;
    float height_ = 144.0f;
};
