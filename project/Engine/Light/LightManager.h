#pragma once
#include "../DirectXCommon/DirectXCommon.h"
#include "../math/Light.h"
#include "../math/Object3DStruct.h"
#include <d3d12.h>
#include <cstdint>
#include <memory>
#include <array>
#include <wrl.h>

using PointLightHandle = uint32_t;
inline constexpr PointLightHandle kInvalidPointLightHandle = 0xffffffffu;
using SpotLightHandle = uint32_t;
inline constexpr SpotLightHandle kInvalidSpotLightHandle = 0xffffffffu;

class Camera;

class LightManager {
public:
    static constexpr uint32_t kMaxPointLights = 32;
    static constexpr uint32_t kMaxSpotLights = 8;

    static LightManager* GetInstance();
    static void Finalize();

    void Initialize(DirectXCommon* dxCommon);
    bool IsInitialized() const { return dxCommon_ != nullptr; }
    void Update();
    void UpdateClusters(const Camera* camera);
    void SetClusteredLightingEnabled(bool isEnabled);
    bool IsClusteredLightingEnabled() const { return isClusteredLightingEnabled_; }
    bool SetEnvironmentLighting(float diffuseStrength, float specularStrength);
    bool SetAtmosphere(bool isEnabled, float density, float strength, float anisotropy);
    Vector4 GetEnvironmentLighting() const { return ambientLightData_->environmentSettings; }
    Vector4 GetAtmosphereSettings() const { return ambientLightData_->atmosphereSettings; }
    D3D12_GPU_VIRTUAL_ADDRESS GetAmbientGpuAddress() const { return ambientLightResource_->GetGPUVirtualAddress(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalGpuAddress() const { return lightResource_->GetGPUVirtualAddress(); }
    void Bind(ID3D12GraphicsCommandList* cmd);

    void SetDirectional(const Vector4& color, const Vector3& dir, float intensity);
    void SetDirection(const Vector3& dir);
    DirectionalLight GetDirectionalLight() const { return *lightData_; }
    Vector3 GetDirectionalDirection() const { return lightData_->direction; }
    void SetIntensity(float intensity);

    void SetPointLight(const Vector4& color, const Vector3& pos, float intensity);
    void SetPointPosition(const Vector3& pos);
    void SetPointIntensity(float intensity);
    void SetPointColor(const Vector4& color);
    void SetPointRadius(float radius);
    void SetPointDecay(float decay);

    PointLightHandle AddPointLight(
        const Vector4& color,
        const Vector3& position,
        float intensity,
        float radius,
        float decay);
    bool UpdatePointLight(
        PointLightHandle handle,
        const Vector4& color,
        const Vector3& position,
        float intensity,
        float radius,
        float decay);
    bool SetPointLightPosition(PointLightHandle handle, const Vector3& position);
    bool SetPointLightIntensity(PointLightHandle handle, float intensity);
    bool SetPointLightColor(PointLightHandle handle, const Vector4& color);
    bool SetPointLightRadius(PointLightHandle handle, float radius);
    bool SetPointLightDecay(PointLightHandle handle, float decay);
    bool RemovePointLight(PointLightHandle handle);
    void ClearDynamicPointLights();

    void SetSpotLightColor(const Vector4& color);
    void SetSpotLightPosition(const Vector3& pos);
    void SetSpotLightDirection(const Vector3& dir);
    void SetSpotLightIntensity(float intensity);
    void SetSpotLightDistance(float distance);
    void SetSpotLightDecay(float decay);
    void SetSpotLightCosAngle(float cosAngle);
    void SetSpotLightCosFalloffStart(float cosFalloffStart);

    SpotLightHandle AddSpotLight(
        const Vector4& color,
        const Vector3& position,
        const Vector3& direction,
        float intensity,
        float distance,
        float decay,
        float cosAngle);
    bool UpdateSpotLight(
        SpotLightHandle handle,
        const Vector4& color,
        const Vector3& position,
        const Vector3& direction,
        float intensity,
        float distance,
        float decay,
        float cosAngle);
    bool SetSpotLightPosition(SpotLightHandle handle, const Vector3& position);
    bool SetSpotLightDirection(SpotLightHandle handle, const Vector3& direction);
    bool SetSpotLightIntensity(SpotLightHandle handle, float intensity);
    bool SetSpotLightColor(SpotLightHandle handle, const Vector4& color);
    bool SetSpotLightDistance(SpotLightHandle handle, float distance);
    bool SetSpotLightDecay(SpotLightHandle handle, float decay);
    bool SetSpotLightCosAngle(SpotLightHandle handle, float cosAngle);
    bool RemoveSpotLight(SpotLightHandle handle);
    void ClearDynamicSpotLights();
    bool SetPointLightShadowEnabled(PointLightHandle handle, bool isEnabled);
    bool SetSpotLightShadowEnabled(SpotLightHandle handle, bool isEnabled);
    bool IsPointLightShadowEnabled(uint32_t lightIndex) const;
    bool IsSpotLightShadowEnabled(uint32_t lightIndex) const;
    PointLight GetPointLight(uint32_t lightIndex) const;
    SpotLight GetSpotLight(uint32_t lightIndex) const;
    bool SetSpotLightCosFalloffStart(SpotLightHandle handle, float cosFalloffStart);

    void SetAmbientColor(const Vector3& color);
    void SetHemisphereColors(const Vector3& sky, const Vector3& ground);
    Vector3 GetAmbientColor() const;
    void SetAmbientIntensity(float intensity);
    float GetAmbientIntensity() const;

private:
    static std::unique_ptr<LightManager> instance_;

    LightManager(const LightManager&) = delete;
    LightManager& operator=(const LightManager&) = delete;

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class LightManager;
    };

    explicit LightManager(ConstructorKey);
    ~LightManager() = default;

private:
    struct PointLightCollection {
        PointLight lights[kMaxPointLights];
        uint32_t activeCount = 0;
        float padding[3] = {};
    };

    struct SpotLightCollection {
        SpotLight lights[kMaxSpotLights];
        uint32_t activeCount = 0;
        float padding[3] = {};
    };

    bool IsValidDynamicPointLightHandle(PointLightHandle handle) const;
    bool IsValidDynamicSpotLightHandle(SpotLightHandle handle) const;

    std::array<uint32_t, kMaxPointLights> pointGenerations_ {};
    std::array<uint32_t, kMaxSpotLights> spotGenerations_ {};
    std::array<bool, kMaxPointLights> pointShadowEnabled_ {};
    std::array<bool, kMaxSpotLights> spotShadowEnabled_ {};
    bool isClusteredLightingEnabled_ = true;
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLightCollection* pointLightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    SpotLightCollection* spotLightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> ambientLightResource_;
    AmbientLight* ambientLightData_ = nullptr;
};
