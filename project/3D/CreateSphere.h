#pragma once
#include "Struct.h"
#include <cstdint>
#include <vector>

// 頂点構造体
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};
const int kSubdivision = 16; // 16分割
int kNumVertices = kSubdivision * kSubdivision * 6; // 頂点数
float waveTime = 0.0f;

// 球を生成するクラス
class CreateSphere {
public:
    // 球を生成して頂点データを返す
    void GenerateSphereVertices(VertexData* vertices, int kSubdivision, float radius);
};
