#include "DlssSuperResolution.h"



#include "Engine/SrvManager/SrvManager.h"

#include "externals/DLSS/include/nvsdk_ngx_helpers.h"
#include <filesystem>
#include <stdexcept>

namespace {
constexpr D3D12_RESOURCE_STATES kShaderRead = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
void CheckSuperResolution(HRESULT result) {
    if (FAILED(result)) { throw std::runtime_error("Super resolution resource/pipeline creation failed"); }
}
void TransitionSuperResolution(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
    DirectXCommon::GetInstance()->GetCommandList()->ResourceBarrier(1, &barrier);
}
float HaltonSample(uint32_t index, uint32_t base) {
    float result = 0;
    float fraction = 1;
    while (index != 0) { fraction /= static_cast<float>(base); result += fraction * (index % base); index /= base; }
    return result;
}
Microsoft::WRL::ComPtr<ID3D12Resource> CreateSuperResolutionTexture(DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags) {
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC description = {};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = WinApp::kClientWidth;
    description.Height = WinApp::kClientHeight;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = format;
    description.SampleDesc.Count = 1;
    description.Flags = flags;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    CheckSuperResolution(DirectXCommon::GetInstance()->GetDevice()->CreateCommittedResource(&heap,
        D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&resource)));
    return resource;
}
}

DlssSuperResolution::~DlssSuperResolution() {
    if (isInitialized_) {
        Logger::Log("DLSS: releasing NGX resources"); Logger::Flush();
        auto* dx = DirectXCommon::GetInstance();
        dx->WaitForGPU();
        if (feature_ != nullptr) { NVSDK_NGX_D3D12_ReleaseFeature(feature_); }
        if (parameters_ != nullptr) { NVSDK_NGX_D3D12_DestroyParameters(parameters_); }
        Logger::Log("DLSS: shutting down NGX"); Logger::Flush();
        NVSDK_NGX_D3D12_Shutdown1(dx->GetDevice());
        Logger::Log("DLSS: NGX shutdown complete"); Logger::Flush();
    }
    auto* srvManager = SrvManager::GetInstance();
    if (outputSrvIndex_ != UINT_MAX) { srvManager->Free(outputSrvIndex_); }


}
void DlssSuperResolution::SetFailure(const char* operation, NVSDK_NGX_Result result) {
    status_ = std::string(operation) + " failed: NGX " + std::to_string(static_cast<unsigned int>(result));
    Logger::Log("DLSS: " + status_);
    isAvailable_ = false;
}
void DlssSuperResolution::Initialize() {
    const auto logDirectory = std::filesystem::absolute("runtime/logs/DLSS");
    std::filesystem::create_directories(logDirectory);
    // Use the existing project GUID; no NVIDIA-assigned application ID is fabricated.
    auto result = NVSDK_NGX_D3D12_Init_with_ProjectID("363657a6-518d-4d5c-90b2-3d50b381f493",
        NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", logDirectory.c_str(), DirectXCommon::GetInstance()->GetDevice());
    if (NVSDK_NGX_FAILED(result)) { SetFailure("Initialization", result); return; }
    isInitialized_ = true;
    result = NVSDK_NGX_D3D12_GetCapabilityParameters(&parameters_);
    if (NVSDK_NGX_FAILED(result)) { SetFailure("Capability query", result); return; }
    int isSupported = 0;
    parameters_->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &isSupported);
    if (isSupported == 0) { status_ = "DLSS Super Resolution unavailable on this GPU/driver"; Logger::Log(status_); return; }
    unsigned int maxWidth = 0, maxHeight = 0, minWidth = 0, minHeight = 0;
    float sharpness = 0;
    result = NGX_DLSS_GET_OPTIMAL_SETTINGS(parameters_, WinApp::kClientWidth, WinApp::kClientHeight,
        NVSDK_NGX_PerfQuality_Value_DLAA, &renderWidth_, &renderHeight_, &maxWidth, &maxHeight, &minWidth, &minHeight, &sharpness);
    if (NVSDK_NGX_FAILED(result)) { SetFailure("DLAA resolution query", result); return; }
    if (renderWidth_ == 0 || renderHeight_ == 0 || renderWidth_ > WinApp::kClientWidth || renderHeight_ > WinApp::kClientHeight) {
        status_ = "Invalid recommended render resolution"; return;
    }
    if (renderWidth_ != WinApp::kClientWidth || renderHeight_ != WinApp::kClientHeight) {
        status_ = "DLAA native resolution unavailable"; return;
    }
    CreateResources();
    isAvailable_ = true;
    status_ = "Available: DLAA " + std::to_string(renderWidth_) + "x" + std::to_string(renderHeight_) + " -> "
        + std::to_string(WinApp::kClientWidth) + "x" + std::to_string(WinApp::kClientHeight);
    Logger::Log("DLSS: " + status_);
}

void DlssSuperResolution::CreateResources() {
    output_ = CreateSuperResolutionTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    output_->SetName(L"DLAA::Output");
    auto* srvManager = SrvManager::GetInstance();
    outputSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforTexture2D(outputSrvIndex_, output_.Get(), DXGI_FORMAT_R16G16B16A16_FLOAT, 1);
}
bool DlssSuperResolution::EnsureFeature() {
    if (feature_ != nullptr) { return true; }
    NVSDK_NGX_DLSS_Create_Params create = {};
    create.Feature.InWidth = renderWidth_; create.Feature.InHeight = renderHeight_;
    create.Feature.InTargetWidth = WinApp::kClientWidth; create.Feature.InTargetHeight = WinApp::kClientHeight;
    create.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_DLAA;
    create.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    auto result = NGX_D3D12_CREATE_DLSS_EXT(DirectXCommon::GetInstance()->GetCommandList(), 1, 1, &feature_, parameters_, &create);
    if (NVSDK_NGX_FAILED(result)) { SetFailure("Feature creation", result); return false; }
    return true;
}
void DlssSuperResolution::BeginFrame(const SuperResolutionHistoryInputs& inputs) {
    bool wasActive = isActive_;
    isActive_ = false;
    if (!inputs.hasCamera || !isEnabled_ || !isAvailable_ || !EnsureFeature()) {

        shouldResetHistory_ = true; return;
    }
    isActive_ = true;
    if (!wasActive || historyCameraId_ != inputs.cameraId || sceneRevision_ != inputs.sceneRevision || cameraHistoryId_ != inputs.cameraHistoryId) { shouldResetHistory_ = true; jitterFrame_ = 0; }
    historyCameraId_ = inputs.cameraId;
    sceneRevision_ = inputs.sceneRevision; cameraHistoryId_ = inputs.cameraHistoryId;
    uint32_t sample = (jitterFrame_++ % 32) + 1;
    jitterPixels_ = {HaltonSample(sample, 2) - 0.5f, HaltonSample(sample, 3) - 0.5f};

}
Vector2 DlssSuperResolution::GetProjectionJitterNdc() const {
    if (!isActive_) { return {}; }
    return {2 * jitterPixels_.x / renderWidth_, -2 * jitterPixels_.y / renderHeight_};
}
void DlssSuperResolution::SetSceneViewport() {
    if (!isActive_) { return; }
    D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(renderWidth_), static_cast<float>(renderHeight_), 0, 1};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(renderWidth_), static_cast<LONG>(renderHeight_)};
    auto* commandList = DirectXCommon::GetInstance()->GetCommandList();
    commandList->RSSetViewports(1, &viewport); commandList->RSSetScissorRects(1, &scissor);
}
D3D12_GPU_DESCRIPTOR_HANDLE DlssSuperResolution::Evaluate(const SuperResolutionFrameInputs& inputs) {
    if (!isActive_) { return inputs.colorSrv; }
    if (inputs.colorTexture == nullptr || inputs.depthTexture == nullptr || inputs.motionVectorTexture == nullptr || inputs.colorSrv.ptr == 0) {
        throw std::invalid_argument("DLAA requires color, depth and motion inputs");
    }
    auto* commandList = DirectXCommon::GetInstance()->GetCommandList();
    TransitionSuperResolution(inputs.colorTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, kShaderRead);
    TransitionSuperResolution(inputs.depthTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, kShaderRead);
    TransitionSuperResolution(inputs.motionVectorTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, kShaderRead);
    TransitionSuperResolution(output_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    NVSDK_NGX_D3D12_DLSS_Eval_Params evaluate = {};
    evaluate.Feature.pInColor = inputs.colorTexture; evaluate.Feature.pInOutput = output_.Get();
    evaluate.pInDepth = inputs.depthTexture; evaluate.pInMotionVectors = inputs.motionVectorTexture;
    evaluate.InRenderSubrectDimensions = {renderWidth_, renderHeight_};
    evaluate.InJitterOffsetX = jitterPixels_.x; evaluate.InJitterOffsetY = jitterPixels_.y;
    evaluate.InMVScaleX = -static_cast<float>(renderWidth_); evaluate.InMVScaleY = -static_cast<float>(renderHeight_);
    evaluate.InPreExposure = 1; evaluate.InExposureScale = 1;
    evaluate.InFrameTimeDeltaInMsec = inputs.frameTimeDeltaMs;
    if (shouldResetHistory_) { evaluate.InReset = 1; }
    auto result = NGX_D3D12_EVALUATE_DLSS_EXT(commandList, feature_, parameters_, &evaluate);
    if (NVSDK_NGX_SUCCEED(result)) {
        ++evaluationCount_; shouldResetHistory_ = false;
        TransitionSuperResolution(output_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    } else {
        SetFailure("Evaluation", result); shouldResetHistory_ = true;
        // Native-resolution fallback retains the already composited scene.
        TransitionSuperResolution(inputs.colorTexture, kShaderRead, D3D12_RESOURCE_STATE_COPY_SOURCE);
        TransitionSuperResolution(output_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->CopyResource(output_.Get(), inputs.colorTexture);
        TransitionSuperResolution(output_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        TransitionSuperResolution(inputs.colorTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, kShaderRead);
    }
    TransitionSuperResolution(inputs.colorTexture, kShaderRead, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    TransitionSuperResolution(inputs.depthTexture, kShaderRead, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    TransitionSuperResolution(inputs.motionVectorTexture, kShaderRead, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    SrvManager::GetInstance()->PreDraw();
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(outputSrvIndex_);
}
