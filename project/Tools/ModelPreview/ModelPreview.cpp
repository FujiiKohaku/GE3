#include "ModelPreviewApp.h"
#include "PreviewSmokeTest.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Winapp/WinApp.h"
#include <filesystem>
#include <stdexcept>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace {
void FindResources()
{
    wchar_t executable[32768] {};
    GetModuleFileNameW(nullptr, executable, _countof(executable));
    std::filesystem::path parent = std::filesystem::path(executable).parent_path();
    const std::filesystem::path executableDirectory = parent;
    // Prefer the repository over old resource copies left beside game builds.
    for (int depth = 0; depth < 6; ++depth) {
        if (std::filesystem::is_directory(parent / "project/resources/Models")) {
            std::filesystem::current_path(parent / "project");
            return;
        }
        parent = parent.parent_path();
    }
    if (std::filesystem::is_directory("resources/Models")) { return; }
    if (std::filesystem::is_directory(executableDirectory / "resources/Models")) {
        std::filesystem::current_path(executableDirectory);
        return;
    }
    throw std::runtime_error("Could not find resources/Models beside the app or in the repository");
}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
    ModelPreviewApp app;
    int exitCode = 0;
    try {
        FindResources();
        Logger::Initialize();
        app.Initialize();
        // Hidden deterministic export verifies real DX12 drawing and both encoders without launching the game.
        bool smokeTest = std::string(commandLine).find("--smoke-test") != std::string::npos;
        if (smokeTest) {
            RunModelPreviewSmokeTest(app);
        } else {
            WinApp::GetInstance()->Show();
            app.Load("resources/Models/Environment/Ice/ice_boulder.obj", false);
            while (!WinApp::GetInstance()->ProcessMessage()) {
                app.Update();
                app.Draw();
            }
        }
        app.Finalize();
    } catch (const std::exception& error) {
        Logger::Error(error.what());
        OutputDebugStringA(error.what());
        exitCode = 1;
    }
    Logger::Finalize();
    WinApp::FinalizeInstance();
    return exitCode;
}
