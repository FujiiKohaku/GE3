#include "VolumetricLightRenderer.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Shadow/ShadowMapRenderer.h"
#include "Engine/Light/LightManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/math/MatrixMath.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
namespace {
void CheckVolume(HRESULT result) {
    if (FAILED(result)) { throw std::runtime_error("Volumetric light resource/pipeline creation failed"); }
}
bool FiniteMatrix(const Matrix4x4& matrix) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (!std::isfinite(matrix.m[row][column])) { return false; }
        }
    }
    return true;
}
float SafeVolumeValue(float value, float minimum, float maximum, float fallback) {
    if (!std::isfinite(value)) { return fallback; }
    return std::clamp(value, minimum, maximum);
}
Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateVolumePipeline(DirectXCommon* dxCommon_, ID3D12RootSignature* rootSignature, const std::wstring& pixelShaderPath)
{
    ID3D12Device* device = dxCommon_->GetDevice();

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->LoadCompiledShader(pixelShaderPath);

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = false;
    depthStencilDesc.StencilEnable = false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
    pipelineStateDesc.pRootSignature = rootSignature;
    pipelineStateDesc.VS.pShaderBytecode = vertexShaderBlob->GetBufferPointer();
    pipelineStateDesc.VS.BytecodeLength = vertexShaderBlob->GetBufferSize();
    pipelineStateDesc.PS.pShaderBytecode = pixelShaderBlob->GetBufferPointer();
    pipelineStateDesc.PS.BytecodeLength = pixelShaderBlob->GetBufferSize();
    pipelineStateDesc.BlendState = blendDesc;
    pipelineStateDesc.RasterizerState = rasterizerDesc;
    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineStateDesc.InputLayout.NumElements = 0;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT result = device->CreateGraphicsPipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&pipelineState));
    CheckVolume(result);

    return pipelineState;
}


}
VolumetricLightRenderer::~VolumetricLightRenderer() {
    if (srvIndex_ != 0xffffffffu) { SrvManager::GetInstance()->Free(srvIndex_); }
}
bool VolumetricLightRenderer::Initialize(DirectXCommon* dxCommon) {
    if (dxCommon == nullptr) { return false; }
    dxCommon_ = dxCommon;
    try {
        CreateResources();
        CreatePipelines();
        ready_ = true;
    } catch (const std::exception& error) {
        Logger::Error(error.what());
        ready_ = false;
    }
    return ready_;
}
void VolumetricLightRenderer::SetLightColor(const Vector3& color) {
    parameters_.lightColor = { SafeVolumeValue(color.x, 0.0f, 4.0f, 1.0f),
        SafeVolumeValue(color.y, 0.0f, 4.0f, 1.0f), SafeVolumeValue(color.z, 0.0f, 4.0f, 1.0f) };
}
void VolumetricLightRenderer::SetLightIntensity(float value) { parameters_.lightIntensity = SafeVolumeValue(value, 0.0f, 4.0f, 0.0f); }
void VolumetricLightRenderer::SetFogDensity(float value) { parameters_.fogDensity = SafeVolumeValue(value, 0.0f, 0.05f, 0.0f); }
void VolumetricLightRenderer::SetMaxDistance(float value) { parameters_.maxDistance = SafeVolumeValue(value, 1.0f, 1000.0f, 240.0f); }
void VolumetricLightRenderer::SetAnisotropy(float value) { parameters_.anisotropy = SafeVolumeValue(value, -0.8f, 0.8f, 0.0f); }
void VolumetricLightRenderer::SetSampleCount(int32_t samples) { parameters_.sampleCount = std::clamp(samples, 8, 64); }
void VolumetricLightRenderer::SetLightDirection(const Vector3& direction) {
    float length = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    if (!std::isfinite(length) || length < 0.000001f) { return; }
    if (!LightManager::GetInstance()->IsInitialized()) { return; }
    float inverseLength = 1.0f / std::sqrt(length);
    LightManager::GetInstance()->SetDirection({ direction.x * inverseLength, direction.y * inverseLength, direction.z * inverseLength });
    // The next shadow pass must use the changed direction before this effect can run.
    frameValid_ = false;
}
void VolumetricLightRenderer::SetFrameInputs(const Camera* camera, const ShadowMapRenderer* shadows) {
    frameValid_ = false;
    generated_ = false;
    shadowSrv_ = {};
    if (!ready_ || camera == nullptr || shadows == nullptr || !shadows->IsReadyForSampling()) { return; }
    if (!LightManager::GetInstance()->IsInitialized()) { return; }
    const auto light = LightManager::GetInstance()->GetDirectionalLight();
    const auto direction = shadows->GetLightDirection();
    const float sunLength = light.direction.x * light.direction.x + light.direction.y * light.direction.y + light.direction.z * light.direction.z;
    if (!std::isfinite(sunLength) || sunLength < 0.000001f) { return; }
    float length = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    if (!std::isfinite(length) || length < 0.000001f || !std::isfinite(light.intensity)) { return; }
    const float alignment = (direction.x * light.direction.x + direction.y * light.direction.y + direction.z * light.direction.z) / std::sqrt(length * sunLength);
    if (!std::isfinite(alignment) || alignment < 0.9999f) { return; }
    if (camera->GetNearClip() <= 0.0f || camera->GetFarClip() <= camera->GetNearClip()) { return; }
    const auto& world = camera->GetWorldMatrix();
    const auto& projection = camera->GetProjectionMatrix();
    if (!FiniteMatrix(world) || !FiniteMatrix(projection) ||
        !FiniteMatrix(camera->GetViewProjectionMatrix()) || !FiniteMatrix(shadows->GetLightViewProjection())) { return; }
    // Validate the camera basis without its translation. A translated VP inverse
    // loses precision and falsely toggles this effect as the camera moves.
    const double determinant =
        double(world.m[0][0]) * (double(world.m[1][1]) * world.m[2][2] - double(world.m[1][2]) * world.m[2][1]) -
        double(world.m[0][1]) * (double(world.m[1][0]) * world.m[2][2] - double(world.m[1][2]) * world.m[2][0]) +
        double(world.m[0][2]) * (double(world.m[1][0]) * world.m[2][1] - double(world.m[1][1]) * world.m[2][0]);
    if (!std::isfinite(determinant) || std::abs(determinant) < 0.00000001) { return; }
    const auto inverseProjection = MatrixMath::Inverse(projection);
    if (!FiniteMatrix(inverseProjection)) { return; }
    const auto identity = MatrixMath::Multiply(projection, inverseProjection);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float expected = 0.0f;
            if (row == column) { expected = 1.0f; }
            if (!std::isfinite(identity.m[row][column]) || std::abs(identity.m[row][column] - expected) > 0.001f) { return; }
        }
    }
    Matrix4x4 relativeWorld = world;
    relativeWorld.m[3][0] = 0.0f; relativeWorld.m[3][1] = 0.0f; relativeWorld.m[3][2] = 0.0f;
    frameConstants_.inverseViewProjection = MatrixMath::Multiply(inverseProjection, relativeWorld);
    if (!FiniteMatrix(frameConstants_.inverseViewProjection)) { return; }
    frameConstants_.lightViewProjection = shadows->GetLightViewProjection();
    frameConstants_.cameraAndDistance = { world.m[3][0], world.m[3][1], world.m[3][2], parameters_.maxDistance };
    float inverseLength = 1.0f / std::sqrt(length);
    frameConstants_.lightDirectionAndDensity = { direction.x * inverseLength, direction.y * inverseLength, direction.z * inverseLength, parameters_.fogDensity };
    frameConstants_.lightColorAndIntensity = {
        SafeVolumeValue(light.color.x, 0.0f, 8.0f, 0.0f) * parameters_.lightColor.x,
        SafeVolumeValue(light.color.y, 0.0f, 8.0f, 0.0f) * parameters_.lightColor.y,
        SafeVolumeValue(light.color.z, 0.0f, 8.0f, 0.0f) * parameters_.lightColor.z,
        SafeVolumeValue(light.intensity, 0.0f, 8.0f, 0.0f) * parameters_.lightIntensity };
    frameConstants_.settings = { parameters_.anisotropy, static_cast<float>(parameters_.sampleCount), SafeVolumeValue(shadows->GetDepthBias(), 0.0f, 0.01f, 0.00025f), 0.0f };
    shadowSrv_ = shadows->GetSrv();
    frameValid_ = shadowSrv_.ptr != 0;
}
void VolumetricLightRenderer::CreateResources() {
    auto* device = dxCommon_->GetDevice();
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heapDesc.NumDescriptors = 1;
    CheckVolume(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap_)));
    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = WinApp::kClientWidth / 2; desc.Height = WinApp::kClientHeight / 2;
    desc.DepthOrArraySize = 1; desc.MipLevels = 1; desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc.Count = 1; desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heap {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_CLEAR_VALUE clear {}; clear.Format = desc.Format;
    CheckVolume(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, textureState_, &clear, IID_PPV_ARGS(&volumeTexture_)));
    volumeTexture_->SetName(L"VolumetricLight::HalfResolutionScattering");
    device->CreateRenderTargetView(volumeTexture_.Get(), nullptr, rtvHeap_->GetCPUDescriptorHandleForHeapStart());
    if (!SrvManager::GetInstance()->CanAllocate()) { throw std::runtime_error("Volumetric light SRV heap full"); }
    srvIndex_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVforTexture2D(srvIndex_, volumeTexture_.Get(), desc.Format, 1);
    volumeSrv_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
    D3D12_HEAP_PROPERTIES uploadHeap {}; uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    auto buffer = CD3DX12_RESOURCE_DESC::Buffer((sizeof(Constants) + 255) & ~size_t(255));
    CheckVolume(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantsResource_)));
    CheckVolume(constantsResource_->Map(0, nullptr, reinterpret_cast<void**>(&constantsData_)));
}
void VolumetricLightRenderer::CreatePipelines() {
    D3D12_DESCRIPTOR_RANGE ranges[3] {};
    D3D12_ROOT_PARAMETER root[4] {};
    for (uint32_t index = 0; index < 3; ++index) {
        ranges[index].BaseShaderRegister = index; ranges[index].NumDescriptors = 1;
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        root[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        root[index].DescriptorTable = { 1, &ranges[index] };
    }
    root[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    root[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; root[3].Descriptor.ShaderRegister = 0;
    D3D12_STATIC_SAMPLER_DESC samplers[2] {};
    for (uint32_t index = 0; index < 2; ++index) {
        samplers[index].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[index].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[index].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[index].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[index].ShaderRegister = index; samplers[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplers[index].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    }
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    D3D12_ROOT_SIGNATURE_DESC desc {};
    desc.NumParameters = 4; desc.pParameters = root;
    desc.NumStaticSamplers = 2; desc.pStaticSamplers = samplers;
    Microsoft::WRL::ComPtr<ID3DBlob> blob, errors;
    CheckVolume(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors));
    CheckVolume(dxCommon_->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_)));
    raymarchPipeline_ = CreateVolumePipeline(dxCommon_, rootSignature_.Get(), L"resources/Shaders/PostEffect/Volumetric/Raymarch.PS.hlsl");
    compositePipeline_ = CreateVolumePipeline(dxCommon_, rootSignature_.Get(), L"resources/Shaders/PostEffect/Volumetric/Composite.PS.hlsl");
}
void VolumetricLightRenderer::Transition(D3D12_RESOURCE_STATES state) {
    if (textureState_ == state) { return; }
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(volumeTexture_.Get(), textureState_, state);
    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    textureState_ = state;
}
void VolumetricLightRenderer::Draw(ID3D12PipelineState* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE color,
    D3D12_GPU_DESCRIPTOR_HANDLE depth, D3D12_GPU_DESCRIPTOR_HANDLE shadow) {
    auto* cmd = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetGraphicsRootSignature(rootSignature_.Get()); cmd->SetPipelineState(pipeline);
    cmd->SetGraphicsRootDescriptorTable(0, color); cmd->SetGraphicsRootDescriptorTable(1, depth);
    cmd->SetGraphicsRootDescriptorTable(2, shadow);
    cmd->SetGraphicsRootConstantBufferView(3, constantsResource_->GetGPUVirtualAddress());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(3, 1, 0, 0);
}
bool VolumetricLightRenderer::Generate(D3D12_GPU_DESCRIPTOR_HANDLE depthHandle, bool depthReady) {
    generated_ = false;
    bool valid = frameValid_;
    frameValid_ = false; // Consume the frame; a missing update never reuses old shadows.
    if (!ready_ || !valid || !depthReady || depthHandle.ptr == 0 || !parameters_.enabled ||
        parameters_.fogDensity <= 0.0f || parameters_.lightIntensity <= 0.0f) { return false; }
    if (frameConstants_.lightColorAndIntensity.w <= 0.0f) { return false; }
    *constantsData_ = frameConstants_;
    depthSrv_ = depthHandle;
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto* cmd = dxCommon_->GetCommandList();
    D3D12_VIEWPORT viewport { 0, 0, float(WinApp::kClientWidth / 2), float(WinApp::kClientHeight / 2), 0, 1 };
    D3D12_RECT scissor { 0, 0, WinApp::kClientWidth / 2, WinApp::kClientHeight / 2 };
    cmd->RSSetViewports(1, &viewport); cmd->RSSetScissorRects(1, &scissor);
    auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    Draw(raymarchPipeline_.Get(), depthHandle, depthHandle, shadowSrv_);
    Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    generated_ = true;
    return true;
}
void VolumetricLightRenderer::Composite(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle) {
    if (!generated_ || sceneColorHandle.ptr == 0) { return; }
    D3D12_VIEWPORT viewport { 0, 0, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0, 1 };
    D3D12_RECT scissor { 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };
    auto* cmd = dxCommon_->GetCommandList();
    cmd->RSSetViewports(1, &viewport); cmd->RSSetScissorRects(1, &scissor);
    Draw(compositePipeline_.Get(), sceneColorHandle, depthSrv_, volumeSrv_);
    generated_ = false;
}
void VolumetricLightRenderer::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Begin("Volumetric Light");
    bool enabled = parameters_.enabled;
    if (ImGui::Checkbox("Enabled", &enabled)) { SetEnabled(enabled); }
    Vector3 color = parameters_.lightColor;
    if (ImGui::ColorEdit3("Light Tint", &color.x)) { SetLightColor(color); }
    float intensity = parameters_.lightIntensity;
    if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 2.0f)) { SetLightIntensity(intensity); }
    float density = parameters_.fogDensity;
    if (ImGui::SliderFloat("Density", &density, 0.0f, 0.02f, "%.4f")) { SetFogDensity(density); }
    float distance = parameters_.maxDistance;
    if (ImGui::SliderFloat("Distance", &distance, 1.0f, 1000.0f)) { SetMaxDistance(distance); }
    float anisotropy = parameters_.anisotropy;
    if (ImGui::SliderFloat("Anisotropy", &anisotropy, -0.8f, 0.8f)) { SetAnisotropy(anisotropy); }
    int samples = parameters_.sampleCount;
    if (ImGui::SliderInt("Samples", &samples, 8, 64)) { SetSampleCount(samples); }
    ImGui::Text("Uses scene sun direction and current-frame shadows.");
    ImGui::End();
#endif
}
