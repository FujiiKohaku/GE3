#pragma once

#include <cstdint>

enum class Object3dRootParameter : uint32_t {
    Material = 0,
    TransformationMatrix,
    Texture,
    DirectionalLight,
    Camera,
    PointLights,
    SpotLights,
    AmbientLight,
    EnvironmentTexture,
    Count
};

constexpr uint32_t RootParameterIndex(Object3dRootParameter parameter)
{
    return static_cast<uint32_t>(parameter);
}
