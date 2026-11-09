#include "Engine/Debug/Profiler/StartupTiming.h"
#include "Engine/Logger/Logger.h"
#include "App/AlmaRun/Application/Game.h"
#include <chrono>
#include <exception>

// AlmaRun is the only application on this branch.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    const auto startupBegin = std::chrono::steady_clock::now();
    StartupTiming::Begin(startupBegin);
    Logger::Initialize();
    D3DResourceLeakChecker leakChecker;
    Game game;
    int exitCode = 0;
    bool firstFrame = true;
    try {
        Logger::Log("Application Start");
        game.Initialize();
        while (!WinApp::GetInstance()->ProcessMessage() && !game.IsEndRequest()) {
            game.Update();
            game.Draw();
            if (firstFrame) {
                firstFrame = false;
                const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startupBegin).count();
                Logger::Log("[StartupTime] First frame presented: " + std::to_string(milliseconds) + " ms");
                StartupTiming::Finish();
                if (StartupTiming::ShouldExitAfterStartup()) { break; }
            }
        }
    } catch (const std::exception& error) {
        Logger::Error(std::string("Application failed: ") + error.what());
        Logger::Flush();
        exitCode = 1;
    }
    game.Finalize();
    Logger::Log("Application End");
    Logger::Finalize();
    return exitCode;
}
