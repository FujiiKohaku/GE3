#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Math/MathStruct.h"
#include <string>
#include <cstdint>
#include "externals/DLSS/include/nvsdk_ngx.h"

struct SuperResolutionHistoryInputs {
    bool hasCamera = false;
    uintptr_t cameraId = 0;
    uint64_t cameraHistoryId = 0;
    uint64_t sceneRevision = 0;
    uint64_t radianceRevision = 0;
};

// Borrowed native-resolution inputs, all PIXEL_SHADER_RESOURCE.
// Evaluate restores these states; depth and motion are never modified.
struct SuperResolutionFrameInputs {
    ID3D12Resource* colorTexture = nullptr;
    ID3D12Resource* depthTexture = nullptr;
    ID3D12Resource* motionVectorTexture = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE colorSrv = {};
    float frameTimeDeltaMs = 0;
};

class DlssSuperResolution {
public:
    ~DlssSuperResolution();
    void Initialize();
    void SetEnabled(bool isEnabled) { isEnabled_ = isEnabled; }
    bool IsEnabled() const { return isEnabled_; }
    bool IsAvailable() const { return isAvailable_; }
    bool IsActive() const { return isActive_; }
    const std::string& GetStatus() const { return status_; }
    uint32_t GetRenderWidth() const { return renderWidth_; }
    uint32_t GetRenderHeight() const { return renderHeight_; }
    uint64_t GetEvaluationCount() const { return evaluationCount_; }
    void ResetHistory() { shouldResetHistory_ = true; }
    void BeginFrame(const SuperResolutionHistoryInputs& inputs);
    Vector2 GetProjectionJitterNdc() const;
    void SetSceneViewport();
    D3D12_GPU_DESCRIPTOR_HANDLE Evaluate(const SuperResolutionFrameInputs& inputs);
private:
    void CreateResources();
    bool EnsureFeature();
    void SetFailure(const char* operation, NVSDK_NGX_Result result);
    NVSDK_NGX_Parameter* parameters_ = nullptr;
    NVSDK_NGX_Handle* feature_ = nullptr;
    bool isInitialized_ = false;
    bool isAvailable_ = false;
    bool isEnabled_ = true;
    bool isActive_ = false;
    bool shouldResetHistory_ = true;
    uint32_t renderWidth_ = 0;
    uint32_t renderHeight_ = 0;
    uint32_t jitterFrame_ = 0;
    uint64_t evaluationCount_ = 0;
    uint64_t sceneRevision_ = 0;
    uint64_t cameraHistoryId_ = 0;
    uintptr_t historyCameraId_ = 0;
    Vector2 jitterPixels_ = {};
    std::string status_ = "Not initialized";
    Microsoft::WRL::ComPtr<ID3D12Resource> output_;
    uint32_t outputSrvIndex_ = UINT_MAX;
};
