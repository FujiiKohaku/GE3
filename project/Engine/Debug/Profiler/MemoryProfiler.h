#pragma once
#include <cstdint>

#ifdef _DEBUG
class MemoryProfiler {
public:
    void Initialize();
    void Update();

    uint64_t GetCPUMemoryBytes() const { return cpuMemoryBytes_; }
    uint64_t GetGPUMemoryBytes() const { return gpuMemoryBytes_; }
    uint64_t GetUploadHeapBytes() const { return uploadHeapBytes_; }
    uint64_t GetDefaultHeapBytes() const { return defaultHeapBytes_; }
    uint64_t GetTextureMemoryBytes() const { return textureMemoryBytes_; }
    uint64_t GetModelMemoryBytes() const { return modelMemoryBytes_; }
    uint64_t GetAudioMemoryBytes() const { return audioMemoryBytes_; }
    uint32_t GetSrvCount() const { return srvCount_; }
    float GetDescriptorHeapUsageRate() const { return descriptorHeapUsageRate_; }

private:
    uint64_t cpuMemoryBytes_ = 0;
    uint64_t gpuMemoryBytes_ = 0;
    uint64_t uploadHeapBytes_ = 0;
    uint64_t defaultHeapBytes_ = 0;
    uint64_t textureMemoryBytes_ = 0;
    uint64_t modelMemoryBytes_ = 0;
    uint64_t audioMemoryBytes_ = 0;
    uint32_t srvCount_ = 0;
    float descriptorHeapUsageRate_ = 0.0f;
};
#else
class MemoryProfiler {
public:
    void Initialize() {}
    void Update() {}
};
#endif
