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

            particle.currentTime += deltaTime;
            particle.transform.translate += particle.velocity * deltaTime;

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

    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    for (auto& particleGroupPair : particleGroups_) {
        ParticleGroup& particleGroup = particleGroupPair.second;

        if (particleGroup.numInstance == 0) {
            continue;
        }

        const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView = particleMeshManager_->GetVertexBufferView(particleGroup.meshType);

        const D3D12_INDEX_BUFFER_VIEW& indexBufferView = particleMeshManager_->GetIndexBufferView(particleGroup.meshType);

        uint32_t indexCount = particleMeshManager_->GetIndexCount(particleGroup.meshType);

        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->IASetIndexBuffer(&indexBufferView);

        commandList->SetGraphicsRootDescriptorTable(1, particleGroup.instancingSrvHandleGPU);
        commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(particleGroup.texturePath));

        commandList->DrawIndexedInstanced(indexCount, particleGroup.numInstance, 0, 0, 0);
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

    *materialBufferData = materialData_;

    materialResource_->Unmap(0, nullptr);
}