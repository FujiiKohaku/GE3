#include "ParticleManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/math/MathStruct.h"
#include <cassert>
#include <cmath>
#include <numbers>

std::unique_ptr<ParticleManager> ParticleManager::instance_ = nullptr;

ParticleManager::ParticleManager(ConstructorKey)
{
}

ParticleManager* ParticleManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<ParticleManager>(ConstructorKey());
    }
    return instance_.get();
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    camera_ = camera;

    particleRenderManager_ = std::make_unique<ParticleRenderManager>();
    particleRenderManager_->Initialize(dxCommon_);

    particleMeshManager_ = std::make_unique<ParticleMeshManager>();
    particleMeshManager_->Initialize(dxCommon_);

    particleEmitter_ = std::make_unique<ParticleEmitter>();
    particleEmitter_->Initialize();

    CreateMaterialResource();
    CreatePerViewResource();

    InitializeGPUParticle();
}
void ParticleManager::InitializeGPUParticle()
{
    CreateGPUParticleResource();
    CreateGPUParticleUAV();
    CreateGPUParticleSRV();
    // GPUパーティクルの初期化に必要なリソースを作成
    CreateFreeCounterResource();
    CreateEmitterResource();
    CreatePerFrameResource();

    CreateGPUParticleInitializeRootSignature();
    CreateGPUParticleInitializePipeline();
    DispatchInitializeGPUParticle();

    CreateEmitParticleRootSignature();
    CreateEmitParticlePipeline();

    CreateUpdateParticleRootSignature();
    CreateUpdateParticlePipeline();
}

#pragma region GPUパーティクル関連

void ParticleManager::DispatchInitializeGPUParticle()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        srvManager_->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetComputeRootSignature(
        gpuParticleInitializeRootSignature_.Get());

    commandList->SetPipelineState(
        gpuParticleInitializePipelineState_.Get());

    commandList->SetComputeRootDescriptorTable(
        0,
        gpuParticleUavHandleGPU_);

    commandList->Dispatch(1, 1, 1);
}
void ParticleManager::CreateGPUParticleResource()
{
    uint32_t bufferSize = sizeof(ParticleCS) * kMaxGPUParticle;

    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&gpuParticleResource_));

    assert(SUCCEEDED(hr));

    gpuParticleResource_->SetName(L"ParticleManager::GPUParticleResource");
}

void ParticleManager::CreateGPUParticleUAV()
{
    gpuParticleUavIndex_ = srvManager_->Allocate();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = kMaxGPUParticle;
    uavDesc.Buffer.StructureByteStride = sizeof(ParticleCS);

    dxCommon_->GetDevice()->CreateUnorderedAccessView(
        gpuParticleResource_.Get(),
        nullptr,
        &uavDesc,
        srvManager_->GetCPUDescriptorHandle(gpuParticleUavIndex_));

    gpuParticleUavHandleGPU_ = srvManager_->GetGPUDescriptorHandle(gpuParticleUavIndex_);
}

void ParticleManager::CreateGPUParticleSRV()
{
    gpuParticleSrvIndex_ = srvManager_->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = kMaxGPUParticle;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleCS);

    dxCommon_->GetDevice()->CreateShaderResourceView(
        gpuParticleResource_.Get(),
        &srvDesc,
        srvManager_->GetCPUDescriptorHandle(gpuParticleSrvIndex_));

    gpuParticleSrvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(gpuParticleSrvIndex_);
}
void ParticleManager::CreateGPUParticleInitializeRootSignature()
{
    D3D12_DESCRIPTOR_RANGE uavRange {};
    uavRange.BaseShaderRegister = 0;
    uavRange.NumDescriptors = 1;
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[1] {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&gpuParticleInitializeRootSignature_));

    assert(SUCCEEDED(hr));
}

void ParticleManager::CreatePerViewResource()
{
    perViewResource_ = dxCommon_->CreateBufferResource(sizeof(PerView));
    perViewResource_->SetName(L"ParticleManager::PerViewCB");

    perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

    perViewData_->viewProjection = MatrixMath::MakeIdentity4x4();
    perViewData_->billboardMatrix = MatrixMath::MakeIdentity4x4();
}
void ParticleManager::CreateGPUParticleInitializePipeline()
{
    Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(L"resources/shaders/InitializeParticle.CS.hlsl", L"cs_6_0");

    assert(computeShaderBlob);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc {};
    pipelineStateDesc.pRootSignature = gpuParticleInitializeRootSignature_.Get();
    pipelineStateDesc.CS.pShaderBytecode = computeShaderBlob->GetBufferPointer();
    pipelineStateDesc.CS.BytecodeLength = computeShaderBlob->GetBufferSize();

    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&gpuParticleInitializePipelineState_));

    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateEmitterResource()
{
    emitterResource_ = dxCommon_->CreateBufferResource(sizeof(EmitterSphere));
    emitterResource_->SetName(L"ParticleManager::EmitterSphere");

    emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterData_));

    emitterData_->count = 10;
    emitterData_->frequency = 0.5f;
    emitterData_->frequencyTime = 0.0f;
    emitterData_->translate = { 0.0f, 0.0f, 0.0f };
    emitterData_->radius = 1.0f;
    emitterData_->emit = 0;
}

void ParticleManager::UpdateEmitter()
{
    emitterData_->frequencyTime += deltaTime_;

    if (emitterData_->frequency <= emitterData_->frequencyTime) {
        emitterData_->frequencyTime -= emitterData_->frequency;
        emitterData_->emit = 1;
    } else {
        emitterData_->emit = 0;
    }
}
void ParticleManager::DispatchEmitParticle()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        srvManager_->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetComputeRootSignature(emitParticleRootSignature_.Get());
    commandList->SetPipelineState(emitParticlePipelineState_.Get());

    // [0] EmitterSphere : b0
    commandList->SetComputeRootConstantBufferView(
        0,
        emitterResource_->GetGPUVirtualAddress());

    // [1] Particle UAV : u0
    commandList->SetComputeRootDescriptorTable(
        1,
        gpuParticleUavHandleGPU_);

    // [2] FreeCounter UAV : u1
    commandList->SetComputeRootDescriptorTable(
        2,
        freeCounterUavHandleGPU_);

    // [3] PerFrame : b1
    commandList->SetComputeRootConstantBufferView(
        3,
        perFrameResource_->GetGPUVirtualAddress());

    uint32_t dispatchCount = (emitterData_->count + 255) / 256;
    commandList->Dispatch(dispatchCount, 1, 1);
}
void ParticleManager::CreateEmitParticleRootSignature()
{
    D3D12_DESCRIPTOR_RANGE particleUavRange {};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE counterUavRange {};
    counterUavRange.BaseShaderRegister = 1;
    counterUavRange.NumDescriptors = 1;
    counterUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    counterUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] {};

    // [0] EmitterSphere : b0
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] Particle UAV : u0
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    // [2] FreeCounter UAV : u1
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &counterUavRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // [3] PerFrame : b1
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&emitParticleRootSignature_));

    assert(SUCCEEDED(hr));
}
void ParticleManager::CreateEmitParticlePipeline()
{
    Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(
        L"resources/shaders/EmitParticle.CS.hlsl",
        L"cs_6_0");

    assert(computeShaderBlob);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc {};
    pipelineStateDesc.pRootSignature = emitParticleRootSignature_.Get();
    pipelineStateDesc.CS.pShaderBytecode = computeShaderBlob->GetBufferPointer();
    pipelineStateDesc.CS.BytecodeLength = computeShaderBlob->GetBufferSize();

    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&emitParticlePipelineState_));

    assert(SUCCEEDED(hr));
}

void ParticleManager::CreatePerFrameResource()
{
    perFrameResource_ = dxCommon_->CreateBufferResource(sizeof(PerFrame));
    perFrameResource_->SetName(L"ParticleManager::PerFrameCB");

    perFrameResource_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));

    perFrameData_->time = 0.0f;
    perFrameData_->deltaTime = deltaTime_;
}

void ParticleManager::UpdatePerFrame()
{
    time_ += deltaTime_;

    perFrameData_->time = time_;
    perFrameData_->deltaTime = deltaTime_;
}

void ParticleManager::CreateFreeCounterResource()
{
    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeof(int32_t);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&freeCounterResource_));

    assert(SUCCEEDED(hr));

    freeCounterResource_->SetName(L"ParticleManager::FreeCounter");

    freeCounterUavIndex_ = srvManager_->Allocate();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 1;
    uavDesc.Buffer.StructureByteStride = sizeof(int32_t);

    dxCommon_->GetDevice()->CreateUnorderedAccessView(
        freeCounterResource_.Get(),
        nullptr,
        &uavDesc,
        srvManager_->GetCPUDescriptorHandle(freeCounterUavIndex_));

    freeCounterUavHandleGPU_ = srvManager_->GetGPUDescriptorHandle(freeCounterUavIndex_);
}

void ParticleManager::CreateUpdateParticleRootSignature()
{
    D3D12_DESCRIPTOR_RANGE uavRange {};
    uavRange.BaseShaderRegister = 0;
    uavRange.NumDescriptors = 1;
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2] {};

    // [0] Particle UAV : u0
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    // [1] PerFrame : b0
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&updateParticleRootSignature_));

    assert(SUCCEEDED(hr));
}
void ParticleManager::CreateUpdateParticlePipeline()
{
    Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(
        L"resources/shaders/UpdateParticle.CS.hlsl",
        L"cs_6_0");

    assert(computeShaderBlob);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc {};
    pipelineStateDesc.pRootSignature = updateParticleRootSignature_.Get();
    pipelineStateDesc.CS.pShaderBytecode = computeShaderBlob->GetBufferPointer();
    pipelineStateDesc.CS.BytecodeLength = computeShaderBlob->GetBufferSize();

    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&updateParticlePipelineState_));

    assert(SUCCEEDED(hr));
}
void ParticleManager::DispatchUpdateParticle()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        srvManager_->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetComputeRootSignature(updateParticleRootSignature_.Get());
    commandList->SetPipelineState(updateParticlePipelineState_.Get());

    // [0] Particle UAV : u0
    commandList->SetComputeRootDescriptorTable(
        0,
        gpuParticleUavHandleGPU_);

    // [1] PerFrame : b0
    commandList->SetComputeRootConstantBufferView(
        1,
        perFrameResource_->GetGPUVirtualAddress());

    uint32_t dispatchCount = (kMaxGPUParticle + 255) / 256;
    commandList->Dispatch(dispatchCount, 1, 1);
}
//=============================
/// GPUパーティクル関連最後
//=============================
#pragma endregion

void ParticleManager::Update()
{
    if (useGPUParticle_) {
        UpdateEmitter();
        UpdatePerFrame();

        DispatchEmitParticle();

        ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = gpuParticleResource_.Get();

        commandList->ResourceBarrier(1, &barrier);

        DispatchUpdateParticle();
    }
    Matrix4x4 cameraMatrix = camera_->GetWorldMatrix();
    cameraMatrix.m[3][0] = 0.0f;
    cameraMatrix.m[3][1] = 0.0f;
    cameraMatrix.m[3][2] = 0.0f;

    Matrix4x4 billboardMatrix = cameraMatrix;
    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();

    perViewData_->billboardMatrix = billboardMatrix;
    perViewData_->viewProjection = viewProjectionMatrix;
    for (auto& particleGroupPair : particleGroups_) {
        ParticleGroup& particleGroup = particleGroupPair.second;
        particleGroup.numInstance = 0;

        for (std::list<Particle>::iterator particleIterator = particleGroup.particles.begin();
            particleIterator != particleGroup.particles.end();) {

            Particle& particle = *particleIterator;

            if (particle.currentTime >= particle.lifeTime) {
                particleIterator = particleGroup.particles.erase(particleIterator);
                continue;
            }

            particle.currentTime += deltaTime_;
            particle.transform.translate += particle.velocity * deltaTime_;

            float alpha = 1.0f - (particle.currentTime / particle.lifeTime);

            Matrix4x4 worldMatrix;
            if (useBillboard_) {
                Matrix4x4 scaleMatrix = MatrixMath::Matrix4x4MakeScaleMatrix(particle.transform.scale);
                Matrix4x4 rotateMatrix = MatrixMath::MakeRotateZMatrix(particle.transform.rotate.z);
                Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(particle.transform.translate);

                worldMatrix = MatrixMath::Multiply(scaleMatrix, rotateMatrix);
                worldMatrix = MatrixMath::Multiply(worldMatrix, billboardMatrix);
                worldMatrix = MatrixMath::Multiply(worldMatrix, translateMatrix);
            } else {
                worldMatrix = MatrixMath::MakeAffineMatrix(
                    particle.transform.scale,
                    particle.transform.rotate,
                    particle.transform.translate);
            }

            Matrix4x4 wvpMatrix = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);

            if (particleGroup.numInstance < kNumMaxInstance) {
                particleGroup.instanceData[particleGroup.numInstance].World = worldMatrix;
                particleGroup.instanceData[particleGroup.numInstance].WVP = wvpMatrix;
                particleGroup.instanceData[particleGroup.numInstance].color = particle.color;
                particleGroup.instanceData[particleGroup.numInstance].color.w = alpha;
                particleGroup.numInstance++;
            }

            ++particleIterator;
        }
    }
}

void ParticleManager::PreDraw()
{
    particleRenderManager_->PreDraw(currentBlendMode_);
}

void ParticleManager::Draw()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetGraphicsRootConstantBufferView(
        0,
        materialResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        3,
        perViewResource_->GetGPUVirtualAddress());

    for (auto& particleGroupPair : particleGroups_) {
        ParticleGroup& particleGroup = particleGroupPair.second;

        const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView = particleMeshManager_->GetVertexBufferView(particleGroup.meshType);

        const D3D12_INDEX_BUFFER_VIEW& indexBufferView = particleMeshManager_->GetIndexBufferView(particleGroup.meshType);

        uint32_t indexCount = particleMeshManager_->GetIndexCount(particleGroup.meshType);

        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->IASetIndexBuffer(&indexBufferView);

        commandList->SetGraphicsRootDescriptorTable(
            1,
            gpuParticleSrvHandleGPU_);

        commandList->SetGraphicsRootDescriptorTable(
            2,
            TextureManager::GetInstance()->GetSrvHandleGPU(particleGroup.texturePath));

        commandList->DrawIndexedInstanced(
            indexCount,
            kMaxGPUParticle,
            0,
            0,
            0);
    }
}
void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath, ParticleMeshManager::ParticleMeshType meshType)
{
    assert(particleGroups_.find(name) == particleGroups_.end());

    ParticleGroup particleGroup {};
    particleGroup.texturePath = textureFilePath;
    particleGroup.meshType = meshType;

    TextureManager::GetInstance()->LoadTexture(textureFilePath);

    particleGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);
    particleGroup.instancingResource->SetName(
        (L"ParticleManager::InstancingBuffer_" + StringUtility::ConvertString(name)).c_str());

    particleGroup.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&particleGroup.instanceData));
    particleGroup.numInstance = 0;

    uint32_t srvIndex = srvManager_->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc {};
    shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    shaderResourceViewDesc.Buffer.FirstElement = 0;
    shaderResourceViewDesc.Buffer.NumElements = kNumMaxInstance;
    shaderResourceViewDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

    dxCommon_->GetDevice()->CreateShaderResourceView(
        particleGroup.instancingResource.Get(),
        &shaderResourceViewDesc,
        srvManager_->GetCPUDescriptorHandle(srvIndex));

    particleGroup.instancingSrvHandleGPU = srvManager_->GetGPUDescriptorHandle(srvIndex);

    particleGroups_.emplace(name, std::move(particleGroup));
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count)
{
    ParticleGroup& particleGroup = particleGroups_.at(name);

    for (uint32_t particleIndex = 0; particleIndex < count; particleIndex++) {
        if (particleGroup.particles.size() > 1000)
            return;
        particleGroup.particles.push_back(particleEmitter_->MakeNewParticleAttack(position));
    }
}

void ParticleManager::EmitFire(const std::string& name, const Vector3& position, uint32_t count)
{
    ParticleGroup& particleGroup = particleGroups_.at(name);

    for (uint32_t particleIndex = 0; particleIndex < count; particleIndex++) {
        if (particleGroup.particles.size() > 1000)
            return;
        particleGroup.particles.push_back(particleEmitter_->MakeFireParticle(position));
    }
}
void ParticleManager::EmitRing(const std::string& name, const Vector3& position, uint32_t count)
{
    ParticleGroup& particleGroup = particleGroups_.at(name);

    for (uint32_t particleIndex = 0; particleIndex < count; particleIndex++) {
        if (particleGroup.particles.size() > 1000) {
            return;
        }

        particleGroup.particles.push_back(particleEmitter_->MakeRingParticle(position));
    }
}
void ParticleManager::Finalize()
{
    if (!instance_) {
        return;
    }

    if (instance_->dxCommon_) {
        instance_->dxCommon_->WaitForGPU();
    }

    for (auto& particleGroupPair : instance_->particleGroups_) {
        ParticleGroup& particleGroup = particleGroupPair.second;

        if (particleGroup.instancingResource && particleGroup.instanceData) {
            particleGroup.instancingResource->Unmap(0, nullptr);
            particleGroup.instanceData = nullptr;
        }

        particleGroup.instancingResource.Reset();
    }

    instance_->particleGroups_.clear();

    instance_->materialResource_.Reset();

    instance_->particleEmitter_.reset();
    instance_->particleRenderManager_.reset();
    instance_->particleMeshManager_.reset();

    instance_->dxCommon_ = nullptr;
    instance_->srvManager_ = nullptr;
    instance_->camera_ = nullptr;

    instance_.reset();
}
void ParticleManager::CreateMaterialResource()
{
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->SetName(L"ParticleManager::MaterialCB");

    Material* materialBufferData = nullptr;
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialBufferData));

    materialData_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_.enableLighting = 0;
    materialData_.padding[0] = 0.0f;
    materialData_.padding[1] = 0.0f;
    materialData_.padding[2] = 0.0f;
    materialData_.uvTransform = MatrixMath::MakeIdentity4x4();
    materialData_.alphaReference = 0.1f;
    materialData_.padding2[0] = 0.0f;
    materialData_.padding2[1] = 0.0f;
    materialData_.padding2[2] = 0.0f;

    *materialBufferData = materialData_;

    materialResource_->Unmap(0, nullptr);
}
void ParticleManager::AddParticle(const std::string& name, const Particle& particle)
{
    ParticleGroup& particleGroup = particleGroups_.at(name);

    if (particleGroup.particles.size() > 1000) {
        return;
    }

    particleGroup.particles.push_back(particle);
}