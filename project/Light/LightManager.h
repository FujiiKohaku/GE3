#pragma once
#include "../DirectXCommon/DirectXCommon.h"
#include "../math/Object3DStruct.h"

#include "../math/Light.h"
class LightManager {

public:
    void Initialize(DirectXCommon* dxCommon);
    void Update();
    void Bind(ID3D12GraphicsCommandList* cmd);

    // 平行光の設定

    void SetDirectional(const Vector4& color, const Vector3& dir, float intensity);
    void SetDirection(const Vector3& dir);
    void SetIntensity(float intensity);
    void SetPointLight(const Vector4& color,const Vector3& pos,float intensity);

    void SetPointPosition(const Vector3& pos);
    void SetPointIntensity(float intensity);
    void SetPointColor(const Vector4& color);
    void SetPointRadius(float radius);
    void SetPointDecay(float decay);
    // SpotLight
    void SetSpotLightColor(const Vector4& color);
    void SetSpotLightPosition(const Vector3& pos);
    void SetSpotLightDirection(const Vector3& dir);
    void SetSpotLightIntensity(float intensity);
    void SetSpotLightDistance(float distance);
    void SetSpotLightDecay(float decay);
    void SetSpotLightCosAngle(float cosAngle);
    

    // ================================
    // インスタンス取得
    // ================================
    static LightManager* GetInstance();
    void Finalize();

private:
    LightManager() = default;
    ~LightManager() = default;
    static LightManager* instance;

    DirectXCommon* dxCommon_ = nullptr;

    // GPU 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLight* pointLightData_ = nullptr;

     Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    SpotLight* spotLightData_ = nullptr;

};
