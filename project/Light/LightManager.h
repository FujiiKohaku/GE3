#pragma once
#include "../DirectXCommon/DirectXCommon.h"
#include "../math/Object3DStruct.h"
class LightManager {

public:
    void Initialize(DirectXCommon* dxCommon);
    void Update();
    void Bind(ID3D12GraphicsCommandList* cmd);

    // 平行光の設定

    void SetDirectional(const Vector4& color, const Vector3& dir, float intensity);
    void SetDirection(const Vector3& dir);
    void SetIntensity(float intensity);

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
};
