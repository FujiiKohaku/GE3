#pragma once
#include <string>
#include <vector>
#include <chrono>

#ifdef _DEBUG
class TimelineProfiler {
public:
    struct Event {
        std::string name;
        float startTimeMs = 0.0f;
        float endTimeMs = 0.0f;
    };

    struct FrameData {
        std::vector<Event> events;
        float totalTimeMs = 0.0f;
    };

public:
    void Initialize();
    void BeginFrame();
    void EndFrame();

    void BeginEvent(const std::string& name);
    void EndEvent(const std::string& name);

    void DrawTimelineUI();

private:
    static constexpr size_t kMaxFrames = 120;
    std::vector<FrameData> history_;
    size_t selectedFrameIndex_ = 0;

    std::chrono::high_resolution_clock::time_point frameStart_;
    
    struct PendingEvent {
        std::string name;
        std::chrono::high_resolution_clock::time_point start;
    };
    std::vector<PendingEvent> pendingEvents_;
    FrameData currentFrame_;
};
#else
class TimelineProfiler {
public:
    void Initialize() {}
    void BeginFrame() {}
    void EndFrame() {}
    void BeginEvent(const std::string&) {}
    void EndEvent(const std::string&) {}
};
#endif
