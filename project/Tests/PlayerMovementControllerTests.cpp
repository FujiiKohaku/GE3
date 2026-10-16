#include "Tests/PlayerTestAccess.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/input/Input.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

// 実機なしでも、同時押し・切断・接続直後の状態を再現する。
struct InputTestAccess {
    static void Set(Input& input, XINPUT_GAMEPAD current, XINPUT_GAMEPAD previous = {},
        bool connected = true, bool ready = true, bool keyboardFire = false) {
        input.gamepadState_.Gamepad = current;
        input.preGamepadState_.Gamepad = previous;
        input.isGamepadConnected_ = connected;
        input.gamepadButtonsReady_ = ready;
        input.isUsingGamepad_ = connected;
        input.blockPrimaryFire_ = false;
        input.keys_[DIK_SPACE] = keyboardFire ? 0x80 : 0;
        input.preKeys_[DIK_SPACE] = 0;
    }
};

namespace {
void TestGamepadFireAndDeadZone()
{
    Input& input = *Input::GetInstance();
    assert(!input.IsGamepadPitchInverted());
    XINPUT_GAMEPAD pad{};
    pad.bRightTrigger = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    pad.sThumbLX = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    InputTestAccess::Set(input, pad);
    assert(!input.IsPrimaryFirePressed());
    assert(input.GetGamepadLeftStickX() == 0.0f);
    pad.bRightTrigger++;
    pad.sThumbLX = 32767;
    InputTestAccess::Set(input, pad);
    assert(input.IsPrimaryFirePressed());
    assert(input.IsPrimaryFireTrigger());
    assert(std::abs(input.GetGamepadLeftStickX() - 1.0f) < 0.0001f);
    pad.sThumbLY = 32767;
    InputTestAccess::Set(input, pad);
    const float x = input.GetGamepadLeftStickX();
    const float y = input.GetGamepadLeftStickY();
    assert(std::abs(x * x + y * y - 1.0f) < 0.0001f);
    XINPUT_GAMEPAD previous = pad;
    pad.wButtons = XINPUT_GAMEPAD_A;
    InputTestAccess::Set(input, pad, previous);
    assert(input.IsPrimaryFirePressed());
    assert(!input.IsPrimaryFireTrigger()); // RTを押したままAを追加しても再発射しない。
    pad.bRightTrigger = 0;
    InputTestAccess::Set(input, pad, previous);
    assert(input.IsPrimaryFirePressed());
    assert(!input.IsPrimaryFireTrigger()); // RTを離してもAが残れば長押しを維持する。
    input.BlockPrimaryFireUntilRelease();
    assert(!input.IsPrimaryFirePressed());
    assert(!input.IsPrimaryFireTrigger());
    InputTestAccess::Set(input, pad, {}, true, false);
    assert(!input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_A));
    assert(!input.IsPrimaryFirePressed()); // 再接続時の押しっぱなしを無視する。
    InputTestAccess::Set(input, pad, {}, false);
    assert(!input.IsPrimaryFirePressed());
    assert(input.GetGamepadLeftStickX() == 0.0f);
    InputTestAccess::Set(input, {}, {}, false, false, true);
    assert(input.IsPrimaryFirePressed()); // パッドなしでも従来のSpace入力が使える。
    assert(input.IsPrimaryFireTrigger());
    InputTestAccess::Set(input, {});
}

void TestGamepadAimAndRailControls()
{
    Input& input = *Input::GetInstance();
    Player aim;
    const float halfHeight = WinApp::GetInstance()->GetClientHeight() * 0.5f;
    const float halfWidth = WinApp::GetInstance()->GetClientWidth() * 0.5f;
    PlayerTestAccess::InitializeAim(aim);
    PlayerTestAccess::UpdateGamepadAim(aim, 0.5f, 0.0f, false, 0.05f);
    const float halfStickDisplacement = aim.GetAimScreenPosition().x - halfWidth;
    PlayerTestAccess::InitializeAim(aim);
    PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, 0.05f);
    const float fullStickDisplacement = aim.GetAimScreenPosition().x - halfWidth;
    assert(halfStickDisplacement > 0.0f);
    assert(std::abs(halfStickDisplacement / fullStickDisplacement - 0.375f) < 0.0001f);
    PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, 10.0f);
    assert(std::abs(aim.GetAimScreenPosition().x - (halfWidth * 2.0f - 64.0f)) < 0.001f);
    PlayerTestAccess::UpdateGamepadAim(aim, 0.0f, 1.0f, true, 10.0f);
    assert(aim.GetAimScreenPosition().y > halfHeight);
    PlayerTestAccess::UpdateGamepadAim(aim, 0.0f, 1.0f, false, 10.0f);
    assert(aim.GetAimScreenPosition().y < halfHeight);
    PlayerTestAccess::InitializeAim(aim);
    PlayerTestAccess::UpdateGamepadAim(aim, 0.0f, 0.0f, true, 1.0f / 60.0f);
    assert(aim.GetAimScreenPosition().x == halfWidth);
    assert(aim.GetAimScreenPosition().y == halfHeight);
    EulerTransform transform{};
    Player& movement = aim;
    PlayerTestAccess::Transform(movement) = transform;
    PlayerTestAccess::InitializeMovement(movement);
    movement.SetControlMode(Player::ControlMode::KeyboardAndMouse);
    PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, true, 10.0f);
    XINPUT_GAMEPAD pad{};
    pad.wButtons = XINPUT_GAMEPAD_LEFT_SHOULDER;
    InputTestAccess::Set(input, pad);
    PlayerTestAccess::UpdateMovement(movement, &input);
    assert(movement.GetRailOffset().x > 0.0f); // WASDモードでもパッドは照準から操舵する。
    assert(movement.GetManualBank() > 0.0f);
    assert(!movement.IsRolling());
    InputTestAccess::Set(input, {}, pad);
    PlayerTestAccess::UpdateMovement(movement, &input);
    InputTestAccess::Set(input, pad);
    PlayerTestAccess::UpdateMovement(movement, &input);
    assert(movement.IsRolling());
    PlayerTestAccess::StopMovement(movement);
    assert(!movement.IsRolling());
    assert(movement.GetManualBank() == 0.0f);
    pad.wButtons = XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
    InputTestAccess::Set(input, pad);
    PlayerTestAccess::UpdateBoost(movement, &input, false);
    assert(!movement.IsBoosting());
    assert(movement.GetForwardSpeedMultiplier() == 0.5f);
    PlayerTestAccess::UpdateBoost(movement, &input, true);
    assert(movement.GetForwardSpeedMultiplier() == 1.0f);
    InputTestAccess::Set(input, {});
}

bool IsNear(float actual, float expected)
{
    return std::abs(actual - expected) < 0.0001f;
}

void TestGamepadAimSlowReturnAndFrameRates()
{
    const float halfWidth = WinApp::GetInstance()->GetClientWidth() * 0.5f;
    float baseline = 0.0f;
    const int frameRates[] = { 30, 60, 120 };
    for (int framesPerSecond : frameRates) {
        Player aim;
        PlayerTestAccess::InitializeAim(aim);
        const float dt = 1.0f / static_cast<float>(framesPerSecond);
        PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, dt);
        assert(aim.GetAimScreenPosition().x > halfWidth);
        assert(aim.GetAimScreenPosition().x < halfWidth * 1.75f);
        for (int frame = 1; frame < framesPerSecond; ++frame) {
            PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, dt);
        }
        const float startOffset = aim.GetAimScreenPosition().x - halfWidth;
        const float previousPosition = aim.GetAimScreenPosition().x;
        PlayerTestAccess::UpdateGamepadAim(aim, 0.0f, 0.0f, false, 0.0f);
        assert(aim.GetAimScreenPosition().x == previousPosition);
        PlayerTestAccess::UpdateGamepadAim(aim, 0.0f, 0.0f, false, dt);
        assert(aim.GetAimScreenPosition().x > halfWidth + startOffset * 0.95f);
        assert(aim.GetAimScreenPosition().x < previousPosition);
        for (int frame = 1; frame < framesPerSecond; ++frame) {
            PlayerTestAccess::UpdateGamepadAim(aim, 0.0f, 0.0f, false, dt);
        }
        const float remainingOffset = aim.GetAimScreenPosition().x - halfWidth;
        assert(std::abs(remainingOffset / startOffset - std::exp(-0.8f)) < 0.0001f);
        if (baseline == 0.0f) {
            baseline = remainingOffset;
        } else {
            assert(std::abs(remainingOffset - baseline) < 0.01f);
        }
        // 操作中は中央へ引き戻されず、弱く倒すと同じ方向へゆっくり進む。
        PlayerTestAccess::InitializeAim(aim);
        PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, 0.05f);
        const float beforeSmallInput = aim.GetAimScreenPosition().x;
        PlayerTestAccess::UpdateGamepadAim(aim, 0.5f, 0.0f, false, dt);
        assert(aim.GetAimScreenPosition().x > beforeSmallInput);
        // 反対側への明示的な入力は、遅い復帰速度に縛られない。
        PlayerTestAccess::UpdateGamepadAim(aim, -1.0f, 0.0f, false, 2.0f);
        assert(aim.GetAimScreenPosition().x < halfWidth);
        PlayerTestAccess::InitializeAim(aim);
        assert(aim.GetAimScreenPosition().x == halfWidth);
    }
}

void TestGamepadAimEdgesAndConstantSpeed()
{
    const float width = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float height = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    Player aim;
    PlayerTestAccess::InitializeAim(aim);
    PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, 0.01f);
    const float firstStep = aim.GetAimScreenPosition().x - width * 0.5f;
    const float timeToNearEdge = 0.01f * (width * 0.5f * 0.85f / firstStep - 1.0f);
    PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, timeToNearEdge);
    const float nearEdge = aim.GetAimScreenPosition().x;
    PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, 0.01f);
    assert(std::abs(aim.GetAimScreenPosition().x - nearEdge - firstStep) < 0.001f);
    PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, 10.0f);
    assert(std::abs(aim.GetAimScreenPosition().x - (width - 64.0f)) < 0.001f);
    PlayerTestAccess::InitializeAim(aim);
    const float diagonal = 1.0f / std::sqrt(2.0f);
    PlayerTestAccess::UpdateGamepadAim(aim, diagonal, -diagonal, false, 10.0f);
    assert(std::abs(aim.GetAimScreenPosition().x - (width - 64.0f)) < 0.001f);
    assert(std::abs(aim.GetAimScreenPosition().y - (height - 64.0f)) < 0.001f);
    PlayerTestAccess::UpdateGamepadAim(aim, -diagonal, diagonal, false, 10.0f);
    assert(std::abs(aim.GetAimScreenPosition().x - 64.0f) < 0.001f);
    assert(std::abs(aim.GetAimScreenPosition().y - 64.0f) < 0.001f);
}

void TestGamepadAimDistanceLimit()
{
    const float width = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float height = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    const Vector2 center{ width * 0.5f, height * 0.5f };
    const float radius = (std::min)(width, height) * 0.40f;
    const Vector2 directions[] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,1}, {1,-1}, {-1,-1} };
    for (const Vector2& direction : directions) {
        Player aim;
        PlayerTestAccess::InitializeAim(aim);
        PlayerTestAccess::UpdateGamepadAim(aim, direction.x, direction.y, false, 10.0f);
        PlayerTestAccess::ClampGamepadAimToPlayer(aim, center);
        const Vector2 position = aim.GetAimScreenPosition();
        const float dx = position.x - center.x;
        const float dy = position.y - center.y;
        assert(std::abs(std::sqrt(dx * dx + dy * dy) - radius) < 0.001f);
    }
    Player aim;
    PlayerTestAccess::InitializeAim(aim);
    for (int frame = 0; frame < 120; ++frame) {
        PlayerTestAccess::UpdateGamepadAim(aim, 1.0f, 0.0f, false, 1.0f / 60.0f);
        PlayerTestAccess::ClampGamepadAimToPlayer(aim, center);
        assert(aim.GetAimScreenPosition().x <= center.x + radius + 0.001f);
    }
    const float atLimit = aim.GetAimScreenPosition().x;
    PlayerTestAccess::UpdateGamepadAim(aim, -1.0f, 0.0f, false, 1.0f / 60.0f);
    PlayerTestAccess::ClampGamepadAimToPlayer(aim, center);
    assert(aim.GetAimScreenPosition().x < atLimit); // 上限で入力を蓄積せず、すぐ切り返す。
    const Vector2 movedPlayer{ center.x + 100.0f, center.y + 50.0f };
    PlayerTestAccess::ClampGamepadAimToPlayer(aim, movedPlayer);
    const Vector2 afterMove = aim.GetAimScreenPosition();
    const float dx = afterMove.x - movedPlayer.x;
    const float dy = afterMove.y - movedPlayer.y;
    assert(dx * dx + dy * dy <= radius * radius + 0.1f);
    const Vector2 corners[] = { {64,64}, {width-64,64}, {64,height-64}, {width-64,height-64} };
    for (const Vector2& corner : corners) {
        PlayerTestAccess::InitializeAim(aim);
        PlayerTestAccess::ClampGamepadAimToPlayer(aim, corner);
        const Vector2 position = aim.GetAimScreenPosition();
        const float cx = position.x - corner.x;
        const float cy = position.y - corner.y;
        assert(cx * cx + cy * cy <= radius * radius + 0.1f);
        assert(position.x >= 64.0f && position.x <= width - 64.0f);
        assert(position.y >= 64.0f && position.y <= height - 64.0f);
    }
    const Vector2 beforeInvalid = aim.GetAimScreenPosition();
    PlayerTestAccess::ClampGamepadAimToPlayer(aim, { std::numeric_limits<float>::quiet_NaN(), center.y });
    assert(aim.GetAimScreenPosition().x == beforeInvalid.x);
    assert(aim.GetAimScreenPosition().y == beforeInvalid.y);
    PlayerTestAccess::InitializeAim(aim);
    PlayerTestAccess::ClampGamepadAimToPlayer(aim, { 200.0f, center.y });
    assert(aim.GetAimScreenPosition().x < center.x);
    // 機体より右の照準には右へ操舵する。画面中央より左でも端へ押し続けない。
    assert(PlayerTestAccess::GetGamepadSteeringScreenPosition(aim).x > center.x);
    PlayerTestAccess::UpdateGamepadAim(aim, 0.0f, 0.0f, false, 1.0f / 60.0f);
    PlayerTestAccess::ClampGamepadAimToPlayer(aim, { 200.0f, center.y });
    assert(PlayerTestAccess::GetGamepadSteeringScreenPosition(aim).x > center.x);
    PlayerTestAccess::InitializeAim(aim);
    assert(PlayerTestAccess::GetGamepadSteeringScreenPosition(aim).x == center.x);
}

void TestRailFrameAndAreaForce()
{
    EulerTransform transform{};
    transform.translate = { 10.0f, 20.0f, 30.0f };
    Player aim;
    Player& movement = aim;
    PlayerTestAccess::Transform(movement) = transform;
    PlayerTestAccess::InitializeMovement(movement);
    movement.SetRailFrame({ 10.0f, 20.0f, 30.0f }, { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f });
    movement.ApplyRailAreaForce({ 100.0f, -100.0f, 0.0f });
    assert(movement.GetRailOffset().x == 20.0f);
    assert(movement.GetRailOffset().y == -15.0f);
    PlayerTestAccess::ApplyRailPosition(movement);
    assert(PlayerTestAccess::Transform(movement).translate.x == 10.0f);
    assert(PlayerTestAccess::Transform(movement).translate.y == 5.0f);
    assert(PlayerTestAccess::Transform(movement).translate.z == 50.0f);
    assert(movement.GetAutomaticWorldVelocity().x == -0.5f);
    PlayerTestAccess::Transform(movement).translate = { 4.0f, 5.0f, 6.0f };
    PlayerTestAccess::SynchronizePosition(movement);
    PlayerTestAccess::ApplyRailPosition(movement);
    assert(PlayerTestAccess::Transform(movement).translate.x == 4.0f);
    assert(PlayerTestAccess::Transform(movement).translate.y == 5.0f);
    assert(PlayerTestAccess::Transform(movement).translate.z == 6.0f);
}

void TestAllRangeAndControls()
{
    EulerTransform transform{};
    transform.translate = { 12.0f, 34.0f, 56.0f };
    transform.rotate = { 0.2f, 0.4f, 0.0f };
    Player aim;
    Player& movement = aim;
    PlayerTestAccess::Transform(movement) = transform;
    PlayerTestAccess::InitializeMovement(movement);
    movement.ApplyRailAreaForce({ 3.0f, 4.0f, 0.0f });
    movement.EnableAllRangeMode(600.0f, 5.0f, 180.0f);
    assert(movement.IsAllRangeMode());
    assert(!movement.IsReturningToFlightArea());
    assert(movement.GetRailOffset().x == 0.0f);
    assert(movement.GetRailOffset().y == 0.0f);
    const Vector3 forward = movement.GetFlightForward();
    assert(IsNear(forward.x, std::sin(-0.4f) * std::cos(-0.2f)));
    assert(IsNear(forward.y, std::sin(-0.2f)));
    assert(IsNear(forward.z, std::cos(-0.4f) * std::cos(-0.2f)));
    PlayerTestAccess::ApplyRailPosition(movement);
    assert(PlayerTestAccess::Transform(movement).translate.x == 12.0f);
    assert(PlayerTestAccess::Transform(movement).translate.y == 34.0f);
    assert(PlayerTestAccess::Transform(movement).translate.z == 56.0f);
    movement.SetMouseSensitivity(0.0f);
    assert(movement.GetMouseSensitivity() == 0.5f);
    movement.SetMouseSensitivity(10.0f);
    assert(movement.GetMouseSensitivity() == 2.0f);
    movement.SetControlMode(Player::ControlMode::KeyboardAndMouse);
    assert(movement.GetControlMode() == Player::ControlMode::KeyboardAndMouse);
    PlayerTestAccess::Transform(movement).rotate = { 0.3f, -0.5f, 0.0f };
    PlayerTestAccess::SynchronizeOrientation(movement);
    const Vector3 synchronizedForward = movement.GetFlightForward();
    assert(IsNear(synchronizedForward.x, std::sin(0.5f) * std::cos(-0.3f)));
    assert(IsNear(synchronizedForward.y, std::sin(-0.3f)));
}

void TestScreenConstraintWithCamera()
{
    Camera camera;
    camera.SetProjectionJitter({ 0.0f, 0.0f });
    Vector3 basePosition{};
    const Vector3 right{ 1.0f, 0.0f, 0.0f };
    const Vector3 up{ 0.0f, 1.0f, 0.0f };
    const Vector3 forward{ 0.0f, 0.0f, 1.0f };
    Player constraint;
    constraint.SetRailFrame(basePosition, right, up, forward);
    constraint.SetCamera(&camera);
    const Vector3 correctedOffset = PlayerTestAccess::ClampRailOffsetToScreen(constraint, { 20.0f, 12.0f, 99.0f });
    assert(correctedOffset.z == 0.0f);
    const Vector3 worldPosition = PlayerTestAccess::CalculateRailWorldPosition(constraint, correctedOffset);
    const float screenWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float screenHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
        float cornerX = -1.5f;
        float cornerY = -1.0f;
        if (cornerIndex % 2 == 1) {
            cornerX = 1.5f;
        }
        if (cornerIndex >= 2) {
            cornerY = 1.0f;
        }
        const Vector2 screenPosition = camera.WorldToScreen(worldPosition + Vector3{ cornerX, cornerY, 0.0f });
        assert(screenPosition.x >= 99.0f);
        assert(screenPosition.x <= screenWidth - 99.0f);
        assert(screenPosition.y >= 99.0f);
        assert(screenPosition.y <= screenHeight - 99.0f);
    }
    basePosition = { 0.0f, 0.0f, 50.0f };
    constraint.SetRailFrame(basePosition, right, up, forward);
    const Vector3 updatedWorldPosition = PlayerTestAccess::CalculateRailWorldPosition(constraint, { 1.0f, 2.0f, 0.0f });
    assert(updatedWorldPosition.z == 50.0f);
}
}

int main()
{
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    TestGamepadFireAndDeadZone();
    TestGamepadAimAndRailControls();
    TestGamepadAimSlowReturnAndFrameRates();
    TestGamepadAimEdgesAndConstantSpeed();
    TestGamepadAimDistanceLimit();
    TestRailFrameAndAreaForce();
    TestAllRangeAndControls();
    TestScreenConstraintWithCamera();
}


