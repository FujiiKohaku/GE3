#include "Engine/Debug/Profiler/FpsMonitor.h"
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <thread>
#include <limits>

namespace {
FpsMonitor::Clock::time_point now;
auto FakeClock() -> FpsMonitor::Clock::time_point { return now; }
void Advance(double milliseconds) {
    now += std::chrono::duration_cast<FpsMonitor::Clock::duration>(std::chrono::duration<double, std::milli>(milliseconds));
}
void Check(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
void Collect(std::string_view line, void* context) {
    static_cast<std::vector<std::string>*>(context)->emplace_back(line);
}
bool Contains(const std::vector<std::string>& logs, std::string_view text) {
    for (const auto& line : logs) { if (line.find(text) != std::string::npos) { return true; } }
    return false;
}
size_t Count(const std::vector<std::string>& logs, std::string_view text) {
    size_t count = 0;
    for (const auto& line : logs) { if (line.find(text) != std::string::npos) { ++count; } }
    return count;
}
FpsMonitor::Config Config() {
    FpsMonitor::Config config;
    config.sampleSeconds = 0.1;
    config.lowFpsSeconds = 0.2;
    config.cooldownSeconds = 0.5;
    config.warmupSeconds = 0;
    config.spikeMilliseconds = 0;
    return config;
}
void CpuFrame(FpsMonitor& monitor, double ms) {
    monitor.BeginFrame();
    {
        auto parent = monitor.Measure("Game.Update");
        { auto child = monitor.Measure("Collision.PlayerBullets"); Advance(ms * 0.8); }
        Advance(ms * 0.2);
    }
    monitor.SetGpuFrame(5, true);
    monitor.SetCounter("Enemies", 12);
    monitor.EndFrame();
}

void DetectionAndRecovery()
{
    now = {};
    FpsMonitor monitor(FakeClock);
    std::vector<std::string> logs;
    monitor.SetLogSink(Collect, &logs);
    monitor.Initialize(Config());
    monitor.SetContext("TestStage");
    for (int i = 0; i < 7; ++i) { CpuFrame(monitor, 25); }
    Check(monitor.GetIncidentCount() == 0, "must wait for sustained low FPS");
    CpuFrame(monitor, 25);
    Check(monitor.GetIncidentCount() == 1 && monitor.IsLowFps(), "40 FPS must trigger");
    Check(Contains(logs, "candidate=CPU-side Collision.PlayerBullets"), "CPU candidate ranking");
    Check(Contains(logs, "context=TestStage") && Contains(logs, "Count Enemies peak=12"), "context/counters missing");
    Check(std::abs(monitor.GetFps() - 40) < 0.001, "FPS must be frames / elapsed seconds");
    for (int i = 0; i < 16; ++i) { CpuFrame(monitor, 25); }
    Check(monitor.GetIncidentCount() == 1, "cooldown must suppress repeated incidents");
    for (int i = 0; i < 4; ++i) { CpuFrame(monitor, 25); }
    Check(monitor.GetIncidentCount() == 2, "continued low FPS must re-report after cooldown");
    for (int i = 0; i < 12; ++i) { CpuFrame(monitor, 10); }
    Check(!monitor.IsLowFps() && Count(logs, "[Recovered]") == 1, "recovery must be reported once");
}

void NestedTimingsAndGaps()
{
    now = {};
    FpsMonitor monitor(FakeClock);
    auto config = Config();
    config.lowFpsSeconds = 60;
    monitor.Initialize(config);
    monitor.BeginFrame();
    {
        auto parent = monitor.Measure("Parent");
        Advance(5);
        { auto child = monitor.Measure("Child"); Advance(10); }
        Advance(5);
    }
    const auto* parent = monitor.FindSection("Parent");
    const auto* child = monitor.FindSection("Child");
    Check(parent && child && std::abs(parent->frameExclusive - 10) < 0.001 &&
        std::abs(parent->frameInclusive - 20) < 0.001 && std::abs(child->frameExclusive - 10) < 0.001,
        "nested timings must not double-count");
    std::thread worker([&] { auto scope = monitor.Measure("Background"); });
    worker.join();
    Check(!monitor.FindSection("Background"), "worker must not mutate main-thread scopes");
    monitor.EndFrame();
    Advance(30);
    monitor.BeginFrame();
    Advance(50);
    monitor.EndFrame();
    Check(std::abs(monitor.GetFps() - 20) < 0.001, "FPS must include frame gap, not game delta");
    const auto* gap = monitor.FindSection("Frame.Gap");
    Check(gap && std::abs(gap->frameExclusive - 30) < 0.001, "message-pump gap must be visible");
}

void GpuAndMissingSamples()
{
    now = {};
    FpsMonitor monitor(FakeClock);
    std::vector<std::string> logs;
    monitor.SetLogSink(Collect, &logs);
    auto config = Config();
    config.lowFpsSeconds = 0;
    config.cooldownSeconds = 0.1;
    monitor.Initialize(config);
    for (int i = 0; i < 3; ++i) {
        monitor.BeginFrame();
        { auto cpu = monitor.Measure("Render.Commands"); Advance(2); }
        { auto wait = monitor.Measure("Wait.GpuFence"); Advance(38); }
        monitor.SetGpuFrame(37, true);
        monitor.SetGpuDetail("DLAA", 3, true);
        monitor.EndFrame();
    }
    Check(Contains(logs, "candidate=GPU workload"), "GPU candidate requires actual GPU measurements");
    Check(Contains(logs, "GPU detail DLAA=3.000ms"), "GPU detail must be available");
    logs.clear();
    for (int i = 0; i < 3; ++i) {
        monitor.BeginFrame();
        { auto wait = monitor.Measure("Wait.Present"); Advance(40); }
        monitor.SetGpuFrame(0, false);
        monitor.EndFrame();
    }
    Check(Contains(logs, "GPU frame=unavailable"), "missing sample must not look like zero GPU work");
    Check(!Contains(logs, "GPU detail DLAA"), "a previous GPU detail must not leak into a new frame");
    Check(Contains(logs, "Present/VSync or GPU fence wait"), "waiting must not be called CPU overload");
}

void SpikesWarmupAndSkippedFrames()
{
    now = {};
    FpsMonitor monitor(FakeClock);
    std::vector<std::string> logs;
    monitor.SetLogSink(Collect, &logs);
    auto config = Config();
    config.warmupSeconds = 1;
    config.spikeMilliseconds = 100;
    monitor.Initialize(config);
    CpuFrame(monitor, 1200); // boot frame starts in warmup; never blame startup
    Check(logs.empty(), "warmup must exclude the whole boot frame");
    monitor.BeginFrame();
    { auto modal = monitor.Measure("Dialog"); Advance(5000); }
    monitor.SkipFrame(); monitor.EndFrame();
    Check(logs.empty(), "modal/minimized frames must be excluded");
    CpuFrame(monitor, 120);
    Check(Contains(logs, "[FrameSpike]") && monitor.GetIncidentCount() == 1, "long frame must produce a spike incident");
    CpuFrame(monitor, 120);
    Check(monitor.GetIncidentCount() == 1, "spikes must share rate limiting");
}

void DisabledAndConfig()
{
    now = {};
    FpsMonitor monitor(FakeClock);
    std::vector<std::string> logs;
    monitor.SetLogSink(Collect, &logs);
    auto config = Config(); config.enabled = false;
    monitor.Initialize(config);
    CpuFrame(monitor, 5000);
    Check(logs.empty() && !monitor.FindSection("Game.Update"), "disabled monitoring must be inert");
    std::filesystem::create_directories("generated/verification");
    const std::string file = "generated/verification/fps-monitor.json";
    { std::ofstream stream(file); stream << R"({"thresholdFps":45,"topSections":-1,"warmupSeconds":-20})"; }
    Check(monitor.LoadConfig(file) && monitor.GetConfig().thresholdFps == 45 && monitor.GetConfig().topSections == 1 &&
        monitor.GetConfig().warmupSeconds == 3, "configuration values must be validated");
    { std::ofstream stream(file); stream << "invalid json"; }
    Check(!monitor.LoadConfig(file) && monitor.GetConfig().thresholdFps == 50, "malformed JSON must fall back safely");
    std::filesystem::remove(file);
}

void ContextChanges()
{
    now = {};
    FpsMonitor monitor(FakeClock);
    std::vector<std::string> logs;
    monitor.SetLogSink(Collect, &logs);
    monitor.Initialize(Config());
    monitor.SetContext("StageA");
    for (int i = 0; i < 8; ++i) { CpuFrame(monitor, 25); }
    Check(monitor.IsLowFps(), "first stage must be low");
    // Even exactly at a sample boundary, a new stage starts a new low-FPS episode.
    monitor.SetContext("StageB");
    logs.clear();
    for (int i = 0; i < 4; ++i) { CpuFrame(monitor, 25); }
    Check(!monitor.IsLowFps() && logs.empty(), "scene changes must reset sustained-low history");
}
}

int main()
{
    try {
        DetectionAndRecovery(); NestedTimingsAndGaps(); GpuAndMissingSamples();
        SpikesWarmupAndSkippedFrames(); DisabledAndConfig(); ContextChanges();
        std::puts("PASS: FpsMonitor sustained threshold, cooldown, recovery, exclusive scopes, GPU validity, real-time gaps, warmup and config");
        return 0;
    } catch (const std::exception& error) { std::printf("FAILED: %s\n", error.what()); return 1; }
}
