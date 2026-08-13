#pragma once

#include "Engine/Math/MathStruct.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class Camera;
class DirectXCommon;

class OceanSurface {
public:
    void Initialize(Camera* camera, float width, float length, float height);
    void Update(float deltaTime);
    void Draw();

    void SetWaveAmplitude(float amplitude) { waveAmplitude_ = amplitude; }
    void SetWaveFrequency(float frequency) { waveFrequency_ = frequency; }
    void SetLength(float length) { length_ = length; }

private:
    struct Vertex {
        Vector4 position;
    };

    struct OceanConstants {
        Matrix4x4 viewProjection;
        Matrix4x4 world;
        Vector4 cameraPositionAndTime;
        Vector4 waveParameters;
        Vector4 deepColor;
        Vector4 crestColor;
    };

    void CreateMesh(uint32_t columns, uint32_t rows);
    void CreateRootSignature();
    void CreatePipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ {};
    OceanConstants* constants_ = nullptr;
    uint32_t indexCount_ = 0;
    float width_ = 0.0f;
    float length_ = 0.0f;
    float height_ = 0.0f;
    float time_ = 0.0f;
    float waveAmplitude_ = 1.55f;
    float waveFrequency_ = 0.075f;
};
