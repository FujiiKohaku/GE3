#include "PerformanceCounter.h"

#ifdef _DEBUG
#include <Windows.h>
#include "Profiler.h"
#include "GPUProfiler.h"

void PerformanceCounter::Initialize()
{
    fps_ = 0.0f;
    frameTimeMs_ = 0.0f;
    avgFps_ = 0.0f;
    minFps_ = 999.0f;
    maxFps_ = 0.0f;
    cpuUsage_ = 0.0f;
    gpuUsage_ = 0.0f;
    deltaTime_ = 0.0f;
    frameCount_ = 0;

    auto now = std::chrono::high_resolution_clock::now();
    lastFPSUpdate_ = now;
    lastFrameTime_ = now;
    lastCPUUpdate_ = now;
    fpsFrameCount_ = 0;
    totalFpsSum_ = 0.0;
    fpsCountTimes_ = 0;

    // CPU使用率初期化
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        lastIdleTime_ = (static_cast<uint64_t>(idleTime.dwHighDateTime) << 32) | idleTime.dwLowDateTime;
        lastKernelTime_ = (static_cast<uint64_t>(kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
        lastUserTime_ = (static_cast<uint64_t>(userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;
    }
}

void PerformanceCounter::Update()
{
    frameCount_++;
    fpsFrameCount_++;

    auto now = std::chrono::high_resolution_clock::now();
    
    // 1. DeltaTime と FrameTime の算出
    auto frameDuration = std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameTime_).count();
    lastFrameTime_ = now;

    deltaTime_ = static_cast<float>(frameDuration) / 1000000.0f;
    frameTimeMs_ = static_cast<float>(frameDuration) / 1000.0f;

    // 2. FPSの算出 (0.5秒おきに更新)
    auto fpsDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSUpdate_).count();
    if (fpsDuration >= 500) {
        fps_ = (static_cast<float>(fpsFrameCount_) * 1000.0f) / static_cast<float>(fpsDuration);
        fpsFrameCount_ = 0;
        lastFPSUpdate_ = now;

        // Min/Max/Avg の更新 (最初の数フレームを除外)
        if (frameCount_ > 10) {
            totalFpsSum_ += fps_;
            fpsCountTimes_++;
            avgFps_ = static_cast<float>(totalFpsSum_ / fpsCountTimes_);

            if (fps_ < minFps_) minFps_ = fps_;
            if (fps_ > maxFps_) maxFps_ = fps_;
        }
    }

    // 3. CPU使用率の更新
    auto cpuDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCPUUpdate_).count();
    if (cpuDuration >= 500) {
        CalculateCPUUsage();
        lastCPUUpdate_ = now;
    }

    // 4. GPU使用率の算出 (GPU実行時間比率から算出)
    // GPU の全体の処理時間を GPUProfiler から取得 (例: "FrameTotal" または各描画ステージの総和)
    auto* gpuProfiler = Profiler::GetInstance()->GetGPUProfiler();
    if (gpuProfiler) {
        float gpuTotalTime = 0.0f;
        for (const auto& pair : gpuProfiler->GetTimes()) {
            // Present 以外の純粋なレンダリングステージ時間を合計
            if (pair.first != "Present") {
                gpuTotalTime += pair.second;
            }
        }

        // 60FPS時の目標時間（16.67ms）に対する割合でGPU使用率を算出
        if (frameTimeMs_ > 0.0f) {
            gpuUsage_ = (gpuTotalTime / frameTimeMs_) * 10000.0f; // 百分率(%)にするため
            gpuUsage_ = (static_cast<float>(static_cast<int>(gpuUsage_)) / 100.0f); // 小数点以下2桁に丸める
            if (gpuUsage_ > 100.0f) gpuUsage_ = 100.0f;
        }
    }
}

void PerformanceCounter::CalculateCPUUsage()
{
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        uint64_t currentIdle = (static_cast<uint64_t>(idleTime.dwHighDateTime) << 32) | idleTime.dwLowDateTime;
        uint64_t currentKernel = (static_cast<uint64_t>(kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
        uint64_t currentUser = (static_cast<uint64_t>(userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;

        uint64_t diffIdle = currentIdle - lastIdleTime_;
        uint64_t diffKernel = currentKernel - lastKernelTime_;
        uint64_t diffUser = currentUser - lastUserTime_;

        uint64_t totalSys = diffKernel + diffUser;
        if (totalSys > 0) {
            cpuUsage_ = static_cast<float>(totalSys - diffIdle) / static_cast<float>(totalSys) * 100.0f;
            if (cpuUsage_ < 0.0f) cpuUsage_ = 0.0f;
            if (cpuUsage_ > 100.0f) cpuUsage_ = 100.0f;
        }

        lastIdleTime_ = currentIdle;
        lastKernelTime_ = currentKernel;
        lastUserTime_ = currentUser;
    }
}
#endif
