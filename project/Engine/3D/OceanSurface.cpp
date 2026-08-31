#include "OceanSurface.h"

#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Math/MatrixMath.h"
#include "Engine/Logger/Logger.h"
#include <cassert>
#include <cstring>
#include <vector>

void OceanSurface::Initialize(Camera* camera, float width, float length, float height)
{
    assert(camera != nullptr);
    camera_ = camera;
    dxCommon_ = DirectXCommon::GetInstance();
    width_ = width;
    length_ = length;
    height_ = height;

    CreateMesh(128, 512);
    CreateRootSignature();
    CreatePipelineState();

    constantResource_ = dxCommon_->CreateBufferResource(sizeof(OceanConstants));
    HRESULT hr = constantResource_->Map(0, nullptr, reinterpret_cast<void**>(&constants_));
    assert(SUCCEEDED(hr));
    std::memset(constants_, 0, sizeof(OceanConstants));
    Update(0.0f);
}

void OceanSurface::Update(float deltaTime)
{
    if (!constants_ || !camera_) {
        return;
    }

    time_ += deltaTime;
    constants_->viewProjection = camera_->GetViewProjectionMatrix();
    const Vector3 scale = { width_, 1.0f, length_ };
    const Vector3 rotation = { 0.0f, 0.0f, 0.0f };
    const Vector3 translation = { 0.0f, height_, length_ * 0.5f };
    constants_->world = MatrixMath::MakeAffineMatrix(scale, rotation, translation);
    const Vector3 cameraPosition = camera_->GetTranslate();
    constants_->cameraPositionAndTime = { cameraPosition.x, cameraPosition.y, cameraPosition.z, time_ };
    constants_->waveParameters = { waveAmplitude_, waveFrequency_, 0.0f, 0.0f };
    constants_->deepColor = { 0.006f, 0.055f, 0.19f, 1.0f };
    constants_->crestColor = { 0.025f, 0.43f, 0.64f, 1.0f };
}

void OceanSurface::Draw()
{
    if (!pipelineState_ || indexCount_ == 0) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootConstantBufferView(0, constantResource_->GetGPUVirtualAddress());
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}

void OceanSurface::CreateMesh(uint32_t columns, uint32_t rows)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(static_cast<size_t>(columns + 1) * (rows + 1));
    indices.reserve(static_cast<size_t>(columns) * rows * 6);

    for (uint32_t z = 0; z <= rows; ++z) {
        const float v = static_cast<float>(z) / static_cast<float>(rows);
        for (uint32_t x = 0; x <= columns; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(columns);
            vertices.push_back({ { u - 0.5f, 0.0f, v - 0.5f, 1.0f } });
        }
    }

    const uint32_t stride = columns + 1;
    for (uint32_t z = 0; z < rows; ++z) {
        for (uint32_t x = 0; x < columns; ++x) {
            const uint32_t i0 = z * stride + x;
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + stride;
            const uint32_t i3 = i2 + 1;
            indices.insert(indices.end(), { i0, i2, i1, i1, i2, i3 });
        }
    }

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * vertices.size());
    Vertex* mappedVertices = nullptr;
    HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
    assert(SUCCEEDED(hr));
    std::memcpy(mappedVertices, vertices.data(), sizeof(Vertex) * vertices.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(Vertex) * vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(Vertex);

    indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices.size());
    uint32_t* mappedIndices = nullptr;
    hr = indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices));
    assert(SUCCEEDED(hr));
    std::memcpy(mappedIndices, indices.data(), sizeof(uint32_t) * indices.size());
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexCount_ = static_cast<uint32_t>(indices.size());
}

void OceanSurface::CreateRootSignature()
{
    D3D12_ROOT_PARAMETER rootParameter {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameter.Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC desc {};
    desc.pParameters = &rootParameter;
    desc.NumParameters = 1;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr) && errorBlob) {
        Logger::Log(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
    }
    assert(SUCCEEDED(hr));
    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void OceanSurface::CreatePipelineState()
{
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShader =
        dxCommon_->LoadCompiledShader(L"resources/Shaders/Ocean/Ocean.VS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShader =
        dxCommon_->LoadCompiledShader(L"resources/Shaders/Ocean/Ocean.PS.hlsl");
    assert(vertexShader && pixelShader);

    D3D12_INPUT_ELEMENT_DESC inputElement {};
    inputElement.SemanticName = "POSITION";
    inputElement.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_RASTERIZER_DESC rasterizer {};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.DepthClipEnable = TRUE;

    D3D12_BLEND_DESC blend {};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depth {};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc {};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = { &inputElement, 1 };
    desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    desc.RasterizerState = rasterizer;
    desc.BlendState = blend;
    desc.DepthStencilState = depth;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}
