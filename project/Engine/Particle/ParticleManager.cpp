#include "ParticleManager.h"

#include "Engine/StringUtility/StringUtility.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/math/MatrixMath.h"

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>

std::unique_ptr<ParticleManager> ParticleManager::instance_ = nullptr;

namespace {
float RandomRange(float min, float max)
{
    static std::random_device seedDevice;
    static std::mt19937 engine(seedDevice());
    std::uniform_real_distribution<float> distribution(min, max);
    return distribution(engine);
}

Particle MakeParticle(
    const Vector3& position,
    const Vector3& velocity,
    const Vector4& color,
    float scale,
    float lifeTime)
{
    Particle particle {};
    particle.transform.scale = { scale, scale, scale };
    particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
    particle.transform.translate = position;
    particle.velocity = velocity;
    particle.color = color;
    particle.lifeTime = lifeTime;
    particle.currentTime = 0.0f;
    return particle;
}
}

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

    CreateMaterialResource();
    CreatePerViewResource();
}

void ParticleManager::Update()
{
    UpdatePerView();

    const Matrix4x4 billboardMatrix = perViewData_->billboardMatrix;
    const Matrix4x4 viewProjectionMatrix = perViewData_->viewProjection;

    for (auto& particleGroupPair : particleGroups_) {
        ParticleGroup& particleGroup = particleGroupPair.second;
        particleGroup.numInstance = 0;

        for (std::list<Particle>::iterator particleIterator = particleGroup.particles.begin();
             particleIterator != particleGroup.particles.end();) {
            Particle& particle = *particleIterator;

            particle.currentTime += deltaTime_;
            if (particle.lifeTime <= particle.currentTime) {
                particleIterator = particleGroup.particles.erase(particleIterator);
                continue;
            }

            particle.transform.translate += particle.velocity * deltaTime_;

            Matrix4x4 worldMatrix {};
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

            if (particleGroup.numInstance < kNumMaxInstance) {
                const float alpha = 1.0f - (particle.currentTime / particle.lifeTime);
                ParticleForGPU& instance = particleGroup.instanceData[particleGroup.numInstance];
                instance.World = worldMatrix;
                instance.WVP = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);
                instance.color = particle.color;
                instance.color.w *= alpha;
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
    commandList->SetGraphicsRootConstantBufferView(3, perViewResource_->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& particleGroupPair : particleGroups_) {
        ParticleGroup& particleGroup = particleGroupPair.second;
        if (particleGroup.numInstance == 0) {
            continue;
        }

        const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView =
            particleMeshManager_->GetVertexBufferView(particleGroup.meshType);
        const D3D12_INDEX_BUFFER_VIEW& indexBufferView =
            particleMeshManager_->GetIndexBufferView(particleGroup.meshType);
        const uint32_t indexCount = particleMeshManager_->GetIndexCount(particleGroup.meshType);

        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->IASetIndexBuffer(&indexBufferView);
        commandList->SetGraphicsRootDescriptorTable(1, particleGroup.instancingSrvHandleGPU);
        commandList->SetGraphicsRootDescriptorTable(
            2,
            TextureManager::GetInstance()->GetSrvHandleGPU(particleGroup.texturePath));

        commandList->DrawIndexedInstanced(indexCount, particleGroup.numInstance, 0, 0, 0);
    }
}

void ParticleManager::SetBlendMode(BlendMode mode)
{
    currentBlendMode_ = mode;
}

void ParticleManager::CreateParticleGroup(
    const std::string& name,
    const std::string& textureFilePath,
    ParticleMeshManager::ParticleMeshType meshType)
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

    particleGroup.instancingSrvIndex = srvManager_->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = kNumMaxInstance;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

    dxCommon_->GetDevice()->CreateShaderResourceView(
        particleGroup.instancingResource.Get(),
        &srvDesc,
        srvManager_->GetCPUDescriptorHandle(particleGroup.instancingSrvIndex));

    particleGroup.instancingSrvHandleGPU = srvManager_->GetGPUDescriptorHandle(particleGroup.instancingSrvIndex);

    particleGroups_.emplace(name, std::move(particleGroup));
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        Vector3 randomDirection {
            RandomRange(-1.0f, 1.0f),
            RandomRange(-1.0f, 1.0f),
            RandomRange(-1.0f, 1.0f),
        };
        Vector3 direction = Normalize(randomDirection);
        const float speed = RandomRange(0.6f, 2.2f);
        const float scale = RandomRange(0.15f, 0.45f);

        AddParticle(
            name,
            MakeParticle(
                position,
                direction * speed,
                { 1.0f, 1.0f, 1.0f, 1.0f },
                scale,
                RandomRange(0.5f, 1.2f)));
    }
}

void ParticleManager::EmitFire(const std::string& name, const Vector3& position, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        Vector3 randomDirection {
            RandomRange(-0.35f, 0.35f),
            RandomRange(0.8f, 1.4f),
            RandomRange(-0.35f, 0.35f),
        };
        Vector3 direction = Normalize(randomDirection);
        const float speed = RandomRange(1.0f, 3.0f);
        const float scale = RandomRange(0.2f, 0.6f);

        AddParticle(
            name,
            MakeParticle(
                position,
                direction * speed,
                { 1.0f, RandomRange(0.25f, 0.65f), 0.0f, 1.0f },
                scale,
                RandomRange(0.4f, 0.9f)));
    }
}

void ParticleManager::EmitRing(const std::string& name, const Vector3& position, uint32_t count)
{
    if (count == 0) {
        return;
    }

    for (uint32_t i = 0; i < count; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(count)) * std::numbers::pi_v<float> * 2.0f;
        Vector3 direction {
            std::cos(angle),
            RandomRange(-0.15f, 0.15f),
            std::sin(angle),
        };
        direction = Normalize(direction);

        AddParticle(
            name,
            MakeParticle(
                position,
                direction * RandomRange(1.2f, 2.4f),
                { 0.6f, 0.8f, 1.0f, 1.0f },
                RandomRange(0.15f, 0.35f),
                RandomRange(0.6f, 1.1f)));
    }
}

void ParticleManager::AddParticle(const std::string& name, const Particle& particle)
{
    auto groupIterator = particleGroups_.find(name);
    if (groupIterator == particleGroups_.end()) {
        return;
    }

    ParticleGroup& particleGroup = groupIterator->second;
    if (particleGroup.particles.size() >= kNumMaxInstance) {
        return;
    }

    particleGroup.particles.push_back(particle);
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

void ParticleManager::CreatePerViewResource()
{
    perViewResource_ = dxCommon_->CreateBufferResource(sizeof(PerView));
    perViewResource_->SetName(L"ParticleManager::PerViewCB");
    perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

    perViewData_->viewProjection = MatrixMath::MakeIdentity4x4();
    perViewData_->billboardMatrix = MatrixMath::MakeIdentity4x4();
}

void ParticleManager::UpdatePerView()
{
    if (!camera_ || !perViewData_) {
        return;
    }

    Matrix4x4 cameraMatrix = camera_->GetWorldMatrix();
    cameraMatrix.m[3][0] = 0.0f;
    cameraMatrix.m[3][1] = 0.0f;
    cameraMatrix.m[3][2] = 0.0f;

    perViewData_->billboardMatrix = cameraMatrix;
    perViewData_->viewProjection = camera_->GetViewProjectionMatrix();
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

        if (instance_->srvManager_ && particleGroup.instancingSrvIndex != kInvalidDescriptorIndex) {
            instance_->srvManager_->Free(particleGroup.instancingSrvIndex);
            particleGroup.instancingSrvIndex = kInvalidDescriptorIndex;
        }

        particleGroup.instancingResource.Reset();
    }

    instance_->particleGroups_.clear();

    instance_->materialResource_.Reset();
    instance_->perViewResource_.Reset();
    instance_->perViewData_ = nullptr;

    instance_->particleRenderManager_.reset();
    instance_->particleMeshManager_.reset();

    instance_->dxCommon_ = nullptr;
    instance_->srvManager_ = nullptr;
    instance_->camera_ = nullptr;

    instance_.reset();
}
