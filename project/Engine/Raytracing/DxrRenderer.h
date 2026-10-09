#pragma once
#include "DxrLocalShadowParameters.h"
#include "Engine/Debug/GpuTimestampTimer.h"
#include "Engine/Math/Object3DStruct.h"
#include <map>
#include <functional>
#include <vector>
#if defined(ENABLE_DEVELOPMENT_TOOLS)
#include "externals/json.hpp"
#endif

class Camera;
class Model;
class CopyImageRenderer;

enum class DxrDebugMode : uint32_t { Normals, BaseColor, InstanceId };

struct DxrSettings {
    bool isEnabled = false;
    bool isDebugVisible = false;
    DxrDebugMode debugMode = DxrDebugMode::Normals;
};

struct DxrStatistics {
    uint32_t instanceCount = 0;
    uint32_t blasCount = 0;
    uint32_t builtBlasCount = 0;
    uint32_t updatedBlasCount = 0;
    uint32_t dynamicBlasCount = 0;
    uint64_t dynamicBufferBytes = 0;
    uint64_t dynamicAllocationBytes = 0;
    uint32_t hitRecordCount = 0;
    uint64_t bufferBytes = 0;
    uint64_t outputAllocationBytes = 0;
    bool hasValidFrame = false;
};

// BeginFrame requires the previous frame's fence to have completed, as in Renderer.
// Opaque triangle geometry; deformed input must contain all primitives in model order.
class DxrRenderer {
public:
    DxrRenderer();
    ~DxrRenderer();
    void Initialize();
    void BeginFrame();
    void Queue(const void* objectId, const Model& model, const Matrix4x4& world,
        const Material& material, bool shouldCastShadow = true, bool shouldReceiveShadow = true);
    // The caller advances geometryRevision after changing vertices and keeps vertexState accurate.
    // EndFrame temporarily adds shader-read access and restores the caller's state.
    void QueueDeformed(const void* objectId, const Model& model, const Matrix4x4& world,
        const Material& material, ID3D12Resource* vertices, uint64_t geometryRevision,
        bool shouldCastShadow = true,
        D3D12_RESOURCE_STATES vertexState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, bool shouldReceiveShadow = true);
    void EndFrame(const Camera* camera, bool shouldTraceDebug = true);
    bool HasValidScene() const { return isValidScene_; }
    uint64_t GetShadowSceneRevision() const { return shadowSceneRevision_; }
    uint64_t GetReflectionSceneRevision() const { return reflectionSceneRevision_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetSceneGpuAddress() const;
    ID3D12RootSignature* GetMaterialRootSignature() const { return localRoot_.Get(); }
    void WriteHitRecords(void* records, uint64_t strideBytes, const void* shaderIdentifier) const;
    void BeginMaterialRead(bool shouldReadOpaqueVertices = false) const { TransitionDynamicVertices(false, !shouldReadOpaqueVertices); }
    void EndMaterialRead(bool shouldReadOpaqueVertices = false) const { TransitionDynamicVertices(true, !shouldReadOpaqueVertices); }
    void SetReflectionCapture(bool isEnabled) { shouldCaptureReflections_ = isEnabled; }
    bool ShouldCaptureReflections() const { return active_ == this && shouldCaptureReflections_; }
    void SetLocalShadowParameters(const DxrLocalShadowParameters& parameters, D3D12_GPU_VIRTUAL_ADDRESS address) {
        localShadowParameters_ = parameters; localShadowConstantsAddress_ = address;
    }
    const DxrLocalShadowParameters& GetLocalShadowParameters() const { return localShadowParameters_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetLocalShadowConstantsAddress() const { return localShadowConstantsAddress_; }
    void SetLocalShadowCapture(bool isEnabled) { shouldCaptureLocalShadows_ = isEnabled; }
    bool ShouldCaptureLocalShadows() const { return active_ == this && shouldCaptureLocalShadows_; }
    void SetDirectionalShadowCapture(bool isEnabled) { shouldCaptureDirectionalShadows_ = isEnabled; }
    bool ShouldCaptureDirectionalShadows() const { return active_ == this && shouldCaptureDirectionalShadows_; }
    void DrawDebug();
    void DrawImGui();
    void ReadCompleted();
    void SetSettings(const DxrSettings& settings) { settings_ = settings; }
    const DxrSettings& GetSettings() const { return settings_; }
    const DxrStatistics& GetStatistics() const { return statistics_; }
    bool IsSupported() const { return isSupported_; }
    bool IsReady() const { return isReady_; }
    const std::string& GetStatus() const { return status_; }
    double GetBuildGpuTimeMs() const { return buildTimer_.GetDurationMs(); }
    double GetTraceGpuTimeMs() const { return traceTimer_.GetDurationMs(); }
    ID3D12Resource* GetOutputTexture() const { return output_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetOutputSrv() const;
    static DxrRenderer* GetActive() { return active_; }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    nlohmann::json GetDevelopmentState() const;
    nlohmann::json GetDevelopmentControls() const;
    bool SetDevelopmentBool(const std::string& key, bool isEnabled);
    bool ExecuteDevelopmentCommand(const std::string& key);
#endif
private:
    DxrLocalShadowParameters localShadowParameters_;
    D3D12_GPU_VIRTUAL_ADDRESS localShadowConstantsAddress_ = 0;
    bool shouldCaptureLocalShadows_ = false;
    struct BlasKey {
        const Model* model = nullptr;
        const void* objectId = nullptr;
        bool isAlphaMasked = false;
        bool operator<(const BlasKey& other) const {
            if (model != other.model) { return std::less<const Model*>()(model, other.model); }
            if (objectId != other.objectId) { return std::less<const void*>()(objectId, other.objectId); }
            return isAlphaMasked < other.isAlphaMasked;
        }
    };
    struct Geometry {
        Microsoft::WRL::ComPtr<ID3D12Resource> vertices;
        Microsoft::WRL::ComPtr<ID3D12Resource> indices;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t materialIndex = 0;
        uint64_t vertexOffsetBytes = 0;
    };
    struct BlasEntry {
        std::vector<Geometry> geometries;
        Microsoft::WRL::ComPtr<ID3D12Resource> result;
        Microsoft::WRL::ComPtr<ID3D12Resource> scratch;
        uint64_t lastFrameId = 0;
        uint64_t geometryRevision = 0;
        uint64_t allocationBytes = 0;
        bool isDynamic = false;
        bool isAlphaMasked = false;
        bool shouldUpdate = false;
        D3D12_RESOURCE_STATES vertexState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    };
    struct Instance {
        BlasKey blasKey;
        Matrix4x4 world = {};
        Material material = {};
        std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textures;
        bool shouldCastShadow = true;
        bool shouldReceiveShadow = true;
    };
    bool CreatePipeline();
    void QueueInternal(const void* objectId, const Model& model, const Matrix4x4& world,
        const Material& material, bool shouldCastShadow, ID3D12Resource* deformedVertices,
        uint64_t geometryRevision, D3D12_RESOURCE_STATES vertexState, bool shouldReceiveShadow);
    void TransitionDynamicVertices(bool shouldRestore, bool shouldLimitToMasked = false) const;
    void CreateMaterialBuffer();
    bool CreateOutput();
    bool BuildScene();
    bool BuildBlas(BlasEntry& entry);
    bool CreateShaderTables();
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(uint64_t sizeBytes,
        D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state, bool shouldAllowUav);
    void SetFailure(const std::string& message);
    inline static DxrRenderer* active_ = nullptr;
    DxrSettings settings_;
    DxrStatistics statistics_;
    bool isSupported_ = false;
    bool isReady_ = false;
    bool isValidScene_ = false;
    bool shouldCaptureDirectionalShadows_ = false;
    bool shouldCaptureReflections_ = false;
    uint64_t reflectionSceneRevision_ = 0;
    uint64_t previousReflectionSceneHash_ = 0;
    uint64_t frameId_ = 0;
    uint64_t shadowSceneRevision_ = 0;
    uint64_t previousShadowSceneHash_ = 0;
    uint32_t outputUavIndex_ = UINT_MAX;
    uint32_t outputSrvIndex_ = UINT_MAX;
    std::string status_ = "Not initialized";
    Microsoft::WRL::ComPtr<ID3D12Device5> device_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> globalRoot_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> localRoot_;
    Microsoft::WRL::ComPtr<ID3D12StateObject> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> pipelineProperties_;
    Microsoft::WRL::ComPtr<ID3D12Resource> output_;
    Microsoft::WRL::ComPtr<ID3D12Resource> tlas_;
    Microsoft::WRL::ComPtr<ID3D12Resource> tlasScratch_;
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> shaderTable_;
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraBuffer_;
    std::unique_ptr<CopyImageRenderer> debugCopy_;
    std::map<BlasKey, BlasEntry> blasEntries_;
    std::vector<Instance> instances_;
    std::vector<const void*> objectIds_;
    // Resources replaced after commands are recorded must survive until the fence.
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> retiredResources_;
    D3D12_DISPATCH_RAYS_DESC dispatch_ = {};
    GpuTimestampTimer buildTimer_;
    GpuTimestampTimer traceTimer_;
};
