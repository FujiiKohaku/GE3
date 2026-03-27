#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "MathStruct.h"
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

    randomEngine_ = std::mt19937(seedGenerator_());

    CreateRootSignature();
    CreateGraphicsPipeline();
    CreateBoardMesh();
}

void ParticleManager::Update()
{
    Matrix4x4 cameraMatrix = camera_->GetWorldMatrix();
    cameraMatrix.m[3][0] = 0.0f;
    cameraMatrix.m[3][1] = 0.0f;
    cameraMatrix.m[3][2] = 0.0f;

    Matrix4x4 billboardMatrix = cameraMatrix;
    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();

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

            particle.currentTime += kDeltaTime_;
            particle.transform.translate += particle.velocity * kDeltaTime_;

            float alpha = 1.0f - (particle.currentTime / particle.lifeTime);

            Matrix4x4 worldMatrix;
            if (useBillboard_) {
                Matrix4x4 scaleMatrix = MatrixMath::Matrix4x4MakeScaleMatrix(particle.transform.scale);
                Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(particle.transform.translate);
                worldMatrix = MatrixMath::Multiply(MatrixMath::Multiply(scaleMatrix, billboardMatrix), translateMatrix);
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
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineStates_[currentBlendMode_].Get());
}

void ParticleManager::Draw()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    for (auto& particleGroupPair : particleGroups_) {
        ParticleGroup& particleGroup = particleGroupPair.second;

        if (particleGroup.numInstance == 0) {
            continue;
        }

        commandList->SetGraphicsRootDescriptorTable(1, particleGroup.instancingSrvHandleGPU);
        commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(particleGroup.texturePath));
        commandList->DrawIndexedInstanced(6, particleGroup.numInstance, 0, 0, 0);
    }
}

void ParticleManager::CreateRootSignature()
{
    D3D12_DESCRIPTOR_RANGE instancingRange {};
    instancingRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    instancingRange.NumDescriptors = 1;
    instancingRange.BaseShaderRegister = 0;
    instancingRange.RegisterSpace = 0;
    instancingRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE textureRange {};
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.NumDescriptors = 1;
    textureRange.BaseShaderRegister = 1;
    textureRange.RegisterSpace = 0;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] = {};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &instancingRange;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &textureRange;

    D3D12_STATIC_SAMPLER_DESC sampler {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &sampler;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT result = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    if (FAILED(result)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    result = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(result));
}

void ParticleManager::CreateGraphicsPipeline()
{
    D3D12_INPUT_ELEMENT_DESC inputElementDescriptions[3] = {};

    inputElementDescriptions[0].SemanticName = "POSITION";
    inputElementDescriptions[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescriptions[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescriptions[1].SemanticName = "TEXCOORD";
    inputElementDescriptions[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescriptions[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescriptions[2].SemanticName = "NORMAL";
    inputElementDescriptions[2].SemanticIndex = 0;
    inputElementDescriptions[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescriptions[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    inputLayoutDesc.pInputElementDescs = inputElementDescriptions;
    inputLayoutDesc.NumElements = _countof(inputElementDescriptions);

    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(
        L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(
        L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
    assert(vertexShaderBlob && pixelShaderBlob);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc {};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();
    pipelineStateDesc.InputLayout = inputLayoutDesc;
    pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    pipelineStateDesc.RasterizerState = rasterizerDesc;
    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    for (int blendModeIndex = 0; blendModeIndex < (int)BlendMode::kCountOfBlendMode; blendModeIndex++) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC currentPipelineStateDesc = pipelineStateDesc;
        currentPipelineStateDesc.BlendState = CreateBlendDesc((BlendMode)blendModeIndex);

        HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
            &currentPipelineStateDesc,
            IID_PPV_ARGS(&pipelineStates_[blendModeIndex]));
        assert(SUCCEEDED(result));
    }
}

void ParticleManager::CreateBoardMesh()
{
    vertices_[0].position = { -0.5f, 0.5f, 0.0f, -1.0f };
    vertices_[0].texcoord = { 0.0f, 0.0f };
    vertices_[0].normal = { 0.0f, 0.0f, -1.0f };

    vertices_[1].position = { 0.5f, 0.5f, 0.0f, -1.0f };
    vertices_[1].texcoord = { 1.0f, 0.0f };
    vertices_[1].normal = { 0.0f, 0.0f, -1.0f };

    vertices_[2].position = { 0.5f, -0.5f, 0.0f, -1.0f };
    vertices_[2].texcoord = { 1.0f, 1.0f };
    vertices_[2].normal = { 0.0f, 0.0f, -1.0f };

    vertices_[3].position = { -0.5f, -0.5f, 0.0f, -1.0f };
    vertices_[3].texcoord = { 0.0f, 1.0f };
    vertices_[3].normal = { 0.0f, 0.0f, -1.0f };

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(vertices_));
    vertexResource_->SetName(L"ParticleManager::VertexBuffer");

    VertexData* vertexBufferData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexBufferData));
    memcpy(vertexBufferData, vertices_, sizeof(vertices_));
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(vertices_);
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

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

    *materialBufferData = materialData_;
    materialResource_->Unmap(0, nullptr);

    transformResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
    transformResource_->SetName(L"ParticleManager::TransformCB");

    lightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    lightResource_->SetName(L"ParticleManager::LightCB");

    DirectionalLight* lightBufferData = nullptr;
    lightResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightBufferData));

    lightData_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    lightData_.direction = { 0.0f, -1.0f, 0.0f };
    lightData_.intensity = 1.0f;

    *lightBufferData = lightData_;
    lightResource_->Unmap(0, nullptr);

    TextureManager::GetInstance()->LoadTexture("resources/circle.png");
    srvHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("resources/circle.png");

    indexResource_ = dxCommon_->CreateBufferResource(sizeof(indexList_));
    indexResource_->SetName(L"ParticleManager::IndexBuffer");

    uint32_t* indexBufferData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexBufferData));
    memcpy(indexBufferData, indexList_, sizeof(indexList_));
    indexResource_->Unmap(0, nullptr);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView_.SizeInBytes = sizeof(indexList_);

    TransformationMatrix* transformBufferData = nullptr;
    transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformBufferData));

    Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(
        transformBoard_.scale,
        transformBoard_.rotate,
        transformBoard_.translate);

    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();
    Matrix4x4 wvpMatrix = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);

    transformData_.World = worldMatrix;
    transformData_.WVP = wvpMatrix;

    *transformBufferData = transformData_;
    transformResource_->Unmap(0, nullptr);
}

void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
    assert(particleGroups_.find(name) == particleGroups_.end());

    ParticleGroup particleGroup {};
    particleGroup.texturePath = textureFilePath;

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
        particleGroup.particles.push_back(MakeParticleDefault(position));
    }
}

void ParticleManager::EmitFire(const std::string& name, const Vector3& position, uint32_t count)
{
    std::unordered_map<std::string, ParticleGroup>::iterator particleGroupIterator = particleGroups_.find(name);
    assert(particleGroupIterator != particleGroups_.end());

    ParticleGroup& particleGroup = particleGroupIterator->second;

    std::uniform_real_distribution<float> randomXZ(-0.1f, 0.1f);
    std::uniform_real_distribution<float> randomUp(0.3f, 0.6f);
    std::uniform_real_distribution<float> randomLife(0.5f, 1.0f);

    for (uint32_t particleIndex = 0; particleIndex < count; particleIndex++) {
        Particle particle {};

        particle.transform.translate = {
            position.x + randomXZ(randomEngine_),
            position.y,
            position.z + randomXZ(randomEngine_)
        };

        float scale = 0.1f;
        particle.transform.scale = { scale * 0.5f, scale * 2.0f, scale * 0.5f };

        particle.velocity = {
            randomXZ(randomEngine_) * 0.1f,
            randomUp(randomEngine_),
            randomXZ(randomEngine_) * 0.1f
        };

        particle.color = { 1.0f, 0.4f, 0.0f, 1.0f };
        particle.lifeTime = randomLife(randomEngine_);
        particle.currentTime = 0.0f;

        particleGroup.particles.push_back(particle);
    }
}

ParticleManager::Particle ParticleManager::MakeParticleDefault(const Vector3& pos)
{
    Particle particle {};

    std::uniform_real_distribution<float> randomOffset(-0.05f, 0.05f);
    std::uniform_real_distribution<float> randomDirection(-1.0f, 1.0f);
    std::uniform_real_distribution<float> randomSpeed(1.0f, 1.5f);
    std::uniform_real_distribution<float> randomLife(0.8f, 1.0f);

    particle.transform.translate = {
        pos.x + randomOffset(randomEngine_),
        pos.y + randomOffset(randomEngine_),
        pos.z + randomOffset(randomEngine_)
    };

    particle.transform.scale = { 0.3f, 0.3f, 0.3f };
    particle.transform.rotate = { 0.0f, 0.0f, 0.0f };

    Vector3 velocityDirection = {
        randomDirection(randomEngine_),
        randomDirection(randomEngine_),
        randomDirection(randomEngine_)
    };

    float velocityLength = std::sqrt(
        velocityDirection.x * velocityDirection.x + velocityDirection.y * velocityDirection.y + velocityDirection.z * velocityDirection.z);

    if (velocityLength > 0.0f) {
        velocityDirection.x /= velocityLength;
        velocityDirection.y /= velocityLength;
        velocityDirection.z /= velocityLength;
    }

    float speed = randomSpeed(randomEngine_);
    particle.velocity = {
        velocityDirection.x * speed,
        velocityDirection.y * speed,
        velocityDirection.z * speed
    };

    particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    particle.lifeTime = randomLife(randomEngine_);
    particle.currentTime = 0.0f;

    return particle;
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
    instance_->transformResource_.Reset();
    instance_->lightResource_.Reset();
    instance_->vertexResource_.Reset();
    instance_->indexResource_.Reset();

    instance_->rootSignature_.Reset();

    for (int blendModeIndex = 0; blendModeIndex < kCountOfBlendMode; blendModeIndex++) {
        instance_->pipelineStates_[blendModeIndex].Reset();
    }

    if (instance_->dxCommon_) {
        instance_->dxCommon_->WaitForGPU();
    }

    instance_->dxCommon_ = nullptr;
    instance_->srvManager_ = nullptr;
    instance_->camera_ = nullptr;

    instance_.reset();
}