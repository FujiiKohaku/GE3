#include "DxrRenderer.h"
#include "Engine/3D/Model.h"
#include "Engine/Camera/Camera.h"
#include "Engine/PostEffect/CopyImageRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/WinApp/WinApp.h"
#include "Engine/Development/DevelopmentWebPanel.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
constexpr uint64_t kShaderRecordStrideBytes = 96;
constexpr uint64_t kTableAlignmentBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
constexpr uint64_t kMaterialStrideBytes = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
constexpr uint32_t kMaxInstanceCount = 0x00ffffff;
struct CameraParameters {
    Matrix4x4 inverseViewProjection;
    Vector3 cameraPosition;
    uint32_t debugMode;
    float nearClip;
    float farClip;
};
uint64_t AlignSize(uint64_t sizeBytes, uint64_t alignmentBytes) {
    return (sizeBytes + alignmentBytes - 1) / alignmentBytes * alignmentBytes;
}
void RequireResult(HRESULT result, const char* message) {
    if (FAILED(result)) { throw std::runtime_error(message); }
}
bool IsValidWorld(const Matrix4x4& world) {
    for (uint32_t row = 0; row < 4; ++row) {
        for (uint32_t column = 0; column < 4; ++column) {
            if (!std::isfinite(world.m[row][column])) { return false; }
        }
    }
    float determinant = world.m[0][0] * (world.m[1][1] * world.m[2][2] - world.m[1][2] * world.m[2][1])
        - world.m[0][1] * (world.m[1][0] * world.m[2][2] - world.m[1][2] * world.m[2][0])
        + world.m[0][2] * (world.m[1][0] * world.m[2][1] - world.m[1][1] * world.m[2][0]);
    return std::isfinite(determinant) && std::abs(determinant) > 0.00000001f
        && world.m[0][3] == 0 && world.m[1][3] == 0 && world.m[2][3] == 0 && world.m[3][3] == 1;
}
uint64_t HashReflectionBytes(uint64_t hash, const void* data, size_t sizeBytes) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t index = 0; index < sizeBytes; ++index) { hash = (hash ^ bytes[index]) * 1099511628211ull; }
    return hash;
}
}

DxrRenderer::DxrRenderer() = default;
DxrRenderer::~DxrRenderer() {
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().UnregisterOwner(this);
#endif
    if (active_ == this) { active_ = nullptr; }
    if (outputUavIndex_ != UINT_MAX) { SrvManager::GetInstance()->Free(outputUavIndex_); }
    if (outputSrvIndex_ != UINT_MAX) { SrvManager::GetInstance()->Free(outputSrvIndex_); }
}
void DxrRenderer::Initialize() {
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().RegisterSource<DxrRenderer>(this, "dxr", "DXR基盤", false,
        &DxrRenderer::GetDevelopmentState, &DxrRenderer::GetDevelopmentControls,
        &DxrRenderer::SetDevelopmentBool, nullptr, &DxrRenderer::ExecuteDevelopmentCommand);
#endif
    auto* dxCommon = DirectXCommon::GetInstance();
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options = {};
    isSupported_ = SUCCEEDED(dxCommon->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options)))
        && options.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
    if (isSupported_) {
        isSupported_ = SUCCEEDED(dxCommon->GetDevice()->QueryInterface(IID_PPV_ARGS(&device_)))
            && SUCCEEDED(dxCommon->GetCommandList()->QueryInterface(IID_PPV_ARGS(&commandList_)));
    }
    if (!isSupported_) { status_ = "DXR unsupported; raster rendering remains available"; return; }
    status_ = "DXR supported (disabled)";
    buildTimer_.Initialize();
    traceTimer_.Initialize();
}
void DxrRenderer::SetFailure(const std::string& message) {
    status_ = message;
    statistics_.hasValidFrame = false;
    settings_.isEnabled = false;
    isValidScene_ = false;
    if (active_ == this) { active_ = nullptr; }
    Logger::Error("DXR: " + message);
}
Microsoft::WRL::ComPtr<ID3D12Resource> DxrRenderer::CreateBuffer(uint64_t sizeBytes,
    D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state, bool shouldAllowUav) {
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = heapType;
    auto description = CD3DX12_RESOURCE_DESC::Buffer(AlignSize(sizeBytes, 256));
    if (shouldAllowUav) { description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; }
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_STATES initialState = state;
    if (state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) { initialState = D3D12_RESOURCE_STATE_COMMON; }
    RequireResult(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        initialState, nullptr, IID_PPV_ARGS(&resource)), "Buffer allocation failed");
    if (initialState != state) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), initialState, state);
        commandList_->ResourceBarrier(1, &barrier);
    }
    return resource;
}
bool DxrRenderer::CreatePipeline() {
    D3D12_DESCRIPTOR_RANGE outputRange = {};
    outputRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    outputRange.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER globalParameters[3] = {};
    globalParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    globalParameters[0].Descriptor.ShaderRegister = 0;
    globalParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    globalParameters[1].DescriptorTable = {1, &outputRange};
    globalParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    globalParameters[2].Descriptor.ShaderRegister = 0;
    D3D12_ROOT_SIGNATURE_DESC globalDescription = {};
    globalDescription.NumParameters = 3;
    globalDescription.pParameters = globalParameters;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    RequireResult(D3D12SerializeRootSignature(&globalDescription, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors), "Global root serialization failed");
    RequireResult(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&globalRoot_)), "Global root creation failed");

    D3D12_DESCRIPTOR_RANGE textureRange = {};
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.NumDescriptors = 1;
    textureRange.BaseShaderRegister = 3;
    textureRange.RegisterSpace = 1;
    D3D12_ROOT_PARAMETER localParameters[5] = {};
    localParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    localParameters[0].Descriptor.ShaderRegister = 1;
    localParameters[0].Descriptor.RegisterSpace = 1;
    localParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    localParameters[1].Descriptor.ShaderRegister = 2;
    localParameters[1].Descriptor.RegisterSpace = 1;
    localParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    localParameters[2].DescriptorTable = {1, &textureRange};
    localParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    localParameters[3].Descriptor.ShaderRegister = 1;
    localParameters[3].Descriptor.RegisterSpace = 1;
    localParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    localParameters[4].Constants = {2, 1, 2};
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.RegisterSpace = 1;
    D3D12_ROOT_SIGNATURE_DESC localDescription = {};
    localDescription.NumParameters = 5;
    localDescription.pParameters = localParameters;
    localDescription.NumStaticSamplers = 1;
    localDescription.pStaticSamplers = &sampler;
    localDescription.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    RequireResult(D3D12SerializeRootSignature(&localDescription, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors), "Local root serialization failed");
    RequireResult(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&localRoot_)), "Local root creation failed");

    auto library = DirectXCommon::GetInstance()->LoadCompiledShader(L"resources/Shaders/Raytracing/Debug.LIB.hlsl");
    D3D12_EXPORT_DESC exports[4] = {{L"RayGeneration", nullptr, D3D12_EXPORT_FLAG_NONE},
        {L"Miss", nullptr, D3D12_EXPORT_FLAG_NONE}, {L"ClosestHit", nullptr, D3D12_EXPORT_FLAG_NONE},
        {L"AnyHit", nullptr, D3D12_EXPORT_FLAG_NONE}};
    D3D12_DXIL_LIBRARY_DESC libraryDescription = {{library->GetBufferPointer(), library->GetBufferSize()}, 4, exports};
    D3D12_HIT_GROUP_DESC hitGroup = {};
    hitGroup.HitGroupExport = L"HitGroup";
    hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroup.ClosestHitShaderImport = L"ClosestHit";
    hitGroup.AnyHitShaderImport = L"AnyHit";
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {12, 8};
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {1};
    ID3D12RootSignature* globalRoot = globalRoot_.Get();
    ID3D12RootSignature* localRoot = localRoot_.Get();
    D3D12_STATE_SUBOBJECT subobjects[7] = {};
    subobjects[0] = {D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &libraryDescription};
    subobjects[1] = {D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroup};
    subobjects[2] = {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig};
    subobjects[3] = {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig};
    subobjects[4] = {D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &globalRoot};
    subobjects[5] = {D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &localRoot};
    const wchar_t* localExports[] = {L"HitGroup"};
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association = {&subobjects[5], 1, localExports};
    subobjects[6] = {D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &association};
    D3D12_STATE_OBJECT_DESC description = {D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE, 7, subobjects};
    RequireResult(device_->CreateStateObject(&description, IID_PPV_ARGS(&pipeline_)), "DXR pipeline creation failed");
    RequireResult(pipeline_.As(&pipelineProperties_), "DXR shader identifiers unavailable");
    return true;
}
bool DxrRenderer::CreateOutput() {
    auto* srvManager = SrvManager::GetInstance();
    if (!srvManager->CanAllocate(2)) { throw std::runtime_error("DXR output descriptors exhausted"); }
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto description = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT,
        WinApp::kClientWidth, WinApp::kClientHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    RequireResult(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&output_)), "DXR output creation failed");
    statistics_.outputAllocationBytes = device_->GetResourceAllocationInfo(0, 1, &description).SizeInBytes;
    outputUavIndex_ = srvManager->Allocate();
    outputSrvIndex_ = srvManager->Allocate();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
    uav.Format = description.Format;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device_->CreateUnorderedAccessView(output_.Get(), nullptr, &uav, srvManager->GetCPUDescriptorHandle(outputUavIndex_));
    srvManager->CreateSRVforTexture2D(outputSrvIndex_, output_.Get(), description.Format, 1);
    cameraBuffer_ = CreateBuffer(sizeof(CameraParameters), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, false);
    debugCopy_ = std::make_unique<CopyImageRenderer>();
    debugCopy_->Initialize(DirectXCommon::GetInstance());
    debugCopy_->SetPostEffectType(PostEffectType::Copy);
    return true;
}
void DxrRenderer::BeginFrame() {
    if (active_ == this) { active_ = nullptr; }
    ++frameId_;
    isValidScene_ = false;
    shouldCaptureDirectionalShadows_ = false;
    shouldCaptureReflections_ = false;
    shouldCaptureLocalShadows_ = false; localShadowParameters_ = {}; localShadowConstantsAddress_ = 0;
    instances_.clear();
    objectIds_.clear();
    retiredResources_.clear();
    statistics_.instanceCount = 0;
    statistics_.builtBlasCount = 0;
    statistics_.updatedBlasCount = 0;
    statistics_.dynamicBlasCount = 0;
    statistics_.dynamicBufferBytes = 0;
    statistics_.dynamicAllocationBytes = 0;
    statistics_.hitRecordCount = 0;
    statistics_.hasValidFrame = false;
    buildTimer_.ResetSample();
    traceTimer_.ResetSample();
    if (!isSupported_ || !settings_.isEnabled) {
        blasEntries_.clear();
        tlas_.Reset(); tlasScratch_.Reset(); instanceBuffer_.Reset();
        materialBuffer_.Reset(); shaderTable_.Reset();
        statistics_.blasCount = 0;
        statistics_.bufferBytes = 0;
        return;
    }
    try {
        if (!isReady_) {
            // A failed initialization disables this renderer; no assert or game shutdown.
            if (!pipeline_) { CreatePipeline(); }
            if (!output_) { CreateOutput(); }
            isReady_ = true;
        }
        active_ = this;
    } catch (const std::exception& error) { SetFailure(error.what()); }
}
void DxrRenderer::Queue(const void* objectId, const Model& model, const Matrix4x4& world,
    const Material& material, bool shouldCastShadow, bool shouldReceiveShadow) {
    QueueInternal(objectId, model, world, material, shouldCastShadow, nullptr, 0,
        D3D12_RESOURCE_STATE_GENERIC_READ, shouldReceiveShadow);
}
void DxrRenderer::QueueDeformed(const void* objectId, const Model& model, const Matrix4x4& world,
    const Material& material, ID3D12Resource* vertices, uint64_t geometryRevision,
    bool shouldCastShadow, D3D12_RESOURCE_STATES vertexState, bool shouldReceiveShadow) {
    if (!vertices || !objectId || vertices->GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) { return; }
    // Only read states are accepted; UAV writes must be completed by the producer.
    if (vertexState != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
        && vertexState != D3D12_RESOURCE_STATE_GENERIC_READ) { return; }
    QueueInternal(objectId, model, world, material, shouldCastShadow, vertices, geometryRevision, vertexState, shouldReceiveShadow);
}
void DxrRenderer::QueueInternal(const void* objectId, const Model& model, const Matrix4x4& world,
    const Material& material, bool shouldCastShadow, ID3D12Resource* deformedVertices,
    uint64_t geometryRevision, D3D12_RESOURCE_STATES vertexState, bool shouldReceiveShadow) {
    if (active_ != this || !IsValidWorld(world) || instances_.size() >= kMaxInstanceCount) { return; }
    if (!std::isfinite(material.alphaCutoff) || material.alphaCutoff < 0 || material.alphaCutoff > 1) { return; }
    if (std::find(objectIds_.begin(), objectIds_.end(), objectId) != objectIds_.end()) { return; }
    std::vector<Geometry> geometries;
    Instance instance;
    instance.blasKey.model = &model;
    instance.blasKey.isAlphaMasked = material.alphaCutoff > 0;
    if (deformedVertices) { instance.blasKey.objectId = objectId; }
    instance.world = world;
    instance.material = material;
    instance.shouldCastShadow = shouldCastShadow;
    instance.shouldReceiveShadow = shouldReceiveShadow;
    uint64_t vertexOffsetBytes = 0;
    for (const MeshPrimitive& primitive : model.GetModelData().primitives) {
        uint64_t primitiveOffsetBytes = vertexOffsetBytes;
        vertexOffsetBytes += primitive.vertices.size() * sizeof(VertexData);
        if (deformedVertices && vertexOffsetBytes > deformedVertices->GetDesc().Width) { return; }
        if (primitive.mode != PrimitiveMode::Triangles || !primitive.vertexResource || primitive.vertices.empty()) { continue; }
        if (primitive.vertices.size() > UINT_MAX || primitive.indices.size() > UINT_MAX) { continue; }
        size_t elementCount = primitive.vertices.size();
        if (!primitive.indices.empty()) { elementCount = primitive.indices.size(); }
        if (elementCount % 3 != 0) { continue; }
        bool hasValidIndices = true;
        for (uint32_t vertexIndex : primitive.indices) {
            if (vertexIndex >= primitive.vertices.size()) { hasValidIndices = false; break; }
        }
        if (!hasValidIndices || (!primitive.indices.empty() && !primitive.indexResource)) { continue; }
        Geometry geometry;
        geometry.vertices = primitive.vertexResource;
        if (deformedVertices) {
            geometry.vertices = deformedVertices;
            geometry.vertexOffsetBytes = primitiveOffsetBytes;
        }
        geometry.indices = primitive.indexResource;
        geometry.vertexCount = static_cast<uint32_t>(primitive.vertices.size());
        geometry.indexCount = static_cast<uint32_t>(primitive.indices.size());
        geometry.materialIndex = primitive.materialIndex;
        geometries.push_back(geometry);
        instance.textures.push_back(TextureManager::GetInstance()->GetSrvHandleGPU(model.GetMaterial(primitive.materialIndex).textureFilePath));
    }
    if (geometries.empty()) { return; }
    BlasEntry& entry = blasEntries_[instance.blasKey];
    bool hasSameGeometry = entry.geometries.size() == geometries.size();
    if (hasSameGeometry) {
        for (size_t index = 0; index < geometries.size(); ++index) {
            const Geometry& previous = entry.geometries[index];
            const Geometry& current = geometries[index];
            if (previous.vertices.Get() != current.vertices.Get() || previous.indices.Get() != current.indices.Get()
                || previous.vertexCount != current.vertexCount || previous.indexCount != current.indexCount
                || previous.vertexOffsetBytes != current.vertexOffsetBytes) { hasSameGeometry = false; break; }
        }
    }
    if (!hasSameGeometry) {
        if (entry.result) { retiredResources_.push_back(entry.result); }
        if (entry.scratch) { retiredResources_.push_back(entry.scratch); }
        for (const Geometry& geometry : entry.geometries) {
            retiredResources_.push_back(geometry.vertices);
            if (geometry.indices) { retiredResources_.push_back(geometry.indices); }
        }
        entry = {};
        entry.geometries = std::move(geometries);
    }
    entry.isDynamic = deformedVertices != nullptr;
    entry.isAlphaMasked = instance.blasKey.isAlphaMasked;
    entry.shouldUpdate = entry.isDynamic && entry.geometryRevision != geometryRevision;
    entry.geometryRevision = geometryRevision;
    entry.vertexState = vertexState;
    entry.lastFrameId = frameId_;
    instances_.push_back(std::move(instance));
    objectIds_.push_back(objectId);
}
void DxrRenderer::TransitionDynamicVertices(bool shouldRestore, bool shouldLimitToMasked) const {
    std::vector<ID3D12Resource*> transitionedResources;
    for (const auto& pair : blasEntries_) {
        const BlasEntry& entry = pair.second;
        if (shouldLimitToMasked && !pair.first.isAlphaMasked) { continue; }
        if (!entry.isDynamic || entry.lastFrameId != frameId_
            || (entry.vertexState & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) != 0) { continue; }
        for (const Geometry& geometry : entry.geometries) {
            if (std::find(transitionedResources.begin(), transitionedResources.end(), geometry.vertices.Get())
                != transitionedResources.end()) { continue; }
            auto shaderReadState = static_cast<D3D12_RESOURCE_STATES>(
                entry.vertexState | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            auto beforeState = entry.vertexState;
            auto afterState = shaderReadState;
            if (shouldRestore) { beforeState = shaderReadState; afterState = entry.vertexState; }
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(geometry.vertices.Get(), beforeState, afterState);
            commandList_->ResourceBarrier(1, &barrier);
            transitionedResources.push_back(geometry.vertices.Get());
        }
    }
}
bool DxrRenderer::BuildBlas(BlasEntry& entry) {
    static_assert(sizeof(VertexData) == 36);
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> descriptions;
    for (const Geometry& geometry : entry.geometries) {
        D3D12_RAYTRACING_GEOMETRY_DESC description = {};
        description.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        description.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        // The key separates masked and opaque BLAS; flags remain fixed during refits.
        if (entry.isAlphaMasked) { description.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE; }
        description.Triangles.VertexBuffer = {geometry.vertices->GetGPUVirtualAddress() + geometry.vertexOffsetBytes, sizeof(VertexData)};
        description.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        description.Triangles.VertexCount = geometry.vertexCount;
        if (geometry.indexCount != 0) {
            description.Triangles.IndexBuffer = geometry.indices->GetGPUVirtualAddress();
            description.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
            description.Triangles.IndexCount = geometry.indexCount;
        }
        descriptions.push_back(description);
    }
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
    build.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    build.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    build.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    bool shouldUpdate = entry.isDynamic && entry.result != nullptr;
    if (entry.isDynamic) {
        build.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD
            | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    }
    build.Inputs.NumDescs = static_cast<UINT>(descriptions.size());
    build.Inputs.pGeometryDescs = descriptions.data();
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes = {};
    device_->GetRaytracingAccelerationStructurePrebuildInfo(&build.Inputs, &sizes);
    if (sizes.ResultDataMaxSizeInBytes == 0) { throw std::runtime_error("Invalid BLAS size"); }
    if (!shouldUpdate) {
        entry.result = CreateBuffer(sizes.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
        uint64_t scratchSizeBytes = sizes.ScratchDataSizeInBytes;
        if (entry.isDynamic) { scratchSizeBytes = (std::max)(scratchSizeBytes, sizes.UpdateScratchDataSizeInBytes); }
        entry.scratch = CreateBuffer(scratchSizeBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
        auto resultDescription = entry.result->GetDesc();
        auto scratchDescription = entry.scratch->GetDesc();
        entry.allocationBytes = device_->GetResourceAllocationInfo(0, 1, &resultDescription).SizeInBytes
            + device_->GetResourceAllocationInfo(0, 1, &scratchDescription).SizeInBytes;
    } else {
        build.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        build.SourceAccelerationStructureData = entry.result->GetGPUVirtualAddress();
        auto scratchBarrier = CD3DX12_RESOURCE_BARRIER::UAV(entry.scratch.Get());
        commandList_->ResourceBarrier(1, &scratchBarrier);
    }
    build.DestAccelerationStructureData = entry.result->GetGPUVirtualAddress();
    build.ScratchAccelerationStructureData = entry.scratch->GetGPUVirtualAddress();
    commandList_->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(entry.result.Get());
    commandList_->ResourceBarrier(1, &barrier);
    if (shouldUpdate) { ++statistics_.updatedBlasCount; }
    else { ++statistics_.builtBlasCount; }
    entry.shouldUpdate = false;
    return true;
}
bool DxrRenderer::BuildScene() {
    uint64_t reflectionSceneHash = 14695981039346656037ull;
    for (const Instance& instance : instances_) {
        reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &instance.world, sizeof(instance.world));
        reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &instance.material, sizeof(instance.material));
        reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &instance.shouldCastShadow, sizeof(instance.shouldCastShadow));
        reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &instance.shouldReceiveShadow, sizeof(instance.shouldReceiveShadow));
        for (const auto& texture : instance.textures) {
            reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &texture.ptr, sizeof(texture.ptr));
        }
        const BlasEntry& entry = blasEntries_.at(instance.blasKey);
        reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &entry.geometryRevision, sizeof(entry.geometryRevision));
        for (const Geometry& geometry : entry.geometries) {
            uint64_t address = geometry.vertices->GetGPUVirtualAddress() + geometry.vertexOffsetBytes;
            reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &address, sizeof(address));
            reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &geometry.vertexCount, sizeof(geometry.vertexCount));
            address = 0;
            if (geometry.indices) { address = geometry.indices->GetGPUVirtualAddress(); }
            reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &address, sizeof(address));
            reflectionSceneHash = HashReflectionBytes(reflectionSceneHash, &geometry.indexCount, sizeof(geometry.indexCount));
        }
    }
    if (reflectionSceneHash != previousReflectionSceneHash_) {
        ++reflectionSceneRevision_; previousReflectionSceneHash_ = reflectionSceneHash;
    }
    uint64_t shadowSceneHash = 14695981039346656037ull;
    for (const Instance& instance : instances_) {
        if (!instance.shouldCastShadow) { continue; }
        if (instance.blasKey.isAlphaMasked) {
            uint32_t alphaBits = 0;
            std::memcpy(&alphaBits, &instance.material.alphaCutoff, sizeof(alphaBits));
            shadowSceneHash = (shadowSceneHash ^ alphaBits) * 1099511628211ull;
            std::memcpy(&alphaBits, &instance.material.color.w, sizeof(alphaBits));
            shadowSceneHash = (shadowSceneHash ^ alphaBits) * 1099511628211ull;
            for (uint32_t row = 0; row < 4; ++row) {
                for (uint32_t column = 0; column < 4; ++column) {
                    std::memcpy(&alphaBits, &instance.material.uvTransform.m[row][column], sizeof(alphaBits));
                    shadowSceneHash = (shadowSceneHash ^ alphaBits) * 1099511628211ull;
                }
            }
            for (const auto& texture : instance.textures) { shadowSceneHash = (shadowSceneHash ^ texture.ptr) * 1099511628211ull; }
        }
        for (uint32_t row = 0; row < 4; ++row) {
            for (uint32_t column = 0; column < 4; ++column) {
                uint32_t bits = 0;
                std::memcpy(&bits, &instance.world.m[row][column], sizeof(bits));
                shadowSceneHash = (shadowSceneHash ^ bits) * 1099511628211ull;
            }
        }
        const BlasEntry& entry = blasEntries_.at(instance.blasKey);
        if (entry.isDynamic) { shadowSceneHash = (shadowSceneHash ^ entry.geometryRevision) * 1099511628211ull; }
        for (const Geometry& geometry : entry.geometries) {
            shadowSceneHash = (shadowSceneHash ^ geometry.vertices->GetGPUVirtualAddress()) * 1099511628211ull;
            shadowSceneHash = (shadowSceneHash ^ geometry.vertexOffsetBytes) * 1099511628211ull;
            shadowSceneHash = (shadowSceneHash ^ geometry.vertexCount) * 1099511628211ull;
            if (geometry.indices) { shadowSceneHash = (shadowSceneHash ^ geometry.indices->GetGPUVirtualAddress()) * 1099511628211ull; }
            shadowSceneHash = (shadowSceneHash ^ geometry.indexCount) * 1099511628211ull;
        }
    }
    if (shadowSceneHash != previousShadowSceneHash_) {
        ++shadowSceneRevision_; previousShadowSceneHash_ = shadowSceneHash;
    }
    for (auto iterator = blasEntries_.begin(); iterator != blasEntries_.end();) {
        if (iterator->second.lastFrameId != frameId_) { iterator = blasEntries_.erase(iterator); }
        else { ++iterator; }
    }
    for (auto& pair : blasEntries_) {
        if (!pair.second.result || pair.second.shouldUpdate) { BuildBlas(pair.second); }
        if (pair.second.isDynamic) {
            ++statistics_.dynamicBlasCount;
            statistics_.dynamicBufferBytes += pair.second.result->GetDesc().Width + pair.second.scratch->GetDesc().Width;
            statistics_.dynamicAllocationBytes += pair.second.allocationBytes;
        }
    }
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> descriptions;
    uint32_t recordOffset = 0;
    for (const Instance& instance : instances_) {
        BlasEntry& entry = blasEntries_.at(instance.blasKey);
        if (recordOffset > kMaxInstanceCount || entry.geometries.size() > kMaxInstanceCount - recordOffset) {
            throw std::runtime_error("DXR hit record limit exceeded");
        }
        D3D12_RAYTRACING_INSTANCE_DESC description = {};
        for (uint32_t row = 0; row < 3; ++row) {
            for (uint32_t column = 0; column < 4; ++column) { description.Transform[row][column] = instance.world.m[column][row]; }
        }
        description.InstanceID = static_cast<UINT>(descriptions.size());
        description.InstanceMask = 1;
        if (instance.shouldCastShadow) { description.InstanceMask |= 2; }
        description.InstanceContributionToHitGroupIndex = recordOffset;
        description.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
        description.AccelerationStructure = entry.result->GetGPUVirtualAddress();
        descriptions.push_back(description);
        recordOffset += static_cast<uint32_t>(entry.geometries.size());
    }
    statistics_.hitRecordCount = recordOffset;
    instanceBuffer_ = CreateBuffer(descriptions.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, false);
    void* data = nullptr;
    RequireResult(instanceBuffer_->Map(0, nullptr, &data), "Instance upload map failed");
    std::memcpy(data, descriptions.data(), descriptions.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
    instanceBuffer_->Unmap(0, nullptr);
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
    build.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    build.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    build.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    build.Inputs.NumDescs = static_cast<UINT>(descriptions.size());
    build.Inputs.InstanceDescs = instanceBuffer_->GetGPUVirtualAddress();
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes = {};
    device_->GetRaytracingAccelerationStructurePrebuildInfo(&build.Inputs, &sizes);
    if (sizes.ResultDataMaxSizeInBytes == 0) { throw std::runtime_error("Invalid TLAS size"); }
    tlas_ = CreateBuffer(sizes.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
    tlasScratch_ = CreateBuffer(sizes.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
    build.DestAccelerationStructureData = tlas_->GetGPUVirtualAddress();
    build.ScratchAccelerationStructureData = tlasScratch_->GetGPUVirtualAddress();
    commandList_->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(tlas_.Get());
    commandList_->ResourceBarrier(1, &barrier);
    return true;
}
void DxrRenderer::CreateMaterialBuffer() {
    materialBuffer_ = CreateBuffer(instances_.size() * kMaterialStrideBytes,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, false);
    static_assert(sizeof(Material) <= kMaterialStrideBytes);
    void* materialData = nullptr;
    RequireResult(materialBuffer_->Map(0, nullptr, &materialData), "Material upload map failed");
    std::memset(materialData, 0, static_cast<size_t>(materialBuffer_->GetDesc().Width));
    for (size_t instanceIndex = 0; instanceIndex < instances_.size(); ++instanceIndex) {
        std::memcpy(static_cast<uint8_t*>(materialData) + instanceIndex * kMaterialStrideBytes,
            &instances_[instanceIndex].material, sizeof(Material));
    }
    materialBuffer_->Unmap(0, nullptr);
}
void DxrRenderer::WriteHitRecords(void* records, uint64_t strideBytes, const void* shaderIdentifier) const {
    if (!records || !shaderIdentifier || strideBytes < kShaderRecordStrideBytes || !materialBuffer_) {
        throw std::runtime_error("Invalid material hit table input");
    }
    auto* bytes = static_cast<uint8_t*>(records);
    uint64_t recordIndex = 0;
    for (size_t instanceIndex = 0; instanceIndex < instances_.size(); ++instanceIndex) {
        const Instance& instance = instances_[instanceIndex];
        const BlasEntry& entry = blasEntries_.at(instance.blasKey);
        for (size_t geometryIndex = 0; geometryIndex < entry.geometries.size(); ++geometryIndex) {
            const Geometry& geometry = entry.geometries[geometryIndex];
            auto* record = bytes + recordIndex * strideBytes;
            std::memset(record, 0, static_cast<size_t>(strideBytes));
            std::memcpy(record, shaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            uint64_t vertexAddress = geometry.vertices->GetGPUVirtualAddress() + geometry.vertexOffsetBytes;
            uint64_t arguments[4] = {vertexAddress, vertexAddress,
                instance.textures[geometryIndex].ptr, materialBuffer_->GetGPUVirtualAddress() + instanceIndex * kMaterialStrideBytes};
            uint32_t hasIndices = 0;
            if (geometry.indexCount != 0) { arguments[1] = geometry.indices->GetGPUVirtualAddress(); hasIndices = 1; }
            std::memcpy(record + 32, arguments, sizeof(arguments));
            std::memcpy(record + 64, &hasIndices, sizeof(hasIndices));
            uint32_t shouldReceiveShadow = 0; if (instance.shouldReceiveShadow) { shouldReceiveShadow = 1; }
            std::memcpy(record + 68, &shouldReceiveShadow, sizeof(shouldReceiveShadow));
            ++recordIndex;
        }
    }
}
bool DxrRenderer::CreateShaderTables() {
    uint64_t recordCount = 0;
    for (const Instance& instance : instances_) { recordCount += blasEntries_.at(instance.blasKey).geometries.size(); }
    const uint64_t kHitOffsetBytes = kTableAlignmentBytes * 2;
    shaderTable_ = CreateBuffer(kHitOffsetBytes + recordCount * kShaderRecordStrideBytes,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, false);
    void* tableData = nullptr;
    RequireResult(shaderTable_->Map(0, nullptr, &tableData), "Shader table map failed");
    auto* bytes = static_cast<uint8_t*>(tableData);
    std::memset(bytes, 0, static_cast<size_t>(shaderTable_->GetDesc().Width));
    const void* rayGenerationId = pipelineProperties_->GetShaderIdentifier(L"RayGeneration");
    const void* missId = pipelineProperties_->GetShaderIdentifier(L"Miss");
    const void* hitId = pipelineProperties_->GetShaderIdentifier(L"HitGroup");
    if (!rayGenerationId || !missId || !hitId) { throw std::runtime_error("Missing DXR shader identifier"); }
    std::memcpy(bytes, rayGenerationId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    std::memcpy(bytes + kTableAlignmentBytes, missId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    WriteHitRecords(bytes + kHitOffsetBytes, kShaderRecordStrideBytes, hitId);
    shaderTable_->Unmap(0, nullptr);
    uint64_t address = shaderTable_->GetGPUVirtualAddress();
    dispatch_ = {};
    dispatch_.RayGenerationShaderRecord = {address, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES};
    dispatch_.MissShaderTable = {address + kTableAlignmentBytes, kTableAlignmentBytes, kTableAlignmentBytes};
    dispatch_.HitGroupTable = {address + kHitOffsetBytes, recordCount * kShaderRecordStrideBytes, kShaderRecordStrideBytes};
    dispatch_.Width = WinApp::kClientWidth;
    dispatch_.Height = WinApp::kClientHeight;
    dispatch_.Depth = 1;
    return true;
}
void DxrRenderer::EndFrame(const Camera* camera, bool shouldTraceDebug) {
    if (active_ == this) { active_ = nullptr; }
    if (!settings_.isEnabled || !isReady_) { return; }
    statistics_.instanceCount = static_cast<uint32_t>(instances_.size());
    if (instances_.empty() || camera == nullptr) {
        blasEntries_.clear();
        statistics_.blasCount = 0;
        status_ = "DXR ready; no eligible geometry or camera";
        return;
    }
    if (!IsValidWorld(camera->GetWorldMatrix()) || !std::isfinite(camera->GetNearClip())
        || !std::isfinite(camera->GetFarClip()) || camera->GetNearClip() <= 0 || camera->GetFarClip() <= camera->GetNearClip()) { return; }
    try {
        TransitionDynamicVertices(false);
        buildTimer_.Begin();
        BuildScene();
        CreateMaterialBuffer();
        buildTimer_.End();
        isValidScene_ = true;
        statistics_.blasCount = static_cast<uint32_t>(blasEntries_.size());
        statistics_.bufferBytes = tlas_->GetDesc().Width + tlasScratch_->GetDesc().Width + instanceBuffer_->GetDesc().Width
            + materialBuffer_->GetDesc().Width;
        for (const auto& pair : blasEntries_) { statistics_.bufferBytes += pair.second.result->GetDesc().Width + pair.second.scratch->GetDesc().Width; }
        if (!shouldTraceDebug) { TransitionDynamicVertices(true); status_ = "DXR scene ready"; return; }
        CreateShaderTables();
        CameraParameters parameters = {};
        parameters.inverseViewProjection = MatrixMath::Inverse(camera->GetUnjitteredViewProjectionMatrix());
        for (uint32_t row = 0; row < 4; ++row) {
            for (uint32_t column = 0; column < 4; ++column) {
                if (!std::isfinite(parameters.inverseViewProjection.m[row][column])) { throw std::runtime_error("Invalid DXR camera matrix"); }
            }
        }
        parameters.cameraPosition = {camera->GetWorldMatrix().m[3][0], camera->GetWorldMatrix().m[3][1], camera->GetWorldMatrix().m[3][2]};
        parameters.debugMode = static_cast<uint32_t>(settings_.debugMode);
        parameters.nearClip = camera->GetNearClip();
        parameters.farClip = camera->GetFarClip();
        void* data = nullptr;
        RequireResult(cameraBuffer_->Map(0, nullptr, &data), "Camera upload map failed");
        std::memcpy(data, &parameters, sizeof(parameters));
        cameraBuffer_->Unmap(0, nullptr);
        auto before = CD3DX12_RESOURCE_BARRIER::Transition(output_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList_->ResourceBarrier(1, &before);
        SrvManager::GetInstance()->PreDraw();
        commandList_->SetComputeRootSignature(globalRoot_.Get());
        commandList_->SetComputeRootShaderResourceView(0, tlas_->GetGPUVirtualAddress());
        commandList_->SetComputeRootDescriptorTable(1, SrvManager::GetInstance()->GetGPUDescriptorHandle(outputUavIndex_));
        commandList_->SetComputeRootConstantBufferView(2, cameraBuffer_->GetGPUVirtualAddress());
        commandList_->SetPipelineState1(pipeline_.Get());
        traceTimer_.Begin();
        commandList_->DispatchRays(&dispatch_);
        traceTimer_.End();
        auto after = CD3DX12_RESOURCE_BARRIER::Transition(output_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(1, &after);
        statistics_.hasValidFrame = true;
        statistics_.blasCount = static_cast<uint32_t>(blasEntries_.size());
        statistics_.bufferBytes = tlas_->GetDesc().Width + tlasScratch_->GetDesc().Width + instanceBuffer_->GetDesc().Width
            + materialBuffer_->GetDesc().Width + shaderTable_->GetDesc().Width + cameraBuffer_->GetDesc().Width;
        for (const auto& pair : blasEntries_) { statistics_.bufferBytes += pair.second.result->GetDesc().Width + pair.second.scratch->GetDesc().Width; }
        status_ = "DXR ready";
    } catch (const std::exception& error) { SetFailure(error.what()); }
    TransitionDynamicVertices(true);
}
D3D12_GPU_DESCRIPTOR_HANDLE DxrRenderer::GetOutputSrv() const {
    if (!statistics_.hasValidFrame || outputSrvIndex_ == UINT_MAX) { return {}; }
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(outputSrvIndex_);
}
D3D12_GPU_VIRTUAL_ADDRESS DxrRenderer::GetSceneGpuAddress() const {
    if (!isValidScene_ || !tlas_) { return 0; }
    return tlas_->GetGPUVirtualAddress();
}
void DxrRenderer::DrawDebug() {
    if (!settings_.isDebugVisible || !statistics_.hasValidFrame || !debugCopy_) { return; }
    auto handle = GetOutputSrv();
    SrvManager::GetInstance()->PreDraw();
    debugCopy_->Draw(handle, handle, handle);
}
void DxrRenderer::ReadCompleted() { buildTimer_.ReadCompleted(); traceTimer_.ReadCompleted(); }
void DxrRenderer::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::Begin("DXR Foundation")) {
        ImGui::TextUnformatted(status_.c_str());
        if (isSupported_) {
            ImGui::Checkbox("Enabled", &settings_.isEnabled);
            ImGui::Checkbox("Show ray tracing output", &settings_.isDebugVisible);
            int debugMode = static_cast<int>(settings_.debugMode);
            if (ImGui::Combo("Output", &debugMode, "Normals\0Base color\0Instance ID\0")) { settings_.debugMode = static_cast<DxrDebugMode>(debugMode); }
        }
        ImGui::Text("Instances %u / BLAS %u / new BLAS %u", statistics_.instanceCount, statistics_.blasCount, statistics_.builtBlasCount);
        ImGui::Text("Dynamic BLAS %u / updated %u / buffers %.3f MiB", statistics_.dynamicBlasCount,
            statistics_.updatedBlasCount, statistics_.dynamicBufferBytes / 1048576.0);
        ImGui::Text("Dynamic BLAS allocation %.3f MiB", statistics_.dynamicAllocationBytes / 1048576.0);
        ImGui::Text("Buffer capacity %.2f MiB / output allocation %.2f MiB", statistics_.bufferBytes / 1048576.0, statistics_.outputAllocationBytes / 1048576.0);
        ImGui::Text("Build %.3f ms / trace %.3f ms", GetBuildGpuTimeMs(), GetTraceGpuTimeMs());
        ImGui::TextWrapped("Opaque rigid and GPU-skinned geometry. Disabled by default. GPU timings exclude CPU preparation and debug composition.");
    }
    ImGui::End();
#endif
}
#if defined(ENABLE_DEVELOPMENT_TOOLS)
nlohmann::json DxrRenderer::GetDevelopmentState() const {
    return {{"isSupported", isSupported_}, {"status", status_}, {"isEnabled", settings_.isEnabled},
        {"isDebugVisible", settings_.isDebugVisible}, {"hasValidFrame", statistics_.hasValidFrame},
        {"instanceCount", statistics_.instanceCount}, {"blasCount", statistics_.blasCount},
        {"builtBlasCount", statistics_.builtBlasCount}, {"bufferBytes", statistics_.bufferBytes},
        {"updatedBlasCount", statistics_.updatedBlasCount}, {"dynamicBlasCount", statistics_.dynamicBlasCount},
        {"dynamicBufferBytes", statistics_.dynamicBufferBytes},
        {"dynamicAllocationBytes", statistics_.dynamicAllocationBytes},
        {"outputAllocationBytes", statistics_.outputAllocationBytes},
        {"buildGpuMs", GetBuildGpuTimeMs()}, {"traceGpuMs", GetTraceGpuTimeMs()}};
}
nlohmann::json DxrRenderer::GetDevelopmentControls() const {
    return nlohmann::json::array({
        {{"key", "status"}, {"label", "状態"}, {"type", "metric"}},
        {{"key", "isSupported"}, {"label", "DXR対応"}, {"type", "metric"}},
        {{"key", "isEnabled"}, {"label", "DXRを有効"}, {"type", "bool"}},
        {{"key", "isDebugVisible"}, {"label", "検証画像を表示"}, {"type", "bool"}},
        {{"key", "normals"}, {"label", "法線"}, {"type", "action"}},
        {{"key", "baseColor"}, {"label", "基本色"}, {"type", "action"}},
        {{"key", "instanceId"}, {"label", "配置ID"}, {"type", "action"}},
        {{"key", "instanceCount"}, {"label", "配置数"}, {"type", "metric"}},
        {{"key", "blasCount"}, {"label", "共有BLAS数"}, {"type", "metric"}},
        {{"key", "builtBlasCount"}, {"label", "新規BLAS数"}, {"type", "metric"}},
        {{"key", "updatedBlasCount"}, {"label", "BLAS更新数"}, {"type", "metric"}},
        {{"key", "dynamicBlasCount"}, {"label", "変形BLAS数"}, {"type", "metric"}},
        {{"key", "dynamicBufferBytes"}, {"label", "変形BLASバッファ容量 (bytes)"}, {"type", "metric"}},
        {{"key", "dynamicAllocationBytes"}, {"label", "変形BLAS実割当 (bytes)"}, {"type", "metric"}},
        {{"key", "bufferBytes"}, {"label", "バッファ容量 (bytes)"}, {"type", "metric"}},
        {{"key", "outputAllocationBytes"}, {"label", "出力画像割当 (bytes)"}, {"type", "metric"}},
        {{"key", "buildGpuMs"}, {"label", "BLAS/TLAS構築 (ms)"}, {"type", "metric"}},
        {{"key", "traceGpuMs"}, {"label", "光線探索 (ms)"}, {"type", "metric"}}
    });
}
bool DxrRenderer::SetDevelopmentBool(const std::string& key, bool isEnabled) {
    if (key == "isEnabled") { settings_.isEnabled = isEnabled; return true; }
    if (key == "isDebugVisible") { settings_.isDebugVisible = isEnabled; return true; }
    return false;
}
bool DxrRenderer::ExecuteDevelopmentCommand(const std::string& key) {
    if (key == "normals") { settings_.debugMode = DxrDebugMode::Normals; return true; }
    if (key == "baseColor") { settings_.debugMode = DxrDebugMode::BaseColor; return true; }
    if (key == "instanceId") { settings_.debugMode = DxrDebugMode::InstanceId; return true; }
    return false;
}
#endif
