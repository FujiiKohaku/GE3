#include "Engine/Renderer/SceneRenderResolution.h"
#include "MotionVectorRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include <stdexcept>
#include <cstring>

namespace {
void CheckMotionVector(HRESULT result) {
    if (FAILED(result)) { throw std::runtime_error("Motion vector resource/pipeline creation failed"); }
}
const std::wstring kDefaultMotionShader = L"resources/Shaders/MotionVector/MotionVector.VS.hlsl";
void TransitionMotionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    DirectXCommon::GetInstance()->GetCommandList()->ResourceBarrier(1, &barrier);
}
}

MotionVectorRenderer::~MotionVectorRenderer() {
    if (active_ == this) { active_ = nullptr; }
    if (srvIndex_ != UINT_MAX) { SrvManager::GetInstance()->Free(srvIndex_); }
    for (uint32_t index : reprojectionSrvIndices_) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); } }
}

void MotionVectorRenderer::Initialize() {
    ResizeSceneTargets();
    auto* dx = DirectXCommon::GetInstance();
    auto* device = dx->GetDevice();
    D3D12_ROOT_PARAMETER parameters[8] = {};
    const uint32_t counts[] = {16, 16, 4, 4, 2};
    for (uint32_t index = 0; index < 5; ++index) {
        parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[index].Constants.ShaderRegister = index;
        parameters[index].Constants.Num32BitValues = counts[index];
        parameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    }
    parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[5].Descriptor.ShaderRegister = 5;
    parameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_DESCRIPTOR_RANGE textureRange = {};
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; textureRange.NumDescriptors = 1;
    parameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[6].DescriptorTable = {1, &textureRange};
    parameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX; sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature = {};
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    parameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[7].Descriptor.ShaderRegister = 6;
    parameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    signature.NumParameters = 8;
    signature.pParameters = parameters;
    signature.NumStaticSamplers = 1; signature.pStaticSamplers = &sampler;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    CheckMotionVector(D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors));
    CheckMotionVector(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_)));
    auto pixelShader = dx->LoadCompiledShader(L"resources/Shaders/MotionVector/MotionVector.PS.hlsl");
    pipelineDescription_.pRootSignature = root_.Get();
    pipelineDescription_.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    pipelineDescription_.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineDescription_.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pipelineDescription_.RasterizerState.DepthClipEnable = TRUE;
    pipelineDescription_.DepthStencilState.DepthEnable = TRUE;
    pipelineDescription_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipelineDescription_.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pipelineDescription_.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipelineDescription_.SampleMask = UINT_MAX;
    pipelineDescription_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDescription_.NumRenderTargets = 2;
    pipelineDescription_.BlendState.IndependentBlendEnable = TRUE;
    pipelineDescription_.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipelineDescription_.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;
    pipelineDescription_.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
    pipelineDescription_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipelineDescription_.SampleDesc.Count = 1;
    GetPipeline(kDefaultMotionShader, false);
    // Keep bytecode alive while constructing subsequent custom pipelines.
    pipelineDescription_.PS = {};

    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER debugParameter = {};
    debugParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    debugParameter.DescriptorTable.NumDescriptorRanges = 1;
    debugParameter.DescriptorTable.pDescriptorRanges = &range;
    debugParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    signature.NumParameters = 1;
    signature.pParameters = &debugParameter;
    signature.NumStaticSamplers = 0; signature.pStaticSamplers = nullptr;
    CheckMotionVector(D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors));
    CheckMotionVector(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&debugRoot_)));
    auto debugVertexShader = dx->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    auto debugPixelShader = dx->LoadCompiledShader(L"resources/Shaders/MotionVector/Debug.PS.hlsl");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC debugDescription = pipelineDescription_;
    debugDescription.NumRenderTargets = 1;
    debugDescription.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
    debugDescription.pRootSignature = debugRoot_.Get();
    debugDescription.VS = {debugVertexShader->GetBufferPointer(), debugVertexShader->GetBufferSize()};
    debugDescription.PS = {debugPixelShader->GetBufferPointer(), debugPixelShader->GetBufferSize()};
    debugDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    debugDescription.DepthStencilState.DepthEnable = FALSE;
    debugDescription.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    debugDescription.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    CheckMotionVector(device->CreateGraphicsPipelineState(&debugDescription, IID_PPV_ARGS(&debugPipeline_)));
}

ID3D12PipelineState* MotionVectorRenderer::GetPipeline(const std::wstring& shaderPath, bool isDoubleSided, bool isAlphaMasked) {
    std::wstring resolvedPath = shaderPath;
    if (resolvedPath.empty()) { resolvedPath = kDefaultMotionShader; }
    const auto key = std::make_tuple(resolvedPath, isDoubleSided, isAlphaMasked);
    auto found = pipelines_.find(key);
    if (found != pipelines_.end()) { return found->second.Get(); }
    auto* dx = DirectXCommon::GetInstance();
    auto vertexShader = dx->LoadCompiledShader(resolvedPath);
    std::wstring pixelPath = L"resources/Shaders/MotionVector/MotionVector.PS.hlsl";
    if (isAlphaMasked) { pixelPath = L"resources/Shaders/MotionVector/AlphaMask.PS.hlsl"; }
    auto pixelShader = dx->LoadCompiledShader(pixelPath);
    D3D12_INPUT_ELEMENT_DESC inputs[4] = {};
    for (uint32_t index = 0; index < 2; ++index) {
        inputs[index].SemanticName = "POSITION";
        inputs[index].SemanticIndex = index;
        inputs[index].InputSlot = index;
        inputs[index].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    inputs[2].SemanticName = "TEXCOORD"; inputs[2].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputs[2].AlignedByteOffset = 16;
    auto description = pipelineDescription_;
    inputs[3].SemanticName = "NORMAL"; inputs[3].InputSlot = 1; inputs[3].Format = DXGI_FORMAT_R32G32B32_FLOAT; inputs[3].AlignedByteOffset = 24;
    description.InputLayout = {inputs, 4};
    description.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    description.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    if (isDoubleSided) { description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; }
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
    CheckMotionVector(dx->GetDevice()->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&pipeline)));
    pipelines_.emplace(key, pipeline);
    return pipeline.Get();
}

void MotionVectorRenderer::BeginFrame() {
    reprojectionIndex_ = 1 - reprojectionIndex_;
    previousFrameId_ = frameId_;
    frameId_ = nextFrameId_++;
    draws_.clear(); copies_.clear();
    active_ = nullptr;
    if (settings_.isEnabled) { active_ = this; }
}
void MotionVectorRenderer::ResetHistory() { frameId_ = 0; previousFrameId_ = 0; }
bool MotionVectorRenderer::HasHistory(const MotionVectorHistory& history, const Camera& camera) const {
    return history.frameId != 0 && history.frameId == previousFrameId_ && history.camera == &camera
        && history.cameraHistoryId == camera.GetMotionHistoryId();
}
void MotionVectorRenderer::CommitHistory(MotionVectorHistory& history, const Matrix4x4& world, const Camera& camera, const Vector4& parameters) {
    history.previousWorldViewProjection = MatrixMath::Multiply(world, camera.GetUnjitteredViewProjectionMatrix());
    history.previousWorldView = MatrixMath::Multiply(world, camera.GetViewMatrix());
    history.previousNormalTransform = MatrixMath::Transpose(MatrixMath::Inverse(world));
    history.previousParameters = parameters;
    history.frameId = frameId_;
    history.camera = &camera;
    history.cameraHistoryId = camera.GetMotionHistoryId();
}
void MotionVectorRenderer::Queue(const D3D12_VERTEX_BUFFER_VIEW& currentVertices,
    const D3D12_VERTEX_BUFFER_VIEW& previousVertices, const D3D12_INDEX_BUFFER_VIEW& indices,
    uint32_t vertexCount, uint32_t indexCount, const Matrix4x4& world, const Camera& camera,
    MotionVectorHistory& history, const std::wstring& shaderPath, const Vector4& parameters, bool isDoubleSided,
    const Material* material, D3D12_GPU_DESCRIPTOR_HANDLE textureSrv) {
    if (material && material->alphaCutoff > 0 && (!shaderPath.empty() || textureSrv.ptr == 0)) { return; }
    if (history.surfaceId == 0 && nextSurfaceId_ <= 0x00ffffffu) { history.surfaceId = nextSurfaceId_++; }
    DrawEntry entry;
    entry.surfaceId = history.surfaceId;
    entry.vertices[0] = currentVertices;
    entry.vertices[1] = previousVertices;
    entry.indices = indices; entry.vertexCount = vertexCount; entry.indexCount = indexCount;
    entry.currentWorldViewProjection = MatrixMath::Multiply(world, camera.GetUnjitteredViewProjectionMatrix());
    entry.jitterNdc = camera.GetProjectionJitter();
    entry.previousWorldViewProjection = entry.currentWorldViewProjection;
    entry.parameters = parameters; entry.previousParameters = parameters;
    if (HasHistory(history, camera)) {
        entry.previousWorldViewProjection = history.previousWorldViewProjection;
        entry.previousParameters = history.previousParameters;
        entry.previousWorldView = history.previousWorldView;
        entry.previousNormalTransform = history.previousNormalTransform;
        entry.hasPreviousGeometry = shaderPath.empty();
    } else { entry.vertices[1] = currentVertices; }
    entry.shaderPath = shaderPath; entry.isDoubleSided = isDoubleSided;
    if (material) { entry.material = *material; entry.textureSrv = textureSrv; }
    draws_.push_back(entry);
}
void MotionVectorRenderer::QueueVertexHistoryCopy(ID3D12Resource* currentVertices, ID3D12Resource* previousVertices) {
    copies_.push_back({currentVertices, previousVertices});
}
void MotionVectorRenderer::EndFrame(D3D12_CPU_DESCRIPTOR_HANDLE depthHandle) {
    active_ = nullptr;
    auto* commandList = DirectXCommon::GetInstance()->GetCommandList();
    bool hasMaskedDraws = false;
    for (const auto& entry : draws_) { if (entry.material.alphaCutoff > 0) { hasMaskedDraws = true; break; } }
    if (hasMaskedDraws) {
        uint64_t uploadSizeBytes = draws_.size() * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        if (!materialUpload_ || materialUpload_->GetDesc().Width < uploadSizeBytes) {
            materialUpload_ = DirectXCommon::GetInstance()->CreateBufferResource(uploadSizeBytes);
        }
        void* materialData = nullptr;
        CheckMotionVector(materialUpload_->Map(0, nullptr, &materialData));
        for (size_t index = 0; index < draws_.size(); ++index) {
            std::memcpy(static_cast<uint8_t*>(materialData) + index * 256, &draws_[index].material, sizeof(Material));
        }
        materialUpload_->Unmap(0, nullptr);
        SrvManager::GetInstance()->PreDraw();
    }
    if (!draws_.empty()) {
        uint64_t uploadSizeBytes = draws_.size() * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        if (!reprojectionUpload_ || reprojectionUpload_->GetDesc().Width < uploadSizeBytes) { reprojectionUpload_ = DirectXCommon::GetInstance()->CreateBufferResource(uploadSizeBytes); }
        struct ReprojectionParameters { Matrix4x4 previousWorldView; Matrix4x4 previousNormalTransform; Vector4 controls; };
        void* data = nullptr; CheckMotionVector(reprojectionUpload_->Map(0, nullptr, &data));
        for (size_t index = 0; index < draws_.size(); ++index) {
            const auto& entry = draws_[index]; ReprojectionParameters parameters = {};
            parameters.previousWorldView = entry.previousWorldView; parameters.previousNormalTransform = entry.previousNormalTransform;
            parameters.controls.x = static_cast<float>(entry.surfaceId);
            if (entry.hasPreviousGeometry) { parameters.controls.y = 1; }
            if (entry.material.normalMapEnabled != 0 && entry.material.normalMapStrength > 0) { parameters.controls.z = 1; }
            std::memcpy(static_cast<uint8_t*>(data) + index * 256, &parameters, sizeof(parameters));
        }
        reprojectionUpload_->Unmap(0, nullptr);
    }
    TransitionMotionResource(reprojectionTextures_[reprojectionIndex_].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionMotionResource(texture_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto target = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    const float clear[] = {0, 0, 0, 0};
    commandList->ClearRenderTargetView(target, clear, 0, nullptr);
    auto metadataTarget = target;
    metadataTarget.ptr += (reprojectionIndex_ + 1) * DirectXCommon::GetInstance()->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    commandList->ClearRenderTargetView(metadataTarget, clear, 0, nullptr);
    D3D12_CPU_DESCRIPTOR_HANDLE targets[] = {target, metadataTarget};
    commandList->OMSetRenderTargets(2, targets, FALSE, &depthHandle);
    commandList->SetGraphicsRootSignature(root_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (size_t index = 0; index < draws_.size(); ++index) {
        const auto& entry = draws_[index];
        bool isAlphaMasked = entry.material.alphaCutoff > 0;
        commandList->SetPipelineState(GetPipeline(entry.shaderPath, entry.isDoubleSided, isAlphaMasked));
        if (isAlphaMasked) {
            commandList->SetGraphicsRootConstantBufferView(5, materialUpload_->GetGPUVirtualAddress() + index * 256);
            commandList->SetGraphicsRootDescriptorTable(6, entry.textureSrv);
        }
        commandList->SetGraphicsRootConstantBufferView(7, reprojectionUpload_->GetGPUVirtualAddress() + index * 256);
        commandList->SetGraphicsRoot32BitConstants(0, 16, &entry.currentWorldViewProjection, 0);
        commandList->SetGraphicsRoot32BitConstants(1, 16, &entry.previousWorldViewProjection, 0);
        commandList->SetGraphicsRoot32BitConstants(2, 4, &entry.parameters, 0);
        commandList->SetGraphicsRoot32BitConstants(3, 4, &entry.previousParameters, 0);
        commandList->SetGraphicsRoot32BitConstants(4, 2, &entry.jitterNdc, 0);
        commandList->IASetVertexBuffers(0, 2, entry.vertices);
        if (entry.indexCount != 0) {
            commandList->IASetIndexBuffer(&entry.indices);
            commandList->DrawIndexedInstanced(entry.indexCount, 1, 0, 0, 0);
        } else { commandList->DrawInstanced(entry.vertexCount, 1, 0, 0); }
    }
    for (const auto& copy : copies_) {
        TransitionMotionResource(copy.currentVertices, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_SOURCE);
        TransitionMotionResource(copy.previousVertices, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->CopyResource(copy.previousVertices, copy.currentVertices);
        TransitionMotionResource(copy.currentVertices, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        TransitionMotionResource(copy.previousVertices, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    }
    TransitionMotionResource(texture_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    TransitionMotionResource(reprojectionTextures_[reprojectionIndex_].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
D3D12_GPU_DESCRIPTOR_HANDLE MotionVectorRenderer::GetSrvHandle() const {
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
}
D3D12_GPU_DESCRIPTOR_HANDLE MotionVectorRenderer::GetReprojectionSrv() const {
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(reprojectionSrvIndices_[reprojectionIndex_]);
}
D3D12_GPU_DESCRIPTOR_HANDLE MotionVectorRenderer::GetPreviousReprojectionSrv() const {
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(reprojectionSrvIndices_[1 - reprojectionIndex_]);
}
void MotionVectorRenderer::DrawDebug() {
    if (!settings_.isDebugVisible) { return; }
    SrvManager::GetInstance()->PreDraw();
    auto* commandList = DirectXCommon::GetInstance()->GetCommandList();
    commandList->SetGraphicsRootSignature(debugRoot_.Get());
    commandList->SetPipelineState(debugPipeline_.Get());
    commandList->SetGraphicsRootDescriptorTable(0, GetSrvHandle());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}

void MotionVectorRenderer::ResizeSceneTargets() {
    if (texture_ && texture_->GetDesc().Width == SceneRenderResolution::GetWidth() && texture_->GetDesc().Height == SceneRenderResolution::GetHeight()) { return; }
    ResetHistory(); reprojectionAllocationBytes_ = 0;
    auto* dx = DirectXCommon::GetInstance();
    auto* device = dx->GetDevice();
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC description = {};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = SceneRenderResolution::GetWidth();
    description.Height = SceneRenderResolution::GetHeight();
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_R16G16_FLOAT;
    description.SampleDesc.Count = 1;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = description.Format;
    CheckMotionVector(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&texture_)));
    texture_->SetName(L"MotionVector::UVDisplacement");
    rtvHeap_ = dx->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3, false);
    device->CreateRenderTargetView(texture_.Get(), nullptr, rtvHeap_->GetCPUDescriptorHandleForHeapStart());
    if (srvIndex_ == UINT_MAX) { srvIndex_ = SrvManager::GetInstance()->Allocate(); }
    SrvManager::GetInstance()->CreateSRVforTexture2D(srvIndex_, texture_.Get(), description.Format, 1);

    description.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; clear.Format = description.Format;
    uint32_t rtvIncrementBytes = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (uint32_t index = 0; index < 2; ++index) {
        CheckMotionVector(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&reprojectionTextures_[index])));
        auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart(); rtv.ptr += (index + 1) * rtvIncrementBytes;
        device->CreateRenderTargetView(reprojectionTextures_[index].Get(), nullptr, rtv);
        if (reprojectionSrvIndices_[index] == UINT_MAX) { reprojectionSrvIndices_[index] = SrvManager::GetInstance()->Allocate(); }
        SrvManager::GetInstance()->CreateSRVforTexture2D(reprojectionSrvIndices_[index], reprojectionTextures_[index].Get(), description.Format, 1);
        reprojectionAllocationBytes_ += device->GetResourceAllocationInfo(0, 1, &description).SizeInBytes;
    }
}
