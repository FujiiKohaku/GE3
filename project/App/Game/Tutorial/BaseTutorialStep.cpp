#include "BaseTutorialStep.h"

std::string GetTutorialGlyphText()
{
    std::string glyphText =
        "SIMULATION INITIALIZING TRAINING START TRAINING AI ONLINE "
        "TRAINING COMPLETE // GATE OPEN SIMULATION COMPLETE // LAUNCH STAGE 1 "
        "SKIP → STAGE 1 [BACK / VIEW] [ENTER] 0123456789 "
        "完了 練習中 できました！ この練習は完了です。 練習クリア "
        "訓練完了。出撃ゲートを開きました。\n前方の緑色のゲートへ進んでください。 前方のゲートへ進む";
    auto steps = CreateTutorialSteps();
    for (auto& step : steps) {
        const TutorialStepData& data = step->GetData();
        glyphText += data.title + data.keyboardInstruction + data.gamepadInstruction +
            data.keyboardOperation + data.gamepadOperation + data.practiceWeapon;
        for (const std::string& button : data.buttonHints) {
            glyphText += button;
        }
        for (int deviceIndex = 0; deviceIndex < 2; ++deviceIndex) {
            TutorialStepInput input;
            input.gamepadConnected = deviceIndex == 1;
            input.usingGamepad = input.gamepadConnected;
            input.invertedPitch = true;
            input.weapon = data.practiceWeapon;
            step->Enter(input);
            glyphText += step->GetOperation(input);
            // 発熱後に変わる操作文とボタンも先に登録する。数値の進捗はASCIIで共通。
            input.heat = 0.5f;
            step->Update(input, 0.0f);
            glyphText += step->GetOperation(input);
            for (const std::string& button : step->GetButtonHints()) {
                glyphText += button;
            }
        }
    }
    return glyphText;
}

std::string BaseTutorialStep::GetOperation(const TutorialStepInput& input) const
{
    return input.gamepadConnected ? data_.gamepadOperation : data_.keyboardOperation;
}

class MoveTutorialStep final : public BaseTutorialStep {
public:
    using BaseTutorialStep::BaseTutorialStep;
    void Enter(const TutorialStepInput& input) override {
        BaseTutorialStep::Enter(input);
        directionTime_.fill(0.0f);
        directions_ = 0;
    }
    void Update(const TutorialStepInput& input, float dt) override {
        const float values[] = { -input.moveX, input.moveX, input.moveY, -input.moveY };
        for (int i = 0; i < 4; ++i) {
            if (values[i] > (input.usingGamepad ? 0.35f : 0.0f)) {
                directionTime_[i] += input.usingGamepad ? dt : values[i] / 150.0f;
                if (directionTime_[i] >= 0.15f) directions_ |= 1u << i;
            }
        }
        achieved_ = achieved_ || directions_ == 15;
    }
    std::string GetOperation(const TutorialStepInput& input) const override {
        int count = 0;
        for (int i = 0; i < 4; ++i) if (directions_ & (1u << i)) ++count;
        return std::string(input.gamepadConnected ? "スティックを4方向 " : "マウス 4方向 ") +
            std::to_string(count) + "/4" +
            (input.gamepadConnected && input.invertedPitch ? " [上下反転ON]" : "");
    }
private:
    std::array<float, 4> directionTime_ {};
    unsigned directions_ = 0;
};

class DefeatTargetsTutorialStep : public BaseTutorialStep {
public:
    using BaseTutorialStep::BaseTutorialStep;
    void Update(const TutorialStepInput& input, float) override {
        achieved_ = achieved_ || input.defeatedTargets >= data_.requiredKills;
    }
    std::string GetOperation(const TutorialStepInput& input) const override {
        return std::string(input.gamepadConnected ? "どちらかで射撃・撃破 " : "左クリック 射撃・撃破 ") +
            std::to_string(input.defeatedTargets) + "/" + std::to_string(data_.requiredKills);
    }
};

class WeaponPracticeTutorialStep final : public DefeatTargetsTutorialStep {
public:
    using DefeatTargetsTutorialStep::DefeatTargetsTutorialStep;
    void Update(const TutorialStepInput& input, float dt) override {
        if (input.weapon == data_.practiceWeapon) DefeatTargetsTutorialStep::Update(input, dt);
    }
    std::string GetOperation(const TutorialStepInput& input) const override {
        return std::string(input.gamepadConnected ? "左右で " : "ホイール / 1〜4：") +
            data_.practiceWeapon + "  撃破 " + std::to_string(input.defeatedTargets) +
            "/" + std::to_string(data_.requiredKills);
    }
};

class HeatTutorialStep final : public BaseTutorialStep {
public:
    using BaseTutorialStep::BaseTutorialStep;
    void Enter(const TutorialStepInput& input) override {
        BaseTutorialStep::Enter(input);
        heatRaised_ = false;
    }
    void Update(const TutorialStepInput& input, float) override {
        heatRaised_ = heatRaised_ || input.heat >= 0.3f;
        achieved_ = achieved_ || (heatRaised_ && input.heat <= 0.05f && !input.overheated);
    }
    std::string GetOperation(const TutorialStepInput& input) const override {
        return heatRaised_ ? "射撃を離してゲージを冷ます" :
            (input.gamepadConnected ? "通常弾を選んで射撃を長押し" : "1でNormal → 左クリックを長押し");
    }
    std::vector<std::string> GetButtonHints() const override {
        return heatRaised_ ? std::vector<std::string> { "A", "RT" } : data_.buttonHints;
    }
private:
    bool heatRaised_ = false;
};

class HoldTutorialStep : public BaseTutorialStep {
public:
    using BaseTutorialStep::BaseTutorialStep;
    void Enter(const TutorialStepInput& input) override {
        BaseTutorialStep::Enter(input);
        holdTime_ = 0.0f;
    }
protected:
    void UpdateHold(bool held, float dt) {
        holdTime_ = held ? holdTime_ + dt : 0.0f;
        achieved_ = achieved_ || holdTime_ >= 1.5f;
    }
private:
    float holdTime_ = 0.0f;
};

class BoostTutorialStep final : public HoldTutorialStep {
public:
    using HoldTutorialStep::HoldTutorialStep;
    void Update(const TutorialStepInput& input, float dt) override { UpdateHold(input.boosting, dt); }
};

class BrakeTutorialStep final : public HoldTutorialStep {
public:
    using HoldTutorialStep::HoldTutorialStep;
    void Update(const TutorialStepInput& input, float dt) override { UpdateHold(input.speedMultiplier < 0.99f, dt); }
};

class BankTutorialStep final : public BaseTutorialStep {
public:
    using BaseTutorialStep::BaseTutorialStep;
    void Enter(const TutorialStepInput& input) override {
        BaseTutorialStep::Enter(input);
        times_.fill(0.0f);
        directions_ = 0;
    }
    void Update(const TutorialStepInput& input, float dt) override {
        if (!input.gamepadConnected) { achieved_ = achieved_ || input.spaceTriggered; return; }
        times_[0] = input.bank > 0.5f ? times_[0] + dt : 0.0f;
        times_[1] = input.bank < -0.5f ? times_[1] + dt : 0.0f;
        if (times_[0] >= 0.35f) directions_ |= 1;
        if (times_[1] >= 0.35f) directions_ |= 2;
        achieved_ = achieved_ || directions_ == 3;
    }
    std::string GetOperation(const TutorialStepInput& input) const override {
        if (!input.gamepadConnected) return "SPACE：次の練習へ";
        return "左右をそれぞれ長押し " +
            std::to_string((directions_ & 1 ? 1 : 0) + (directions_ & 2 ? 1 : 0)) + "/2";
    }
private:
    std::array<float, 2> times_ {};
    unsigned directions_ = 0;
};

class RollTutorialStep final : public BaseTutorialStep {
public:
    using BaseTutorialStep::BaseTutorialStep;
    void Enter(const TutorialStepInput& input) override {
        BaseTutorialStep::Enter(input);
        wasRolling_ = input.rolling;
    }
    void Update(const TutorialStepInput& input, float) override {
        achieved_ = achieved_ || (input.gamepadConnected ? input.rolling && !wasRolling_ : input.spaceTriggered);
        wasRolling_ = input.rolling;
    }
private:
    bool wasRolling_ = false;
};

class PauseTutorialStep final : public BaseTutorialStep {
public:
    using BaseTutorialStep::BaseTutorialStep;
    void Enter(const TutorialStepInput& input) override {
        BaseTutorialStep::Enter(input);
        pauseOpened_ = false;
    }
    void NotifyManualPauseOpened() override { pauseOpened_ = true; }
    void Update(const TutorialStepInput&, float) override { achieved_ = achieved_ || pauseOpened_; }
private:
    bool pauseOpened_ = false;
};

std::array<std::unique_ptr<BaseTutorialStep>, kTutorialStepCount> CreateTutorialSteps()
{
    return {
        std::make_unique<MoveTutorialStep>(TutorialStepData { "MOVE", "マウスで照準を操作します。\n左右・上下の4方向へ動かしてください。", "左スティックで機体と照準を操作します。\n左右・上下の4方向へ動かしてください。", "", "", { "LS" }, 0, false, "", 180.0f, false }),
        std::make_unique<DefeatTargetsTutorialStep>(TutorialStepData { "FIRE", "マウスで照準を敵に合わせ、左クリックで撃ちます。\n反撃しない敵を3体倒しましょう。長押しでも撃てます。", "照準を敵に合わせ、AまたはRTで撃ちます。\n反撃しない敵を3体倒しましょう。長押しでも撃てます。", "", "", { "A", "RT" }, 3, false, "", 450.0f, false }),
        std::make_unique<HeatTutorialStep>(TutorialStepData { "HEAT", "照準の周りの熱ゲージが高いほど、威力も上がります。\n通常弾・ミニガンは熱を共有し、威力は最大2倍。\n満タンで撃ち続けると、冷え切るまで撃てません。\nNormalを少し連射し、射撃を離して冷ましましょう。", "照準の周りの熱ゲージが高いほど、威力も上がります。\n通常弾・ミニガンは熱を共有し、威力は最大2倍。\n満タンで撃ち続けると、冷え切るまで撃てません。\nNormalを少し連射し、射撃を離して冷ましましょう。", "", "", { "DPAD", "A", "RT" }, 0, false, "", 450.0f, true }),
        std::make_unique<DefeatTargetsTutorialStep>(TutorialStepData { "MOVING TARGET", "今度は、ゆっくり動く敵を3体倒しましょう。\n照準で敵を追いかけて撃ちます。時間制限はありません。", "今度は、ゆっくり動く敵を3体倒しましょう。\n照準で敵を追いかけて撃ちます。時間制限はありません。", "", "", { "A", "RT" }, 3, true, "", 600.0f, false }),
        std::make_unique<BoostTutorialStep>(TutorialStepData { "BOOST", "SHIFTまたは右クリックを長押しすると加速します。\n1.5秒続けて押し、速さと視野の変化を確かめましょう。", "Xを長押しすると加速します。\n1.5秒続けて押し、速さと視野の変化を確かめましょう。", "SHIFT / 右クリック 長押し", "長押しで加速", { "X" }, 0, false, "", 750.0f, false }),
        std::make_unique<BrakeTutorialStep>(TutorialStepData { "BRAKE", "CTRLを長押しすると速度を落とせます。\n1.5秒続けて押しましょう。離すと元の速度に戻ります。", "Yを長押しすると速度を落とせます。\n1.5秒続けて押しましょう。離すと元の速度に戻ります。", "CTRL 長押し", "長押しで減速", { "Y" }, 0, false, "", 1050.0f, false }),
        std::make_unique<BankTutorialStep>(TutorialStepData { "BANK", "機体を傾ける操作は、パッドのLB・RBを使います。\nキーボードでは練習を省略できます。SPACEで次へ進みます。", "LBで左、RBで右へ機体を傾けます。\n片方ずつ少し長押しし、機体の動きを確かめましょう。", "", "", { "LB", "RB" }, 0, false, "", 1350.0f, false }),
        std::make_unique<RollTutorialStep>(TutorialStepData { "ROLL", "回転する操作は、パッドのLB・RBの2回押しです。\nキーボードでは練習を省略できます。SPACEで次へ進みます。", "同じLBまたはRBを素早く2回押すと機体が回転します。\nまずボタンを離してから、トントンと2回押しましょう。", "SPACE：次の練習へ", "どちらかを素早く2回押す", { "LB", "RB" }, 0, false, "", 1710.0f, false }),
        std::make_unique<WeaponPracticeTutorialStep>(TutorialStepData { "NORMAL", "Normal（通常弾）は、狙った方向へ飛ぶ基本の弾です。\n長押しで連射し、熱が高くなるほど威力が上がります。\nNormalを選び、敵を3体倒してください。", "Normal（通常弾）は、狙った方向へ飛ぶ基本の弾です。\n長押しで連射し、熱が高くなるほど威力が上がります。\nNormalを選び、敵を3体倒してください。", "", "", { "DPAD" }, 3, false, "Normal", 2070.0f, true }),
        std::make_unique<WeaponPracticeTutorialStep>(TutorialStepData { "MISSILE", "Missileは、通常弾より一発の威力が高い武器です。\n押すたびに発射し、次の一発まで2秒待ちます。\n熱はたまりません。Missileで敵を3体倒しましょう。", "Missileは、通常弾より一発の威力が高い武器です。\n押すたびに発射し、次の一発まで2秒待ちます。\n熱はたまりません。Missileで敵を3体倒しましょう。", "", "", { "DPAD" }, 3, false, "Missile", 2070.0f, true }),
        std::make_unique<WeaponPracticeTutorialStep>(TutorialStepData { "HOMING MISSILE", "Homing Missileは、敵を追いかけるミサイルです。\n射撃を長押しして敵を狙い、ロック後に離すと発射。\n熱はたまらず、発射間隔は2秒。敵を3体倒しましょう。", "Homing Missileは、敵を追いかけるミサイルです。\n射撃を長押しして敵を狙い、ロック後に離すと発射。\n熱はたまらず、発射間隔は2秒。敵を3体倒しましょう。", "", "", { "DPAD" }, 3, false, "Homing Missile", 2070.0f, true }),
        std::make_unique<WeaponPracticeTutorialStep>(TutorialStepData { "MINIGUN", "Minigunは、一発は弱くても高速連射できる武器です。\n熱が高いと威力も上がりますが、撃ち続けると過熱します。\n敵を3体倒しましょう。通常弾と熱ゲージを共有します。", "Minigunは、一発は弱くても高速連射できる武器です。\n熱が高いと威力も上がりますが、撃ち続けると過熱します。\n敵を3体倒しましょう。通常弾と熱ゲージを共有します。", "", "", { "DPAD" }, 3, false, "Minigun", 2070.0f, true }),
        std::make_unique<PauseTutorialStep>(TutorialStepData { "PAUSE", "TABでポーズし、設定を確認できます。\nTABまたはESCで訓練へ戻ってください。", "STARTでポーズし、設定を確認できます。\nSTARTまたはBで訓練へ戻れます。", "TAB → 設定 / ESCで戻る", "メニューでポーズ・もう一度押して戻る", { "MENU" }, 0, false, "", 2400.0f, false }),
        std::make_unique<DefeatTargetsTutorialStep>(TutorialStepData { "PRACTICE", "最後は、動く敵を5体倒す練習です。\n移動と射撃を組み合わせましょう。敵は反撃しません。", "最後は、動く敵を5体倒す練習です。\n移動と射撃を組み合わせましょう。敵は反撃しません。", "", "", { "A", "RT" }, 5, true, "", 2600.0f, false })
    };
}
