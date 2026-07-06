#pragma once
#include <cstdint>
#include <chrono>

#ifdef _DEBUG
class PerformanceCounter {
public:
    void Initialize();
    void Update();

    float GetFPS() const { return fps_; }
    float GetFrameTimeMs() const { return frameTimeMs_; }
    float GetAverageFPS() const { return avgFps_; }
    float GetMinFPS() const { return minFps_; }
    float GetMaxFPS() const { return maxFps_; }
    float GetCPUUsage() const { return cpuUsage_; }
    float GetGPUUsage() const { return gpuUsage_; }
    float GetDeltaTime() const { return deltaTime_; }
    uint64_t GetFrameCount() const { return frameCount_; }

private:
    void CalculateCPUUsage();

private:
    float fps_ = 0.0f;
    float frameTimeMs_ = 0.0f;
    float avgFps_ = 0.0f;
    float minFps_ = 999.0f;
    float maxFps_ = 0.0f;
    float cpuUsage_ = 0.0f;
    float gpuUsage_ = 0.0f;
    float deltaTime_ = 0.0f;
    uint64_t frameCount_ = 0;

    // FPS計算用
    std::chrono::high_resolution_clock::time_point lastFPSUpdate_;
    std::chrono::high_resolution_clock::time_point lastFrameTime_;
    uint32_t fpsFrameCount_ = 0;
    
    // 全体平均用
    double totalFpsSum_ = 0.0;
    uint64_t fpsCountTimes_ = 0;

    // CPU使用率計算用
    uint64_t lastIdleTime_ = 0;
    uint64_t lastKernelTime_ = 0;
    uint64_t lastUserTime_ = 0;
    std::chrono::high_resolution_clock::time_point lastCPUUpdate_;
};
#else
class PerformanceCounter {
public:
    void Initialize() {}
    void Update() {}
};
#endif
