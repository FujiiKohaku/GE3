#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

// Releaseでも有効。ゲーム時間の倍率・上限を使わず、実際のフレーム時間を監視する。
class FpsMonitor {
public:
    using Clock = std::chrono::steady_clock;
    using ClockSource = Clock::time_point (*)();
    using LogSink = void (*)(std::string_view, void*);
    struct Config {
        bool enabled = true;
        double thresholdFps = 50.0;
        double sampleSeconds = 0.5;
        double lowFpsSeconds = 1.0;
        double cooldownSeconds = 5.0;
        double warmupSeconds = 3.0;
        double spikeMilliseconds = 100.0;
        double recoveryMarginFps = 5.0;
        uint32_t topSections = 6;
    };
    struct Section {
        std::array<char, 64> name {};
        double frameInclusive = 0, frameExclusive = 0;
        double sumInclusive = 0, sumExclusive = 0, maxExclusive = 0;
        double worstExclusive = 0;
    };
    class Scope {
    public:
        Scope(FpsMonitor& owner, std::string_view name);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        friend class FpsMonitor;
        FpsMonitor* owner_ = nullptr;
        Clock::time_point start_ {};
        double childrenMs_ = 0;
        size_t section_ = 0;
    };

    explicit FpsMonitor(ClockSource clock = &Clock::now);
    static FpsMonitor& GetInstance();
    void Initialize(const Config& config);
    bool LoadConfig(const std::string& path);
    const Config& GetConfig() const { return config_; }
    bool IsEnabled() const { return config_.enabled; }
    void SetLogSink(LogSink sink, void* context = nullptr) { sink_ = sink; sinkContext_ = context; }
    void SetContext(std::string_view name);
    void BeginFrame();
    void EndFrame();
    void SkipFrame(); // minimized windows/modal UI do not generate false incidents
    void SetGpuFrame(double milliseconds, bool valid);
    void SetGpuDetail(std::string_view name, double milliseconds, bool valid);
    void SetCounter(std::string_view name, uint64_t value);
    Scope Measure(std::string_view name) { return Scope(*this, name); }
    double GetFps() const { return lastFps_; }
    bool IsLowFps() const { return lowActive_; }
    uint64_t GetIncidentCount() const { return incidentCount_; }
    const Section* FindSection(std::string_view name) const;

private:
    static constexpr size_t kMaxSections = 64, kMaxDepth = 32, kMaxDetails = 12;
    struct Detail {
        std::array<char, 64> name {};
        double frame = 0, sum = 0, peak = 0;
        uint64_t samples = 0;
        bool valid = false;
    };
    size_t SectionIndex(std::string_view name);
    void ResetWindow();
    void EmitIncident(std::string_view kind, double fps, double frameMs, bool singleFrame);
    void Emit(std::string_view text);
    ClockSource clock_;
    static thread_local FpsMonitor* activeMonitor_;
    Config config_;
    LogSink sink_ = nullptr;
    void* sinkContext_ = nullptr;
    std::string context_ = "Engine", windowContext_;
    std::array<Section, kMaxSections> sections_ {};
    size_t sectionCount_ = 0;
    std::array<Scope*, kMaxDepth> stack_ {};
    size_t depth_ = 0;
    std::array<Detail, kMaxDetails> gpuDetails_ {}, counters_ {};
    size_t gpuDetailCount_ = 0, counterCount_ = 0;
    Clock::time_point initialized_ {}, frameStart_ {}, previousEnd_ {};
    bool frameActive_ = false, havePreviousEnd_ = false, haveBegunFrame_ = false, ignored_ = false;
    bool lowActive_ = false, haveIncident_ = false, capacityExceeded_ = false;
    double elapsedSeconds_ = 0, lastIncidentSeconds_ = 0, lowSeconds_ = 0;
    double windowMs_ = 0, worstFrameMs_ = 0, lastFps_ = 0;
    double frameGpuMs_ = 0, sumGpuMs_ = 0, worstGpuMs_ = 0;
    bool frameGpuValid_ = false;
    uint64_t windowFrames_ = 0, gpuSamples_ = 0, frameIndex_ = 0, worstFrameIndex_ = 0, incidentCount_ = 0;
};

// String literals recommended: labels are stored in a bounded table, not allocated per frame.
class FpsScope : public FpsMonitor::Scope {
public:
    explicit FpsScope(std::string_view name) : Scope(FpsMonitor::GetInstance(), name) {}
};
