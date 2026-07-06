#pragma once
#include <memory>
#include <string>

class CPUProfiler;
class GPUProfiler;
class MemoryProfiler;
class PerformanceCounter;
class BootProfiler;
class TimelineProfiler;

#ifdef _DEBUG
class Profiler {
public:
    static Profiler* GetInstance();
    static void FinalizeInstance();
    ~Profiler() = default;

    void Initialize();
    void Finalize();

    void BeginFrame();
    void EndFrame();
    void Update();
    void DrawImGui();

    CPUProfiler* GetCPUProfiler() const { return cpuProfiler_.get(); }
    GPUProfiler* GetGPUProfiler() const { return gpuProfiler_.get(); }
    MemoryProfiler* GetMemoryProfiler() const { return memoryProfiler_.get(); }
    PerformanceCounter* GetPerformanceCounter() const { return performanceCounter_.get(); }
    BootProfiler* GetBootProfiler() const { return bootProfiler_.get(); }
    TimelineProfiler* GetTimelineProfiler() const { return timelineProfiler_.get(); }

    void RecordDrawCall() { stats_.drawCalls++; }
    void RecordTriangleCount(uint32_t count) { stats_.triangles += count; }
    void RecordVertexCount(uint32_t count) { stats_.vertices += count; }
    void RecordIndexCount(uint32_t count) { stats_.indices += count; }
    void RecordObjectCount() { stats_.objects++; }
    void RecordMeshCount() { stats_.meshes++; }
    void RecordTextureCount() { stats_.textures++; }
    void RecordMaterialCount() { stats_.materials++; }
    void RecordParticleCount() { stats_.particles++; }
    void RecordLightCount() { stats_.lights++; }
    void RecordAudioCount() { stats_.audios++; }
    void RecordSceneCount() { stats_.scenes++; }

private:
    Profiler() = default;
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

private:
    static std::unique_ptr<Profiler> instance_;

    std::unique_ptr<CPUProfiler> cpuProfiler_;
    std::unique_ptr<GPUProfiler> gpuProfiler_;
    std::unique_ptr<MemoryProfiler> memoryProfiler_;
    std::unique_ptr<PerformanceCounter> performanceCounter_;
    std::unique_ptr<BootProfiler> bootProfiler_;
    std::unique_ptr<TimelineProfiler> timelineProfiler_;

    struct StatisticsData {
        uint32_t drawCalls = 0;
        uint32_t triangles = 0;
        uint32_t vertices = 0;
        uint32_t indices = 0;
        uint32_t objects = 0;
        uint32_t meshes = 0;
        uint32_t textures = 0;
        uint32_t materials = 0;
        uint32_t particles = 0;
        uint32_t lights = 0;
        uint32_t audios = 0;
        uint32_t scenes = 0;

        void Reset() {
            drawCalls = 0;
            triangles = 0;
            vertices = 0;
            indices = 0;
            objects = 0;
            meshes = 0;
            textures = 0;
            materials = 0;
            particles = 0;
            lights = 0;
            audios = 0;
            scenes = 0;
        }
    } stats_;
};
#else
class Profiler {
public:
    static Profiler* GetInstance() {
        static Profiler dummy;
        return &dummy;
    }
    static void FinalizeInstance() {}

    void Initialize() {}
    void Finalize() {}

    void BeginFrame() {}
    void EndFrame() {}
    void Update() {}
    void DrawImGui() {}

    CPUProfiler* GetCPUProfiler() const { return nullptr; }
    GPUProfiler* GetGPUProfiler() const { return nullptr; }
    MemoryProfiler* GetMemoryProfiler() const { return nullptr; }
    PerformanceCounter* GetPerformanceCounter() const { return nullptr; }
    BootProfiler* GetBootProfiler() const { return nullptr; }
    TimelineProfiler* GetTimelineProfiler() const { return nullptr; }

    void RecordDrawCall() {}
    void RecordTriangleCount(uint32_t) {}
    void RecordVertexCount(uint32_t) {}
    void RecordIndexCount(uint32_t) {}
    void RecordObjectCount() {}
    void RecordMeshCount() {}
    void RecordTextureCount() {}
    void RecordMaterialCount() {}
    void RecordParticleCount() {}
    void RecordLightCount() {}
    void RecordAudioCount() {}
    void RecordSceneCount() {}
};
#endif
