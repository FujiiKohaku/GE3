#pragma once
#include <string>
#include <unordered_map>
#include <chrono>

#ifdef _DEBUG
class BootProfiler {
public:
    void Initialize();
    void Begin(const std::string& name);
    void End(const std::string& name);
    void FinalizeBootMeasure();

    float GetBootTimeMs(const std::string& name) const;
    const std::unordered_map<std::string, float>& GetBootTimes() const { return bootTimes_; }
    float GetTotalBootTimeMs() const { return totalBootTimeMs_; }

private:
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> startTimes_;
    std::unordered_map<std::string, float> bootTimes_;
    std::chrono::high_resolution_clock::time_point totalStart_;
    float totalBootTimeMs_ = 0.0f;
    bool isBootCompleted_ = false;
};
#else
class BootProfiler {
public:
    void Initialize() {}
    void Begin(const std::string&) {}
    void End(const std::string&) {}
    void FinalizeBootMeasure() {}
};
#endif
