#pragma once
#include "Struct.h" // Vector2, Vector3, Vector4, Matrix4x4 など
#include "TextureManager.h" // テクスチャ管理
#include <cstdint> // uint32_t など
#include <d3d12.h> // D3D12関連型（ID3D12Resourceなど）
#include <string> // std::string
#include <wrl.h> // ComPtrスマートポインタ
class Particle {
public:
   

    void Initialize();
    void Update();
    void Draw();

private:


    static Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    static Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
