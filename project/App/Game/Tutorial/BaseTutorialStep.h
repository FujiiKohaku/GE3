#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <utility>

struct TutorialStepData {
    std::string title;
    std::string keyboardInstruction;
    std::string gamepadInstruction;
    std::string keyboardOperation;
    std::string gamepadOperation;
    std::vector<std::string> buttonHints;
    int requiredKills = 0;
    bool movingTargets = false;
    std::string practiceWeapon;
    float checkpoint = 0.0f;
    bool holdRail = false;
};

// ゲームから取得した値。達成判定は描画・入力シングルトンに依存しない。
struct TutorialStepInput {
    bool gamepadConnected = false;
    bool usingGamepad = false;
    bool invertedPitch = false;
    float moveX = 0.0f;
    float moveY = 0.0f;
    bool spaceTriggered = false;
    bool boosting = false;
    float speedMultiplier = 1.0f;
    float bank = 0.0f;
    bool rolling = false;
    float heat = 0.0f;
    bool overheated = false;
    std::string weapon;
    int defeatedTargets = 0;
};

class BaseTutorialStep {
public:
    explicit BaseTutorialStep(TutorialStepData data) : data_(std::move(data)) {}
    virtual ~BaseTutorialStep() = default;
    virtual void Enter(const TutorialStepInput&) { achieved_ = false; }
    virtual void Update(const TutorialStepInput& input, float dt) = 0;
    virtual void NotifyManualPauseOpened() {}
    virtual std::string GetOperation(const TutorialStepInput& input) const;
    virtual std::vector<std::string> GetButtonHints() const { return data_.buttonHints; }
    bool IsAchieved() const { return achieved_; }
    const TutorialStepData& GetData() const { return data_; }
protected:
    TutorialStepData data_;
    bool achieved_ = false;
};

constexpr int kTutorialStepCount = 14;
std::array<std::unique_ptr<BaseTutorialStep>, kTutorialStepCount> CreateTutorialSteps();

// 静的な説明文と、進捗に応じて変わる操作文の使用文字をロード中に準備する。
std::string GetTutorialGlyphText();
