// 基本ベクトル.行列
#pragma once
#include <cmath>

#include <string>
#include <vector>
struct Vector2 {
    float x, y;
};
struct Vector3 {
    float x, y, z;
};
struct Vector4 {
    float x, y, z, w;
};
struct Matrix3x3 {
    float m[3][3];
};
struct Matrix4x4 {
    float m[4][4];
};
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
// トランスフォーム情報（位置・回転・拡縮）
struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};
// モデル全体データ（頂点配列＋マテリアル）
struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};
// ===============================
// Vector3 演算関数
// ===============================

inline Vector3 operator+(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vector3 operator-(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vector3 operator*(const Vector3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline Vector3& operator+=(Vector3& a, const Vector3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}
inline Vector3& operator-=(Vector3& a, const Vector3& b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}
inline Vector3 Normalize(const Vector3& v)
{
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

    // 長さ0ならそのまま返す（ゼロ除算を防ぐ）
    if (len == 0.0f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    return { v.x / len, v.y / len, v.z / len };
}
