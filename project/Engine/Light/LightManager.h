#pragma once
#include "../DirectXCommon/DirectXCommon.h"
#include "../math/Light.h"
#include "../math/Object3DStruct.h"
#include <d3d12.h>
#include <cstdint>
#include <memory>
#include <wrl.h>

using PointLightHandle = uint32_t;
inline constexpr PointLightHandle kInvalidPointLightHandle = 0xffffffffu;

class LightManager {
public:
    static constexpr uint32_t kMaxPointLights = 16;

    static LightManager* GetInstance();
    static void Finalize();

    void Initialize(DirectXCommon* dxCommon);
    void Update();
    void Bind(ID3D12GraphicsCommandList* cmd);

    void SetDirectional(const Vector4& color, const Vector3& dir, float intensity);
    void SetDirection(const Vector3& dir);
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

    void SetAmbientColor(const Vector3& color);
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

    bool IsValidDynamicPointLightHandle(PointLightHandle handle) const;

    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLightCollection* pointLightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    SpotLight* spotLightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> ambientLightResource_;
    AmbientLight* ambientLightData_ = nullptr;
};
