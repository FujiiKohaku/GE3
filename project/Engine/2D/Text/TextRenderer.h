#pragma once

#include <d3d12.h>
#include <memory>
#include <string>
#include <wrl.h>

class DirectXCommon;

class TextRenderer {
public:
    static TextRenderer* GetInstance();
    static void Finalize();

    void Initialize(DirectXCommon* dxCommon);
    void PreDraw();
    void Draw(
        const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
        const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
        uint32_t indexCount,
        D3D12_GPU_VIRTUAL_ADDRESS transformAddress,
        D3D12_GPU_VIRTUAL_ADDRESS appearanceAddress,
        const std::string& textureKey);

    class ConstructorKey {
        ConstructorKey() = default;
        friend class TextRenderer;
    };

    explicit TextRenderer(ConstructorKey);
    ~TextRenderer() = default;

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();

private:
    static std::unique_ptr<TextRenderer> instance_;
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
