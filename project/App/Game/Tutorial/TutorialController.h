// チュートリアルの項目順・達成判定・説明文を管理する。画面表示はTutorialCommPanelへ任せる。
#pragma once
#include <array>
#include <string>
#include <vector>

class Input;
class Player;

// 訓練の進行・達成判定・説明文を管理する。表示はTutorialCommPanelが担当する。
class TutorialController {
public:
    // 先頭の項目へ戻し、プレイヤーの現在状態を使って訓練を開始する。
    void Initialize(const Player& player);
    // 達成判定と成功表示の待ち時間を進める。成功表示を終えたら次の項目へ移る。
    void Update(const Input& input, const Player& player, float deltaTimeSeconds);
    // ポーズの練習中だけ、手動でポーズを開いた事実を記録する。
    void NotifyManualPauseOpened();
    // 成功表示中や武器が違う場合を除き、この項目で必要な標的数を返す。
    int GetTargetCount() const;
    bool HasMovingTargets() const;
    void SetDefeatedTargets(int count) { defeatedTargets_ = count; }
    bool IsComplete() const { return stepIndex_ >= kStepCount; }
    bool IsCompact() const { return false; }
    // 成功表示中の項目も完了数に含め、全項目数を超えない値を返す。
    int GetCompletedCount() const;
    static constexpr int kStepCount = 14;
    // 標的練習中はレール進行を止め、それ以外でも次のチェックポイントを越えないようにする。
    float LimitRailDistance(float distance) const;
    std::string GetTitle() const;
    std::string GetInstructionText(const Input& input) const;
    std::string GetOperation(const Input& input, const Player& player) const;
    std::vector<std::string> GetButtonHints(const Input& input) const;
    // 項目切替時の文字生成を避けるため、ロード中に使用文字を準備する。
    static std::string GetGlyphText();

private:
    friend struct TutorialTestAccess;
    enum class StepType { Move, DefeatTargets, Heat, Boost, Brake, Bank, Roll, WeaponPractice, Pause };
    struct StepData {
        StepType type;
        std::string title;
        std::string keyboardInstruction;
        std::string gamepadInstruction;
        std::string keyboardOperation;
        std::string gamepadOperation;
        std::vector<std::string> buttonHints;
        int requiredKills = 0;
        bool hasMovingTargets = false;
        std::string practiceWeapon;
        float checkpointDistance = 0.0f;
        bool shouldHoldRail = false;
    };
    struct StepInput {
        bool isGamepadConnected = false;
        bool isUsingGamepad = false;
        bool isPitchInverted = false;
        float moveX = 0.0f;
        float moveY = 0.0f;
        bool isSpaceTriggered = false;
        bool isBoosting = false;
        float speedMultiplier = 1.0f;
        float bank = 0.0f;
        bool isRolling = false;
        float heat = 0.0f;
        bool isOverheated = false;
        std::string weapon;
        int defeatedTargets = 0;
    };
    // 全項目の設定を一か所にまとめる。説明文や標的数を変更するときはこの表を編集する。
    static const std::array<StepData, kStepCount>& GetSteps();
    const StepData* GetCurrentStep() const;
    // 入力とプレイヤーの状態を、項目の達成判定に必要な値へまとめる。
    StepInput ReadStepInput(const Input& input, const Player& player) const;
    // 撃破数や経過時間をリセットし、新しい項目の開始時点の状態を記録する。
    void EnterStep(const Player& player);
    // 方向入力・長押し・発熱などの達成記録を消し、前の項目の状態を持ち越さないようにする。
    void ResetStep(const StepInput& input);
    // 項目の種類に応じて、移動・射撃・冷却・長押しなどの達成条件を調べる。
    void UpdateStep(const StepInput& input, float deltaTimeSeconds);
    // 左右・上下の各方向を十分に入力したかを記録し、四方向が揃ったら達成にする。
    void UpdateMoveStep(const StepInput& input, float deltaTimeSeconds);
    // 押し続けた時間を数える。途中で離した場合は連続時間を数え直す。
    void UpdateHoldStep(bool isHeld, float deltaTimeSeconds);
    // 左右それぞれのバンク長押しを記録する。キーボードではSPACEによる省略を認める。
    void UpdateBankStep(const StepInput& input, float deltaTimeSeconds);
    // 進捗や入力機器に合わせて、現在行う操作を説明する文章を作る。
    std::string GetStepOperation(const StepInput& input) const;
    // 発熱後の案内変更を含め、現在の項目に表示するボタン一覧を返す。
    std::vector<std::string> GetStepButtonHints() const;

    std::string currentWeapon_;
    int defeatedTargets_ = 0;
    int stepIndex_ = 0;
    bool hasAchievedStep_ = false;
    float stepTimeSeconds_ = 0.0f;
    float successTimeSeconds_ = 0.0f;
    std::array<float, 4> directionTimeSeconds_ {};
    unsigned completedDirections_ = 0;
    std::array<float, 2> bankTimeSeconds_ {};
    unsigned completedBankDirections_ = 0;
    float holdTimeSeconds_ = 0.0f;
    bool hasHeatRaised_ = false;
    bool wasRolling_ = false;
    bool hasPauseOpened_ = false;
};
