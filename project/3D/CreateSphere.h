#pragma once
#include "Struct.h"
#include <cstdint>
#include <vector>


const int kSubdivision = 16; // 16分割
int kNumVertices = kSubdivision * kSubdivision * 6; // 頂点数
float waveTime = 0.0f;

// 球を生成するクラス
class CreateSphere {
public:
    // 球を生成して頂点データを返す
    void GenerateSphereVertices(VertexData* vertices, int kSubdivision, float radius);
};
