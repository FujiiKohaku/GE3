// チュートリアルの項目順・達成判定・説明文を管理する。画面表示はTutorialCommPanelへ任せる。
#include "TutorialController.h"
#include "App/Game/Player/Player.h"
#include "Engine/Input/Input.h"
#include "Engine/audio/SoundManager.h"
#include <algorithm>
#include <cmath>

// 先頭の項目へ戻し、プレイヤーの現在状態を使って訓練を開始する。
void TutorialController::Initialize(const Player& player)
{
    stepIndex_ = 0;
    EnterStep(player);
}

const TutorialController::StepData* TutorialController::GetCurrentStep() const
{
    if (IsComplete()) { return nullptr; }
    return &GetSteps()[stepIndex_];
}

// 撃破数や経過時間をリセットし、新しい項目の開始時点の状態を記録する。
void TutorialController::EnterStep(const Player& player)
{
    defeatedTargets_ = 0;
    stepTimeSeconds_ = 0.0f;
    successTimeSeconds_ = 0.0f;
    currentWeapon_ = player.GetCurrentWeaponDisplayName();
    StepInput initial;
    initial.weapon = currentWeapon_;
    initial.isRolling = player.IsRolling();
    ResetStep(initial);
}

// 方向入力・長押し・発熱などの達成記録を消し、前の項目の状態を持ち越さないようにする。
void TutorialController::ResetStep(const StepInput& input)
{
    hasAchievedStep_ = false;
    directionTimeSeconds_.fill(0.0f);
    completedDirections_ = 0;
    bankTimeSeconds_.fill(0.0f);
    completedBankDirections_ = 0;
    holdTimeSeconds_ = 0.0f;
    hasHeatRaised_ = false;
    wasRolling_ = input.isRolling;
    hasPauseOpened_ = false;
}

// 入力とプレイヤーの状態を、項目の達成判定に必要な値へまとめる。
TutorialController::StepInput TutorialController::ReadStepInput(const Input& input, const Player& player) const
{
    StepInput state;
    state.isGamepadConnected = input.IsGamepadConnected();
    state.isUsingGamepad = input.IsUsingGamepad();
    state.isPitchInverted = input.IsGamepadPitchInverted();
    if (state.isUsingGamepad) {
        state.moveX = input.GetGamepadLeftStickX();
        state.moveY = input.GetGamepadLeftStickY();
    } else {
        state.moveX = static_cast<float>(input.GetMouseDeltaX());
        state.moveY = -static_cast<float>(input.GetMouseDeltaY());
    }
    state.isSpaceTriggered = input.IsKeyTrigger(DIK_SPACE);
    state.isBoosting = player.IsBoosting();
    state.speedMultiplier = player.GetForwardSpeedMultiplier();
    state.bank = player.GetManualBank();
    state.isRolling = player.IsRolling();
    state.heat = player.GetHeatRatio();
    state.isOverheated = player.IsOverheated();
    state.weapon = player.GetCurrentWeaponDisplayName();
    state.defeatedTargets = defeatedTargets_;
    return state;
}

// 成功表示中や武器が違う場合を除き、この項目で必要な標的数を返す。
int TutorialController::GetTargetCount() const
{
    const StepData* step = GetCurrentStep();
    if (step == nullptr || successTimeSeconds_ > 0.0f) { return 0; }
    if (!step->practiceWeapon.empty() && currentWeapon_ != step->practiceWeapon) { return 0; }
    return step->requiredKills;
}

bool TutorialController::HasMovingTargets() const
{
    const StepData* step = GetCurrentStep();
    return step != nullptr && step->hasMovingTargets;
}

// 達成判定と成功表示の待ち時間を進める。成功表示を終えたら次の項目へ移る。
void TutorialController::Update(const Input& input, const Player& player, float deltaTimeSeconds)
{
    if (IsComplete() || player.IsDead()) { return; }
    currentWeapon_ = player.GetCurrentWeaponDisplayName();
    if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0f) { return; }
    stepTimeSeconds_ += deltaTimeSeconds;
    // 成功の表示中は次の項目の判定を始めず、表示時間を確保する。
    if (successTimeSeconds_ > 0.0f) {
        successTimeSeconds_ -= deltaTimeSeconds;
        if (successTimeSeconds_ <= 0.0f) {
            ++stepIndex_;
            EnterStep(player);
        }
        return;
    }
    UpdateStep(ReadStepInput(input, player), deltaTimeSeconds);
    // 素早く達成しても、説明を読むための最低時間を過ぎてから成功を表示する。
    if (hasAchievedStep_ && stepTimeSeconds_ >= 6.0f) {
        successTimeSeconds_ = 3.0f;
        SoundManager::GetInstance()->Play("TutorialConfirm");
    }
}

// 項目の種類に応じて、移動・射撃・冷却・長押しなどの達成条件を調べる。
void TutorialController::UpdateStep(const StepInput& input, float deltaTimeSeconds)
{
    const StepData* step = GetCurrentStep();
    if (step == nullptr) { return; }
    switch (step->type) {
    case StepType::Move:
        UpdateMoveStep(input, deltaTimeSeconds);
        break;
    case StepType::DefeatTargets:
        hasAchievedStep_ = hasAchievedStep_ || input.defeatedTargets >= step->requiredKills;
        break;
    case StepType::WeaponPractice:
        if (input.weapon == step->practiceWeapon) {
            hasAchievedStep_ = hasAchievedStep_ || input.defeatedTargets >= step->requiredKills;
        }
        break;
    case StepType::Heat:
        hasHeatRaised_ = hasHeatRaised_ || input.heat >= 0.3f;
        hasAchievedStep_ = hasAchievedStep_ || (hasHeatRaised_ && input.heat <= 0.05f && !input.isOverheated);
        break;
    case StepType::Boost:
        UpdateHoldStep(input.isBoosting, deltaTimeSeconds);
        break;
    case StepType::Brake:
        UpdateHoldStep(input.speedMultiplier < 0.99f, deltaTimeSeconds);
        break;
    case StepType::Bank:
        UpdateBankStep(input, deltaTimeSeconds);
        break;
    case StepType::Roll:
        if (input.isGamepadConnected) {
            hasAchievedStep_ = hasAchievedStep_ || (input.isRolling && !wasRolling_);
        } else {
            hasAchievedStep_ = hasAchievedStep_ || input.isSpaceTriggered;
        }
        wasRolling_ = input.isRolling;
        break;
    case StepType::Pause:
        hasAchievedStep_ = hasAchievedStep_ || hasPauseOpened_;
        break;
    }
}

// 左右・上下の各方向を十分に入力したかを記録し、四方向が揃ったら達成にする。
void TutorialController::UpdateMoveStep(const StepInput& input, float deltaTimeSeconds)
{
    const float values[] = { -input.moveX, input.moveX, input.moveY, -input.moveY };
    float threshold = 0.0f;
    if (input.isUsingGamepad) { threshold = 0.35f; }
    for (int directionIndex = 0; directionIndex < 4; ++directionIndex) {
        if (values[directionIndex] > threshold) {
            float directionTimeSeconds = values[directionIndex] / 150.0f;
            if (input.isUsingGamepad) { directionTimeSeconds = deltaTimeSeconds; }
            directionTimeSeconds_[directionIndex] += directionTimeSeconds;
            if (directionTimeSeconds_[directionIndex] >= 0.15f) {
                completedDirections_ |= 1u << directionIndex;
            }
        }
    }
    hasAchievedStep_ = hasAchievedStep_ || completedDirections_ == 15;
}

// 押し続けた時間を数える。途中で離した場合は連続時間を数え直す。
void TutorialController::UpdateHoldStep(bool isHeld, float deltaTimeSeconds)
{
    if (isHeld) { holdTimeSeconds_ += deltaTimeSeconds; }
    else { holdTimeSeconds_ = 0.0f; }
    hasAchievedStep_ = hasAchievedStep_ || holdTimeSeconds_ >= 1.5f;
}

// 左右それぞれのバンク長押しを記録する。キーボードではSPACEによる省略を認める。
void TutorialController::UpdateBankStep(const StepInput& input, float deltaTimeSeconds)
{
    if (!input.isGamepadConnected) {
        hasAchievedStep_ = hasAchievedStep_ || input.isSpaceTriggered;
        return;
    }
    if (input.bank > 0.5f) { bankTimeSeconds_[0] += deltaTimeSeconds; }
    else { bankTimeSeconds_[0] = 0.0f; }
    if (input.bank < -0.5f) { bankTimeSeconds_[1] += deltaTimeSeconds; }
    else { bankTimeSeconds_[1] = 0.0f; }
    if (bankTimeSeconds_[0] >= 0.35f) { completedBankDirections_ |= 1; }
    if (bankTimeSeconds_[1] >= 0.35f) { completedBankDirections_ |= 2; }
    hasAchievedStep_ = hasAchievedStep_ || completedBankDirections_ == 3;
}

// ポーズの練習中だけ、手動でポーズを開いた事実を記録する。
void TutorialController::NotifyManualPauseOpened()
{
    const StepData* step = GetCurrentStep();
    if (step != nullptr && step->type == StepType::Pause && successTimeSeconds_ <= 0.0f) {
        hasPauseOpened_ = true;
    }
}

// 成功表示中の項目も完了数に含め、全項目数を超えない値を返す。
int TutorialController::GetCompletedCount() const
{
    int completedCount = stepIndex_;
    if (successTimeSeconds_ > 0.0f) { ++completedCount; }
    return (std::min)(kStepCount, completedCount);
}

// 標的練習中はレール進行を止め、それ以外でも次のチェックポイントを越えないようにする。
float TutorialController::LimitRailDistance(float distance) const
{
    const StepData* step = GetCurrentStep();
    if (step == nullptr) { return distance; }
    if (GetTargetCount() > 0 || step->shouldHoldRail) { return 0.0f; }
    return (std::min)(distance, step->checkpointDistance);
}

std::string TutorialController::GetTitle() const
{
    const StepData* step = GetCurrentStep();
    if (step == nullptr) { return "TRAINING COMPLETE // GATE OPEN"; }
    return "TRAINING " + std::to_string(stepIndex_ + 1) + " / " + std::to_string(kStepCount) + " // " + step->title;
}

std::string TutorialController::GetInstructionText(const Input& input) const
{
    const StepData* step = GetCurrentStep();
    if (step == nullptr) { return "訓練完了。出撃ゲートを開きました。\n前方の緑色のゲートへ進んでください。"; }
    if (successTimeSeconds_ > 0.0f) { return "できました！ この練習は完了です。"; }
    if (input.IsGamepadConnected()) { return step->gamepadInstruction; }
    return step->keyboardInstruction;
}

std::string TutorialController::GetOperation(const Input& input, const Player& player) const
{
    if (IsComplete()) { return "前方のゲートへ進む"; }
    if (successTimeSeconds_ > 0.0f) { return "練習クリア"; }
    return GetStepOperation(ReadStepInput(input, player));
}

// 進捗や入力機器に合わせて、現在行う操作を説明する文章を作る。
std::string TutorialController::GetStepOperation(const StepInput& input) const
{
    const StepData* step = GetCurrentStep();
    if (step == nullptr) { return "前方のゲートへ進む"; }
    switch (step->type) {
    case StepType::Move: {
        int count = 0;
        for (int directionIndex = 0; directionIndex < 4; ++directionIndex) {
            if (completedDirections_ & (1u << directionIndex)) { ++count; }
        }
        std::string operation = "マウス 4方向 ";
        if (input.isGamepadConnected) { operation = "スティックを4方向 "; }
        operation += std::to_string(count) + "/4";
        if (input.isGamepadConnected && input.isPitchInverted) { operation += " [上下反転ON]"; }
        return operation;
    }
    case StepType::DefeatTargets: {
        std::string operation = "左クリック 射撃・撃破 ";
        if (input.isGamepadConnected) { operation = "どちらかで射撃・撃破 "; }
        return operation + std::to_string(input.defeatedTargets) + "/" + std::to_string(step->requiredKills);
    }
    case StepType::WeaponPractice: {
        std::string operation = "ホイール / 1〜4：";
        if (input.isGamepadConnected) { operation = "左右で "; }
        return operation + step->practiceWeapon + "  撃破 " + std::to_string(input.defeatedTargets) + "/" + std::to_string(step->requiredKills);
    }
    case StepType::Heat:
        if (hasHeatRaised_) { return "射撃を離してゲージを冷ます"; }
        if (input.isGamepadConnected) { return "通常弾を選んで射撃を長押し"; }
        return "1でNormal → 左クリックを長押し";
    case StepType::Bank: {
        if (!input.isGamepadConnected) { return "SPACE：次の練習へ"; }
        int count = 0;
        if (completedBankDirections_ & 1) { ++count; }
        if (completedBankDirections_ & 2) { ++count; }
        return "左右をそれぞれ長押し " + std::to_string(count) + "/2";
    }
    default:
        if (input.isGamepadConnected) { return step->gamepadOperation; }
        return step->keyboardOperation;
    }
}

std::vector<std::string> TutorialController::GetButtonHints(const Input& input) const
{
    if (!input.IsGamepadConnected() || IsComplete() || successTimeSeconds_ > 0.0f) { return {}; }
    return GetStepButtonHints();
}

// 発熱後の案内変更を含め、現在の項目に表示するボタン一覧を返す。
std::vector<std::string> TutorialController::GetStepButtonHints() const
{
    const StepData* step = GetCurrentStep();
    if (step == nullptr) { return {}; }
    if (step->type == StepType::Heat && hasHeatRaised_) { return { "A", "RT" }; }
    return step->buttonHints;
}

// 進捗で変わる文章も先に集め、プレイ中の文字アトラス生成による停止を避ける。
std::string TutorialController::GetGlyphText()
{
    std::string glyphText =
        "SIMULATION INITIALIZING TRAINING START TRAINING AI ONLINE "
        "TRAINING COMPLETE // GATE OPEN SIMULATION COMPLETE // LAUNCH STAGE 1 "
        "SKIP → STAGE 1 [BACK / VIEW] [ENTER] 0123456789 "
        "完了 練習中 できました！ この練習は完了です。 練習クリア "
        "訓練完了。出撃ゲートを開きました。\n前方の緑色のゲートへ進んでください。 前方のゲートへ進む";
    TutorialController tutorial;
    for (int stepIndex = 0; stepIndex < kStepCount; ++stepIndex) {
        tutorial.stepIndex_ = stepIndex;
        const StepData& data = GetSteps()[stepIndex];
        glyphText += data.title + data.keyboardInstruction + data.gamepadInstruction +
            data.keyboardOperation + data.gamepadOperation + data.practiceWeapon;
        for (const std::string& button : data.buttonHints) { glyphText += button; }
        for (int deviceIndex = 0; deviceIndex < 2; ++deviceIndex) {
            StepInput input;
            input.isGamepadConnected = deviceIndex == 1;
            input.isUsingGamepad = input.isGamepadConnected;
            input.isPitchInverted = true;
            input.weapon = data.practiceWeapon;
            tutorial.ResetStep(input);
            glyphText += tutorial.GetStepOperation(input);
            input.heat = 0.5f;
            tutorial.UpdateStep(input, 0.0f);
            glyphText += tutorial.GetStepOperation(input);
            for (const std::string& button : tutorial.GetStepButtonHints()) { glyphText += button; }
        }
    }
    return glyphText;
}

// 全項目の設定を一か所にまとめる。説明文や標的数を変更するときはこの表を編集する。
const std::array<TutorialController::StepData, TutorialController::kStepCount>& TutorialController::GetSteps()
{
    static const std::array<StepData, kStepCount> steps = {{
        StepData { StepType::Move, "MOVE", "マウスで照準を操作します。\n左右・上下の4方向へ動かしてください。", "左スティックで機体と照準を操作します。\n左右・上下の4方向へ動かしてください。", "", "", { "LS" }, 0, false, "", 180.0f, false },
        StepData { StepType::DefeatTargets, "FIRE", "マウスで照準を敵に合わせ、左クリックで撃ちます。\n反撃しない敵を3体倒しましょう。長押しでも撃てます。", "照準を敵に合わせ、AまたはRTで撃ちます。\n反撃しない敵を3体倒しましょう。長押しでも撃てます。", "", "", { "A", "RT" }, 3, false, "", 450.0f, false },
        StepData { StepType::Heat, "HEAT", "照準の周りの熱ゲージが高いほど、威力も上がります。\n通常弾・ミニガンは熱を共有し、威力は最大2倍。\n満タンで撃ち続けると、冷え切るまで撃てません。\nNormalを少し連射し、射撃を離して冷ましましょう。", "照準の周りの熱ゲージが高いほど、威力も上がります。\n通常弾・ミニガンは熱を共有し、威力は最大2倍。\n満タンで撃ち続けると、冷え切るまで撃てません。\nNormalを少し連射し、射撃を離して冷ましましょう。", "", "", { "DPAD", "A", "RT" }, 0, false, "", 450.0f, true },
        StepData { StepType::DefeatTargets, "MOVING TARGET", "今度は、ゆっくり動く敵を3体倒しましょう。\n照準で敵を追いかけて撃ちます。時間制限はありません。", "今度は、ゆっくり動く敵を3体倒しましょう。\n照準で敵を追いかけて撃ちます。時間制限はありません。", "", "", { "A", "RT" }, 3, true, "", 600.0f, false },
        StepData { StepType::Boost, "BOOST", "SHIFTまたは右クリックを長押しすると加速します。\n1.5秒続けて押し、速さと視野の変化を確かめましょう。", "Xを長押しすると加速します。\n1.5秒続けて押し、速さと視野の変化を確かめましょう。", "SHIFT / 右クリック 長押し", "長押しで加速", { "X" }, 0, false, "", 750.0f, false },
        StepData { StepType::Brake, "BRAKE", "CTRLを長押しすると速度を落とせます。\n1.5秒続けて押しましょう。離すと元の速度に戻ります。", "Yを長押しすると速度を落とせます。\n1.5秒続けて押しましょう。離すと元の速度に戻ります。", "CTRL 長押し", "長押しで減速", { "Y" }, 0, false, "", 1050.0f, false },
        StepData { StepType::Bank, "BANK", "機体を傾ける操作は、パッドのLB・RBを使います。\nキーボードでは練習を省略できます。SPACEで次へ進みます。", "LBで左、RBで右へ機体を傾けます。\n片方ずつ少し長押しし、機体の動きを確かめましょう。", "", "", { "LB", "RB" }, 0, false, "", 1350.0f, false },
        StepData { StepType::Roll, "ROLL", "回転する操作は、パッドのLB・RBの2回押しです。\nキーボードでは練習を省略できます。SPACEで次へ進みます。", "同じLBまたはRBを素早く2回押すと機体が回転します。\nまずボタンを離してから、トントンと2回押しましょう。", "SPACE：次の練習へ", "どちらかを素早く2回押す", { "LB", "RB" }, 0, false, "", 1710.0f, false },
        StepData { StepType::WeaponPractice, "NORMAL", "Normal（通常弾）は、狙った方向へ飛ぶ基本の弾です。\n長押しで連射し、熱が高くなるほど威力が上がります。\nNormalを選び、敵を3体倒してください。", "Normal（通常弾）は、狙った方向へ飛ぶ基本の弾です。\n長押しで連射し、熱が高くなるほど威力が上がります。\nNormalを選び、敵を3体倒してください。", "", "", { "DPAD" }, 3, false, "Normal", 2070.0f, true },
        StepData { StepType::WeaponPractice, "MISSILE", "Missileは、通常弾より一発の威力が高い武器です。\n押すたびに発射し、次の一発まで2秒待ちます。\n熱はたまりません。Missileで敵を3体倒しましょう。", "Missileは、通常弾より一発の威力が高い武器です。\n押すたびに発射し、次の一発まで2秒待ちます。\n熱はたまりません。Missileで敵を3体倒しましょう。", "", "", { "DPAD" }, 3, false, "Missile", 2070.0f, true },
        StepData { StepType::WeaponPractice, "HOMING MISSILE", "Homing Missileは、敵を追いかけるミサイルです。\n射撃を長押しして敵を狙い、ロック後に離すと発射。\n熱はたまらず、発射間隔は2秒。敵を3体倒しましょう。", "Homing Missileは、敵を追いかけるミサイルです。\n射撃を長押しして敵を狙い、ロック後に離すと発射。\n熱はたまらず、発射間隔は2秒。敵を3体倒しましょう。", "", "", { "DPAD" }, 3, false, "Homing Missile", 2070.0f, true },
        StepData { StepType::WeaponPractice, "MINIGUN", "Minigunは、一発は弱くても高速連射できる武器です。\n熱が高いと威力も上がりますが、撃ち続けると過熱します。\n敵を3体倒しましょう。通常弾と熱ゲージを共有します。", "Minigunは、一発は弱くても高速連射できる武器です。\n熱が高いと威力も上がりますが、撃ち続けると過熱します。\n敵を3体倒しましょう。通常弾と熱ゲージを共有します。", "", "", { "DPAD" }, 3, false, "Minigun", 2070.0f, true },
        StepData { StepType::Pause, "PAUSE", "TABでポーズし、設定を確認できます。\nTABまたはESCで訓練へ戻ってください。", "STARTでポーズし、設定を確認できます。\nSTARTまたはBで訓練へ戻れます。", "TAB → 設定 / ESCで戻る", "メニューでポーズ・もう一度押して戻る", { "MENU" }, 0, false, "", 2400.0f, false },
        StepData { StepType::DefeatTargets, "PRACTICE", "最後は、動く敵を5体倒す練習です。\n移動と射撃を組み合わせましょう。敵は反撃しません。", "最後は、動く敵を5体倒す練習です。\n移動と射撃を組み合わせましょう。敵は反撃しません。", "", "", { "A", "RT" }, 5, true, "", 2600.0f, false }
    }};
    return steps;
}
