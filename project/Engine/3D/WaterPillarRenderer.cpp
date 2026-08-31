#include "WaterPillarRenderer.h"

#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Math/MatrixMath.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <numbers>
#include <vector>

void WaterPillarRenderer::Initialize(Camera* camera)
{
    assert(camera != nullptr);
    camera_ = camera;
    dxCommon_ = DirectXCommon::GetInstance();
    CreateMesh(32);
    CreateRootSignature();
    CreatePipelineState();

    constantResource_ = dxCommon_->CreateBufferResource(sizeof(Constants) * kMaxPillars);
    const HRESULT hr = constantResource_->Map(0, nullptr, reinterpret_cast<void**>(&constants_));
    assert(SUCCEEDED(hr));
    std::memset(constants_, 0, sizeof(Constants) * kMaxPillars);
}

void WaterPillarRenderer::Update(float deltaTime)
{
    time_ += deltaTime;
}

void WaterPillarRenderer::PreDraw()
{
    drawIndex_ = 0;
    auto* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void WaterPillarRenderer::Draw(const Vector3& position, const Vector3& scale, const Vector4& color)
{
    if (drawIndex_ >= kMaxPillars || scale.y <= 0.0f || color.w <= 0.0f) return;

    Constants& data = constants_[drawIndex_];
    data.viewProjection = camera_->GetViewProjectionMatrix();
    const Vector3 cameraPosition = camera_->GetTranslate();
    data.cameraPositionAndTime = { cameraPosition.x, cameraPosition.y, cameraPosition.z, time_ };
    data.color = color;
    data.pillarPositionAndRadius = { position.x, position.y, position.z, scale.x };
    data.heightAndShape = { scale.y, 0.0f, 0.0f, 0.0f };

    auto* commandList = dxCommon_->GetCommandList();
    const D3D12_GPU_VIRTUAL_ADDRESS address = constantResource_->GetGPUVirtualAddress() + sizeof(Constants) * drawIndex_;
    commandList->SetGraphicsRootConstantBufferView(0, address);
    commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
    ++drawIndex_;
}

void WaterPillarRenderer::CreateMesh(uint32_t divisions)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const float angleStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divisions);
    constexpr uint32_t kHeightSegments = 20;
    for (uint32_t ring = 0; ring <= kHeightSegments; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(kHeightSegments);
        const float lowerBulge = 0.84f + std::sin(v * std::numbers::pi_v<float>) * 0.18f;
        const float tipT = v > 0.78f ? (v - 0.78f) / 0.22f : 0.0f;
        const float tipRound = 1.0f - tipT * tipT * 0.88f;
        const float profileRadius = lowerBulge * tipRound;
        for (uint32_t side = 0; side <= divisions; ++side) {
            const float angle = static_cast<float>(side) * angleStep;
            const float x = std::sin(angle);
            const float z = std::cos(angle);
            const float u = static_cast<float>(side) / static_cast<float>(divisions);
            vertices.push_back({ { x * profileRadius, v, z * profileRadius, 1.0f },
                { x, tipT * 0.65f, z }, { u, v, 0.0f } });
        }
    }
    const uint32_t ringStride = divisions + 1;
    for (uint32_t ring = 0; ring < kHeightSegments; ++ring) {
        for (uint32_t side = 0; side < divisions; ++side) {
            const uint32_t i0 = ring * ringStride + side;
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + ringStride;
            const uint32_t i3 = i2 + 1;
            indices.insert(indices.end(), { i0, i2, i1, i1, i2, i3 });
        }
    }

    const std::array<Vector3, 9> bubbleCenters = {
        Vector3 { -0.48f, 0.08f, 0.05f }, Vector3 { 0.42f, 0.15f, -0.12f },
        Vector3 { 0.08f, 0.30f, 0.38f }, Vector3 { -0.18f, 0.40f, -0.34f },
        Vector3 { 0.56f, 0.48f, 0.22f }, Vector3 { -0.60f, 0.58f, -0.18f },
        Vector3 { 0.22f, 0.72f, -0.08f }, Vector3 { -0.26f, 0.82f, 0.20f },
        Vector3 { 0.02f, 0.98f, 0.04f }
    };
    constexpr uint32_t kBubbleLatitude = 6;
    constexpr uint32_t kBubbleLongitude = 8;
    for (uint32_t bubble = 0; bubble < bubbleCenters.size(); ++bubble) {
        const float bubbleRadius = 0.075f + static_cast<float>((bubble * 5u) % 4u) * 0.022f;
        const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
        for (uint32_t latitude = 0; latitude <= kBubbleLatitude; ++latitude) {
            const float v = static_cast<float>(latitude) / static_cast<float>(kBubbleLatitude);
            const float phi = -std::numbers::pi_v<float> * 0.5f + v * std::numbers::pi_v<float>;
            for (uint32_t longitude = 0; longitude <= kBubbleLongitude; ++longitude) {
                const float u = static_cast<float>(longitude) / static_cast<float>(kBubbleLongitude);
                const float theta = u * 2.0f * std::numbers::pi_v<float>;
                const Vector3 normal = { std::cos(phi) * std::sin(theta), std::sin(phi), std::cos(phi) * std::cos(theta) };
                const Vector3 center = bubbleCenters[bubble];
                vertices.push_back({
                    { center.x + normal.x * bubbleRadius, center.y + normal.y * bubbleRadius,
                        center.z + normal.z * bubbleRadius, 1.0f },
                    normal, { u, v, 1.0f + static_cast<float>(bubble) }
                });
            }
        }
        const uint32_t bubbleStride = kBubbleLongitude + 1;
        for (uint32_t latitude = 0; latitude < kBubbleLatitude; ++latitude) {
            for (uint32_t longitude = 0; longitude < kBubbleLongitude; ++longitude) {
                const uint32_t i0 = baseVertex + latitude * bubbleStride + longitude;
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + bubbleStride;
                const uint32_t i3 = i2 + 1;
                indices.insert(indices.end(), { i0, i2, i1, i1, i2, i3 });
            }
        }
    }

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * vertices.size());
    Vertex* mappedVertices = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
    std::memcpy(mappedVertices, vertices.data(), sizeof(Vertex) * vertices.size());
    vertexBufferView_ = { vertexResource_->GetGPUVirtualAddress(), static_cast<UINT>(sizeof(Vertex) * vertices.size()), sizeof(Vertex) };

    indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices.size());
    uint32_t* mappedIndices = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices));
    std::memcpy(mappedIndices, indices.data(), sizeof(uint32_t) * indices.size());
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexCount_ = static_cast<uint32_t>(indices.size());
}

void WaterPillarRenderer::CreateRootSignature()
{
    D3D12_ROOT_PARAMETER parameter {};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameter.Descriptor.ShaderRegister = 0;
    D3D12_ROOT_SIGNATURE_DESC desc {};
    desc.NumParameters = 1;
    desc.pParameters = &parameter;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr) && error) Logger::Log(reinterpret_cast<const char*>(error->GetBufferPointer()));
    assert(SUCCEEDED(hr));
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void WaterPillarRenderer::CreatePipelineState()
{
    auto vertexShader = dxCommon_->LoadCompiledShader(L"resources/Shaders/WaterPillar/WaterPillar.VS.hlsl");
    auto pixelShader = dxCommon_->LoadCompiledShader(L"resources/Shaders/WaterPillar/WaterPillar.PS.hlsl");
    assert(vertexShader && pixelShader);

    D3D12_INPUT_ELEMENT_DESC inputs[3] {};
    inputs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputs[1] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputs[2] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

    D3D12_RASTERIZER_DESC rasterizer {};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.DepthClipEnable = TRUE;

    D3D12_BLEND_DESC blend {};
    auto& target = blend.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOp = D3D12_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D12_BLEND_ONE;
    target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depth {};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc {};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = { inputs, 3 };
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
    const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}
