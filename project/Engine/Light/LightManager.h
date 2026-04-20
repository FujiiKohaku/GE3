#pragma once
#include "../DirectXCommon/DirectXCommon.h"
#include "../math/Light.h"
#include "../math/Object3DStruct.h"
#include <d3d12.h>
#include <memory>
#include <wrl.h>

class LightManager {
public:
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

    void SetSpotLightColor(const Vector4& color);
    void SetSpotLightPosition(const Vector3& pos);
    void SetSpotLightDirection(const Vector3& dir);
    void SetSpotLightIntensity(float intensity);
    void SetSpotLightDistance(float distance);
    void SetSpotLightDecay(float decay);
    void SetSpotLightCosAngle(float cosAngle);

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
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLight* pointLightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    SpotLight* spotLightData_ = nullptr;
};