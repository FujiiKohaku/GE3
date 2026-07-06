#include "CPUProfiler.h"

#ifdef _DEBUG
void CPUProfiler::Initialize()
{
    activeSections_.clear();
    times_.clear();
}

void CPUProfiler::Begin(const std::string& name)
{
    activeSections_[name].startTime = std::chrono::high_resolution_clock::now();
    activeSections_[name].isRunning = true;
}

void CPUProfiler::End(const std::string& name)
{
    auto it = activeSections_.find(name);
    if (it != activeSections_.end() && it->second.isRunning) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - it->second.startTime).count();
        times_[name] = static_cast<float>(duration) / 1000.0f; // msに変換
        it->second.isRunning = false;
    }
}

void CPUProfiler::Reset()
{
    // 新しいフレームの計測に備えて times_ は維持するが、実行中でないセクション情報などをリセット
    for (auto& pair : activeSections_) {
        pair.second.isRunning = false;
    }
}

float CPUProfiler::GetTimeMs(const std::string& name) const
{
    auto it = times_.find(name);
    if (it != times_.end()) {
        return it->second;
    }
    return 0.0f;
}
#endif
