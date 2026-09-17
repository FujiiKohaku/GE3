#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "MathStruct.h"
#include <string>
#include <vector>
#include <wrl.h>
#include<map>
// 頂点データ
struct VertexData {
	Vector4 position; // 位置
	Vector2 texcoord; // UV座標
	Vector3 normal; // 法線（使わないが整合性のため）
};
// マテリアル外部データ（ファイルパス・テクスチャ番号）
struct MaterialData {
	std::string textureFilePath;
};

// Keep these values in sync with the shading mode constants in
// resources/Shaders/Object3D/Object3d.hlsli. The value is stored in the
// existing material constant buffer, so adding Toon does not change its GPU
// layout.
enum class MaterialShadingMode : int32_t {
    Unlit = 0,
    Standard = 1,
    Ice = 2,
    ArchivePaper = 3,
    ArchiveLeather = 4,
    ArchiveBrass = 5,
    Toon = 6,
};

// マテリアルデータ（色情報など）
struct Material {
    Vector4 color;
    int32_t enableLighting;
    int32_t enableEnvironmentMap;
    float shininess;
    float environmentCoefficient;
    Matrix4x4 uvTransform;
};

// 変換行列データ（GPU定数バッファ用）
struct TransformationMatrix {
	Matrix4x4 WVP; // ワールド×ビュー×プロジェクション行列
	Matrix4x4 World; // ワールド行列
	Matrix4x4 WorldInverseTranspose;
};

struct Node {
	QuaternionTransform transform;
	Matrix4x4 localMatrix;
	std::string name;
	std::vector<Node> children;
};
enum class PrimitiveMode {
	Points,
	Lines,
	Triangles,
};

struct MeshPrimitive {
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	PrimitiveMode mode;
	uint32_t materialIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vbView;

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW ibView;
};

struct VertexWeightData {
	float weight;
	uint32_t vertexIndex;

};
struct JointWeightData {
	Matrix4x4 inverseBindPoseMatrix;
	std::vector<VertexWeightData> vertexWeights; 
};

// モデル全体データ（頂点配列＋マテリアル）
struct ModelData {
	std::map<std::string, JointWeightData> skinClusterData;
	std::vector<MeshPrimitive> primitives;
	std::vector<MaterialData> materials;
	Node rootNode;
};
