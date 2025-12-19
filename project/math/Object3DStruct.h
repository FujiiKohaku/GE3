#pragma once
#include "MathStruct.h"
#include <string>
#include <vector>
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
    Vector4 color; // RGBAカラー
    int32_t enableLighting; // ライティング有効フラグ（Spriteではfalse）
    float padding[3]; // アラインメント調整
    Matrix4x4 uvTransform; // UV変換行列
};
// 変換行列データ（GPU定数バッファ用）
struct TransformationMatrix {
    Matrix4x4 WVP; // ワールド×ビュー×プロジェクション行列
    Matrix4x4 World; // ワールド行列
};
// 平行光源データ
struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};

// モデル全体データ（頂点配列＋マテリアル）
struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};
