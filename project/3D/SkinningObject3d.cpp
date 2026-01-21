#include "SkinningObject3d.h"
#include "MatrixMath.h"
#include "Model.h"
#include "ModelManager.h"
#include "SkinningObject3dManager.h"
#include <cassert>
#include <fstream>
#include <sstream>
#pragma region 初期化処理
void SkinningObject3d::Initialize(SkinningObject3dManager* skinningObject3DManager) {
	// Manager を保持
	skinningObject3dManager_ = skinningObject3DManager;

	// デフォルトカメラ取得
	camera_ = skinningObject3dManager_->GetDefaultCamera();

	// ================================
	// Transform 定数バッファ
	// ================================
	transformationMatrixResource =
		skinningObject3dManager_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	transformationMatrixResource->SetName(L"SkinningObject3d::TransformCB");

	transformationMatrixResource->Map(
		0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));

	transformationMatrixData->WVP = MatrixMath::MakeIdentity4x4();
	transformationMatrixData->World = MatrixMath::MakeIdentity4x4();
	transformationMatrixData->WorldInverseTranspose = MatrixMath::MakeIdentity4x4();

	// ================================
	// Material 定数バッファ
	// ================================
	materialResource = skinningObject3dManager_->GetDxCommon()->CreateBufferResource(sizeof(Material));

	materialResource->SetName(L"SkinningObject3d::MaterialCB");

	materialResource->Map(
		0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = true;
	materialData_->uvTransform = MatrixMath::MakeIdentity4x4();
	materialData_->shininess = 32.0f;

	// ================================
	// Transform 初期値
	// ================================
	transform = {
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f }
	};

	cameraTransform = {
		{ 1.0f, 1.0f, 1.0f },
		{ 0.3f, 0.0f, 0.0f },
		{ 0.0f, 4.0f, -10.0f }
	};
}

#pragma endregion

#pragma region 更新処理

void SkinningObject3d::Update() {
	// ================================
	// 各種行列を作成
	// ================================

	worldMatrix_ = MatrixMath::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	Matrix4x4 worldViewProjectionMatrix;

	if (camera_) {
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = MatrixMath::Multiply(worldMatrix_, viewProjectionMatrix);
	}
	else {
		worldViewProjectionMatrix = worldMatrix_;
	}

	// ================================
	// WVP行列を計算して転送
	// ================================
	transformationMatrixData->WVP = worldViewProjectionMatrix;

	// ワールド行列も送る（ライティングなどで使用）
	transformationMatrixData->World = worldMatrix_;

	Matrix4x4 inv = MatrixMath::Inverse(worldViewProjectionMatrix);
	transformationMatrixData->WorldInverseTranspose = MatrixMath::Transpose(inv);
}

#pragma endregion

#pragma region 描画処理
void SkinningObject3d::Draw() {
	ID3D12GraphicsCommandList* commandList =
		skinningObject3dManager_->GetDxCommon()->GetCommandList();

	// Material
	commandList->SetGraphicsRootConstantBufferView(
		0, materialResource->GetGPUVirtualAddress());

	// Transform
	commandList->SetGraphicsRootConstantBufferView(
		1, transformationMatrixResource->GetGPUVirtualAddress());

	// ★ MatrixPalette（SRV）
	commandList->SetGraphicsRootDescriptorTable(2,skinCluster_->paletteSrvHandle.second);

	// VB（2本）
	D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
		model_->GetVertexBufferView(),          // slot 0
		skinCluster_->influenceBufferView       // slot 1
	};
	commandList->IASetVertexBuffers(0, 2, vbvs);

	// Camera
	commandList->SetGraphicsRootConstantBufferView(
		4, camera_->GetGPUAddress());

	if (model_) {
		model_->Draw();
	}
}

#pragma endregion

#pragma region OBJ読み込み処理
// ===============================================
// OBJファイルの読み込み
// ===============================================
ModelData Object3d::LoadModeFile(const std::string& directoryPath,
	const std::string filename) {
	ModelData modelData;

	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;

	const aiScene* scene = importer.ReadFile(
		filePath.c_str(),
		aiProcess_Triangulate | aiProcess_FlipWindingOrder | aiProcess_FlipUVs);

	assert(scene);
	assert(scene->HasMeshes());

	// -------------------------
	// Mesh -> MeshPrimitive
	// -------------------------
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];

		MeshPrimitive primitive;
		primitive.mode = PrimitiveMode::Triangles; // 今は固定でOK

		// ---- vertices ----
		for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
			VertexData vertex{};

			aiVector3D pos = mesh->mVertices[v];
			aiVector3D nrm = mesh->HasNormals()
				? mesh->mNormals[v]
				: aiVector3D(0, 1, 0);

			aiVector3D uv = mesh->HasTextureCoords(0)
				? mesh->mTextureCoords[0][v]
				: aiVector3D(0, 0, 0);

			// 右手 → 左手（X反転）
			vertex.position = { -pos.x, pos.y, pos.z, 1.0f };
			vertex.normal = { -nrm.x, nrm.y, nrm.z };
			vertex.texcoord = { uv.x, uv.y };

			primitive.vertices.push_back(vertex);
		}

		// ---- indices ----
		if (mesh->HasFaces()) {
			for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
				aiFace& face = mesh->mFaces[f];
				// Triangulate してるので 3 のはず
				for (uint32_t i = 0; i < face.mNumIndices; ++i) {
					primitive.indices.push_back(face.mIndices[i]);
				}
			}
		}
		// indices が空なら drawArrays 扱いでOK


		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			aiBone* bone = mesh->mBones[boneIndex];

			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
			aiVector3D scale, translate;
			aiQuaternion rotate;
			bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

			Matrix4x4 bindPoseMatrix = MatrixMath::MakeAffineMatrix(
				{ scale.x, scale.y, scale.z },
				{ rotate.x, -rotate.y, -rotate.z, rotate.w },
				{ -translate.x, translate.y, translate.z }
			);

			jointWeightData.inverseBindPoseMatrix = MatrixMath::Inverse(bindPoseMatrix);

			for (uint32_t weightIndex = 0;
				weightIndex < bone->mNumWeights;
				++weightIndex) {

				jointWeightData.vertexWeights.push_back({
					bone->mWeights[weightIndex].mWeight,
					bone->mWeights[weightIndex].mVertexId
					});
			}
		}


		modelData.primitives.push_back(primitive);
	}

	// -------------------------
	// Material（最小）
	// -------------------------
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];

		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
			break; // 1枚で十分
		}
	}

	// -------------------------
	// Node（既存の処理）
	// -------------------------
	modelData.rootNode = ReadNode(scene->mRootNode);

	return modelData;
}

#pragma endregion

void Object3d::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
}

Node Object3d::ReadNode(aiNode* node) {
	Node result;

	aiVector3D scale, translate;
	aiQuaternion rotate;

	node->mTransformation.Decompose(scale, rotate, translate);

	// scale（
	result.transform.scale = { scale.x, scale.y, scale.z };

	// 回転：右手 → 左手
	result.transform.rotate = {
		rotate.x,
		-rotate.y,
		-rotate.z,
		rotate.w
	};

	// 平行移動：X反転
	result.transform.translate = {
		-translate.x,
		translate.y,
		translate.z
	};

	// SRTから localMatrix を再構築
	result.localMatrix = MatrixMath::MakeAffineMatrix(
		result.transform.scale,
		result.transform.rotate,
		result.transform.translate);

	result.name = node->mName.C_Str();

	result.children.resize(node->mNumChildren);
	for (uint32_t i = 0; i < node->mNumChildren; ++i) {
		result.children[i] = ReadNode(node->mChildren[i]);
	}

	return result;
}

const Node& Object3d::GetRootNode() const {
	assert(model_);
	return model_->GetModelData().rootNode;
}

void Object3d::SetAnimation(PlayAnimation* anim) {
	animation_ = anim;
}