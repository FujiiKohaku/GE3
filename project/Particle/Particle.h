#pragma once
#include"../math/SpriteStruct.h"
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
