#pragma once
#include <string>
#include <unordered_map>
#include <chrono>

#ifdef _DEBUG
class CPUProfiler {
public:
    void Initialize();
    void Begin(const std::string& name);
    void End(const std::string& name);
    void Reset();

    float GetTimeMs(const std::string& name) const;
    const std::unordered_map<std::string, float>& GetTimes() const { return times_; }

private:
    struct Section {
        std::chrono::high_resolution_clock::time_point startTime;
        bool isRunning = false;
    };
    std::unordered_map<std::string, Section> activeSections_;
    std::unordered_map<std::string, float> times_;
};
#else
class CPUProfiler {
public:
    void Initialize() {}
    void Begin(const std::string&) {}
    void End(const std::string&) {}
    void Reset() {}
};
#endif
