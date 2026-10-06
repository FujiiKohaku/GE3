#pragma once
#include "MathStruct.h"
#include <cstdint>
// 平行光源データ
struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};

struct AmbientLight {
    Vector4 color;
    Vector4 skyColor;
    Vector4 groundColor;
    Vector4 environmentSettings;
    Vector4 atmosphereSettings;
    Matrix4x4 clusterView;
    Matrix4x4 clusterProjection;
    Vector4 clusterSettings;
    // 12×8タイル、対数分割した16層。各要素はポイント／スポットのビット集合。
    uint32_t clusterMasks[1536][4];
    Vector4 componentSettings;
};

static_assert(sizeof(AmbientLight) == 24816);

struct PointLight {
    Vector4 color; // ライトの色
    Vector3 position; // 位置
    float intensity; // 輝度
    float radius; // ライトの届く最大距離 
    float decay; // 減衰率     
    int isActive;
    float padding;
};

// スポットライトデータ
struct SpotLight {
    Vector4 color;
    Vector3 position;
    float intensity;
    Vector3 direction;
    float distance;
    float decay;
    float cosAngle;
    int isActive;
    float cosFalloffStart;
};
