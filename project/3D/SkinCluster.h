#pragma once
#include <vector>     
#include <span>       // std::span
#include <utility>    // std::pair
#include <cstdint>    // uint32_t / int32_t を使うなら
#include <array>      // std::array を使うなら（VertexInfluenceで使う想定）
#include <wrl.h>
#include <d3d12.h>
#include <iostream>
#include"../math/MathStruct.h"
#include"../math/MatrixMath.h"
#include"../Skeleton/Skeleton.h"
#include"../DirectXCommon/DirectXCommon.h"
class SkinCluster
{
public:
	SkinCluster() = default;
	~SkinCluster() = default;

	void Initialize();
	void Update();


	static constexpr uint32_t kNumMaxInfluence = 4;

	struct VertexInfluence {
		std::array<float, kNumMaxInfluence> weights;
		std::array<int32_t, kNumMaxInfluence> jointIndices;
	};

	struct WellForGPU {
		Matrix4x4 skeletonSpaceMatrix;
		Matrix4x4 skeletonSpaceInverseTransposeMatrix;
	};
	struct SkinClusterData
	{
		// JointIndex順の InverseBindPoseMatrix
		std::vector<Matrix4x4> inverseBindPoseMatrices;

		// Influence
		Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
		D3D12_VERTEX_BUFFER_VIEW influenceBufferView{};
		std::span<VertexInfluence> mappedInfluence;

		// MatrixPalette
		Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
		std::span<WellForGPU> mappedPalette;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle{};
	};
	SkinClusterData CreateSkinCluster(ID3D12Device* device, const Skeleton& skeleton, const ModelData& modelData, ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize);
	void UpdateSkinCluster(SkinClusterData& skinCluster, const Skeleton& skeleton);
private:

};
