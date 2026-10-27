#include "StartupTiming.h"
#include "Engine/Logger/Logger.h"
#include <Windows.h>
#include <array>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string_view>

namespace {
constexpr size_t kMaxStartupRecords = 128;
struct StartupRecord {
    const char* name = nullptr;
    double totalMs = 0.0;
    double maxMs = 0.0;
    size_t callCount = 0;
};
struct StartupState {
    std::array<StartupRecord, kMaxStartupRecords> records {};
    size_t recordCount = 0;
    bool hasBegun = false;
    bool isFinished = false;
    std::chrono::steady_clock::time_point startedAt;
};
StartupState& GetState()
{
    static StartupState state;
    return state;
}
bool IsEnabled()
{
    static const bool kIsEnabled = std::wstring_view(GetCommandLineW()).find(L"--measure-startup") != std::wstring_view::npos;
    return kIsEnabled;
}
void Record(const char* name, double elapsedMs)
{
    StartupState& state = GetState();
    size_t recordIndex = 0;
    while (recordIndex < state.recordCount && std::strcmp(state.records[recordIndex].name, name) != 0) {
        ++recordIndex;
    }
    if (recordIndex == state.recordCount) {
        if (state.recordCount >= kMaxStartupRecords) { return; }
        state.records[recordIndex].name = name;
        ++state.recordCount;
    }
    StartupRecord& record = state.records[recordIndex];
    record.totalMs += elapsedMs;
    ++record.callCount;
    if (elapsedMs > record.maxMs) { record.maxMs = elapsedMs; }
}
}

bool StartupTiming::IsRecording()
{
    return IsEnabled() && GetState().hasBegun && !GetState().isFinished;
}
bool StartupTiming::ShouldExitAfterStartup()
{
    return IsEnabled() && std::wstring_view(GetCommandLineW()).find(L"--exit-after-startup") != std::wstring_view::npos;
}
void StartupTiming::Begin(std::chrono::steady_clock::time_point startedAt)
{
    if (!IsEnabled()) { return; }
    StartupState& state = GetState();
    state.startedAt = startedAt;
    state.hasBegun = true;
}
void StartupTiming::Finish()
{
    if (!IsRecording()) { return; }
    StartupState& state = GetState();
    Record("Main.Total", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - state.startedAt).count());
    state.isFinished = true;
    // 子の時間は親にも含まれるため、異なる階層の値を合算しない。
    Logger::Log("[StartupTiming] Inclusive CPU wall times; nested rows overlap.");
    for (size_t recordIndex = 0; recordIndex < state.recordCount; ++recordIndex) {
        const StartupRecord& record = state.records[recordIndex];
        std::ostringstream line;
        line.imbue(std::locale::classic());
        line << std::fixed << std::setprecision(3) << "[StartupTiming] name=" << record.name
            << " total_ms=" << record.totalMs << " calls=" << record.callCount << " max_ms=" << record.maxMs;
        Logger::Log(line.str());
    }
    Logger::Flush();
}
StartupTimingScope::StartupTimingScope(const char* name)
    : name_(name), isRecording_(StartupTiming::IsRecording())
{
    if (isRecording_) { startedAt_ = std::chrono::steady_clock::now(); }
}
StartupTimingScope::~StartupTimingScope()
{
    if (!isRecording_) { return; }
    const double elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startedAt_).count();
    Record(name_, elapsedMs);
}
