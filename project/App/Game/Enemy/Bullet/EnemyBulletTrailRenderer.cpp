#include "EnemyBulletTrailRenderer.h"

#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"
#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace {
constexpr float kTrailWidthScale = 1.3f;
constexpr float kTrailOpacity = 0.80f;

Vector4 GetClipPlane(const Matrix4x4& matrix, size_t axis, float axisScale, float wScale)
{
    const Vector3 normal {
        matrix.m[0][axis] * axisScale + matrix.m[0][3] * wScale,
        matrix.m[1][axis] * axisScale + matrix.m[1][3] * wScale,
        matrix.m[2][axis] * axisScale + matrix.m[2][3] * wScale
    };
    const float inverseLength = 1.0f / Vector3Length(normal);
    return { normal.x * inverseLength, normal.y * inverseLength, normal.z * inverseLength,
        (matrix.m[3][axis] * axisScale + matrix.m[3][3] * wScale) * inverseLength };
}

bool IsTrailVisible(const EnemyBullet& bullet, const std::array<Vector4, 6>& clipPlanes)
{
    // Every rendered point lies within the maximum path length from the head.
    const Vector3& head = bullet.GetPosition();
    const EnemyBulletTrailStyle& style = bullet.GetTrailStyle();
    const float radius = style.maxLength + style.halfWidth * kTrailWidthScale;
    for (const Vector4& plane : clipPlanes) {
        if (head.x * plane.x + head.y * plane.y + head.z * plane.z + plane.w < -radius) {
            return false;
        }
    }
    return true;
}
}

void EnemyBulletTrailRenderer::Initialize()
{
    auto* dx = DirectXCommon::GetInstance();
    D3D12_ROOT_PARAMETER parameter {};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameter.Constants.Num32BitValues = sizeof(Matrix4x4) / sizeof(float);
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootDesc {};
    rootDesc.NumParameters = 1;
    rootDesc.pParameters = &parameter;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr)) ||
        FAILED(dx->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_)))) {
        throw std::runtime_error("Failed to create the enemy bullet trail root signature");
    }

    const auto vertexShader = dx->LoadCompiledShader(L"resources/Shaders/EnemyBulletTrail/Trail.VS.hlsl");
    const auto pixelShader = dx->LoadCompiledShader(L"resources/Shaders/EnemyBulletTrail/Trail.PS.hlsl");
    const D3D12_INPUT_ELEMENT_DESC inputs[] {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline {};
    pipeline.pRootSignature = rootSignature_.Get();
    pipeline.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    pipeline.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    pipeline.InputLayout = { inputs, _countof(inputs) };
    pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline.RasterizerState.DepthClipEnable = TRUE;
    auto& target = pipeline.BlendState.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOp = D3D12_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D12_BLEND_ONE;
    target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline.DepthStencilState.DepthEnable = TRUE;
    pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets = 1;
    pipeline.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pipeline.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipeline.SampleDesc.Count = 1;
    if (FAILED(dx->GetDevice()->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&pipelineState_)))) {
        throw std::runtime_error("Failed to create the enemy bullet trail graphics pipeline");
    }

    constexpr size_t kInitialBulletCapacity = 128;
    constexpr size_t kVerticesPerTrail = EnemyBullet::kMaxTrailPoints * 2;
    constexpr size_t kIndicesPerTrail = (EnemyBullet::kMaxTrailPoints - 1) * 6;
    vertices_.reserve(kInitialBulletCapacity * kVerticesPerTrail);
    indices_.reserve(kInitialBulletCapacity * kIndicesPerTrail);
    ReserveUploadBuffer(kInitialBulletCapacity * (kVerticesPerTrail * sizeof(Vertex) + kIndicesPerTrail * sizeof(uint32_t)));
}

void EnemyBulletTrailRenderer::Draw(const Camera& camera, const std::vector<std::unique_ptr<EnemyBullet>>& bullets)
{
    if (bullets.empty()) { return; }
    vertices_.clear();
    indices_.clear();
    const Matrix4x4& viewProjection = camera.GetUnjitteredViewProjectionMatrix();
    const std::array<Vector4, 6> clipPlanes {
        GetClipPlane(viewProjection, 0, 1.0f, 1.0f), GetClipPlane(viewProjection, 0, -1.0f, 1.0f),
        GetClipPlane(viewProjection, 1, 1.0f, 1.0f), GetClipPlane(viewProjection, 1, -1.0f, 1.0f),
        GetClipPlane(viewProjection, 2, 1.0f, 0.0f), GetClipPlane(viewProjection, 2, -1.0f, 1.0f)
    };
    const Matrix4x4& cameraWorld = camera.GetWorldMatrix();
    const Vector3 cameraRight { cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] };
    for (const auto& bullet : bullets) {
        if (bullet->IsAlive() && bullet->GetTrailPoints().size() >= 2 && IsTrailVisible(*bullet, clipPlanes)) {
            AppendTrail(*bullet, camera.GetTranslate(), cameraRight);
        }
    }
    if (indices_.empty()) { return; }

    const auto vertexBytes = static_cast<UINT>(vertices_.size() * sizeof(Vertex));
    const auto indexBytes = static_cast<UINT>(indices_.size() * sizeof(uint32_t));
    // End-of-frame fencing in DirectXCommon makes the upload buffer safe to reuse here.
    ReserveUploadBuffer(static_cast<size_t>(vertexBytes) + indexBytes);
    std::memcpy(uploadData_, vertices_.data(), vertexBytes);
    std::memcpy(uploadData_ + vertexBytes, indices_.data(), indexBytes);
    const D3D12_GPU_VIRTUAL_ADDRESS address = uploadBuffer_->GetGPUVirtualAddress();
    const D3D12_VERTEX_BUFFER_VIEW vertexView { address, vertexBytes, sizeof(Vertex) };
    const D3D12_INDEX_BUFFER_VIEW indexView { address + vertexBytes, indexBytes, DXGI_FORMAT_R32_UINT };

    auto* commands = DirectXCommon::GetInstance()->GetCommandList();
    commands->SetGraphicsRootSignature(rootSignature_.Get());
    commands->SetPipelineState(pipelineState_.Get());
    commands->SetGraphicsRoot32BitConstants(0, sizeof(Matrix4x4) / sizeof(float), &viewProjection, 0);
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->IASetVertexBuffers(0, 1, &vertexView);
    commands->IASetIndexBuffer(&indexView);
    commands->DrawIndexedInstanced(static_cast<UINT>(indices_.size()), 1, 0, 0, 0);
}

void EnemyBulletTrailRenderer::AppendTrail(const EnemyBullet& bullet, const Vector3& cameraPosition, const Vector3& cameraRight)
{
    const auto history = bullet.GetTrailPoints();
    const EnemyBulletTrailStyle& style = bullet.GetTrailStyle();
    std::array<Vector3, EnemyBullet::kMaxTrailPoints> points;
    std::array<float, EnemyBullet::kMaxTrailPoints> distances;
    points[0] = history.back();
    distances[0] = 0.0f;
    size_t pointCount = 1;
    float length = 0.0f;
    for (size_t index = history.size() - 1; index > 0 && length < style.maxLength; --index) {
        const Vector3& center = points[pointCount - 1];
        const Vector3 segment = history[index - 1] - center;
        const float segmentLength = Vector3Length(segment);
        const float visibleLength = (std::min)(segmentLength, style.maxLength - length);
        if (visibleLength <= 0.0001f) { break; }
        length += visibleLength;
        points[pointCount] = center + segment * (visibleLength / segmentLength);
        distances[pointCount] = length;
        ++pointCount;
    }
    if (pointCount < 2) { return; }

    const auto firstVertex = static_cast<uint32_t>(vertices_.size());
    const float halfWidth = style.halfWidth * kTrailWidthScale;
    const Vector4 color { style.color.x, style.color.y, style.color.z, kTrailOpacity };
    Vector3 previousSide = cameraRight;
    for (size_t index = 0; index < pointCount; ++index) {
        const Vector3& center = points[index];
        Vector3 tangent = points[1] - points[0];
        if (index > 0) {
            tangent = center - points[index - 1];
            if (index + 1 < pointCount) {
                const Vector3 across = points[index + 1] - points[index - 1];
                if (Dot(across, across) > 0.000001f) { tangent = across; }
            }
        }
        Vector3 side = Cross(tangent, cameraPosition - center);
        if (Dot(side, side) <= 0.000001f) { side = previousSide; }
        side = Normalize(side);
        if (Dot(side, previousSide) < 0.0f) { side = side * -1.0f; }
        previousSide = side;

        const float widthFactor = 1.0f - distances[index] / length;
        const Vector3 offset = side * (halfWidth * widthFactor);
        vertices_.push_back({ center - offset, -1.0f, color });
        vertices_.push_back({ center + offset, 1.0f, color });
    }
    for (size_t index = 0; index + 1 < pointCount; ++index) {
        const auto start = firstVertex + static_cast<uint32_t>(index * 2);
        indices_.insert(indices_.end(), { start, start + 1, start + 2, start + 1, start + 3, start + 2 });
    }
}

void EnemyBulletTrailRenderer::ReserveUploadBuffer(size_t requiredBytes)
{
    if (requiredBytes <= uploadCapacityBytes_) { return; }
    const size_t capacity = (std::max)(requiredBytes, uploadCapacityBytes_ * 2);
    auto buffer = DirectXCommon::GetInstance()->CreateBufferResource(capacity);
    const D3D12_RANGE readRange { 0, 0 };
    void* mappedData = nullptr;
    if (FAILED(buffer->Map(0, &readRange, &mappedData))) {
        throw std::runtime_error("Failed to map the enemy bullet trail upload buffer");
    }
    uploadBuffer_ = std::move(buffer);
    uploadData_ = static_cast<std::byte*>(mappedData);
    uploadCapacityBytes_ = capacity;
}
