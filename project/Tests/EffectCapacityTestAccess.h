#pragma once
#include "Engine/Effect/EffectManager.h"
#include <stdexcept>

// Read back actual GPU particles and the free-list counter after the frame fence.
class EffectCapacityTestAccess {
public:
    static void VerifyInstanceParameters(EffectManager* effects, EffectHandle handle, const Vector3& velocity,
        const Vector3& direction, const EffectSkeletonPose& skeletonPose, bool shouldHaveVelocityOverride)
    {
        const size_t index = effects->FindActiveEffectIndex(handle);
        if (index == static_cast<size_t>(-1)) { throw std::runtime_error("Instance parameter test handle is not alive"); }
        const auto& resource = effects->activeResources_[index];
        const Vector3 actualVelocity = resource.effectSettingsData->velocity;
        const Vector3 actualDirection = resource.emitterData->direction;
        if (actualVelocity.x != velocity.x || actualVelocity.y != velocity.y || actualVelocity.z != velocity.z ||
            actualDirection.x != direction.x || actualDirection.y != direction.y || actualDirection.z != direction.z ||
            effects->activeEffects_[index].hasVelocityOverride != shouldHaveVelocityOverride) {
            throw std::runtime_error("Per-instance velocity/direction was lost or mixed between handles");
        }
        const auto& actualPose = *resource.skeletonPoseData;
        if (actualPose.jointCount != skeletonPose.jointCount) { throw std::runtime_error("Skeleton joint count was lost"); }
        for (size_t jointIndex = 0; jointIndex < skeletonPose.jointPositions.size(); ++jointIndex) {
            const auto& actualJoint = actualPose.jointPositions[jointIndex];
            const auto& expectedJoint = skeletonPose.jointPositions[jointIndex];
            if (actualJoint.x != expectedJoint.x || actualJoint.y != expectedJoint.y ||
                actualJoint.z != expectedJoint.z || actualJoint.w != expectedJoint.w) {
                throw std::runtime_error("Skeleton joint positions were lost or mixed between handles");
            }
        }
    }

    void Record(EffectManager* effects, EffectHandle handle, bool shouldTestRejectedEmission = false)
    {
        const size_t index = effects->FindActiveEffectIndex(handle);
        if (index == static_cast<size_t>(-1)) { throw std::runtime_error("Readback handle is not alive"); }
        auto& resource = effects->activeResources_[index];
        if (shouldTestRejectedEmission) {
            const auto& runtime = effects->effects_.at(effects->activeEffects_[index].effectName);
            const uint32_t originalCount = resource.emitterData->count;
            const uint32_t kInvalidCounts[] = { 4097u, 16777216u, 0xffffffffu };
            for (uint32_t invalidCount : kInvalidCounts) {
                resource.emitterData->count = invalidCount;
                effects->DispatchEmit(runtime, resource);
            }
            resource.emitterData->count = originalCount;
        }
        maxParticles_ = resource.maxParticles;
        const size_t particleBytes = sizeof(ParticleCS) * maxParticles_;
        D3D12_HEAP_PROPERTIES heapProperties {};
        heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC bufferDesc {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = particleBytes + sizeof(int32_t);
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        const HRESULT result = effects->dxCommon_->GetDevice()->CreateCommittedResource(
            &heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_));
        if (FAILED(result)) { throw std::runtime_error("Could not allocate particle readback buffer"); }
        const auto particleState = resource.particleState;
        const auto freeListState = resource.freeListIndexState;
        effects->TransitionResource(resource.particleResource.Get(), resource.particleState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        effects->TransitionResource(resource.freeListIndexResource.Get(), resource.freeListIndexState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        auto* commands = effects->dxCommon_->GetCommandList();
        commands->CopyBufferRegion(readback_.Get(), 0, resource.particleResource.Get(), 0, particleBytes);
        commands->CopyBufferRegion(readback_.Get(), particleBytes, resource.freeListIndexResource.Get(), 0, sizeof(int32_t));
        effects->TransitionResource(resource.particleResource.Get(), resource.particleState, particleState);
        effects->TransitionResource(resource.freeListIndexResource.Get(), resource.freeListIndexState, freeListState);
    }

    uint32_t Verify(uint32_t expectedLiveCount, bool shouldRequireWind = false)
    {
        const size_t particleBytes = sizeof(ParticleCS) * maxParticles_;
        D3D12_RANGE readRange { 0, particleBytes + sizeof(int32_t) };
        void* mappedData = nullptr;
        if (FAILED(readback_->Map(0, &readRange, &mappedData))) { throw std::runtime_error("Particle readback mapping failed"); }
        const auto* particles = static_cast<const ParticleCS*>(mappedData);
        uint32_t liveCount = 0;
        bool hasWindFailure = false;
        for (uint32_t index = 0; index < maxParticles_; ++index) {
            if (particles[index].color.w > 0.0f) {
                ++liveCount;
                if (shouldRequireWind && particles[index].velocity.x <= 20.0f) { hasWindFailure = true; }
            }
        }
        const auto* freeListIndex = reinterpret_cast<const int32_t*>(static_cast<const unsigned char*>(mappedData) + particleBytes);
        const int32_t actualFreeIndex = *freeListIndex;
        D3D12_RANGE writtenRange { 0, 0 };
        readback_->Unmap(0, &writtenRange);
        bool hasCountMismatch = false;
        if (expectedLiveCount != 0xffffffffu && liveCount != expectedLiveCount) { hasCountMismatch = true; }
        if (hasCountMismatch || actualFreeIndex != static_cast<int32_t>(maxParticles_ - liveCount) - 1) {
            throw std::runtime_error("GPU particle/free-list mismatch at capacity " + std::to_string(maxParticles_) +
                ": expected live=" + std::to_string(expectedLiveCount) + " actual=" + std::to_string(liveCount));
        }
        if (hasWindFailure) { throw std::runtime_error("Wind did not reach all emitted GPU particles"); }
        return liveCount;
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
    uint32_t maxParticles_ = 0;
};
