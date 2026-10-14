#pragma once
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include <memory>
#include <string>
#include <vector>

// 表示だけを担当する。課題の達成判定は呼び出し元が行う。
class TutorialCommPanel {
public:
    void Initialize();
    void ShowMessage(const std::string& message, const std::string& operation,
        const std::vector<std::string>& buttons = {});
    void SetCompact(bool compact) { compact_ = compact; }
    void SetProgress(int completed, int total = 5);
    void Hide() { visible_ = false; }
    void Update(float deltaTime);
    void Draw();

private:
    std::vector<std::unique_ptr<Sprite>> parts_;
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
