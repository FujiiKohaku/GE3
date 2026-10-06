#pragma once
#include <string>

struct ShadowMaterialSettings {
    std::wstring vertexShaderPath;
    bool isDoubleSided = false;
    float boundsPadding = 0.0f;
};
