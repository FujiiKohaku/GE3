#include "App/Game/Camera/CameraPreferences.h"
#include "App/Scene/Gameplay/Pause/BasePauseState.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>

void Check(bool passed, const char* why) { if (!passed) throw std::runtime_error(why); }
int main() {
    try {
        CameraPreferences settings;
        Check(std::abs(settings.FovRadians() - 0.45f) < 0.00001f, "Original FOV changed");
        Check(std::abs(settings.BoostFovRadians() - 0.60f) < 0.00001f, "Original boost FOV changed");
        settings.CyclePreset(1); Check(settings.distance == 27 && settings.verticalFovDegrees == 32, "Low flight preset");
        settings.CyclePreset(1); Check(settings.distance == 22 && settings.verticalFovDegrees == 28, "Aircraft preset");
        settings.CyclePreset(1); Check(settings.preset == 0 && settings.distance == 35, "Preset wrap");
        settings.CyclePreset(-1); Check(settings.preset == 2, "Reverse preset wrap");
        settings.AdjustDistance(100); settings.AdjustFov(100);
        Check(settings.distance == 60 && settings.verticalFovDegrees == 60 && settings.preset == -1, "Upper limits/custom");
        settings.AdjustDistance(-100); settings.AdjustFov(-100);
        Check(settings.distance == 18 && settings.verticalFovDegrees == 20, "Lower limits");
        const auto path = std::filesystem::temp_directory_path() / "KohakuCameraPreferencesTest.json";
        Check(settings.Save(path), "Save failed");
        CameraPreferences loaded; Check(loaded.Load(path) && loaded.distance == 18 && loaded.verticalFovDegrees == 20, "Round trip");
        { std::ofstream broken(path); broken << "{broken"; }
        Check(!loaded.Load(path) && loaded.distance == 35, "Corrupt settings fallback");
        { std::ofstream invalid(path); invalid << R"({"distance":0,"vertical_fov_degrees":180})"; }
        Check(!loaded.Load(path) && loaded.verticalFovDegrees < 26, "Invalid settings fallback");
        std::filesystem::remove(path);
        int selection = 3; PauseStateContext context{selection};
        PauseViewState state = PauseViewState::Settings;
        context.changeState = [&](PauseViewState next, int row) { state = next; selection = row; };
        GetPauseState(PauseViewState::Settings).Activate(context);
        Check(state == PauseViewState::Graphics && selection == 0, "Settings graphics entry");
        auto& graphics = GetPauseState(PauseViewState::Graphics);
        int aaDirection = 0, qualityDirection = 0;
        context.cycleAntiAliasing = [&](int d) { aaDirection = d; };
        context.cycleRenderQuality = [&](int d) { qualityDirection = d; };
        PauseStateInput graphicsInput; graphicsInput.left = true;
        graphics.HandleInput(context, graphicsInput);
        Check(aaDirection == -1, "AA reverse input");
        selection = 1; graphicsInput = {}; graphicsInput.confirm = true;
        graphics.HandleInput(context, graphicsInput);
        Check(qualityDirection == 1, "Quality confirm input");
        selection = 2; graphics.Activate(context);
        Check(state == PauseViewState::Camera && selection == 0, "Graphics camera entry");
        int presetDirection = 0; float distanceChange = 0, fovChange = 0;
        context.cycleCameraPreset = [&](int d) { presetDirection = d; };
        context.adjustCameraDistance = [&](float d) { distanceChange = d; };
        context.adjustCameraFov = [&](float d) { fovChange = d; };
        PauseStateInput input; input.right = true;
        auto& camera = GetPauseState(PauseViewState::Camera);
        camera.HandleInput(context,input); Check(presetDirection == 1, "Preset right input");
        selection = 1; camera.HandleInput(context,input); Check(distanceChange == 1, "Distance right input");
        selection = 2; camera.HandleInput(context,input); Check(fovChange == 1, "FOV right input");
        input = {}; input.back = true; camera.HandleInput(context,input);
        Check(state == PauseViewState::Graphics && selection == 2, "Camera back input");
        graphicsInput = {}; graphicsInput.back = true;
        graphics.HandleInput(context, graphicsInput);
        Check(state == PauseViewState::Settings && selection == 3, "Graphics back input");
        std::cout << "PASS: camera presets, adjustment limits, persistent settings, fallback and pause navigation\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
