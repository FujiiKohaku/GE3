#pragma once

#include "Engine/math/MathStruct.h"
#include <cstdint>

struct DistanceFogData {
    float start;
    float end;
    float density;
    float curve;
};

struct HeightFogData {
    float startHeight;
    float endHeight;
    float density;
    float padding;
};

struct ExponentialFogData {
    float density;
    float falloff;
    float padding[2];
};

struct NoiseFogData {
    float scale;
    float speed;
    float strength;
    float padding;
};

struct VolumetricFogData {
    float scattering;
    float stepLength;
    float maxDistance;
    float padding;
};

struct FogData {
    Vector4 color;

    DistanceFogData distance;
    HeightFogData height;
    ExponentialFogData exponential;
    NoiseFogData noise;
    VolumetricFogData volumetric;

    float nearClip;
    float farClip;
    float fovY;
    float aspectRatio;

    int32_t isEnabled;
    int32_t distanceEnabled;
    int32_t heightEnabled;
    int32_t exponentialEnabled;

    int32_t noiseEnabled;
    int32_t volumetricEnabled;
    float time;
    float padding;
};

static_assert((sizeof(FogData) % 16) == 0);
