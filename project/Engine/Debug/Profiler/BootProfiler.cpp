#include "BootProfiler.h"

#ifdef _DEBUG
#include "Engine/Logger/Logger.h"

void BootProfiler::Initialize()
{
    startTimes_.clear();
    bootTimes_.clear();
    totalStart_ = std::chrono::high_resolution_clock::now();
    totalBootTimeMs_ = 0.0f;
    isBootCompleted_ = false;
}

void BootProfiler::Begin(const std::string& name)
{
    if (isBootCompleted_) return;
    startTimes_[name] = std::chrono::high_resolution_clock::now();
}

void BootProfiler::End(const std::string& name)
{
    if (isBootCompleted_) return;
    auto it = startTimes_.find(name);
    if (it != startTimes_.end()) {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - it->second).count();
        float ms = static_cast<float>(duration) / 1000.0f;
        bootTimes_[name] = ms;
        
        // ログ出力も行う
        Logger::Log("[BootProfiler] " + name + " initialized in " + std::to_string(ms) + " ms.");
    }
}

void BootProfiler::FinalizeBootMeasure()
{
    if (isBootCompleted_) return;
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - totalStart_).count();
    totalBootTimeMs_ = static_cast<float>(duration) / 1000.0f;
    isBootCompleted_ = true;

    Logger::Log("[BootProfiler] Total Boot Time: " + std::to_string(totalBootTimeMs_) + " ms.");
}

float BootProfiler::GetBootTimeMs(const std::string& name) const
{
    auto it = bootTimes_.find(name);
    if (it != bootTimes_.end()) {
        return it->second;
    }
    return 0.0f;
}
#endif
