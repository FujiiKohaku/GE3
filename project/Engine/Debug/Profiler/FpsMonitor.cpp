#include "FpsMonitor.h"
#include "Engine/Logger/Logger.h"
#include "externals/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>

namespace {
double Milliseconds(FpsMonitor::Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}
template <size_t N> void CopyName(std::array<char, N>& output, std::string_view name) {
    const size_t count = (std::min)(N - 1, name.size());
    std::memcpy(output.data(), name.data(), count);
    output[count] = '\0';
}
bool Valid(double value, double minimum, double maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
}

thread_local FpsMonitor* FpsMonitor::activeMonitor_ = nullptr;
FpsMonitor::FpsMonitor(ClockSource clock) : clock_(clock ? clock : &Clock::now) { Initialize(Config {}); }
FpsMonitor& FpsMonitor::GetInstance() { static FpsMonitor monitor; return monitor; }

void FpsMonitor::Initialize(const Config& input)
{
    config_ = input;
    if (activeMonitor_ == this) { activeMonitor_ = nullptr; }
    const Config defaults;
    if (!Valid(config_.thresholdFps, 1, 1000)) { config_.thresholdFps = defaults.thresholdFps; }
    if (!Valid(config_.sampleSeconds, 0.1, 10)) { config_.sampleSeconds = defaults.sampleSeconds; }
    if (!Valid(config_.lowFpsSeconds, 0, 60)) { config_.lowFpsSeconds = defaults.lowFpsSeconds; }
    if (!Valid(config_.cooldownSeconds, 0.1, 600)) { config_.cooldownSeconds = defaults.cooldownSeconds; }
    if (!Valid(config_.warmupSeconds, 0, 120)) { config_.warmupSeconds = defaults.warmupSeconds; }
    if (!Valid(config_.spikeMilliseconds, 0, 60000)) { config_.spikeMilliseconds = defaults.spikeMilliseconds; }
    if (!Valid(config_.recoveryMarginFps, 0, 100)) { config_.recoveryMarginFps = defaults.recoveryMarginFps; }
    config_.topSections = std::clamp(config_.topSections, 1u, 12u);
    initialized_ = clock_();
    sectionCount_ = depth_ = gpuDetailCount_ = counterCount_ = 0;
    frameActive_ = havePreviousEnd_ = haveBegunFrame_ = lowActive_ = haveIncident_ = capacityExceeded_ = false;
    elapsedSeconds_ = lastIncidentSeconds_ = lowSeconds_ = lastFps_ = 0;
    frameIndex_ = incidentCount_ = 0;
    windowContext_.clear();
    sections_ = {}; gpuDetails_ = {}; counters_ = {};
    ResetWindow();
}

bool FpsMonitor::LoadConfig(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream) { return false; }
    try {
        nlohmann::json json;
        stream >> json;
        if (!json.is_object()) { throw std::runtime_error("expected JSON object"); }
        Config config;
        config.enabled = json.value("enabled", config.enabled);
        config.thresholdFps = json.value("thresholdFps", config.thresholdFps);
        config.sampleSeconds = json.value("sampleSeconds", config.sampleSeconds);
        config.lowFpsSeconds = json.value("lowFpsSeconds", config.lowFpsSeconds);
        config.cooldownSeconds = json.value("cooldownSeconds", config.cooldownSeconds);
        config.warmupSeconds = json.value("warmupSeconds", config.warmupSeconds);
        config.spikeMilliseconds = json.value("spikeMilliseconds", config.spikeMilliseconds);
        config.recoveryMarginFps = json.value("recoveryMarginFps", config.recoveryMarginFps);
        const int top = json.value("topSections", static_cast<int>(config.topSections));
        config.topSections = static_cast<uint32_t>(std::clamp(top, 1, 12));
        Initialize(config);
        return true;
    } catch (const std::exception& error) {
        Emit(std::string("[FpsMonitor] Invalid config; using defaults: ") + error.what());
        Initialize(Config {});
        return false;
    }
}

void FpsMonitor::SetContext(std::string_view name)
{
    if (context_ != name) { context_.assign(name); }
}

size_t FpsMonitor::SectionIndex(std::string_view name)
{
    name = name.substr(0, 63);
    for (size_t index = 0; index < sectionCount_; ++index) {
        if (name == sections_[index].name.data()) { return index; }
    }
    if (sectionCount_ == sections_.size()) { capacityExceeded_ = true; return sections_.size(); }
    const size_t index = sectionCount_++;
    CopyName(sections_[index].name, name);
    return index;
}

const FpsMonitor::Section* FpsMonitor::FindSection(std::string_view name) const
{
    for (size_t index = 0; index < sectionCount_; ++index) {
        if (name == sections_[index].name.data()) { return &sections_[index]; }
    }
    return nullptr;
}

FpsMonitor::Scope::Scope(FpsMonitor& owner, std::string_view name)
{
    if (FpsMonitor::activeMonitor_ != &owner || !owner.frameActive_ || !owner.config_.enabled) { return; }
    section_ = owner.SectionIndex(name);
    if (section_ >= owner.sections_.size() || owner.depth_ >= owner.stack_.size()) {
        owner.capacityExceeded_ = true;
        return;
    }
    owner_ = &owner;
    start_ = owner.clock_();
    owner.stack_[owner.depth_++] = this;
}

FpsMonitor::Scope::~Scope()
{
    if (!owner_) { return; }
    const double inclusive = (std::max)(0.0, Milliseconds(owner_->clock_() - start_));
    auto& section = owner_->sections_[section_];
    section.frameInclusive += inclusive;
    section.frameExclusive += (std::max)(0.0, inclusive - childrenMs_);
    if (owner_->depth_ > 0) { --owner_->depth_; }
    if (owner_->depth_ > 0) { owner_->stack_[owner_->depth_ - 1]->childrenMs_ += inclusive; }
}

void FpsMonitor::BeginFrame()
{
    if (!config_.enabled) { return; }
    const auto now = clock_();
    if (!haveBegunFrame_) { initialized_ = now; haveBegunFrame_ = true; }
    frameStart_ = now;
    frameActive_ = true; ignored_ = false; depth_ = 0;
    activeMonitor_ = this;
    frameGpuValid_ = false;
    for (auto& section : sections_) { section.frameInclusive = section.frameExclusive = 0; }
    for (auto& detail : gpuDetails_) { detail.valid = false; }
    for (auto& counter : counters_) { counter.valid = false; }
    if (havePreviousEnd_) {
        const size_t index = SectionIndex("Frame.Gap");
        if (index < sections_.size()) {
            sections_[index].frameInclusive = sections_[index].frameExclusive =
                (std::max)(0.0, Milliseconds(now - previousEnd_));
        }
    }
}

void FpsMonitor::SkipFrame() { ignored_ = true; }
void FpsMonitor::SetGpuFrame(double milliseconds, bool valid) {
    frameGpuValid_ = valid && Valid(milliseconds, 0, 60000);
    frameGpuMs_ = frameGpuValid_ ? milliseconds : 0;
}

void FpsMonitor::SetGpuDetail(std::string_view name, double milliseconds, bool valid)
{
    if (!config_.enabled || !valid || !Valid(milliseconds, 0, 60000)) { return; }
    name = name.substr(0, 63);
    for (size_t index = 0; index <= gpuDetailCount_ && index < gpuDetails_.size(); ++index) {
        auto& detail = gpuDetails_[index];
        if (index == gpuDetailCount_) { CopyName(detail.name, name); ++gpuDetailCount_; }
        if (name == detail.name.data()) { detail.frame = milliseconds; detail.valid = true; return; }
    }
}

void FpsMonitor::SetCounter(std::string_view name, uint64_t value)
{
    if (!config_.enabled) { return; }
    name = name.substr(0, 63);
    for (size_t index = 0; index <= counterCount_ && index < counters_.size(); ++index) {
        auto& counter = counters_[index];
        if (index == counterCount_) { CopyName(counter.name, name); ++counterCount_; }
        if (name == counter.name.data()) { counter.frame = static_cast<double>(value); counter.valid = true; return; }
    }
}

void FpsMonitor::ResetWindow()
{
    windowMs_ = worstFrameMs_ = sumGpuMs_ = worstGpuMs_ = 0;
    windowFrames_ = gpuSamples_ = worstFrameIndex_ = 0;
    for (auto& section : sections_) {
        section.sumInclusive = section.sumExclusive = section.maxExclusive = section.worstExclusive = 0;
    }
    for (auto& detail : gpuDetails_) { detail.sum = detail.peak = 0; detail.samples = 0; }
    for (auto& counter : counters_) { counter.sum = counter.peak = 0; counter.samples = 0; }
}

void FpsMonitor::EndFrame()
{
    if (!config_.enabled || !frameActive_) { return; }
    const auto now = clock_();
    frameActive_ = false;
    if (activeMonitor_ == this) { activeMonitor_ = nullptr; }
    const double frameMs = Milliseconds(now - (havePreviousEnd_ ? previousEnd_ : frameStart_));
    previousEnd_ = now; havePreviousEnd_ = true; ++frameIndex_;
    elapsedSeconds_ = std::chrono::duration<double>(now - initialized_).count();
    const bool warmup = std::chrono::duration<double>(frameStart_ - initialized_).count() < config_.warmupSeconds;
    if (ignored_ || warmup || frameMs <= 0 || !std::isfinite(frameMs)) {
        ResetWindow(); lowSeconds_ = 0; return;
    }
    if (!windowContext_.empty() && windowContext_ != context_) {
        ResetWindow(); lowSeconds_ = 0; lowActive_ = false;
    }
    windowContext_ = context_;
    ++windowFrames_; windowMs_ += frameMs;
    for (auto& section : sections_) {
        section.sumInclusive += section.frameInclusive;
        section.sumExclusive += section.frameExclusive;
        section.maxExclusive = (std::max)(section.maxExclusive, section.frameExclusive);
    }
    if (frameGpuValid_) { sumGpuMs_ += frameGpuMs_; ++gpuSamples_; }
    for (auto& detail : gpuDetails_) {
        if (detail.valid) { detail.sum += detail.frame; detail.peak = (std::max)(detail.peak, detail.frame); ++detail.samples; }
    }
    for (auto& counter : counters_) {
        if (counter.valid) { counter.peak = (std::max)(counter.peak, counter.frame); ++counter.samples; }
    }
    if (frameMs > worstFrameMs_) {
        worstFrameMs_ = frameMs; worstFrameIndex_ = frameIndex_;
        worstGpuMs_ = frameGpuValid_ ? frameGpuMs_ : -1;
        for (auto& section : sections_) { section.worstExclusive = section.frameExclusive; }
    }
    const bool canLog = !haveIncident_ || elapsedSeconds_ - lastIncidentSeconds_ + 0.000000001 >= config_.cooldownSeconds;
    const double fps = 1000.0 * windowFrames_ / windowMs_;
    if (config_.spikeMilliseconds > 0 && frameMs >= config_.spikeMilliseconds && canLog) {
        EmitIncident("FrameSpike", 1000.0 / frameMs, frameMs, true);
    }
    if (windowMs_ + 0.000001 < config_.sampleSeconds * 1000) { return; }
    lastFps_ = fps;
    if (fps < config_.thresholdFps) {
        lowSeconds_ += windowMs_ / 1000;
        if (lowSeconds_ + 0.000001 >= config_.lowFpsSeconds) {
            lowActive_ = true;
            if (!haveIncident_ || elapsedSeconds_ - lastIncidentSeconds_ + 0.000000001 >= config_.cooldownSeconds) {
                EmitIncident("LowFPS", fps, windowMs_ / windowFrames_, false);
            }
        }
    } else if (lowActive_ && fps >= config_.thresholdFps + config_.recoveryMarginFps) {
        Emit(std::format("[FpsMonitor][Recovered] context={} fps={:.2f}", context_, fps));
        lowActive_ = false; lowSeconds_ = 0;
    } else if (!lowActive_) { lowSeconds_ = 0; }
    ResetWindow();
}

void FpsMonitor::EmitIncident(std::string_view kind, double fps, double frameMs, bool singleFrame)
{
    ++incidentCount_; haveIncident_ = true; lastIncidentSeconds_ = elapsedSeconds_;
    std::array<size_t, kMaxSections> order {};
    for (size_t index = 0; index < sectionCount_; ++index) { order[index] = index; }
    const auto value = [&](const Section& section) {
        return singleFrame ? section.frameExclusive : section.sumExclusive / windowFrames_;
    };
    std::sort(order.begin(), order.begin() + sectionCount_, [&](size_t a, size_t b) { return value(sections_[a]) > value(sections_[b]); });
    double busy = 0, waiting = 0, gap = 0;
    const Section* topBusy = nullptr;
    for (size_t index = 0; index < sectionCount_; ++index) {
        const auto& section = sections_[order[index]];
        const std::string_view name(section.name.data());
        if (name.starts_with("Wait.")) { waiting += value(section); }
        else if (name == "Frame.Gap") { gap += value(section); }
        else { busy += value(section); if (!topBusy) { topBusy = &section; } }
    }
    const double gpu = singleFrame ? (frameGpuValid_ ? frameGpuMs_ : -1) : (gpuSamples_ ? sumGpuMs_ / gpuSamples_ : -1);
    const double budget = 1000 / config_.thresholdFps;
    std::string candidate = "insufficient timing coverage";
    if (gap > frameMs * 0.35) { candidate = "frame gap/message pump/monitor logging (unattributed)"; }
    else if (gpu >= budget && gpu >= busy * 0.8) { candidate = "GPU workload"; }
    else if (busy >= budget && topBusy) { candidate = std::string("CPU-side ") + topBusy->name.data(); }
    else if (waiting > frameMs * 0.35) { candidate = "Present/VSync or GPU fence wait; not proof of GPU overload"; }
    else if (topBusy && value(*topBusy) > 0.1) { candidate = std::string("CPU-side ") + topBusy->name.data(); }
    Emit(std::format("[FpsMonitor][{}] context={} frame={} fps={:.2f} threshold={:.2f} frameMs={:.3f} "
        "windowFrames={} windowMs={:.3f} worstFrame={} worstMs={:.3f} candidate={} (inference)",
        kind, context_, frameIndex_, fps, config_.thresholdFps, frameMs, windowFrames_, windowMs_, worstFrameIndex_, worstFrameMs_, candidate));
    Emit(std::format("[FpsMonitor] CPU-side wall busy={:.3f}ms wait={:.3f}ms gap={:.3f}ms; "
        "GPU frame={}ms samples={} worstFrameGPU={}ms; CPU/GPU overlap, do not add them",
        busy, waiting, gap, gpu < 0 ? "unavailable" : std::format("{:.3f}", gpu),
        singleFrame ? (frameGpuValid_ ? 1 : 0) : gpuSamples_, worstGpuMs_ < 0 ? "unavailable" : std::format("{:.3f}", worstGpuMs_)));
    for (size_t rank = 0; rank < (std::min)(sectionCount_, static_cast<size_t>(config_.topSections)); ++rank) {
        const auto& section = sections_[order[rank]];
        const double exclusive = value(section);
        if (exclusive < 0.001) { continue; }
        Emit(std::format("[FpsMonitor] CPU {} exclusive={:.3f}ms inclusive={:.3f}ms peak={:.3f}ms worstFrame={:.3f}ms",
            section.name.data(), exclusive, singleFrame ? section.frameInclusive : section.sumInclusive / windowFrames_,
            section.maxExclusive, section.worstExclusive));
    }
    for (size_t index = 0; index < gpuDetailCount_; ++index) {
        const auto& detail = gpuDetails_[index];
        if (singleFrame ? detail.valid : detail.samples > 0) {
            Emit(std::format("[FpsMonitor] GPU detail {}={:.3f}ms (overlaps GPU frame)",
                detail.name.data(), singleFrame ? detail.frame : detail.sum / detail.samples));
        }
    }
    for (size_t index = 0; index < counterCount_; ++index) {
        const auto& counter = counters_[index];
        if (counter.samples) { Emit(std::format("[FpsMonitor] Count {} peak={:.0f}", counter.name.data(), counter.peak)); }
    }
    if (capacityExceeded_) { Emit("[FpsMonitor] timing capacity exceeded; some sections were not captured"); }
    if (!sink_) { Logger::Flush(); }
}

void FpsMonitor::Emit(std::string_view text)
{
    if (sink_) { sink_(text, sinkContext_); }
    else { Logger::Log(std::string(text)); }
}
