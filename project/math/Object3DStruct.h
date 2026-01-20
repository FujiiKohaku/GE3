#pragma once
#include "DirectXCommon.h"
#include "MathStruct.h"
#include <map>
#include <string>
#include <vector>
#include <wrl.h>
// 頂点データ
struct VertexData {
    Vector4 position; // 位置
    Vector2 texcoord; // UV座標
    Vector3 normal; // 法線（使わないが整合性のため）
};
// マテリアル外部データ（ファイルパス・テクスチャ番号）
struct MaterialData {
    std::string textureFilePath;
    uint32_t textureIndex = 0;
};
// マテリアルデータ（色情報など）
struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[2];
    float shininess;
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
    std::vector<uint32_t> indices;
    MaterialData material;
    Node rootNode;
};
const uint32_t kNumMaxInfluence = 4;
struct VertexInfluence {
    std::array<float, kNumMaxInfluence> weights;
    std::array<int32_t, kNumMaxInfluence> jointIndices;
};
struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix; // 位置用
    Matrix4x4 skeletonSpaceInverseTransposeMatrix; // 法線用


};