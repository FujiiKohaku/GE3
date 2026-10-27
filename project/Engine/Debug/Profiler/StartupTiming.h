#pragma once
#include <chrono>

namespace StartupTiming {
bool IsRecording();
bool ShouldExitAfterStartup();
void Begin(std::chrono::steady_clock::time_point startedAt);
void Finish();
}

// --measure-startup指定時だけ、最初の描画完了までのCPU経過時間を集計する。
class StartupTimingScope {
public:
    explicit StartupTimingScope(const char* name);
    ~StartupTimingScope();
    StartupTimingScope(const StartupTimingScope&) = delete;
    StartupTimingScope& operator=(const StartupTimingScope&) = delete;

private:
    const char* name_;
    bool isRecording_;
    std::chrono::steady_clock::time_point startedAt_;
};
