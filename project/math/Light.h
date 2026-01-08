#pragma once
#include "MathStruct.h"
// 平行光源データ
struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};
struct PointLight {
    Vector4 color; // ライトの色
    Vector3 position; // 位置
    float intensity; // 輝度
    float radius; // ライトの届く最大距離 
    float decay; // 減衰率     
    float padding[2]; // 16バイト境界合わせ
};

// スポットライトデータ
struct SpotLight {
    Vector4 color; // 16
    Vector3 position; // 12
    float intensity; // 4  → 16
    Vector3 direction; // 12
    float distance; // 4  → 16
    float decay; // 4
    float cosAngle; // 4
    float cosFalloffStart; // 4
    float padding; // 4  → 16
};