#pragma once
#include "MathStruct.h"
// 平行光源データ
struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};

struct AmbientLight {
    Vector4 color;
};

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
    float padding[2];
};
