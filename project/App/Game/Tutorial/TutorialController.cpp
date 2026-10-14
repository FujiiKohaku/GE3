#include "TutorialController.h"
#include "App/Game/Player/Player.h"
#include "Engine/Input/Input.h"
#include "Engine/audio/SoundManager.h"
#include <algorithm>
#include <cmath>

void TutorialController::Initialize(const Player& player)
{
    step_ = Step::Move;
    EnterStep(player);
}

void TutorialController::EnterStep(const Player& player)
{
    defeatedTargets_ = 0;
    achieved_ = false;
    stepTime_ = successTime_ = holdTime_ = 0.0f;
    directionTime_ = {};
    directions_ = bankDirections_ = 0;
    currentWeapon_ = player.GetCurrentWeaponDisplayName();
    heatRaised_ = false;
    wasRolling_ = player.IsRolling();
    pauseOpened_ = false;
}

int TutorialController::GetTargetCount() const
{
    if (successTime_ > 0.0f) return 0;
    if (step_ == Step::Fire || step_ == Step::FireMoving) return 3;
    if (GetPracticeWeapon()) return currentWeapon_ == GetPracticeWeapon() ? 3 : 0;
    return step_ == Step::Combat ? 5 : 0;
}

const char* TutorialController::GetPracticeWeapon() const
{
    switch (step_) {
    case Step::Weapon: return "Normal";
    case Step::Missile: return "Missile";
    case Step::Homing: return "Homing Missile";
    case Step::Minigun: return "Minigun";
    default: return nullptr;
    }
}

bool TutorialController::HasMovingTargets() const
{
    return step_ == Step::FireMoving || step_ == Step::Combat;
}

void TutorialController::Update(const Input& input, const Player& player, float deltaTime)
{
    if (IsComplete() || player.IsDead()) return;
    currentWeapon_ = player.GetCurrentWeaponDisplayName();
    const float dt = std::isfinite(deltaTime) ? (std::max)(deltaTime, 0.0f) : 0.0f;
    if (dt <= 0.0f) return;
    stepTime_ += dt;
    if (successTime_ > 0.0f) {
        successTime_ -= dt;
        if (successTime_ <= 0.0f) {
            step_ = static_cast<Step>(static_cast<int>(step_) + 1);
            EnterStep(player);
        }
        return;
    }
    bool achieved = false;
    switch (step_) {
    case Step::Move: {
        const bool pad = input.IsUsingGamepad();
        const float x = pad ? input.GetGamepadLeftStickX() : static_cast<float>(input.GetMouseDeltaX());
        const float y = pad ? input.GetGamepadLeftStickY() : -static_cast<float>(input.GetMouseDeltaY());
        const float values[] = { -x, x, y, -y };
        for (int i = 0; i < 4; ++i) {
            if (values[i] > (pad ? 0.35f : 0.0f)) {
                directionTime_[i] += pad ? dt : values[i] / 150.0f;
                if (directionTime_[i] >= 0.15f) directions_ |= 1u << i;
            }
        }
        achieved = directions_ == 15;
        break;
    }
    case Step::Fire:
    case Step::FireMoving:
    case Step::Combat:
        achieved = defeatedTargets_ >= (step_ == Step::Combat ? 5 : 3);
        break;
    case Step::Heat:
        heatRaised_ = heatRaised_ || player.GetHeatRatio() >= 0.3f;
        achieved = heatRaised_ && player.GetHeatRatio() <= 0.05f && !player.IsOverheated();
        break;
    case Step::Boost:
        holdTime_ = player.IsBoosting() ? holdTime_ + dt : 0.0f;
        achieved = holdTime_ >= 1.5f;
        break;
    case Step::Brake:
        holdTime_ = player.GetForwardSpeedMultiplier() < 0.99f ? holdTime_ + dt : 0.0f;
        achieved = holdTime_ >= 1.5f;
        break;
    case Step::Bank: {
        if (!input.IsGamepadConnected()) {
            achieved = input.IsKeyTrigger(DIK_SPACE);
            break;
        }
        const float bank = player.GetManualBank();
        directionTime_[0] = bank > 0.5f ? directionTime_[0] + dt : 0.0f;
        directionTime_[1] = bank < -0.5f ? directionTime_[1] + dt : 0.0f;
        if (directionTime_[0] >= 0.35f) bankDirections_ |= 1;
        if (directionTime_[1] >= 0.35f) bankDirections_ |= 2;
        achieved = bankDirections_ == 3;
        break;
    }
    case Step::Roll:
        achieved = input.IsGamepadConnected() ? player.IsRolling() && !wasRolling_ : input.IsKeyTrigger(DIK_SPACE);
        wasRolling_ = player.IsRolling();
        break;
    case Step::Weapon:
    case Step::Missile:
    case Step::Homing:
    case Step::Minigun:
        achieved = currentWeapon_ == GetPracticeWeapon() && defeatedTargets_ >= 3;
        break;
    case Step::Pause:
        // 呼び出し元はゲーム再開後だけUpdateするため、手動ポーズから戻れたことも確認できる。
        achieved = pauseOpened_;
        break;
    case Step::Complete:
        break;
    }
    achieved_ = achieved_ || achieved;
    if (achieved_ && stepTime_ >= 6.0f) {
        successTime_ = 3.0f;
        SoundManager::GetInstance()->Play("TutorialConfirm");
    }
}

void TutorialController::NotifyManualPauseOpened()
{
    if (step_ == Step::Pause && successTime_ <= 0.0f) pauseOpened_ = true;
}

int TutorialController::CountDirections() const
{
    int count = 0;
    for (int i = 0; i < 4; ++i) if (directions_ & (1u << i)) ++count;
    return count;
}

int TutorialController::GetCompletedCount() const
{
    return (std::min)(kStepCount, static_cast<int>(step_) + (successTime_ > 0.0f ? 1 : 0));
}

float TutorialController::LimitRailDistance(float distance) const
{
    if (IsComplete()) return distance;
    if (GetTargetCount() > 0 || step_ == Step::Heat || GetPracticeWeapon()) return 0.0f;
    constexpr float checkpoints[] = { 180, 450, 450, 600, 750, 1050, 1350, 1710, 2070, 2070, 2070, 2070, 2400, 2600 };
    return (std::min)(distance, checkpoints[static_cast<int>(step_)]);
}

std::string TutorialController::GetTitle() const
{
    if (IsComplete()) return "TRAINING COMPLETE // GATE OPEN";
    const char* names[] = { "MOVE", "FIRE", "HEAT", "MOVING TARGET", "BOOST", "BRAKE", "BANK", "ROLL", "NORMAL", "MISSILE", "HOMING MISSILE", "MINIGUN", "PAUSE", "PRACTICE" };
    return "TRAINING " + std::to_string(static_cast<int>(step_) + 1) + " / " + std::to_string(kStepCount) + " // " + names[static_cast<int>(step_)];
}

std::string TutorialController::GetInstructionText(const Input& input) const
{
    if (IsComplete()) return "訓練完了。出撃ゲートを開きました。\n前方の緑色のゲートへ進んでください。";
    if (successTime_ > 0.0f) return "できました！ この練習は完了です。";
    const bool pad = input.IsGamepadConnected();
    switch (step_) {
    case Step::Move:
        return pad ? "左スティックで機体と照準を操作します。\n左右・上下の4方向へ動かしてください。"
            : "マウスで照準を操作します。\n左右・上下の4方向へ動かしてください。";
    case Step::Fire:
        return pad ? "照準を敵に合わせ、AまたはRTで撃ちます。\n反撃しない敵を3体倒しましょう。長押しでも撃てます。"
            : "マウスで照準を敵に合わせ、左クリックで撃ちます。\n反撃しない敵を3体倒しましょう。長押しでも撃てます。";
    case Step::FireMoving:
        return "今度は、ゆっくり動く敵を3体倒しましょう。\n照準で敵を追いかけて撃ちます。時間制限はありません。";
    case Step::Heat:
        return "照準の周りの熱ゲージが高いほど、威力も上がります。\n通常弾・ミニガンは熱を共有し、威力は最大2倍。\n満タンで撃ち続けると、冷え切るまで撃てません。\nNormalを少し連射し、射撃を離して冷ましましょう。";
    case Step::Combat:
        return "最後は、動く敵を5体倒す練習です。\n移動と射撃を組み合わせましょう。敵は反撃しません。";
    case Step::Boost:
        return pad ? "Xを長押しすると加速します。\n1.5秒続けて押し、速さと視野の変化を確かめましょう。"
            : "SHIFTまたは右クリックを長押しすると加速します。\n1.5秒続けて押し、速さと視野の変化を確かめましょう。";
    case Step::Brake:
        return pad ? "Yを長押しすると速度を落とせます。\n1.5秒続けて押しましょう。離すと元の速度に戻ります。"
            : "CTRLを長押しすると速度を落とせます。\n1.5秒続けて押しましょう。離すと元の速度に戻ります。";
    case Step::Bank:
        return pad ? "LBで左、RBで右へ機体を傾けます。\n片方ずつ少し長押しし、機体の動きを確かめましょう。"
            : "機体を傾ける操作は、パッドのLB・RBを使います。\nキーボードでは練習を省略できます。SPACEで次へ進みます。";
    case Step::Roll:
        return pad ? "同じLBまたはRBを素早く2回押すと機体が回転します。\nまずボタンを離してから、トントンと2回押しましょう。"
            : "回転する操作は、パッドのLB・RBの2回押しです。\nキーボードでは練習を省略できます。SPACEで次へ進みます。";
    case Step::Weapon:
        return "Normal（通常弾）は、狙った方向へ飛ぶ基本の弾です。\n長押しで連射し、熱が高くなるほど威力が上がります。\nNormalを選び、敵を3体倒してください。";
    case Step::Missile:
        return "Missileは、通常弾より一発の威力が高い武器です。\n押すたびに発射し、次の一発まで2秒待ちます。\n熱はたまりません。Missileで敵を3体倒しましょう。";
    case Step::Homing:
        return "Homing Missileは、敵を追いかけるミサイルです。\n射撃を長押しして敵を狙い、ロック後に離すと発射。\n熱はたまらず、発射間隔は2秒。敵を3体倒しましょう。";
    case Step::Minigun:
        return "Minigunは、一発は弱くても高速連射できる武器です。\n熱が高いと威力も上がりますが、撃ち続けると過熱します。\n敵を3体倒しましょう。通常弾と熱ゲージを共有します。";
    case Step::Pause:
        return pad ? "STARTでポーズし、設定を確認できます。\nSTARTまたはBで訓練へ戻れます。"
            : "TABでポーズし、設定を確認できます。\nTABまたはESCで訓練へ戻ってください。";
    default:
        return "";
    }
}

std::string TutorialController::GetOperation(const Input& input, const Player& player) const
{
    if (IsComplete()) return "前方のゲートへ進む";
    if (successTime_ > 0.0f) return "練習クリア";
    const bool pad = input.IsGamepadConnected();
    switch (step_) {
    case Step::Move:
        return std::string(pad ? "スティックを4方向 " : "マウス 4方向 ") + std::to_string(CountDirections()) + "/4" +
            (pad && input.IsGamepadPitchInverted() ? " [上下反転ON]" : "");
    case Step::Fire:
    case Step::FireMoving:
    case Step::Combat:
        return std::string(pad ? "どちらかで射撃・撃破 " : "左クリック 射撃・撃破 ") +
            std::to_string(defeatedTargets_) + (step_ == Step::Combat ? "/5" : "/3");
    case Step::Boost: return pad ? "長押しで加速" : "SHIFT / 右クリック 長押し";
    case Step::Heat:
        return heatRaised_ ? "射撃を離してゲージを冷ます" : (pad ? "通常弾を選んで射撃を長押し" : "1でNormal → 左クリックを長押し");
    case Step::Brake: return pad ? "長押しで減速" : "CTRL 長押し";
    case Step::Bank:
        if (!pad) return "SPACE：次の練習へ";
        return "左右をそれぞれ長押し " + std::to_string((bankDirections_ & 1 ? 1 : 0) + (bankDirections_ & 2 ? 1 : 0)) + "/2";
    case Step::Roll: return pad ? "どちらかを素早く2回押す" : "SPACE：次の練習へ";
    case Step::Weapon:
    case Step::Missile:
    case Step::Homing:
    case Step::Minigun:
        return std::string(pad ? "左右で " : "ホイール / 1〜4：") + GetPracticeWeapon() + "  撃破 " + std::to_string(defeatedTargets_) + "/3";
    case Step::Pause: return pad ? "メニューでポーズ・もう一度押して戻る" : "TAB → 設定 / ESCで戻る";
    default: return player.GetCurrentWeaponDisplayName();
    }
}

std::vector<std::string> TutorialController::GetButtonHints(const Input& input) const
{
    if (!input.IsGamepadConnected() || IsComplete() || successTime_ > 0.0f) return {};
    switch (step_) {
    case Step::Move: return { "LS" };
    case Step::Fire:
    case Step::FireMoving:
    case Step::Combat: return { "A", "RT" };
    case Step::Heat: return heatRaised_ ? std::vector<std::string>{ "A", "RT" } : std::vector<std::string>{ "DPAD", "A", "RT" };
    case Step::Boost: return { "X" };
    case Step::Brake: return { "Y" };
    case Step::Bank:
    case Step::Roll: return { "LB", "RB" };
    case Step::Weapon:
    case Step::Missile:
    case Step::Homing:
    case Step::Minigun: return { "DPAD" };
    case Step::Pause: return { "MENU" };
    default: return {};
    }
}
