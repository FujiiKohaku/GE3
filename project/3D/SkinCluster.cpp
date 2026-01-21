#include "SkinCluster.h"

SkinCluster::SkinClusterData SkinCluster::CreateSkinCluster(ID3D12Device* device, const Skeleton& skeleton, const ModelData& modelData, ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize) {
	SkinClusterData skinCluster;
	// ------------------------------------------
// palette用Resourceを確保
// ------------------------------------------
	skinCluster.paletteResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(WellForGPU) * skeleton.joints.size());

	// ------------------------------------------
	// paletteをMap
	// ------------------------------------------
	skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&skinCluster.mappedPalette));

	// ------------------------------------------
	// palette用SRVを作成
	// ------------------------------------------
	skinCluster.paletteSrvHandle.first = DirectXCommon::GetInstance()->GetCPUDescriptorHandle(descriptorHeap, descriptorSize, 0);

	skinCluster.paletteSrvHandle.second = DirectXCommon::GetInstance()->GetGPUDescriptorHandle(descriptorHeap, descriptorSize, 0);

	D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
	paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	paletteSrvDesc.Buffer.FirstElement = 0;
	paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	paletteSrvDesc.Buffer.NumElements = static_cast<UINT>(skeleton.joints.size());
	paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);

	device->CreateShaderResourceView(skinCluster.paletteResource.Get(), &paletteSrvDesc, skinCluster.paletteSrvHandle.first);
	// ------------------------------------------
// influence用Resourceを確保
// 頂点ごとにVertexInfluenceを持つ
// ------------------------------------------
	const size_t vertexCount = modelData.primitives[0].vertices.size();
	skinCluster.influenceResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(VertexInfluence) * vertexCount);

	VertexInfluence* mappedInfluence = nullptr;
	// ------------------------------------------
	// influenceをMap
	// ------------------------------------------
	skinCluster.influenceResource->Map(0, nullptr,reinterpret_cast<void**>(&mappedInfluence));

	// ------------------------------------------
	// 初期化（weight=0 / jointIndex=-1）
	// ------------------------------------------
	std::memset(mappedInfluence,0,sizeof(VertexInfluence) * vertexCount);

	skinCluster.mappedInfluence = { mappedInfluence, vertexCount };

	// ------------------------------------------
	// VBV作成
	// ------------------------------------------
	skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
	skinCluster.influenceBufferView.SizeInBytes =static_cast<UINT>(sizeof(VertexInfluence) * vertexCount);
	skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

	// ------------------------------------------
	// inverseBindPoseMatrices 初期化
	// ------------------------------------------
	skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());



	for (size_t i = 0; i < skinCluster.inverseBindPoseMatrices.size(); ++i) {
		skinCluster.inverseBindPoseMatrices[i] = MatrixMath::MakeIdentity4x4();
	}

	for (const auto& jointWeight : modelData.skinClusterData) {

		// ------------------------------
		// joint名 → jointIndex を取得
		// ------------------------------
		auto it = skeleton.jointMap.find(jointWeight.first);
		if (it == skeleton.jointMap.end()) {
			// Skeletonに存在しないJointは無視
			continue;
		}

		uint32_t jointIndex = it->second;

		// ------------------------------
		// inverseBindPoseMatrix を保存
		// ------------------------------
		skinCluster.inverseBindPoseMatrices[jointIndex] =jointWeight.second.inverseBindPoseMatrix;

		// ------------------------------
		// 頂点ごとの影響を詰める
		// ------------------------------
		for (const auto& vertexWeight : jointWeight.second.vertexWeights) {

			VertexInfluence& currentInfluence =
				skinCluster.mappedInfluence[vertexWeight.vertexIndex];

			// 最大4枠のうち空いているところに入れる
			for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {

				if (currentInfluence.weights[i] == 0.0f) {
					currentInfluence.weights[i] = vertexWeight.weight;
					currentInfluence.jointIndices[i] = jointIndex;
					break;
				}
			}
		}
	}



	return skinCluster;
}
void SkinCluster::UpdateSkinCluster(SkinClusterData& skinCluster, const Skeleton& skeleton) {
	for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {

		// inverseBindPoseMatrices と joint 数が一致している前提
		assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());

		skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix = MatrixMath::Multiply(skinCluster.inverseBindPoseMatrices[jointIndex], skeleton.joints[jointIndex].skeletonSpaceMatrix);

		// 法線用：逆転置行列
		skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = MatrixMath::Transpose(
			MatrixMath::Inverse(skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix
			)
		);
	}

}
