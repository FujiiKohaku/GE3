#include "Engine/Raytracing/DxrRenderer.h"
#include "Engine/Raytracing/DxrShadowRenderer.h"
#include "Engine/Raytracing/DxrReflectionRenderer.h"
#include "Engine/Raytracing/DxrGlobalIlluminationRenderer.h"
#include "Engine/Raytracing/DxrLocalShadowRenderer.h"
#include "Engine/Lighting/ScreenSpaceGlobalIllumination.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/3D/SkinningObject3d.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/Shadow/ShadowMapRenderer.h"
#include "Engine/Shadow/LocalShadowRenderer.h"
#include "Engine/Light/LightManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/PostEffect/Bloom/BloomRenderer.h"
#include "Engine/SuperResolution/TemporalSuperResolution.h"
#include "Engine/PostEffect/Volumetric/VolumetricLightRenderer.h"
#include "Engine/3D/Model.h"
#include "Engine/3D/ModelCommon.h"
#include "Engine/Camera/Camera.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include <DirectXPackedVector.h>
#include <d3d12sdklayers.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <wincodec.h>

namespace {
void Require(bool isValid, const char* message) {
    if (!isValid) { throw std::runtime_error(message); }
}
void RunSceneBoundsValidation() {
    auto projection = MatrixMath::MakeIdentity4x4();
    Require(IsDxrSceneBoundsVisible({{0, 0, 0.5f}, 0.1f}, projection), "Inside-frustum sphere was culled");
    const Vector3 kOutsideCenters[] = {{2, 0, 0.5f}, {-2, 0, 0.5f}, {0, 2, 0.5f}, {0, -2, 0.5f}, {0, 0, -1}, {0, 0, 2}};
    for (const Vector3& center : kOutsideCenters) {
        Require(!IsDxrSceneBoundsVisible({center, 0.1f}, projection), "Frustum plane did not reject an outside sphere");
    }
    Require(IsDxrSceneBoundsVisible({{0, 0, -0.05f}, 0.1f}, projection), "Near-plane intersection was falsely culled");
    Require(IsDxrSceneBoundsVisible({{100, 100, 100}, -1}, projection), "Unknown bounds were culled");
    projection.m[0][0] = std::nanf("");
    Require(IsDxrSceneBoundsVisible({{100, 100, 100}, 1}, projection), "Invalid camera matrix culled an object");
    auto world = MatrixMath::MakeIdentity4x4(); world.m[0][1] = 4; world.m[1][1] = 2; world.m[3][0] = 5;
    auto bounds = TransformDxrSceneBounds({{0, 0, 0}, 1}, world);
    auto transformedPoint = MatrixMath::Transform({1, 0, 0}, world);
    Vector3 delta = transformedPoint - bounds.center;
    Require(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z <= bounds.radius * bounds.radius,
        "Sheared geometry escaped conservative transformed bounds");
}
Vector3 ReadTextureCenter(DirectXCommon* dxCommon, ID3D12Resource* source, const char* captureName,
    size_t* fractionalPixels = nullptr, std::vector<float>* values = nullptr, float* alpha = nullptr,
    D3D12_RESOURCE_STATES sourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
    Require(source != nullptr, "Readback source texture is missing");
    auto description = source->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    uint64_t sizeBytes = 0;
    dxCommon->GetDevice()->GetCopyableFootprints(&description, 0, 1, 0, &layout, nullptr, nullptr, &sizeBytes);
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    auto bufferDescription = CD3DX12_RESOURCE_DESC::Buffer(sizeBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    Require(SUCCEEDED(dxCommon->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bufferDescription,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback))), "Readback allocation failed");
    auto before = CD3DX12_RESOURCE_BARRIER::Transition(source, sourceState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    dxCommon->GetCommandList()->ResourceBarrier(1, &before);
    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource = readback.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = layout;
    D3D12_TEXTURE_COPY_LOCATION origin = {};
    origin.pResource = source;
    origin.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dxCommon->GetCommandList()->CopyTextureRegion(&destination, 0, 0, 0, &origin, nullptr);
    auto after = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE, sourceState);
    dxCommon->GetCommandList()->ResourceBarrier(1, &after);
    dxCommon->PostDraw();
    void* mappedData = nullptr;
    D3D12_RANGE range = {0, static_cast<SIZE_T>(sizeBytes)};
    Require(SUCCEEDED(readback->Map(0, &range, &mappedData)), "Readback mapping failed");
    auto* pixels = static_cast<uint8_t*>(mappedData) + layout.Offset;
    size_t pixelStrideBytes = 8;
    if (description.Format == DXGI_FORMAT_R32_FLOAT || description.Format == DXGI_FORMAT_D32_FLOAT
        || description.Format == DXGI_FORMAT_R32_TYPELESS) { pixelStrideBytes = 4; }
    if (description.Format == DXGI_FORMAT_R16G16_FLOAT) { pixelStrideBytes = 4; }
    if (description.Format == DXGI_FORMAT_R16_FLOAT) { pixelStrideBytes = 2; }
    if (description.Format == DXGI_FORMAT_R32G32B32A32_FLOAT) { pixelStrideBytes = 16; }
    if (description.Format == DXGI_FORMAT_R8G8B8A8_UNORM) { pixelStrideBytes = 4; }
    auto* centerBytes = pixels + layout.Footprint.RowPitch * (description.Height / 2) + (description.Width / 2) * pixelStrideBytes;
    Vector3 result = {};
    if (description.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
        result = {centerBytes[0] / 255.0f, centerBytes[1] / 255.0f, centerBytes[2] / 255.0f};
        if (alpha) { *alpha = centerBytes[3] / 255.0f; }
    } else if (description.Format == DXGI_FORMAT_R16_FLOAT) {
        float value = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<const DirectX::PackedVector::HALF*>(centerBytes));
        result = {value, value, value};
    } else if (description.Format == DXGI_FORMAT_R16G16_FLOAT) {
        auto* center = reinterpret_cast<const DirectX::PackedVector::HALF*>(centerBytes);
        result = {DirectX::PackedVector::XMConvertHalfToFloat(center[0]), DirectX::PackedVector::XMConvertHalfToFloat(center[1]), 0};
    } else if (description.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
        auto* center = reinterpret_cast<const DirectX::PackedVector::HALF*>(centerBytes);
        result = {DirectX::PackedVector::XMConvertHalfToFloat(center[0]),
            DirectX::PackedVector::XMConvertHalfToFloat(center[1]), DirectX::PackedVector::XMConvertHalfToFloat(center[2])};
        if (alpha) { *alpha = DirectX::PackedVector::XMConvertHalfToFloat(center[3]); }
    } else {
        auto* center = reinterpret_cast<const float*>(centerBytes);
        result = {center[0], center[0], center[0]};
        if (description.Format == DXGI_FORMAT_R32G32B32A32_FLOAT) { result = {center[0], center[1], center[2]}; }
        if (alpha && description.Format == DXGI_FORMAT_R32G32B32A32_FLOAT) { *alpha = center[3]; }
    }
    if (values != nullptr) {
        values->resize(static_cast<size_t>(description.Width) * description.Height);
        for (uint32_t row = 0; row < description.Height; ++row) {
            for (uint32_t column = 0; column < description.Width; ++column) {
                auto* valueBytes = pixels + row * layout.Footprint.RowPitch + column * pixelStrideBytes;
                float value = 0;
                if (description.Format == DXGI_FORMAT_R16_FLOAT || description.Format == DXGI_FORMAT_R16G16_FLOAT || description.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
                    value = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<const DirectX::PackedVector::HALF*>(valueBytes));
                } else { std::memcpy(&value, valueBytes, sizeof(value)); }
                (*values)[static_cast<size_t>(row) * description.Width + column] = value;
            }
        }
    }
    if (fractionalPixels != nullptr && description.Format == DXGI_FORMAT_R32_FLOAT) {
        *fractionalPixels = 0;
        for (uint32_t row = 0; row < description.Height; ++row) {
            auto* values = reinterpret_cast<const float*>(pixels + row * layout.Footprint.RowPitch);
            for (uint32_t column = 0; column < description.Width; ++column) {
                if (values[column] > 0.001f && values[column] < 0.999f) { ++*fractionalPixels; }
            }
        }
    }
    DirectX::Image image = {};
    image.width = description.Width;
    image.height = description.Height;
    image.format = description.Format;
    if (description.Format == DXGI_FORMAT_D32_FLOAT || description.Format == DXGI_FORMAT_R32_TYPELESS) {
        image.format = DXGI_FORMAT_R32_FLOAT;
    }
    image.rowPitch = layout.Footprint.RowPitch;
    image.slicePitch = image.rowPitch * image.height;
    image.pixels = pixels;
    DirectX::ScratchImage converted;
    const DirectX::Image* captureImage = &image;
    if (image.format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        Require(SUCCEEDED(DirectX::Convert(image, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0, converted)), "Capture conversion failed");
        captureImage = converted.GetImage(0, 0, 0);
    }
    std::filesystem::path capturePath = std::filesystem::path("runtime/captures/DxrTests") / captureName;
    Require(SUCCEEDED(DirectX::SaveToWICFile(*captureImage, DirectX::WIC_FLAGS_NONE,
        GUID_ContainerFormatPng, capturePath.c_str())), "Capture save failed");
    D3D12_RANGE written = {0, 0};
    readback->Unmap(0, &written);
    return result;
}
Vector3 ReadCenter(DirectXCommon* dxCommon, DxrRenderer& renderer, const char* captureName) {
    Vector3 result = ReadTextureCenter(dxCommon, renderer.GetOutputTexture(), captureName);
    renderer.ReadCompleted();
    return result;
}
void RequireColor(const Vector3& actual, const Vector3& expected, const char* message) {
    Require(std::abs(actual.x - expected.x) < 0.02f && std::abs(actual.y - expected.y) < 0.02f
        && std::abs(actual.z - expected.z) < 0.02f, message);
}
void CheckValidationMessages(ID3D12InfoQueue* infoQueue) {
    if (!infoQueue) { return; }
    uint64_t count = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (uint64_t index = 0; index < count; ++index) {
        SIZE_T sizeBytes = 0;
        infoQueue->GetMessage(index, nullptr, &sizeBytes);
        std::vector<uint8_t> storage(sizeBytes);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        if (SUCCEEDED(infoQueue->GetMessage(index, message, &sizeBytes)) &&
            (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR || message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)) {
            throw std::runtime_error(message->pDescription);
        }
    }
}
void RunValidation(DirectXCommon* dxCommon) {
    DxrRenderer renderer;
    renderer.Initialize();
    std::ofstream report("runtime/captures/DxrTests/result.txt");
    report << renderer.GetStatus() << '\n';
    if (!renderer.IsSupported()) {
        renderer.BeginFrame(); renderer.EndFrame(nullptr);
        Require(!renderer.GetStatistics().hasValidFrame && renderer.GetOutputTexture() == nullptr, "Unsupported DXR allocated output");
        report << "SKIP: hardware ray tracing unavailable; fallback passed\n";
        return;
    }
    Camera camera;
    camera.Initialize();
    camera.LookAt({0, 0, -4}, {0, 0, 0});
    camera.Update();
    ModelCommon modelCommon;
    modelCommon.Initialize(dxCommon);
    ModelData modelData;
    modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    modelData.materials.push_back({"resources/Textures/white.png"});
    MeshPrimitive primitive = {};
    primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices = {{{-1, -1, 0, 1}, {0, 0}, {0, 0, -1}},
        {{0, 1, 0, 1}, {0.5f, 1}, {0, 0, -1}}, {{1, -1, 0, 1}, {1, 0}, {0, 0, -1}}};
    primitive.indices = {0, 1, 2};
    modelData.primitives.push_back(primitive);
    Model model;
    model.Initialize(&modelCommon, modelData);
    TextureManager::GetInstance()->FlushUploads();
    Material material = {};
    material.color = {0.8f, 0.2f, 0.1f, 1};
    material.uvTransform = MatrixMath::MakeIdentity4x4();
    auto world = MatrixMath::MakeIdentity4x4();
    DxrSettings settings;
    settings.isEnabled = true;
    renderer.SetSettings(settings);
    renderer.BeginFrame();
    renderer.Queue(&model, model, world, material);
    renderer.Queue(&model, model, world, material);
    renderer.EndFrame(&camera);
    Require(renderer.GetStatistics().hasValidFrame, renderer.GetStatus().c_str());
    Require(renderer.GetStatistics().instanceCount == 1 && renderer.GetStatistics().builtBlasCount == 1, "Initial BLAS or duplicate suppression failed");
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, renderer, "normal.png"), {0.5f, 0.5f, 0}, "Indexed hit normal mismatch");
    report << "initial buildMs=" << renderer.GetBuildGpuTimeMs() << " traceMs=" << renderer.GetTraceGpuTimeMs()
        << " bufferBytes=" << renderer.GetStatistics().bufferBytes << " outputAllocationBytes=" << renderer.GetStatistics().outputAllocationBytes << '\n';

    settings.debugMode = DxrDebugMode::BaseColor;
    settings.isDebugVisible = true;
    renderer.SetSettings(settings);
    renderer.BeginFrame();
    renderer.Queue(&model, model, world, material);
    int secondObjectId = 0;
    auto translatedWorld = world;
    translatedWorld.m[3][0] = 3;
    renderer.Queue(&secondObjectId, model, translatedWorld, material);
    renderer.EndFrame(&camera);
    Require(renderer.GetStatistics().hasValidFrame, renderer.GetStatus().c_str());
    Require(renderer.GetStatistics().blasCount == 1 && renderer.GetStatistics().builtBlasCount == 0
        && renderer.GetStatistics().instanceCount == 2, "BLAS sharing failed");
    dxCommon->PreDraw();
    renderer.DrawDebug();
    RequireColor(ReadCenter(dxCommon, renderer, "material.png"), {0.8f, 0.2f, 0.1f}, "Material or UV access mismatch");
    report << "cached buildMs=" << renderer.GetBuildGpuTimeMs() << " traceMs=" << renderer.GetTraceGpuTimeMs() << '\n';

    renderer.BeginFrame();
    translatedWorld.m[3][0] = 50;
    renderer.Queue(&model, model, translatedWorld, material);
    renderer.EndFrame(&camera);
    Require(renderer.GetStatistics().instanceCount == 1 && renderer.GetStatistics().builtBlasCount == 0, "Instance removal or rigid movement failed");
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, renderer, "moved.png"), {0.03f, 0.04f, 0.06f}, "TLAS retained old transform");

    ModelData nonIndexedData = modelData;
    nonIndexedData.primitives[0].indices.clear();
    nonIndexedData.primitives[0].indexResource.Reset();
    model.Initialize(&modelCommon, nonIndexedData);
    renderer.BeginFrame();
    auto scaledWorld = world;
    scaledWorld.m[0][0] = -2;
    scaledWorld.m[1][1] = 0.5f;
    renderer.Queue(&model, model, scaledWorld, material);
    renderer.EndFrame(&camera);
    Require(renderer.GetStatistics().hasValidFrame && renderer.GetStatistics().builtBlasCount == 1, "Replaced mesh BLAS not rebuilt");
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, renderer, "nonindexed.png"), {0.8f, 0.2f, 0.1f}, "Nonindexed or mirrored transform failed");

    // The closest geometry must select its own shader record and texture, including
    // when the same multi-geometry BLAS is instanced more than once.
    const uint8_t kGreenPixels[] = {0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255};
    TextureManager::GetInstance()->LoadTextureFromBGRA("dxr-test-green", kGreenPixels, 2, 2);
    TextureManager::GetInstance()->FlushUploads();
    ModelData multiGeometryData = modelData;
    multiGeometryData.materials.push_back({"dxr-test-green"});
    MeshPrimitive frontPrimitive = primitive;
    frontPrimitive.materialIndex = 1;
    for (VertexData& vertex : frontPrimitive.vertices) { vertex.position.z = -0.5f; }
    multiGeometryData.primitives.push_back(frontPrimitive);
    model.Initialize(&modelCommon, multiGeometryData);
    material.color = {1, 1, 1, 1};
    renderer.BeginFrame();
    renderer.Queue(&secondObjectId, model, translatedWorld, material);
    renderer.Queue(&model, model, world, material);
    renderer.EndFrame(&camera);
    Require(renderer.GetStatistics().hasValidFrame && renderer.GetStatistics().blasCount == 1, "Multi-geometry BLAS failed");
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, renderer, "multi-geometry.png"), {0, 1, 0}, "Geometry or instance shader record selected wrong material");

    settings.debugMode = DxrDebugMode::InstanceId;
    renderer.SetSettings(settings);
    renderer.BeginFrame();
    renderer.Queue(&model, model, world, material);
    renderer.EndFrame(&camera);
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, renderer, "instance-id.png"), {0.618034f, 0.414214f, 0.732051f}, "Instance ID output mismatch");

    renderer.BeginFrame();
    renderer.EndFrame(&camera);
    Require(!renderer.GetStatistics().hasValidFrame && renderer.GetOutputSrv().ptr == 0
        && renderer.GetStatistics().blasCount == 0, "Empty scene retained valid output");
    dxCommon->PreDraw(); dxCommon->PostDraw();
    renderer.BeginFrame();
    auto singularWorld = world;
    singularWorld.m[0][0] = 0;
    renderer.Queue(&model, model, singularWorld, material);
    renderer.EndFrame(&camera);
    Require(!renderer.GetStatistics().hasValidFrame && renderer.GetStatistics().instanceCount == 0, "Singular transform accepted");
    dxCommon->PreDraw(); dxCommon->PostDraw();
    settings.isEnabled = false;
    renderer.SetSettings(settings);
    renderer.BeginFrame(); renderer.EndFrame(&camera);
    Require(renderer.GetStatistics().bufferBytes == 0 && DxrRenderer::GetActive() == nullptr, "Disabled renderer retained acceleration structures");
    dxCommon->PreDraw(); dxCommon->PostDraw();
    settings.isEnabled = true;
    renderer.SetSettings(settings);
    renderer.BeginFrame(); renderer.Queue(&model, model, world, material); renderer.EndFrame(&camera);
    Require(renderer.GetStatistics().hasValidFrame && renderer.GetStatistics().builtBlasCount == 1, "Reenable failed");
    dxCommon->PreDraw(); dxCommon->PostDraw(); renderer.ReadCompleted();
    report << "PASS: hits, misses, material, shared BLAS, movement, removal, nonindexed, mirrored scale, multi-geometry/material shader records, instance ID, empty scene, invalid transform, disable/reenable\n";
}

void UploadDeformedVertices(ID3D12Resource* resource, const std::vector<VertexData>& vertices) {
    void* mappedData = nullptr;
    Require(SUCCEEDED(resource->Map(0, nullptr, &mappedData)), "Deformed test upload mapping failed");
    std::memcpy(mappedData, vertices.data(), vertices.size() * sizeof(VertexData));
    resource->Unmap(0, nullptr);
}
void RunDeformedValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene;
    scene.Initialize();
    if (!scene.IsSupported()) { return; }
    DxrSettings settings;
    settings.isEnabled = true; settings.debugMode = DxrDebugMode::BaseColor; scene.SetSettings(settings);
    Camera camera;
    camera.Initialize(); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    ModelCommon modelCommon;
    modelCommon.Initialize(dxCommon);
    ModelData modelData;
    modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    modelData.materials = {{"resources/Textures/white.png"}, {"dxr-test-green"}};
    MeshPrimitive back = {};
    back.mode = PrimitiveMode::Triangles;
    back.vertices = {{{-1, -1, 1, 1}, {0, 0}, {0, 0, -1}},
        {{0, 1, 1, 1}, {0.5f, 1}, {0, 0, -1}}, {{1, -1, 1, 1}, {1, 0}, {0, 0, -1}}};
    back.indices = {0, 1, 2};
    MeshPrimitive front = back;
    front.materialIndex = 1; front.indices.clear();
    for (VertexData& vertex : front.vertices) { vertex.position.z = 0; }
    modelData.primitives = {back, front};
    Model model;
    model.Initialize(&modelCommon, modelData);
    TextureManager::GetInstance()->FlushUploads();
    std::vector<VertexData> vertices = back.vertices;
    vertices.insert(vertices.end(), front.vertices.begin(), front.vertices.end());
    uint64_t sizeBytes = vertices.size() * sizeof(VertexData);
    auto firstVertices = dxCommon->CreateBufferResource(sizeBytes);
    auto secondVertices = dxCommon->CreateBufferResource(sizeBytes);
    UploadDeformedVertices(firstVertices.Get(), vertices); UploadDeformedVertices(secondVertices.Get(), vertices);
    int firstObjectId = 0;
    int secondObjectId = 0;
    auto world = MatrixMath::MakeIdentity4x4();
    auto distantWorld = world; distantWorld.m[3][0] = 30;
    Material material = {}; material.color = {1, 1, 1, 1}; material.uvTransform = world;
    scene.BeginFrame();
    scene.QueueDeformed(&firstObjectId, model, world, material, firstVertices.Get(), 1, true, D3D12_RESOURCE_STATE_GENERIC_READ);
    scene.QueueDeformed(&secondObjectId, model, distantWorld, material, secondVertices.Get(), 1, true, D3D12_RESOURCE_STATE_GENERIC_READ);
    scene.Queue(&model, model, distantWorld, material);
    scene.EndFrame(&camera);
    Require(scene.GetStatistics().hasValidFrame && scene.GetStatistics().dynamicBlasCount == 2
        && scene.GetStatistics().blasCount == 3 && scene.GetStatistics().builtBlasCount == 3, "Dynamic BLAS instance isolation failed");
    uint64_t initialBufferBytes = scene.GetStatistics().dynamicBufferBytes;
    Require(scene.GetStatistics().dynamicAllocationBytes >= initialBufferBytes, "Dynamic allocation metric omitted heap alignment");
    uint64_t initialRevision = scene.GetShadowSceneRevision();
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, scene, "deformed-multiple-primitives.png"), {0, 1, 0}, "Deformed vertex offset or material lookup failed");
    std::ofstream report("runtime/captures/DxrTests/deformed-result.txt");
    report << "initialBuildMs=" << scene.GetBuildGpuTimeMs() << " dynamicBufferBytes=" << initialBufferBytes
        << " dynamicAllocationBytes=" << scene.GetStatistics().dynamicAllocationBytes << '\n';
    for (VertexData& vertex : vertices) { vertex.position.x += 50; }
    UploadDeformedVertices(firstVertices.Get(), vertices);
    scene.BeginFrame();
    scene.QueueDeformed(&firstObjectId, model, world, material, firstVertices.Get(), 2, true, D3D12_RESOURCE_STATE_GENERIC_READ);
    scene.QueueDeformed(&secondObjectId, model, world, material, secondVertices.Get(), 1, true, D3D12_RESOURCE_STATE_GENERIC_READ);
    scene.Queue(&model, model, distantWorld, material);
    scene.EndFrame(&camera);
    Require(scene.GetStatistics().updatedBlasCount == 1 && scene.GetStatistics().builtBlasCount == 0
        && scene.GetStatistics().dynamicBufferBytes == initialBufferBytes, "Refit reallocated or rebuilt dynamic buffers");
    Require(scene.GetShadowSceneRevision() != initialRevision, "Vertex changes did not invalidate shadow history");
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, scene, "deformed-independent-pose.png"), {0, 1, 0}, "One object's deformation affected another object's BLAS");
    report << "refitAndTlasMs=" << scene.GetBuildGpuTimeMs() << '\n';
    scene.BeginFrame();
    scene.QueueDeformed(&firstObjectId, model, world, material, firstVertices.Get(), 2, true, D3D12_RESOURCE_STATE_GENERIC_READ);
    scene.EndFrame(&camera);
    Require(scene.GetStatistics().updatedBlasCount == 0 && scene.GetStatistics().builtBlasCount == 0
        && scene.GetStatistics().dynamicBlasCount == 1, "Frozen deformation was refitted or removed instance survived");
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, scene, "deformed-moved.png"), {0.03f, 0.04f, 0.06f}, "Refit kept stale triangle bounds");
    for (size_t index = 0; index < back.vertices.size(); ++index) { vertices[index] = back.vertices[index]; }
    UploadDeformedVertices(firstVertices.Get(), vertices);
    scene.BeginFrame();
    scene.QueueDeformed(&firstObjectId, model, world, material, firstVertices.Get(), 3, true, D3D12_RESOURCE_STATE_GENERIC_READ);
    scene.EndFrame(&camera);
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, scene, "deformed-indexed-back.png"), {1, 1, 1}, "Multi-primitive refit failed to expose indexed geometry");
    auto replacement = dxCommon->CreateBufferResource(sizeBytes);
    UploadDeformedVertices(replacement.Get(), vertices);
    scene.BeginFrame();
    scene.QueueDeformed(&firstObjectId, model, world, material, replacement.Get(), 3, true, D3D12_RESOURCE_STATE_GENERIC_READ);
    scene.EndFrame(&camera);
    Require(scene.GetStatistics().builtBlasCount == 1 && scene.GetStatistics().updatedBlasCount == 0, "Dynamic buffer replacement failed to rebuild BLAS");
    dxCommon->PreDraw(); dxCommon->PostDraw(); scene.ReadCompleted();
    auto undersized = dxCommon->CreateBufferResource(sizeof(VertexData));
    scene.BeginFrame();
    scene.QueueDeformed(&firstObjectId, model, world, material, undersized.Get(), 4, true, D3D12_RESOURCE_STATE_GENERIC_READ);
    scene.QueueDeformed(&secondObjectId, model, world, material, secondVertices.Get(), 4, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    scene.EndFrame(&camera);
    Require(!scene.HasValidScene() && scene.GetStatistics().instanceCount == 0, "Invalid deformed input was registered");
    dxCommon->PreDraw(); dxCommon->PostDraw();
    settings.isEnabled = false; scene.SetSettings(settings);
    scene.BeginFrame(); scene.EndFrame(&camera);
    Require(scene.GetStatistics().bufferBytes == 0 && scene.GetStatistics().dynamicBufferBytes == 0, "DXR OFF retained dynamic acceleration buffers");
    dxCommon->PreDraw(); dxCommon->PostDraw();
    report << "PASS: independent poses, rigid coexistence, indexed/nonindexed primitive offsets/materials, refit bounds, frozen reuse, removal, buffer replacement, invalid input, DXR OFF\n";
}

ID3D12Resource* DrawShadowFrame(DirectXCommon* dxCommon, DxrRenderer& scene, DxrShadowRenderer& shadows,
    OffscreenRenderer& offscreen, PostEffectManager& postEffects, Camera& camera,
    Object3d& receiver, Object3d& occluder, uint64_t sceneRevision = 0, MotionVectorRenderer* motionVectors = nullptr,
    const std::vector<SkinningObject3d*>* skinnedObjects = nullptr, bool shouldUpdateSkin = true,
    Object3d* additionalOccluder = nullptr, bool shouldDrawOccluder = true) {
    scene.BeginFrame();
    scene.SetDrawSubmissionEnabled(false);
    postEffects.PreDrawDepth();
    offscreen.PreDraw(postEffects.GetDepthDSVHandle(), true);
    scene.SetDirectionalShadowCapture(offscreen.IsDirectionalCaptureActive());
    if (motionVectors != nullptr) { motionVectors->BeginFrame(); }
    SrvManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->UpdateClusters(&camera);
    Object3dManager::GetInstance()->PreDraw();
    receiver.Update(); receiver.SubmitRaytracing(scene); receiver.Draw();
    occluder.Update(); occluder.SubmitRaytracing(scene);
    if (shouldDrawOccluder) { occluder.Draw(); }
    if (skinnedObjects != nullptr) {
        for (SkinningObject3d* object : *skinnedObjects) {
            if (shouldUpdateSkin) { object->Update(); }
            object->SubmitRaytracing(scene);
            SkinningObject3dManager::GetInstance()->PreDraw();
            object->Draw();
        }
    }
    if (additionalOccluder != nullptr) {
        Object3dManager::GetInstance()->PreDraw(); additionalOccluder->Update();
        additionalOccluder->SubmitRaytracing(scene); additionalOccluder->Draw();
    }
    scene.EndFrame(&camera, false);
    if (motionVectors != nullptr) { motionVectors->EndFrame(postEffects.GetDepthDSVHandle()); }
    Require(scene.HasValidScene(), scene.GetStatus().c_str());
    Require(!scene.GetStatistics().hasValidFrame, "Shadow-only frame ran debug tracing");
    postEffects.PostDrawDepth(); offscreen.PostDraw();
    DxrShadowInputs inputs;
    inputs.scene = &scene; inputs.camera = &camera;
    inputs.depthTexture = postEffects.GetDepthTexture(); inputs.depthSrv = postEffects.GetDepthSrv();
    inputs.normalTexture = offscreen.GetNormalTexture(); inputs.normalSrv = offscreen.GetNormalSrvHandleGPU();
    inputs.directionalLightTexture = offscreen.GetDirectionalLightTexture(); inputs.directionalLightSrv = offscreen.GetDirectionalLightSrv();
    inputs.colorSrv = offscreen.GetSrvHandleGPU(); inputs.lightDirection = LightManager::GetInstance()->GetDirectionalDirection();
    inputs.sceneRevision = sceneRevision;
    if (motionVectors != nullptr) { inputs.motionVectorSrv = motionVectors->GetSrvHandle(); inputs.reprojectionSrv = motionVectors->GetReprojectionSrv(); inputs.previousReprojectionSrv = motionVectors->GetPreviousReprojectionSrv(); }
    auto result = shadows.Draw(inputs);
    if (shadows.GetSettings().isEnabled) { Require(shadows.HasValidFrame(), shadows.GetStatus().c_str()); }
    else { Require(!shadows.HasValidFrame() && result.ptr == inputs.colorSrv.ptr, "Disabled shadows changed input handle"); }
    dxCommon->PreDraw();
    if (shadows.HasValidFrame()) { return shadows.GetColorTexture(); }
    return offscreen.GetColorTexture();
}
double ShadowMeanSquareError(const std::vector<float>& values, const std::vector<float>& reference) {
    Require(values.size() == reference.size(), "Shadow reference dimensions mismatch");
    double sum = 0;
    size_t count = 0;
    for (size_t index = 0; index < values.size(); ++index) {
        Require(std::isfinite(values[index]) && values[index] >= 0 && values[index] <= 1, "Denoised visibility outside valid range");
        if (reference[index] <= 0.02f || reference[index] >= 0.98f) { continue; }
        double error = static_cast<double>(values[index]) - reference[index];
        sum += error * error; ++count;
    }
    Require(count > 100, "Shadow reference has no penumbra");
    return sum / static_cast<double>(count);
}
void RunShadowValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene;
    scene.Initialize();
    if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings;
    sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    DxrShadowRenderer shadows;
    shadows.Initialize();
    DxrShadowSettings settings;
    settings.isEnabled = true; settings.sunAngularRadiusRadians = 0; settings.sampleCount = 1;
    settings.isDenoisingEnabled = false;
    Require(shadows.SetSettings(settings), "Shadow settings rejected");
    DxrShadowSettings invalidSettings = settings;
    invalidSettings.sampleCount = 0;
    Require(!shadows.SetSettings(invalidSettings), "Zero shadow samples accepted");
    invalidSettings = settings; invalidSettings.normalBias = std::nanf("");
    Require(!shadows.SetSettings(invalidSettings), "Nonfinite shadow bias accepted");
    Object3dManager::GetInstance()->Initialize(dxCommon);
    Camera camera;
    camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 5}); camera.Update();
    Object3dManager::GetInstance()->SetDefaultCamera(&camera);
    LightingPreset lighting;
    lighting.direction = {1, 0, 1}; lighting.intensity = 1;
    lighting.ambient = {1, 1, 1, 0.2f}; lighting.pointIntensity = 0.6f;
    lighting.pointPosition = {0, 0, 1}; lighting.pointRadius = 10;
    LightManager::GetInstance()->ApplyLightingPreset(lighting);
    LightManager::GetInstance()->SetEnvironmentLighting(0, 0);
    OffscreenRenderer offscreen;
    offscreen.Initialize();
    PostEffectManager postEffects;
    postEffects.Initialize(dxCommon);
    ModelCommon modelCommon;
    modelCommon.Initialize(dxCommon);
    ModelData modelData;
    modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    modelData.materials.push_back({"resources/Textures/white.png"});
    MeshPrimitive primitive = {};
    primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices = {{{-1, -1, 0, 1}, {0, 0}, {0, 0, -1}}, {{-1, 1, 0, 1}, {0, 1}, {0, 0, -1}},
        {{1, -1, 0, 1}, {1, 0}, {0, 0, -1}}, {{1, 1, 0, 1}, {1, 1}, {0, 0, -1}}};
    primitive.indices = {0, 1, 2, 2, 1, 3}; modelData.primitives.push_back(primitive);
    Model model;
    model.Initialize(&modelCommon, modelData);
    Object3d receiver;
    receiver.Initialize(Object3dManager::GetInstance()); receiver.SetModel(&model);
    receiver.SetScale({4, 4, 1}); receiver.SetTranslate({0, 0, 5});
    receiver.SetShadingMode(MaterialShadingMode::Standard); receiver.SetReceiveShadow(true); receiver.SetCastShadow(false);
    receiver.GetMaterial()->shininess = 0;
    Object3d occluder;
    occluder.Initialize(Object3dManager::GetInstance()); occluder.SetModel(&model);
    occluder.SetScale({0.6f, 0.6f, 1}); occluder.SetTranslate({-2, 0, 3});
    occluder.SetShadingMode(MaterialShadingMode::Standard); occluder.SetCastShadow(true); occluder.SetReceiveShadow(false);
    TextureManager::GetInstance()->FlushUploads();
    settings.isEnabled = false; shadows.SetSettings(settings);
    auto* source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    Vector3 baseline = ReadTextureCenter(dxCommon, source, "shadow-off.png");
    scene.ReadCompleted(); shadows.ReadCompleted();
    dxCommon->PreDraw();
    Vector3 sun = ReadTextureCenter(dxCommon, offscreen.GetDirectionalLightTexture(), "shadow-direct-light.png");
    Require(sun.x > 0.4f && baseline.x > sun.x + 0.1f, "Directional capture lost local/ambient light");
    settings.isEnabled = true; shadows.SetSettings(settings);
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    Vector3 shadowed = ReadTextureCenter(dxCommon, source, "shadow-hard.png");
    scene.ReadCompleted(); shadows.ReadCompleted();
    RequireColor(shadowed, baseline - sun, "RT shadow dimmed ambient/local light or failed to remove sunlight");
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, nullptr, nullptr, true, nullptr, false);
    RequireColor(ReadTextureCenter(dxCommon, source, "shadow-undrawn-caster.png"), baseline - sun, "Undrawn caster disappeared from RT shadow scene");
    Require(scene.GetStatistics().instanceCount == 2, "Undrawn sun caster was not registered");
    std::ofstream report("runtime/captures/DxrTests/shadow-result.txt");
    report << "hard traceMs=" << shadows.GetTraceGpuTimeMs() << " compositeMs=" << shadows.GetCompositeGpuTimeMs()
        << " shadowTextureAllocationBytes=" << shadows.GetTextureAllocationBytes() << '\n';
    report << "directionalCaptureAllocationBytes=" << offscreen.GetDirectionalLightAllocationBytes() << '\n';
    dxCommon->PreDraw();
    size_t fractionalPixels = 0;
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "shadow-hard-mask.png", &fractionalPixels), {0, 0, 0}, "Occluder did not block sun ray");
    Require(fractionalPixels == 0, "Hard shadow contains fractional visibility");
    settings.sampleCount = 16; settings.sunAngularRadiusRadians = 0.1f; shadows.SetSettings(settings);
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    ReadTextureCenter(dxCommon, source, "shadow-soft.png"); scene.ReadCompleted(); shadows.ReadCompleted();
    report << "soft16 traceMs=" << shadows.GetTraceGpuTimeMs() << " compositeMs=" << shadows.GetCompositeGpuTimeMs() << '\n';
    dxCommon->PreDraw();
    ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "shadow-soft-mask.png", &fractionalPixels);
    Require(fractionalPixels > 50, "Area sun did not generate a penumbra");
    report << "fractionalVisibilityPixels=" << fractionalPixels << '\n';
    settings.sampleCount = 1; settings.sunAngularRadiusRadians = 0; shadows.SetSettings(settings);
    occluder.SetCastShadow(false);
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    RequireColor(ReadTextureCenter(dxCommon, source, "shadow-caster-disabled.png"), baseline, "CastShadow=false still occludes");
    occluder.SetCastShadow(true); occluder.SetTranslate({3, 0, 3});
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    RequireColor(ReadTextureCenter(dxCommon, source, "shadow-moved.png"), baseline, "Rigid shadow movement retained old placement");
    occluder.SetTranslate({0, 0, 3});
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    Vector3 foreground = ReadTextureCenter(dxCommon, source, "shadow-unsupported-foreground.png");
    settings.isEnabled = false; shadows.SetSettings(settings);
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    RequireColor(ReadTextureCenter(dxCommon, source, "shadow-foreground-off.png"), foreground, "Stale captured sunlight affected nonreceiver foreground");
    // ShadowToon has a separate lighting implementation; its local/ambient terms
    // must also survive the directional-only replacement.
    receiver.SetShadingMode(MaterialShadingMode::Toon);
    occluder.SetTranslate({-2, 0, 3});
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    Vector3 toonBaseline = ReadTextureCenter(dxCommon, source, "shadow-toon-off.png");
    dxCommon->PreDraw();
    Vector3 toonSun = ReadTextureCenter(dxCommon, offscreen.GetDirectionalLightTexture(), "shadow-toon-direct.png");
    settings.isEnabled = true; shadows.SetSettings(settings);
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    RequireColor(ReadTextureCenter(dxCommon, source, "shadow-toon-on.png"), toonBaseline - toonSun, "Toon capture changed ambient/local light");
    receiver.SetShadingMode(MaterialShadingMode::Standard);
    settings.sunAngularRadiusRadians = 0.1f; settings.sampleCount = 64;
    settings.isDenoisingEnabled = false; shadows.SetSettings(settings);
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    std::vector<float> reference;
    ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "shadow-denoise-reference64.png", nullptr, &reference);
    settings.sampleCount = 1; shadows.SetSettings(settings);
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder);
    std::vector<float> raw;
    ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "shadow-denoise-raw1.png", nullptr, &raw);
    settings.isDenoisingEnabled = true; settings.shouldUseTemporalHistory = true;
    settings.maxHistoryFrames = 32; settings.spatialPassCount = 2; shadows.SetSettings(settings);
    MotionVectorRenderer motionVectors;
    motionVectors.Initialize();
    for (uint32_t frame = 0; frame < 32; ++frame) {
        DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors);
        dxCommon->PostDraw(); scene.ReadCompleted(); shadows.ReadCompleted();
    }
    Require(shadows.HasUsedShadowHistory(), "Static scene did not reuse shadow history");
    double denoiseGpuTimeMs = shadows.GetDenoiseGpuTimeMs();
    std::vector<float> filtered;
    dxCommon->PreDraw();
    ReadTextureCenter(dxCommon, shadows.GetDenoisedMaskTexture(), "shadow-denoise-filtered1.png", nullptr, &filtered);
    std::vector<float> temporal;
    dxCommon->PreDraw();
    Vector3 historyCenter = ReadTextureCenter(dxCommon, shadows.GetDenoiseHistoryTexture(), "shadow-denoise-history.png", nullptr, &temporal);
    Require(historyCenter.y >= 30 && historyCenter.y <= 32, "Per-pixel temporal history did not accumulate");
    double rawError = ShadowMeanSquareError(raw, reference);
    double temporalError = ShadowMeanSquareError(temporal, reference);
    double filteredError = ShadowMeanSquareError(filtered, reference);
    Require(temporalError < rawError * 0.3, "Temporal accumulation did not reduce shadow noise");
    Require(filteredError < temporalError && filteredError < rawError * 0.15, "Spatial filtering did not reduce shadow noise");
    std::ofstream denoiseReport("runtime/captures/DxrTests/denoise-result.txt");
    denoiseReport << "penumbraMse raw1=" << rawError << " temporal32=" << temporalError << " filtered=" << filteredError << '\n';
    denoiseReport << "denoiseGpuMs=" << denoiseGpuTimeMs << " allocationBytes=" << shadows.GetDenoiseAllocationBytes() << '\n';
    settings.sunAngularRadiusRadians = 0.001f; settings.spatialPassCount = 0; shadows.SetSettings(settings);
    for (uint32_t frameIndex = 0; frameIndex < 4; ++frameIndex) {
        DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors); dxCommon->PostDraw();
    }
    occluder.SetTranslate({100, 100, 3});
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors);
    historyCenter = ReadTextureCenter(dxCommon, shadows.GetDenoiseHistoryTexture(), "shadow-history-moving-caster.png");
    Require(historyCenter.x == 1 && historyCenter.y == 1, "Moving sun caster retained stale visibility/count");
    occluder.SetTranslate({101, 100, 3});
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors);
    historyCenter = ReadTextureCenter(dxCommon, shadows.GetDenoiseHistoryTexture(), "shadow-history-unrelated-caster.png");
    Require(historyCenter.x == 1 && historyCenter.y >= 2, "Unrelated moving sun caster discarded stable pixel history");
    occluder.SetTranslate({-2, 0, 3}); settings.sunAngularRadiusRadians = 0.1f; settings.spatialPassCount = 2; shadows.SetSettings(settings);
    for (uint32_t frameIndex = 0; frameIndex < 2; ++frameIndex) {
        DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors); dxCommon->PostDraw();
    }
    camera.SetTranslate({0.1f, 0, -4}); camera.Update(); camera.SetProjectionJitter({0.0005f, -0.0005f});
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors);
    dxCommon->PostDraw(); scene.ReadCompleted(); shadows.ReadCompleted();
    Require(shadows.HasUsedShadowHistory(), "Camera movement or jitter discarded all history");
    dxCommon->PreDraw();
    historyCenter = ReadTextureCenter(dxCommon, shadows.GetDenoiseHistoryTexture(), "shadow-denoise-camera-motion.png");
    Require(historyCenter.y > 1, "Motion vectors or jitter reprojection rejected stable receiver");
    camera.ResetMotionHistory();
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors);
    dxCommon->PostDraw(); shadows.ReadCompleted();
    Require(!shadows.HasUsedShadowHistory(), "Camera cut reused old shadow history");
    dxCommon->PreDraw();
    historyCenter = ReadTextureCenter(dxCommon, shadows.GetDenoiseHistoryTexture(), "shadow-denoise-camera-cut.png");
    Require(historyCenter.y == 1, "Camera cut retained per-pixel history count");
    occluder.SetTranslate({3, 0, 3});
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors);
    dxCommon->PostDraw(); shadows.ReadCompleted();
    Require(shadows.HasUsedShadowHistory(), "Moving caster discarded unaffected shadow history globally");
    dxCommon->PreDraw();
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetDenoisedMaskTexture(), "shadow-denoise-caster-move.png"), {1, 1, 1}, "Caster movement left a shadow trail");
    LightManager::GetInstance()->SetDirection({-1, 0, 1});
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors);
    dxCommon->PostDraw(); shadows.ReadCompleted();
    Require(shadows.HasUsedShadowHistory(), "Sun change discarded unaffected shadow history globally");
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 1, &motionVectors);
    dxCommon->PostDraw(); shadows.ReadCompleted();
    Require(!shadows.HasUsedShadowHistory(), "Scene revision reused shadow history");
    settings.shouldUseTemporalHistory = false; shadows.SetSettings(settings);
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 1, &motionVectors);
    dxCommon->PostDraw(); shadows.ReadCompleted();
    Require(!shadows.HasUsedShadowHistory(), "Disabled temporal filter reused history");
    settings.isDenoisingEnabled = false; shadows.SetSettings(settings);
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 1, &motionVectors);
    dxCommon->PostDraw(); shadows.ReadCompleted();
    Require(shadows.GetDenoisedMaskTexture() == nullptr && shadows.GetDenoiseGpuTimeMs() == 0, "Disabled denoiser retained valid output");
    denoiseReport << "PASS: low-sample error reduction, motion/jitter reprojection, camera-cut/caster/sun/scene reset, temporal OFF, denoise OFF\n";
    settings.isEnabled = true; shadows.SetSettings(settings);
    DxrShadowInputs missingInputs;
    missingInputs.colorSrv = offscreen.GetSrvHandleGPU();
    Require(shadows.Draw(missingInputs).ptr == missingInputs.colorSrv.ptr && !shadows.HasValidFrame(), "Missing inputs reused old shadow output");
    scene.BeginFrame(); scene.EndFrame(&camera, false);
    missingInputs.scene = &scene;
    Require(shadows.Draw(missingInputs).ptr == missingInputs.colorSrv.ptr && !shadows.HasValidFrame(), "Empty scene reused old shadow output");
    dxCommon->PreDraw(); dxCommon->PostDraw();
    report << "PASS: Standard/Toon material capture, sunlight-only composition, hard/soft shadows, CastShadow, movement, nonreceiver foreground, invalid settings, missing inputs, empty scene\n";
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}
void DrawAlphaFrame(DirectXCommon* dxCommon, DxrRenderer& scene, OffscreenRenderer& offscreen,
    PostEffectManager& postEffects, Camera& camera, MotionVectorRenderer& motionVectors,
    Object3d& background, Object3d& foreground, Object3d& opaqueInstance) {
    scene.BeginFrame(); motionVectors.BeginFrame();
    postEffects.PreDrawDepth(); offscreen.PreDraw(postEffects.GetDepthDSVHandle());
    SrvManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->UpdateClusters(&camera); Object3dManager::GetInstance()->PreDraw();
    background.Update(); background.Draw(); foreground.Update(); foreground.Draw(); opaqueInstance.Update(); opaqueInstance.Draw();
    scene.EndFrame(&camera);
    Require(scene.GetStatistics().hasValidFrame, scene.GetStatus().c_str());
    motionVectors.EndFrame(postEffects.GetDepthDSVHandle());
    postEffects.PostDrawDepth(); offscreen.PostDraw(); dxCommon->PreDraw();
}
void RunAlphaValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene;
    scene.Initialize();
    if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings;
    sceneSettings.isEnabled = true; sceneSettings.debugMode = DxrDebugMode::BaseColor; scene.SetSettings(sceneSettings);
    Camera camera;
    camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 5}); camera.Update();
    Object3dManager::GetInstance()->SetDefaultCamera(&camera);
    LightingPreset lighting;
    lighting.direction = {1, 0, 1}; lighting.intensity = 1; lighting.ambient = {1, 1, 1, 0.2f};
    LightManager::GetInstance()->ApplyLightingPreset(lighting); LightManager::GetInstance()->SetEnvironmentLighting(0, 0);
    std::vector<uint8_t> maskPixels(16 * 16 * 4, 0);
    for (uint32_t row = 0; row < 16; ++row) {
        for (uint32_t column = 0; column < 16; ++column) {
            size_t index = (row * 16 + column) * 4;
            maskPixels[index + 1] = 255;
            if (row < 4 || row >= 12 || column < 4 || column >= 12) { maskPixels[index + 3] = 255; }
        }
    }
    TextureManager::GetInstance()->LoadTextureFromBGRA("dxr-alpha-ring", maskPixels.data(), 16, 16);
    ModelCommon modelCommon;
    modelCommon.Initialize(dxCommon);
    ModelData modelData;
    modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    modelData.materials = {{"dxr-alpha-ring"}};
    MeshPrimitive primitive = {};
    primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices = {{{-1, -1, 0, 1}, {0, 0}, {0, 0, -1}}, {{-1, 1, 0, 1}, {0, 1}, {0, 0, -1}},
        {{1, -1, 0, 1}, {1, 0}, {0, 0, -1}}, {{1, 1, 0, 1}, {1, 1}, {0, 0, -1}}};
    primitive.indices = {0, 1, 2, 2, 1, 3}; modelData.primitives = {primitive};
    Model maskedModel;
    maskedModel.Initialize(&modelCommon, modelData);
    modelData.materials[0].textureFilePath = "resources/Textures/white.png";
    Model backgroundModel;
    backgroundModel.Initialize(&modelCommon, modelData);
    Object3d background;
    background.Initialize(Object3dManager::GetInstance()); background.SetModel(&backgroundModel);
    background.SetTranslate({0, 0, 1}); background.SetColor({1, 0, 0, 1});
    Object3d foreground;
    foreground.Initialize(Object3dManager::GetInstance()); foreground.SetModel(&maskedModel);
    foreground.SetCastShadow(true);
    Require(foreground.SetAlphaCutoff(0.5f), "Alpha cutoff rejected");
    Require(!foreground.SetAlphaCutoff(-1) && !foreground.SetAlphaCutoff(std::nanf("")) && !foreground.SetAlphaCutoff(1.1f), "Invalid alpha cutoff accepted");
    Object3d opaqueInstance;
    opaqueInstance.Initialize(Object3dManager::GetInstance()); opaqueInstance.SetModel(&maskedModel); opaqueInstance.SetTranslate({30, 0, 0});
    TextureManager::GetInstance()->FlushUploads();
    OffscreenRenderer offscreen;
    offscreen.Initialize();
    PostEffectManager postEffects;
    postEffects.Initialize(dxCommon);
    MotionVectorRenderer motionVectors;
    motionVectors.Initialize();
    DrawAlphaFrame(dxCommon, scene, offscreen, postEffects, camera, motionVectors, background, foreground, opaqueInstance);
    Require(scene.GetStatistics().blasCount == 3, "Masked and opaque instances shared incompatible BLAS flags");
    RequireColor(ReadTextureCenter(dxCommon, offscreen.GetColorTexture(), "alpha-raster-hole.png"), {1, 0, 0}, "Raster hole wrote foreground color/depth");
    dxCommon->PreDraw();
    RequireColor(ReadCenter(dxCommon, scene, "alpha-trace-hole.png"), {1, 0, 0}, "AnyHit did not continue to opaque background");
    foreground.SetTranslate({0.1f, 0, 0});
    DrawAlphaFrame(dxCommon, scene, offscreen, postEffects, camera, motionVectors, background, foreground, opaqueInstance);
    std::vector<float> motionValues;
    Vector3 motion = ReadTextureCenter(dxCommon, motionVectors.GetTexture(), "alpha-motion-hole.png", nullptr, &motionValues);
    Require(std::abs(motion.x) < 0.0001f && std::abs(motion.y) < 0.0001f, "Transparent hole overwrote background motion");
    float maximumMotion = 0;
    for (float value : motionValues) { maximumMotion = (std::max)(maximumMotion, std::abs(value)); }
    Require(maximumMotion > 0.005f, "Alpha test removed motion from opaque border");
    foreground.SetTranslate({0, 0, 0});
    foreground.GetMaterial()->uvTransform.m[3][0] = 0.4f;
    DrawAlphaFrame(dxCommon, scene, offscreen, postEffects, camera, motionVectors, background, foreground, opaqueInstance);
    RequireColor(ReadTextureCenter(dxCommon, offscreen.GetColorTexture(), "alpha-raster-uv.png"), {0, 1, 0}, "Raster alpha ignored UV transform");
    dxCommon->PreDraw(); RequireColor(ReadCenter(dxCommon, scene, "alpha-trace-uv.png"), {0, 1, 0}, "AnyHit alpha ignored UV transform");
    Require(foreground.SetAlphaCutoff(1), "Cutoff one rejected");
    DrawAlphaFrame(dxCommon, scene, offscreen, postEffects, camera, motionVectors, background, foreground, opaqueInstance);
    RequireColor(ReadCenter(dxCommon, scene, "alpha-trace-equality.png"), {0, 1, 0}, "Alpha equal to cutoff was rejected");
    foreground.SetAlphaCutoff(0.5f); foreground.SetColor({1, 1, 1, 0.25f});
    DrawAlphaFrame(dxCommon, scene, offscreen, postEffects, camera, motionVectors, background, foreground, opaqueInstance);
    RequireColor(ReadCenter(dxCommon, scene, "alpha-trace-material.png"), {1, 0, 0}, "AnyHit ignored material color alpha");
    foreground.SetColor({1, 1, 1, 1}); foreground.GetMaterial()->uvTransform = MatrixMath::MakeIdentity4x4();
    foreground.SetAlphaCutoff(0);
    DrawAlphaFrame(dxCommon, scene, offscreen, postEffects, camera, motionVectors, background, foreground, opaqueInstance);
    RequireColor(ReadCenter(dxCommon, scene, "alpha-disabled.png"), {0, 1, 0}, "Cutoff zero did not restore opaque geometry");
    foreground.SetAlphaCutoff(0.5f);
    const std::string kMaterials[] = {"Unlit", "Standard", "ShadowStandard", "Toon", "ShadowToon"};
    for (const auto& materialName : kMaterials) {
        foreground.SetMaterial("resources/Shaders/Object3D/" + materialName);
        DrawAlphaFrame(dxCommon, scene, offscreen, postEffects, camera, motionVectors, background, foreground, opaqueInstance);
        std::string captureName = "alpha-raster-" + materialName + ".png";
        RequireColor(ReadTextureCenter(dxCommon, offscreen.GetColorTexture(), captureName.c_str()), {1, 0, 0}, "A supported material failed raster alpha masking");
    }
    ShadowMapRenderer shadowMap;
    try {
        shadowMap.Initialize(dxCommon, 512);
        shadowMap.UpdatePerspective({0, 0, -4}, {0, 0, 1}, 20, 0.7f);
        foreground.Update(); shadowMap.BeginShadowPass(); foreground.DrawShadow(shadowMap); shadowMap.EndShadowPass(); dxCommon->PreDraw();
    }
    catch (...) {
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> validationQueue;
        dxCommon->GetDevice()->QueryInterface(IID_PPV_ARGS(&validationQueue));
        CheckValidationMessages(validationQueue.Get());
        throw;
    }
    Require(ReadTextureCenter(dxCommon, shadowMap.GetDepthTexture(), "alpha-shadowmap-hole.png").x > 0.999f, "Shadow map filled alpha hole");
    foreground.GetMaterial()->uvTransform.m[3][0] = 0.4f;
    shadowMap.BeginShadowPass(); foreground.DrawShadow(shadowMap); shadowMap.EndShadowPass(); dxCommon->PreDraw();
    Require(ReadTextureCenter(dxCommon, shadowMap.GetDepthTexture(), "alpha-shadowmap-uv.png").x < 0.99f, "Shadow map ignored UV-transformed alpha border");
    DxrShadowRenderer shadows;
    shadows.Initialize();
    DxrShadowSettings settings;
    settings.isEnabled = true; settings.isDenoisingEnabled = false; settings.sunAngularRadiusRadians = 0;
    shadows.SetSettings(settings);
    background.SetColor({1, 1, 1, 1}); background.SetScale({4, 4, 1}); background.SetTranslate({0, 0, 5});
    background.SetShadingMode(MaterialShadingMode::Standard); background.SetReceiveShadow(true); background.GetMaterial()->shininess = 0;
    foreground.SetShadingMode(MaterialShadingMode::Standard); foreground.SetScale({0.6f, 0.6f, 1}); foreground.SetTranslate({-2, 0, 3});
    foreground.GetMaterial()->uvTransform = MatrixMath::MakeIdentity4x4();
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, background, foreground, 0, &motionVectors);
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "alpha-rt-shadow-hole.png"), {1, 1, 1}, "RT shadow filled alpha hole");
    shadows.ReadCompleted(); double maskedTraceGpuTimeMs = shadows.GetTraceGpuTimeMs();
    foreground.SetAlphaCutoff(0);
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, background, foreground, 0, &motionVectors);
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "alpha-rt-shadow-disabled.png"), {0, 0, 0}, "Opaque shadow disappeared after disabling alpha mask");
    shadows.ReadCompleted(); double opaqueTraceGpuTimeMs = shadows.GetTraceGpuTimeMs();
    foreground.SetAlphaCutoff(0.5f);
    opaqueInstance.SetScale({0.6f, 0.6f, 1}); opaqueInstance.SetTranslate({-3, 0, 2}); opaqueInstance.SetCastShadow(true);
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, background, foreground, 0, &motionVectors, nullptr, true, &opaqueInstance);
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "alpha-rt-shadow-layered.png"), {0, 0, 0}, "Ignoring alpha hit skipped solid blocker behind hole");
    opaqueInstance.SetCastShadow(false);
    settings.sunAngularRadiusRadians = 0.1f; settings.sampleCount = 1; settings.isDenoisingEnabled = true; shadows.SetSettings(settings);
    for (uint32_t index = 0; index < 3; ++index) {
        DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, background, foreground, 0, &motionVectors);
        dxCommon->PostDraw(); scene.ReadCompleted(); shadows.ReadCompleted();
    }
    Require(shadows.HasUsedShadowHistory(), "Stable alpha mask rejected all shadow history");
    foreground.GetMaterial()->uvTransform.m[3][0] = 0.4f;
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, background, foreground, 0, &motionVectors);
    dxCommon->PostDraw(); shadows.ReadCompleted();
    Require(shadows.HasUsedShadowHistory(), "UV alpha change discarded unaffected shadow history globally");
    maskedModel.SetTexture("resources/Textures/white.png");
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, background, foreground, 0, &motionVectors);
    dxCommon->PostDraw(); shadows.ReadCompleted();
    Require(shadows.HasUsedShadowHistory(), "Alpha texture replacement discarded unaffected shadow history globally");
    std::ofstream report("runtime/captures/DxrTests/alpha-result.txt");
    report << "hardShadowTraceMs masked=" << maskedTraceGpuTimeMs << " opaque=" << opaqueTraceGpuTimeMs << '\n';
    report << "PASS: raster/RT holes, continuation to background/blocker, UV transforms, material alpha, cutoff equality/disabled/invalid, shared masked/opaque model, motion-hole rejection, Standard/Toon variants, shadow map holes, UV/texture history invalidation\n";
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}
struct RaytracingTestSceneInputs {
    const std::vector<Object3d*>* raytracingObjects = nullptr;
    bool shouldDrawSkinnedTarget = true;
    bool shouldEnableSceneCulling = false;
    DxrSceneBounds cullingSphere;
};
template<class LightingPass>
ID3D12Resource* DrawReflectionFrame(DirectXCommon* dxCommon, DxrRenderer& scene, LightingPass& reflections,
    OffscreenRenderer& offscreen, PostEffectManager& postEffects, MotionVectorRenderer& motionVectors, Camera& camera,
    const std::vector<Object3d*>& objects, uint64_t sceneRevision = 0, SkinningObject3d* skinnedTarget = nullptr, DxrLocalShadowRenderer* localShadows = nullptr, bool shouldExpectValidFrame = true,
    const RaytracingTestSceneInputs* sceneInputs = nullptr) {
    scene.BeginFrame(); motionVectors.BeginFrame();
    scene.SetDrawSubmissionEnabled(false);
    if (sceneInputs && sceneInputs->shouldEnableSceneCulling) {
        Require(scene.SetSceneCullingSphere(sceneInputs->cullingSphere.center, sceneInputs->cullingSphere.radius), "Invalid RT scene selection");
    }
    bool shouldCaptureLocalShadows = false;
    if (localShadows) { localShadows->Prepare(scene); shouldCaptureLocalShadows = localShadows->GetSelectedLightCount() > 0; }
    postEffects.PreDrawDepth(); offscreen.PreDraw(postEffects.GetDepthDSVHandle(), false, true, shouldCaptureLocalShadows);
    Require(offscreen.IsReflectionCaptureActive(), "Reflection material capture not available");
    scene.SetReflectionCapture(true);
    scene.SetLocalShadowCapture(offscreen.IsLocalShadowCaptureActive());
    SrvManager::GetInstance()->PreDraw(); LightManager::GetInstance()->UpdateClusters(&camera);
    Object3dManager::GetInstance()->PreDraw();
    const auto* raytracingObjects = &objects;
    if (sceneInputs && sceneInputs->raytracingObjects) { raytracingObjects = sceneInputs->raytracingObjects; }
    for (auto* object : *raytracingObjects) { object->Update(); object->SubmitRaytracing(scene); }
    for (auto* object : objects) {
        if (std::find(raytracingObjects->begin(), raytracingObjects->end(), object) == raytracingObjects->end()) { object->Update(); }
        object->Draw();
    }
    if (skinnedTarget != nullptr) {
        skinnedTarget->Update(); skinnedTarget->SubmitRaytracing(scene);
        if (!sceneInputs || sceneInputs->shouldDrawSkinnedTarget) { SkinningObject3dManager::GetInstance()->PreDraw(); skinnedTarget->Draw(); }
    }
    scene.EndFrame(&camera, false);
    if (shouldExpectValidFrame) { Require(scene.HasValidScene(), scene.GetStatus().c_str()); }
    motionVectors.EndFrame(postEffects.GetDepthDSVHandle()); postEffects.PostDrawDepth(); offscreen.PostDraw();
    DxrReflectionInputs inputs;
    inputs.scene = &scene; inputs.camera = &camera; inputs.colorSrv = offscreen.GetSrvHandleGPU();
    inputs.depthTexture = postEffects.GetDepthTexture(); inputs.depthSrv = postEffects.GetDepthSrv();
    inputs.surfaceTexture = offscreen.GetReflectionSurfaceTexture(); inputs.surfaceSrv = offscreen.GetReflectionSurfaceSrv();
    inputs.environmentTexture = offscreen.GetReflectionEnvironmentTexture(); inputs.environmentSrv = offscreen.GetReflectionEnvironmentSrv();
    inputs.materialTexture = offscreen.GetMaterialTexture(); inputs.materialSrv = offscreen.GetMaterialSrvHandleGPU();
    inputs.localLightTexture = offscreen.GetLocalLightTexture(); inputs.localLightSrv = offscreen.GetLocalLightSrv();
    inputs.indirectTexture = offscreen.GetIndirectTexture(); inputs.indirectSrv = offscreen.GetIndirectSrvHandleGPU();
    inputs.motionVectorSrv = motionVectors.GetSrvHandle(); inputs.reprojectionSrv = motionVectors.GetReprojectionSrv(); inputs.previousReprojectionSrv = motionVectors.GetPreviousReprojectionSrv(); inputs.sceneRevision = sceneRevision;
    auto result = reflections.Draw(inputs);
    dxCommon->PreDraw();
    if (reflections.GetSettings().isEnabled && shouldExpectValidFrame) { Require(reflections.HasValidFrame(), reflections.GetStatus().c_str()); return reflections.GetColorTexture(); }
    Require(result.ptr == inputs.colorSrv.ptr && !reflections.HasValidFrame(), "Disabled RT reflection changed the source image");
    return offscreen.GetColorTexture();
}
double ReflectionMeanSquareError(const std::vector<float>& values, const std::vector<float>& reference) {
    Require(values.size() == reference.size(), "Reflection reference dimensions mismatch");
    double errorSum = 0; size_t count = 0;
    for (size_t index = 0; index < values.size(); ++index) {
        Require(std::isfinite(values[index]) && values[index] >= 0 && values[index] < 65000, "Nonfinite reflection radiance");
        if (reference[index] <= 0.03f || reference[index] >= 0.9f) { continue; }
        double difference = static_cast<double>(values[index]) - reference[index]; errorSum += difference * difference; ++count;
    }
    Require(count > 100, "Rough reflection reference has no measurable edge");
    return errorSum / static_cast<double>(count);
}
void RunReflectionValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene;
    scene.Initialize(); if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings; sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    DxrReflectionRenderer reflections; reflections.Initialize();
    DxrReflectionSettings settings;
    settings.isEnabled = true; settings.sampleCount = 1; settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0;
    Require(reflections.SetSettings(settings), "Reflection settings rejected");
    auto invalid = settings; invalid.sampleCount = 0; Require(!reflections.SetSettings(invalid), "Zero reflection samples accepted");
    invalid = settings; invalid.maxDistance = std::nanf(""); Require(!reflections.SetSettings(invalid), "Nonfinite reflection distance accepted");
    Camera camera;
    camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    Object3dManager::GetInstance()->SetDefaultCamera(&camera);
    auto* lights = LightManager::GetInstance();
    LightingPreset lighting; lighting.intensity = 0; lighting.ambient = {1, 1, 1, 0.2f}; lighting.pointIntensity = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0); lights->SetLightingComponents(1, 1, 1, 0);
    ModelCommon modelCommon; modelCommon.Initialize(dxCommon);
    ModelData modelData;
    modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4(); modelData.materials = {{"resources/Textures/white.png"}};
    MeshPrimitive primitive = {}; primitive.mode = PrimitiveMode::Triangles;
    const float kDiagonal = 0.70710678f;
    primitive.vertices = {{{-1, -1, 0, 1}, {0, 0}, {kDiagonal, 0, -kDiagonal}}, {{-1, 1, 0, 1}, {0, 1}, {kDiagonal, 0, -kDiagonal}},
        {{1, -1, 0, 1}, {1, 0}, {kDiagonal, 0, -kDiagonal}}, {{1, 1, 0, 1}, {1, 1}, {kDiagonal, 0, -kDiagonal}}};
    primitive.indices = {0, 1, 2, 2, 1, 3}; modelData.primitives = {primitive};
    modelData.rootNode.name = "reflectionRoot";
    modelData.rootNode.transform = {{1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0}};
    JointWeightData skinWeights; skinWeights.inverseBindPoseMatrix = MatrixMath::MakeIdentity4x4();
    for (uint32_t index = 0; index < 4; ++index) { skinWeights.vertexWeights.push_back({1, index}); }
    modelData.skinClusterData["reflectionRoot"] = skinWeights;
    Model model; model.Initialize(&modelCommon, modelData);
    Object3d receiver; receiver.Initialize(Object3dManager::GetInstance()); receiver.SetModel(&model);
    receiver.SetShadingMode(MaterialShadingMode::Standard);
    receiver.GetMaterial()->roughness = 0; receiver.GetMaterial()->metallic = 1; receiver.GetMaterial()->specularStrength = 1;
    receiver.GetMaterial()->shininess = 0;
    Object3d target; target.Initialize(Object3dManager::GetInstance()); target.SetModel(&model);
    target.SetRotate({0, 1.57079633f, 0}); target.SetTranslate({4, 0, 0}); target.SetColor({1, 0, 0, 1});
    target.GetMaterial()->specularStrength = 0; target.GetMaterial()->shininess = 0;
    target.Update();
    for (const VertexData& vertex : primitive.vertices) {
        auto world = MatrixMath::Transform({vertex.position.x, vertex.position.y, vertex.position.z}, target.GetWorldMatrix());
        auto ndc = MatrixMath::Transform(world, camera.GetViewProjectionMatrix());
        Require(ndc.x > 1, "Reflection target is visible on screen");
    }
    Object3d blocker; blocker.Initialize(Object3dManager::GetInstance()); blocker.SetModel(&model);
    blocker.SetTranslate({4, 0, -2}); blocker.SetColor({0, 0, 0, 1});
    std::vector<Object3d*> objects = {&receiver, &target};
    TextureManager::GetInstance()->FlushUploads();
    OffscreenRenderer offscreen; offscreen.Initialize();
    PostEffectManager postEffects; postEffects.Initialize(dxCommon);
    MotionVectorRenderer motionVectors; motionVectors.Initialize();
    settings.isEnabled = false; reflections.SetSettings(settings);
    auto* source = DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    Vector3 baseline = ReadTextureCenter(dxCommon, source, "reflection-off.png");
    settings.isEnabled = true; reflections.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    Vector3 reflected = ReadTextureCenter(dxCommon, source, "reflection-offscreen-object.png");
    Require(reflected.x > baseline.x + 0.8f && std::abs(reflected.y - baseline.y) < 0.02f, "Offscreen red target did not appear in reflection");
    dxCommon->PreDraw(); RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-mirror-raw.png"), {1, 0, 0}, "Mirror reflected radiance mismatch");
    scene.ReadCompleted(); reflections.ReadCompleted();
    std::ofstream report("runtime/captures/DxrTests/reflection-result.txt");
    report << "mirrorTraceMs=" << reflections.GetTraceGpuTimeMs() << " compositeMs=" << reflections.GetCompositeGpuTimeMs()
        << " targetsBytes=" << reflections.GetAllocationBytes() << " captureBytes=" << offscreen.GetReflectionAllocationBytes()
        << " directionalSlotBytes=" << offscreen.GetDirectionalLightAllocationBytes() << '\n';
    std::ofstream sceneReport("runtime/captures/DxrTests/scene-submission-result.txt");
    std::vector<Object3d*> rasterObjects = {&receiver};
    std::vector<Object3d*> raytracingObjects = {&receiver, &target, &target};
    RaytracingTestSceneInputs sceneInputs; sceneInputs.raytracingObjects = &raytracingObjects;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, rasterObjects, 0, nullptr, nullptr, true, &sceneInputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-undrawn-target.png"), {1, 0, 0}, "Undrawn target disappeared from reflection");
    Require(scene.GetStatistics().instanceCount == 2, "Explicit submission duplicated an instance");
    target.SetTranslate({8, 0, 0}); target.Update(); target.SetFrustumCullingEnabled(true);
    Require(!target.IsVisible(camera) && receiver.IsVisible(camera), "Raster frustum selection did not distinguish the offscreen target");
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects, 0, nullptr, nullptr, true, &sceneInputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-frustum-culled-target.png"), {1, 0, 0}, "Raster frustum culling removed RT reflection target");
    sceneInputs.shouldEnableSceneCulling = true; sceneInputs.cullingSphere = {{0, 0, -4}, 2};
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, rasterObjects, 0, nullptr, nullptr, true, &sceneInputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-rt-range-excluded.png"), {0, 0, 0}, "RT range selection retained distant target");
    Require(scene.GetStatistics().instanceCount == 1 && scene.GetStatistics().culledInstanceCount == 1, "RT range selection/dedup count mismatch");
    sceneInputs.shouldEnableSceneCulling = false; target.SetRaytracingEnabled(false);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, rasterObjects, 0, nullptr, nullptr, true, &sceneInputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-rt-disabled-target.png"), {0, 0, 0}, "RT-disabled target entered scene");
    target.SetRaytracingEnabled(true); target.SetTranslate({4, 0, 0}); target.SetFrustumCullingEnabled(false);
    auto temporaryObject = std::make_unique<Object3d>(); temporaryObject->Initialize(Object3dManager::GetInstance()); temporaryObject->SetModel(&model);
    temporaryObject->SetTranslate({100, 100, 100}); raytracingObjects.push_back(temporaryObject.get());
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, rasterObjects, 0, nullptr, nullptr, true, &sceneInputs); dxCommon->PostDraw();
    Require(scene.GetStatistics().instanceCount == 3, "Undrawn added object was not registered");
    raytracingObjects.pop_back(); temporaryObject.reset();
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, rasterObjects, 0, nullptr, nullptr, true, &sceneInputs); dxCommon->PostDraw();
    Require(scene.GetStatistics().instanceCount == 2, "Deleted object survived the next frame's scene registration");
    raytracingObjects.clear();
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, rasterObjects, 0, nullptr, nullptr, false, &sceneInputs); dxCommon->PostDraw();
    Require(scene.GetStatistics().instanceCount == 0 && !scene.HasValidScene(), "Raster Draw repopulated an explicitly empty RT scene");
    sceneReport << "PASS: independent raster/RT lists, undrawn reflection target, frustum culling, RT range, duplicate submissions, RT opt-out, addition/deletion, empty scene with raster Draw\n";
    // Validate per-pixel accumulation, not merely the global history-available flag.
    settings.shouldUseTemporalHistory = true; reflections.SetSettings(settings);
    for (uint32_t frameIndex = 0; frameIndex < 4; ++frameIndex) {
        DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    }
    dxCommon->PreDraw();
    Vector3 statistics = ReadTextureCenter(dxCommon, reflections.GetHistoryStatisticsTexture(), "reflection-history-stable.png");
    Require(statistics.z >= 4, "Mirror history count did not accumulate");
    blocker.SetTranslate({100, 100, 100}); objects.push_back(&blocker);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    statistics = ReadTextureCenter(dxCommon, reflections.GetHistoryStatisticsTexture(), "reflection-history-unrelated-object.png");
    Require(statistics.z >= 5, "Unrelated object invalidated a stable mirror pixel");
    report << "unrelatedObjectHistoryCount=" << statistics.z << '\n';
    target.SetTranslate({5, 0, 0});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    statistics = ReadTextureCenter(dxCommon, reflections.GetHistoryStatisticsTexture(), "reflection-history-hit-distance.png");
    Require(statistics.z == 1, "Same-colored moving reflected surface retained its old hit position");
    report << "movingHitHistoryCount=" << statistics.z << '\n';
    target.SetColor({0, 1, 0, 1});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetFilteredTexture(), "reflection-history-color-change.png"), {0, 1, 0}, "Changed reflection retained a red trail");
    dxCommon->PreDraw();
    statistics = ReadTextureCenter(dxCommon, reflections.GetHistoryStatisticsTexture(), "reflection-history-color-count.png");
    Require(statistics.z == 1, "Changed reflected radiance retained its old accumulation count");
    target.SetTranslate({5, 5, 0});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetFilteredTexture(), "reflection-history-disocclusion.png"), {0, 0, 0}, "Disoccluded reflection retained old radiance");
    objects.pop_back(); blocker.SetTranslate({4, 0, -2}); target.SetTranslate({4, 0, 0}); target.SetColor({1, 0, 0, 1});
    settings.shouldUseTemporalHistory = false; reflections.SetSettings(settings);
    lights->SetEnvironmentLighting(0, 1);
    settings.isEnabled = false; reflections.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    baseline = ReadTextureCenter(dxCommon, source, "reflection-environment-baseline.png");
    dxCommon->PreDraw(); Vector3 environment = ReadTextureCenter(dxCommon, offscreen.GetReflectionEnvironmentTexture(), "reflection-captured-environment.png");
    settings.isEnabled = true; reflections.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    reflected = ReadTextureCenter(dxCommon, source, "reflection-environment-replaced.png");
    RequireColor(reflected, baseline - environment + Vector3{1, 0, 0}, "RT reflection double-counted environment or removed diffuse lighting");
    target.SetTranslate({4, 4, 0});
    source = DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "lighting-reflection-environment-miss.png"), baseline, "Reflection miss discarded environment specular fallback");
    lights->SetEnvironmentLighting(0, 0);
    source = DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    baseline = ReadTextureCenter(dxCommon, source, "reflection-miss.png");
    dxCommon->PreDraw(); RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-miss-raw.png"), {0, 0, 0}, "Reflection miss retained an old target");
    RequireColor(baseline, {0, 0, 0}, "Metal reflection miss generated diffuse base lighting");
    target.SetTranslate({4, 0, 0}); target.SetShadingMode(MaterialShadingMode::Standard);
    target.GetMaterial()->specularStrength = 0; target.GetMaterial()->shininess = 0;
    lighting.intensity = 1; lighting.direction = {0, 0, 1}; lighting.ambient.w = 0; lights->ApplyLightingPreset(lighting);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-lit-hit.png"), {kDiagonal, 0, 0}, "Reflection hit ignored material lighting");
    blocker.SetCastShadow(true); target.SetReceiveShadow(true); objects.push_back(&blocker);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-hit-shadowed.png"), {0, 0, 0}, "Sun shadow ray at reflected hit missed caster");
    settings.shouldTraceSunShadows = false; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-hit-shadows-off.png"), {kDiagonal, 0, 0}, "Disabling reflection-hit shadows did not restore sunlight");
    settings.shouldTraceSunShadows = true; reflections.SetSettings(settings); objects.pop_back(); target.SetReceiveShadow(false);
    target.SetShadingMode(MaterialShadingMode::Unlit); target.SetAlphaCutoff(0.5f); target.SetColor({1, 0, 0, 0.25f});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-alpha-hole.png"), {0, 0, 0}, "Reflection AnyHit failed to ignore transparent target");
    target.SetColor({1, 0, 0, 1}); target.SetAlphaCutoff(0);
    lighting.intensity = 0; lighting.ambient.w = 0.2f; lights->ApplyLightingPreset(lighting);
    receiver.GetMaterial()->roughness = 0.6f;
    settings.sampleCount = 16; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    std::vector<float> reference; ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-rough-reference16.png", nullptr, &reference);
    settings.sampleCount = 1; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    std::vector<float> raw; ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-rough-raw1.png", nullptr, &raw);
    settings.shouldUseTemporalHistory = true; settings.spatialPassCount = 2; reflections.SetSettings(settings);
    for (uint32_t index = 0; index < 32; ++index) {
        DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
        dxCommon->PostDraw(); scene.ReadCompleted(); reflections.ReadCompleted();
    }
    Require(reflections.HasUsedHistory(), "Static reflection did not reuse history");
    double roughTraceMs = reflections.GetTraceGpuTimeMs(); double filterMs = reflections.GetFilterGpuTimeMs();
    dxCommon->PreDraw(); std::vector<float> filtered;
    ReadTextureCenter(dxCommon, reflections.GetFilteredTexture(), "reflection-rough-filtered.png", nullptr, &filtered);
    double rawError = ReflectionMeanSquareError(raw, reference); double filteredError = ReflectionMeanSquareError(filtered, reference);
    Require(filteredError < rawError * 0.5, "Reflection denoising failed to reduce rough-surface noise");
    report << "roughTraceMs=" << roughTraceMs << " filterMs=" << filterMs << " mseRaw=" << rawError << " mseFiltered=" << filteredError << '\n';
    camera.SetTranslate({0.01f, 0, -4}); camera.Update(); camera.SetProjectionJitter({0.0005f, -0.0005f});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    dxCommon->PostDraw(); reflections.ReadCompleted(); Require(reflections.HasUsedHistory(), "Camera movement discarded static reflection history");
    camera.ResetMotionHistory();
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    dxCommon->PostDraw(); Require(!reflections.HasUsedHistory(), "Camera cut reused reflection history");
    target.SetTranslate({4, 0.1f, 0});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    dxCommon->PostDraw(); Require(reflections.HasUsedHistory(), "Reflection target movement discarded unaffected history globally");
    target.SetColor({0, 1, 0, 1});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    dxCommon->PostDraw(); Require(reflections.HasUsedHistory(), "Reflection material change discarded unaffected history globally");
    lights->SetIntensity(0.5f);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    dxCommon->PostDraw(); Require(reflections.HasUsedHistory(), "Reflection lighting change discarded unaffected history globally");
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects, 1);
    dxCommon->PostDraw(); Require(!reflections.HasUsedHistory(), "Scene revision reused reflection history");
    // ClosestHit reads opaque GPU-skinned vertices after the motion-history copy.
    receiver.GetMaterial()->roughness = 0; settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0;
    reflections.SetSettings(settings); camera.ResetMotionHistory(); camera.SetTranslate({0, 0, -4}); camera.Update(); camera.SetProjectionJitter({});
    auto* skinManager = SkinningObject3dManager::GetInstance(); skinManager->SetDefaultCamera(&camera);
    skinManager->SetBlendMode(kBlendModeNone); skinManager->SetEnvironmentTexture(Object3dManager::GetInstance()->GetEnvironmentTexture());
    Skeleton skeleton = Skeleton::CreateSkeleton(modelData.rootNode); skeleton.UpdateSkeleton();
    PlayAnimation animation; animation.SetSkeleton(&skeleton);
    SkinningObject3d skinnedTarget; skinnedTarget.SetModel(&model); skinnedTarget.SetAnimation(&animation); skinnedTarget.Initialize(skinManager);
    skinnedTarget.SetTranslate({4, 0, 0}); skinnedTarget.SetRotate({0, 1.57079633f, 0}); skinnedTarget.SetColor({1, 0, 0, 1});
    const std::vector<Object3d*> kSkinObjects = {&receiver};
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinnedTarget);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-skinned-hit.png"), {1, 0, 0}, "Opaque skinned ClosestHit read wrong vertex data/state");
    RaytracingTestSceneInputs skinSceneInputs; skinSceneInputs.shouldDrawSkinnedTarget = false;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinnedTarget, nullptr, true, &skinSceneInputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-undrawn-skinned-hit.png"), {1, 0, 0}, "Undrawn skinned model disappeared from reflection");
    skinSceneInputs.shouldEnableSceneCulling = true; skinSceneInputs.cullingSphere = {{0, 0, -4}, 2};
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinnedTarget, nullptr, true, &skinSceneInputs);
    Require(scene.GetStatistics().dynamicBlasCount == 1, "Unknown deformed bounds incorrectly culled skinned model");
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-unknown-skin-bounds.png"), {1, 0, 0}, "Unknown skin bounds lost reflection");
    Require(skinnedTarget.SetRaytracingBounds({{0, 0, 0}, 2}), "Valid deformed bounds rejected");
    Require(!skinnedTarget.SetRaytracingBounds({{0, 0, 0}, std::nanf("")}), "Nonfinite deformed bounds accepted");
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinnedTarget, nullptr, true, &skinSceneInputs);
    Require(scene.GetStatistics().dynamicBlasCount == 0 && scene.GetStatistics().culledInstanceCount == 1, "Trusted deformed bounds did not select RT range");
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-trusted-skin-bounds.png"), {0, 0, 0}, "RT range excluded skin remained in TLAS");
    Require(skinnedTarget.SetRaytracingBounds({}), "Clearing deformed bounds failed");
    skinSceneInputs.shouldEnableSceneCulling = false;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinnedTarget, nullptr, true, &skinSceneInputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-skin-range-restored.png"), {1, 0, 0}, "Returning skinned object failed to rebuild scene");
    skeleton.joints[0].transform.translate.x = 4; skeleton.UpdateSkeleton();
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinnedTarget, nullptr, true, &skinSceneInputs);
    Require(scene.GetStatistics().updatedBlasCount == 1, "Reflected skinning pose did not refit");
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-skinned-moved.png"), {0, 0, 0}, "Reflection retained old skinned pose");
    skeleton.joints[0].transform.translate.x = 0; skeleton.UpdateSkeleton();
    skinnedTarget.SetAlphaCutoff(0.5f); skinnedTarget.SetColor({1, 0, 0, 0.25f});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinnedTarget);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-skinned-alpha.png"), {0, 0, 0}, "Reflected skinned alpha mask failed");
    Object3d foreground; foreground.Initialize(Object3dManager::GetInstance()); foreground.SetModel(&model);
    foreground.SetTranslate({0, 0, -0.1f}); foreground.SetColor({0, 0, 1, 1}); objects.push_back(&foreground);
    source = DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "reflection-unsupported-foreground.png"), {0, 0, 1}, "Stale reflection capture modified unsupported foreground");
    DxrReflectionInputs missingInputs; missingInputs.colorSrv = offscreen.GetSrvHandleGPU();
    Require(reflections.Draw(missingInputs).ptr == missingInputs.colorSrv.ptr && !reflections.HasValidFrame(), "Missing reflection inputs reused stale color");
    settings.isEnabled = false; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    dxCommon->PostDraw(); Require(reflections.GetFilteredTexture() == nullptr && reflections.GetTraceGpuTimeMs() == 0, "Disabled reflections retained valid output/timing");
    report << "PASS: offscreen target, mirror/roughness, environment replacement, miss fallback, lit hit/sun shadows, alpha AnyHit, temporal/spatial noise reduction, camera/jitter/cut, geometry/material/light/scene history reset, skinned ClosestHit/refit/alpha, foreground rejection, invalid/missing inputs, OFF\n";
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    skinManager->SetDefaultCamera(nullptr);
}
void WriteMaterialFixture(const wchar_t* path, const std::array<uint8_t, 4>& color) {
    DirectX::ScratchImage image;
    Require(SUCCEEDED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2, 1, 1)), "Material texture fixture allocation failed");
    auto* pixels = image.GetPixels();
    for (size_t pixelIndex = 0; pixelIndex < 4; ++pixelIndex) {
        std::memcpy(pixels + pixelIndex * 4, color.data(), 4);
    }
    Require(SUCCEEDED(DirectX::SaveToWICFile(*image.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
        DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path)), "Material texture fixture save failed");
}

void WriteMipFixture(const wchar_t* path, uint32_t size, const std::array<uint8_t, 4>& firstColor,
    const std::array<uint8_t, 4>& coarseColor) {
    DirectX::ScratchImage image;
    Require(SUCCEEDED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, size, size, 1, 0)), "Mip fixture allocation failed");
    for (size_t mipIndex = 0; mipIndex < image.GetImageCount(); ++mipIndex) {
        const auto* mip = image.GetImage(mipIndex, 0, 0);
        const auto* color = &coarseColor; if (mipIndex == 0) { color = &firstColor; }
        for (size_t row = 0; row < mip->height; ++row) {
            for (size_t column = 0; column < mip->width; ++column) {
                std::memcpy(mip->pixels + row * mip->rowPitch + column * 4, color->data(), 4);
            }
        }
    }
    // Compressed DDS preserves explicitly colored mip levels through TextureManager.
    DirectX::ScratchImage compressed;
    Require(SUCCEEDED(DirectX::Compress(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        DXGI_FORMAT_BC1_UNORM, DirectX::TEX_COMPRESS_DEFAULT, 0.5f, compressed)), "Mip fixture compression failed");
    Require(SUCCEEDED(DirectX::SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(),
        compressed.GetMetadata(), DirectX::DDS_FLAGS_NONE, path)), "Mip fixture save failed");
}

void RunTextureMipValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene; scene.Initialize(); if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings; sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    DxrReflectionRenderer reflections; reflections.Initialize(); auto settings = reflections.GetSettings();
    settings.isEnabled = true; settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0;
    settings.shouldTraceSunShadows = false; settings.maxRoughness = 1; reflections.SetSettings(settings);
    Camera camera; camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    auto* manager = Object3dManager::GetInstance(); manager->SetDefaultCamera(&camera); manager->SetBlendMode(kBlendModeNone);
    manager->SetShadowRenderer(nullptr); manager->SetLocalShadowRenderer(nullptr);
    auto* lights = LightManager::GetInstance(); lights->ClearDynamicPointLights(); lights->ClearDynamicSpotLights();
    LightingPreset lighting; lighting.intensity = 1; lighting.direction = {1, 0, 0};
    lighting.ambient = {1, 1, 1, 0}; lighting.pointIntensity = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0); lights->SetLightingComponents(1, 1, 1, 0);
    WriteMipFixture(L"runtime/captures/DxrTests/mip-color.dds", 256, {255, 0, 0, 255}, {0, 255, 0, 255});
    WriteMipFixture(L"runtime/captures/DxrTests/mip-normal.dds", 128, {255, 128, 128, 255}, {128, 128, 255, 255});
    WriteMipFixture(L"runtime/captures/DxrTests/mip-mr.dds", 64, {0, 255, 255, 255}, {0, 128, 0, 255});
    ModelCommon common; common.Initialize(dxCommon); ModelData data;
    data.rootNode.name = "mipRoot"; data.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    data.rootNode.transform = {{1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0}};
    data.materials = {{"resources/Textures/white.png"}}; MeshPrimitive plane = {}; plane.mode = PrimitiveMode::Triangles;
    constexpr float kDiagonal = 0.70710678f;
    plane.vertices = {{{-1, -1, 0, 1}, {0, 0}, {kDiagonal, 0, -kDiagonal}}, {{-1, 1, 0, 1}, {0, 1}, {kDiagonal, 0, -kDiagonal}},
        {{1, -1, 0, 1}, {1, 0}, {kDiagonal, 0, -kDiagonal}}, {{1, 1, 0, 1}, {1, 1}, {kDiagonal, 0, -kDiagonal}}};
    plane.indices = {0, 1, 2, 2, 1, 3}; data.primitives = {plane};
    Model receiverModel; receiverModel.Initialize(&common, data);
    data.materials = {{"runtime/captures/DxrTests/mip-color.dds"}};
    Model targetModel; targetModel.Initialize(&common, data);
    Object3d receiver; receiver.Initialize(manager); receiver.SetModel(&receiverModel); receiver.SetShadingMode(MaterialShadingMode::Standard);
    receiver.SetSurfaceProperties(0, 1, 1); receiver.GetMaterial()->roughness = 0; receiver.GetMaterial()->shininess = 0;
    Object3d target; target.Initialize(manager); target.SetModel(&targetModel); target.SetShadingMode(MaterialShadingMode::Unlit);
    target.SetRotate({0, -1.57079633f, 0}); target.SetTranslate({4, 0, 0}); target.SetCastShadow(false);
    std::vector<Object3d*> rasterObjects = {&receiver}; std::vector<Object3d*> rayObjects = {&target};
    RaytracingTestSceneInputs inputs; inputs.raytracingObjects = &rayObjects;
    OffscreenRenderer offscreen; offscreen.Initialize(); PostEffectManager post; post.Initialize(dxCommon); MotionVectorRenderer motion; motion.Initialize();
    TextureManager::GetInstance()->FlushUploads();
    std::ofstream report("runtime/captures/DxrTests/texture-mip-result.txt"); report.setf(std::ios::unitbuf);
    const float kUvScales[] = {0.01f, 1, 64, -64, 0};
    for (uint32_t index = 0; index < 5; ++index) {
        target.GetMaterial()->uvTransform.m[0][0] = kUvScales[index]; target.GetMaterial()->uvTransform.m[1][1] = kUvScales[index];
        DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
        std::string capture = "mip-color-" + std::to_string(index) + ".png";
        Vector3 color = ReadTextureCenter(dxCommon, reflections.GetRawTexture(), capture.c_str());
        report << "uvScale=" << kUvScales[index] << " color=" << color.x << ',' << color.y << ',' << color.z << '\n';
        if (index == 0 || index == 4) { RequireColor(color, {1, 0, 0}, "Magnification/degenerate UV did not retain mip zero"); }
        if (index == 2 || index == 3) { RequireColor(color, {0, 1, 0}, "Minification/mirrored UV did not select coarse mip"); }
        if (index == 1) { Require(color.y > 0.1f, "Camera and hit distance footprint failed to select mip"); }
    }
    target.GetMaterial()->uvTransform.m[0][0] = 0.5f; target.GetMaterial()->uvTransform.m[1][1] = 0.5f;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    Vector3 fractionalColor = ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "mip-fractional.png");
    double pixelDiameter = 4.0 * 4.0 * std::tan(0.35) / (reflections.GetRawTexture()->GetDesc().Height * 2);
    double expectedMip = std::log2(pixelDiameter * (std::sqrt(2.0) + 1) * 64);
    report << "fractional mip=" << expectedMip << " color=" << fractionalColor.x << ',' << fractionalColor.y << '\n';
    Require(std::abs(fractionalColor.y - expectedMip) < 0.025 && std::abs(fractionalColor.x - (1 - expectedMip)) < 0.025,
        "Trilinear mip color disagreed with independent center footprint calculation");
    target.GetMaterial()->uvTransform.m[0][0] = 0.1f; target.GetMaterial()->uvTransform.m[1][1] = 0.1f;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "mip-near-camera.png"), {1, 0, 0}, "Near camera overfiltered magnified texture");
    camera.LookAt({0, 0, -100}, {0, 0, 0}); camera.Update();
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "mip-far-camera.png"), {0, 1, 0}, "Camera distance did not enlarge footprint");
    camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    target.GetMaterial()->uvTransform = MatrixMath::MakeIdentity4x4(); target.SetScale({10, 10, 10});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "mip-wide-mirror.png"), {1, 0, 0}, "Mirror failed to retain large target detail");
    receiver.GetMaterial()->roughness = 0.6f; settings.sampleCount = 16; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    Vector3 roughColor = ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "mip-wide-rough.png");
    Require(roughColor.y > 0.1f && roughColor.x < 0.02f, "Rough reflection failed to widen texture footprint");
    DxrGlobalIlluminationRenderer indirect; indirect.Initialize(); auto indirectSettings = indirect.GetSettings();
    indirectSettings.isEnabled = true; indirectSettings.shouldUseTemporalHistory = false; indirectSettings.spatialPassCount = 0;
    indirectSettings.sampleCount = 16; indirectSettings.shouldTraceSunShadows = false; indirect.SetSettings(indirectSettings);
    receiver.GetMaterial()->metallic = 0;
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    Vector3 indirectColor = ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "mip-diffuse-indirect.png");
    Require(indirectColor.y > 0.1f && indirectColor.x < 0.02f, "Diffuse indirect rays failed to select coarse texture mip");
    indirectSettings.shouldUseTextureMipmaps = false; indirect.SetSettings(indirectSettings);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    Vector3 indirectUnfiltered = ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "mip-diffuse-disabled.png");
    Require(indirectUnfiltered.x > 0.1f && indirectUnfiltered.y < 0.02f, "Diffuse mip OFF did not retain mip zero");
    report << "rough=" << roughColor.x << ',' << roughColor.y << " indirect=" << indirectColor.x << ',' << indirectColor.y << '\n';
    receiver.GetMaterial()->roughness = 0; receiver.GetMaterial()->metallic = 1; target.SetScale({1, 1, 1});
    target.GetMaterial()->uvTransform.m[0][0] = 64; target.GetMaterial()->uvTransform.m[1][1] = 64;
    settings.shouldUseTextureMipmaps = false; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "mip-disabled.png"), {1, 0, 0}, "Mip OFF did not restore old mip zero sampling");
    settings.shouldUseTextureMipmaps = true; reflections.SetSettings(settings);
    target.SetScale({1, 2, 0.5f});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "mip-nonuniform.png"), {0, 1, 0}, "World scale broke mip footprint");
    target.SetScale({1, 1, 1}); targetModel.SetTexture("resources/Textures/white.png");
    target.SetShadingMode(MaterialShadingMode::Standard); target.SetSurfaceProperties(1, 0, 1); target.GetMaterial()->shininess = 0;
    target.SetNormalMap("runtime/captures/DxrTests/mip-normal.dds", 1); TextureManager::GetInstance()->FlushUploads();
    Vector3 normalColors[2];
    for (uint32_t index = 0; index < 2; ++index) {
        settings.shouldUseTextureMipmaps = index == 0; reflections.SetSettings(settings);
        DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
        std::string capture = "mip-normal-" + std::to_string(index) + ".png";
        normalColors[index] = ReadTextureCenter(dxCommon, reflections.GetRawTexture(), capture.c_str());
    }
    Require(std::abs(normalColors[0].x - normalColors[1].x) > 0.1f, "Normal map did not use its mip chain");
    target.SetNormalMap(""); target.SetSurfaceProperties(1, 1, 1);
    target.SetMetallicRoughnessMap("runtime/captures/DxrTests/mip-mr.dds"); TextureManager::GetInstance()->FlushUploads();
    Vector3 materialColors[2];
    for (uint32_t index = 0; index < 2; ++index) {
        settings.shouldUseTextureMipmaps = index == 0; reflections.SetSettings(settings);
        DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
        std::string capture = "mip-mr-" + std::to_string(index) + ".png";
        materialColors[index] = ReadTextureCenter(dxCommon, reflections.GetRawTexture(), capture.c_str());
    }
    report << "normal on/off=" << normalColors[0].x << '/' << normalColors[1].x
        << " material on/off=" << materialColors[0].x << '/' << materialColors[1].x << '\n';
    Require(materialColors[0].x > materialColors[1].x + 0.1f, "Metallic/roughness map did not use coarse linear mip channels");
    report << "PASS: camera/hit footprint, fractional mip, camera distance, rough/diffuse cones, magnification, mirrored/degenerate UV, nonuniform scale, mip OFF, normal/MR mips\n";
    manager->SetDefaultCamera(nullptr);
}

template<class SurfaceObject>
Vector3 ReadRasterMaterial(DirectXCommon* dxCommon, DxrRenderer& scene, SurfaceObject& object, Camera& camera,
    OffscreenRenderer& offscreen, PostEffectManager& postEffects, const char* captureName, bool isSkinned = false) {
    scene.BeginFrame(); scene.SetDrawSubmissionEnabled(false); scene.SetReflectionCapture(true);
    postEffects.PreDrawDepth(); offscreen.PreDraw(postEffects.GetDepthDSVHandle(), false, true);
    SrvManager::GetInstance()->PreDraw(); LightManager::GetInstance()->UpdateClusters(&camera);
    object.SetCamera(&camera); object.Update();
    if (isSkinned) { SkinningObject3dManager::GetInstance()->PreDraw(); }
    else { Object3dManager::GetInstance()->PreDraw(); }
    object.Draw(); scene.EndFrame(&camera, false); postEffects.PostDrawDepth(); offscreen.PostDraw();
    dxCommon->PreDraw();
    return ReadTextureCenter(dxCommon, offscreen.GetColorTexture(), captureName);
}

Vector3 CompareMaterialPaths(DirectXCommon* dxCommon, DxrRenderer& scene, DxrReflectionRenderer& reflections,
    Object3d& receiver, Object3d& target, Camera& camera, Camera& surfaceCamera,
    OffscreenRenderer& offscreen, PostEffectManager& postEffects, MotionVectorRenderer& motionVectors,
    const char* label, std::ofstream& report) {
    std::string rasterName = std::string("material-") + label + "-raster.png";
    Vector3 rasterColor = ReadRasterMaterial(dxCommon, scene, target, surfaceCamera, offscreen, postEffects, rasterName.c_str());
    target.SetCamera(&camera);
    std::vector<Object3d*> rasterObjects = {&receiver}; std::vector<Object3d*> rtObjects = {&receiver, &target};
    RaytracingTestSceneInputs sceneInputs; sceneInputs.raytracingObjects = &rtObjects;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, rasterObjects,
        0, nullptr, nullptr, true, &sceneInputs);
    std::string rayName = std::string("material-") + label + "-rt.png";
    Vector3 rayColor = ReadTextureCenter(dxCommon, reflections.GetRawTexture(), rayName.c_str());
    report << label << " raster=" << rasterColor.x << ',' << rasterColor.y << ',' << rasterColor.z
        << " rt=" << rayColor.x << ',' << rayColor.y << ',' << rayColor.z << '\n';
    RequireColor(rayColor, rasterColor, "Raster and RT material response mismatch");
    return rayColor;
}

void RunMaterialValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene; scene.Initialize(); if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings; sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    DxrReflectionRenderer reflections; reflections.Initialize(); auto settings = reflections.GetSettings();
    settings.isEnabled = true; settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0;
    settings.shouldTraceSunShadows = false; reflections.SetSettings(settings);
    Camera camera; camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    Camera surfaceCamera; surfaceCamera.Initialize(); surfaceCamera.SetFovY(0.7f); surfaceCamera.LookAt({0, 0, 0}, {4, 0, 0}); surfaceCamera.Update();
    auto* objectManager = Object3dManager::GetInstance(); objectManager->SetDefaultCamera(&camera);
    objectManager->SetBlendMode(kBlendModeNone); objectManager->SetShadowRenderer(nullptr); objectManager->SetLocalShadowRenderer(nullptr);
    auto* lights = LightManager::GetInstance(); lights->ClearDynamicPointLights(); lights->ClearDynamicSpotLights();
    LightingPreset lighting; lighting.intensity = 1; lighting.direction = {0, 0, 1}; lighting.ambient = {1, 1, 1, 0.15f}; lighting.pointIntensity = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0); lights->SetLightingComponents(1, 1, 1, 0);
    WriteMaterialFixture(L"runtime/captures/DxrTests/material-normal.png", {204, 204, 204, 255});
    WriteMaterialFixture(L"runtime/captures/DxrTests/material-mr.png", {255, 128, 64, 255});
    ModelCommon modelCommon; modelCommon.Initialize(dxCommon); ModelData data;
    data.rootNode.name = "materialRoot"; data.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    data.rootNode.transform = {{1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0}};
    data.materials = {{"resources/Textures/white.png"}}; MeshPrimitive primitive = {}; primitive.mode = PrimitiveMode::Triangles;
    constexpr float kDiagonal = 0.70710678f;
    primitive.vertices = {{{-1, -1, 0, 1}, {0, 0}, {kDiagonal, 0, -kDiagonal}}, {{-1, 1, 0, 1}, {0, 1}, {kDiagonal, 0, -kDiagonal}},
        {{1, -1, 0, 1}, {1, 0}, {kDiagonal, 0, -kDiagonal}}, {{1, 1, 0, 1}, {1, 1}, {kDiagonal, 0, -kDiagonal}}};
    primitive.indices = {0, 1, 2, 2, 1, 3}; data.primitives = {primitive};
    JointWeightData weights; weights.inverseBindPoseMatrix = MatrixMath::MakeIdentity4x4();
    for (uint32_t vertexIndex = 0; vertexIndex < 4; ++vertexIndex) { weights.vertexWeights.push_back({1, vertexIndex}); }
    data.skinClusterData["materialRoot"] = weights; Model model; model.Initialize(&modelCommon, data);
    Object3d receiver; receiver.Initialize(objectManager); receiver.SetModel(&model); receiver.SetShadingMode(MaterialShadingMode::Standard);
    receiver.SetSurfaceProperties(0, 1, 1); receiver.GetMaterial()->shininess = 0; receiver.SetCamera(&camera);
    receiver.GetMaterial()->roughness = 0;
    Object3d target; target.Initialize(objectManager); target.SetModel(&model); target.SetShadingMode(MaterialShadingMode::Standard);
    target.SetRotate({0, -1.57079633f, 0}); target.SetTranslate({4, 0, 0}); target.SetSurfaceProperties(0.6f, 0, 1);
    target.GetMaterial()->shininess = 0;
    OffscreenRenderer offscreen; offscreen.Initialize(); PostEffectManager postEffects; postEffects.Initialize(dxCommon);
    MotionVectorRenderer motionVectors; motionVectors.Initialize(); TextureManager::GetInstance()->FlushUploads();
    std::ofstream report("runtime/captures/DxrTests/material-result.txt"); report.setf(std::ios::unitbuf);
    static_assert(sizeof(Material) == 144); static_assert(offsetof(Material, alphaCutoff) == 128);
    Vector3 baseline = CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "standard-specular-off", report);
    target.GetMaterial()->metallic = 1;
    Vector3 metalDiffuse = CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "metal-no-specular", report);
    RequireColor(metalDiffuse, {0, 0, 0}, "Metal retained diffuse lighting");
    target.GetMaterial()->shininess = 32;
    Vector3 specular = CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "standard-specular-on", report);
    Require(specular.x > metalDiffuse.x + 0.1f, "Shininess gate did not control material specular");
    target.GetMaterial()->shininess = 0; target.GetMaterial()->metallic = 0;
    target.SetNormalMap("runtime/captures/DxrTests/material-normal.png", 1); TextureManager::GetInstance()->FlushUploads();
    Vector3 mapped = CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "normal-map", report);
    Require(std::abs(mapped.x - baseline.x) > 0.02f, "Normal map did not affect RT hit lighting");
    target.SetNormalMapStrength(0);
    RequireColor(CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "normal-strength-zero", report), baseline, "Zero normal strength changed lighting");
    target.SetNormalMapStrength(1); lights->SetDirection({0, -1, 1});
    Vector3 normalY = CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "normal-y", report);
    target.SetNormalMap("runtime/captures/DxrTests/material-normal.png", 1, true);
    Vector3 flippedY = CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "normal-y-flipped", report);
    Require(std::abs(flippedY.x - normalY.x) > 0.1f, "Normal map Y inversion had no effect");
    target.GetMaterial()->uvTransform.m[0][0] = -1; target.GetMaterial()->uvTransform.m[3][0] = 1;
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "mirrored-uv", report);
    target.GetMaterial()->uvTransform.m[0][0] = 0; target.GetMaterial()->uvTransform.m[1][1] = 0;
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "degenerate-uv", report);
    target.GetMaterial()->uvTransform = MatrixMath::MakeIdentity4x4(); target.SetScale({1, 1.3f, 0.6f});
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "nonuniform-scale", report);
    target.SetScale({1, 1, 1}); target.SetNormalMap(""); lights->SetDirection({0, 0, 1});
    target.SetSurfaceProperties(0.6f, 0.8f, 1); target.GetMaterial()->shininess = 32;
    target.SetMetallicRoughnessMap("runtime/captures/DxrTests/material-mr.png"); TextureManager::GetInstance()->FlushUploads();
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "metallic-roughness-map", report);
    ReadRasterMaterial(dxCommon, scene, target, surfaceCamera, offscreen, postEffects, "material-mr-capture.png");
    dxCommon->PreDraw(); float roughnessAlpha = 0;
    ReadTextureCenter(dxCommon, offscreen.GetReflectionSurfaceTexture(), "material-resolved-roughness.png", nullptr, nullptr, &roughnessAlpha);
    Require(std::abs(roughnessAlpha - 0.6f * 128 / 255) < 0.005f, "Roughness map used wrong channel, factor or color space");
    dxCommon->PreDraw(); float metallicAlpha = 0;
    ReadTextureCenter(dxCommon, offscreen.GetMaterialTexture(), "material-resolved-metallic.png", nullptr, nullptr, &metallicAlpha);
    Require(std::abs(metallicAlpha - (1 + 0.8f * 64 / 255 * 254) / 255) < 0.005f, "Metallic map used wrong channel or factor");
    target.SetMetallicRoughnessMap(""); target.SetShadingMode(MaterialShadingMode::Toon); target.SetSurfaceProperties(0.6f, 0.2f, 0.5f);
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "toon", report);
    target.SetReceiveShadow(true);
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "shadow-toon", report);
    target.SetReceiveShadow(false); target.SetShadingMode(MaterialShadingMode::Standard);
    target.SetNormalMap("runtime/captures/DxrTests/material-normal.png", 1);
    target.SetMetallicRoughnessMap("runtime/captures/DxrTests/material-mr.png"); target.GetMaterial()->shininess = 0;
    lights->SetEnvironmentLighting(0.4f, 0.6f);
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "environment-lighting", report);
    Vector3 iblColor = ReadRasterMaterial(dxCommon, scene, target, surfaceCamera, offscreen, postEffects, "lighting-ibl-only.png");
    target.GetMaterial()->enableEnvironmentMap = 1; target.GetMaterial()->environmentCoefficient = 0.3f;
    Vector3 legacyIblColor = ReadRasterMaterial(dxCommon, scene, target, surfaceCamera, offscreen, postEffects, "lighting-ibl-with-legacy.png");
    RequireColor(legacyIblColor, iblColor, "Legacy environment was added to Standard IBL specular");
    lights->SetAmbientIntensity(0.9f);
    RequireColor(ReadRasterMaterial(dxCommon, scene, target, surfaceCamera, offscreen, postEffects, "lighting-ibl-with-ambient.png"), iblColor, "Hemisphere ambient was added to diffuse IBL");
    lights->SetAmbientIntensity(lighting.ambient.w);
    const Material kSavedMaterial = *target.GetMaterial();
    const DirectionalLight kSavedSun = lights->GetDirectionalLight();
    const Vector4 kSavedAtmosphere = lights->GetAtmosphereSettings();
    Require(!lights->SetSkyLighting(true, std::nanf("")) && !lights->SetSkyLighting(true, -1)
        && !lights->SetSkyLighting(true, 3), "Invalid sky lighting strength accepted");
    Require(lights->SetSkyLighting(true, 1), "Sky lighting could not be enabled");
    lights->SetEnvironmentLighting(0.4f, 0.6f);
    Require(lights->IsSkyLightingEnabled(), "Environment strength reset sky source selection");
    lights->SetAtmosphere(true, 0.00035f, 1, 0.65f);
    lights->SetLightingComponents(0, 1, 1, 0);
    lights->SetDirectional({1, 1, 1, 1}, {0, -1, 0}, 4);
    target.SetNormalMap(""); target.SetMetallicRoughnessMap(""); target.SetSurfaceProperties(0.6f, 0.2f, 1);
    std::ofstream skyReport("runtime/captures/DxrTests/sky-lighting-result.txt"); skyReport.setf(std::ios::unitbuf);
    Vector3 skyDay = CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera,
        offscreen, postEffects, motionVectors, "sky-day", skyReport);
    lights->SetDirectional({1, 0.5f, 0.2f, 1}, {0, -0.08f, 1}, 2);
    Vector3 skySunset = CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera,
        offscreen, postEffects, motionVectors, "sky-sunset", skyReport);
    lights->SetDirectional({1, 1, 1, 1}, {0, 1, 0}, 4);
    Vector3 skyNight = CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera,
        offscreen, postEffects, motionVectors, "sky-night", skyReport);
    Require(skyDay.z > skyNight.z + 0.02f, "Night retained daylight environment illumination");
    Require(std::abs(skySunset.x - skyDay.x) > 0.01f, "Sun elevation/color did not affect sky lighting");
    lights->SetSkyLighting(true, 0);
    RequireColor(CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera,
        offscreen, postEffects, motionVectors, "sky-zero", skyReport), {0, 0, 0}, "Zero sky strength retained indirect light");
    lights->SetSkyLighting(true, 1);
    settings.shouldUseTemporalHistory = true; reflections.SetSettings(settings);
    std::vector<Object3d*> skyRasterObjects = {&receiver};
    std::vector<Object3d*> skyRtObjects = {&receiver, &target};
    RaytracingTestSceneInputs skySceneInputs; skySceneInputs.raytracingObjects = &skyRtObjects;
    for (uint32_t index = 0; index < 2; ++index) {
        DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera,
            skyRasterObjects, 0, nullptr, nullptr, true, &skySceneInputs);
        ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "sky-history-stable.png");
    }
    Require(reflections.HasUsedHistory(), "Stable sky lighting did not reuse RT history");
    lights->SetAtmosphere(true, 0.00035f, 0.7f, 0.4f);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera,
        skyRasterObjects, 0, nullptr, nullptr, true, &skySceneInputs);
    ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "sky-history-atmosphere-change.png");
    Require(!reflections.HasUsedHistory(), "Atmosphere change retained stale RT history");
    settings.shouldUseTemporalHistory = false; reflections.SetSettings(settings);
    lights->SetAtmosphere(true, 0.00035f, 1, 0.65f);
    SkyBoxManager::GetInstance()->Initialize(dxCommon);
    SkyBox sky; sky.Initialize(dxCommon); sky.Update(&surfaceCamera);
    GpuTimestampTimer skyTimer; skyTimer.Initialize();
    Vector3 visibleSky[2];
    for (uint32_t index = 0; index < 2; ++index) {
        float sunStrength = 1.0f + float(index) * 3.0f;
        lights->SetDirectional({1, 1, 1, 1}, {0, -1, 0}, sunStrength);
        offscreen.PreDraw(dxCommon->GetDSVHandle()); SrvManager::GetInstance()->PreDraw();
        dxCommon->GetCommandList()->ClearDepthStencilView(dxCommon->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
        skyTimer.Begin(); SkyBoxManager::GetInstance()->PreDraw(); sky.Draw(dxCommon->GetCommandList()); skyTimer.End();
        offscreen.PostDraw(); dxCommon->PreDraw();
        std::string name = "sky-visible-" + std::to_string(index) + ".png";
        visibleSky[index] = ReadTextureCenter(dxCommon, offscreen.GetColorTexture(), name.c_str());
        skyTimer.ReadCompleted();
        skyReport << "visible strength=" << sunStrength << " rgb=" << visibleSky[index].x << ','
            << visibleSky[index].y << ',' << visibleSky[index].z << " GPU ms=" << skyTimer.GetDurationMs() << '\n';
    }
    Require(visibleSky[1].z > visibleSky[0].z * 2 && visibleSky[1].z > 1,
        "Visible sky did not retain HDR sunlight intensity");
    lights->SetSkyLighting(false, 1); lights->SetLightingComponents(1, 1, 1, 0);
    lights->SetDirectional(kSavedSun.color, kSavedSun.direction, kSavedSun.intensity);
    lights->SetAtmosphere(kSavedAtmosphere.x > 0.5f, kSavedAtmosphere.y, kSavedAtmosphere.z, kSavedAtmosphere.w);
    *target.GetMaterial() = kSavedMaterial;
    target.SetNormalMap("runtime/captures/DxrTests/material-normal.png", 1);
    target.SetMetallicRoughnessMap("runtime/captures/DxrTests/material-mr.png");
    RequireColor(ReadRasterMaterial(dxCommon, scene, target, surfaceCamera, offscreen, postEffects,
        "sky-cubemap-restored.png"), iblColor, "Sky OFF did not restore cubemap lighting");
    skyReport << "PASS: day/sunset/night raster and RT agreement, zero strength, invalid settings, cubemap restore, visible HDR sky\n";
    target.SetCamera(&camera);
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "legacy-environment", report);
    target.SetShadingMode(MaterialShadingMode::Unlit);
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "unlit-environment", report);
    target.SetShadingMode(MaterialShadingMode::Standard); target.GetMaterial()->enableEnvironmentMap = 0;
    lights->SetEnvironmentLighting(0, 0); lights->SetIntensity(0);
    lights->SetPointIntensity(0.7f); lights->SetPointPosition({0, 2, 0}); lights->SetPointRadius(10); lights->SetPointDecay(0);
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "point-light", report);
    lights->SetPointIntensity(0); lights->SetSpotLightPosition({0, 2, 0}); lights->SetSpotLightDirection({4, -2, 0});
    lights->SetSpotLightIntensity(0.6f); lights->SetSpotLightDistance(10); lights->SetSpotLightDecay(0);
    lights->SetSpotLightCosAngle(0.5f); lights->SetSpotLightCosFalloffStart(0.9f);
    CompareMaterialPaths(dxCommon, scene, reflections, receiver, target, camera, surfaceCamera, offscreen, postEffects, motionVectors, "spot-light", report);
    lights->SetSpotLightIntensity(0); lights->SetIntensity(1);
    auto* skinManager = SkinningObject3dManager::GetInstance(); skinManager->SetDefaultCamera(&camera); skinManager->SetBlendMode(kBlendModeNone);
    skinManager->SetEnvironmentTexture(objectManager->GetEnvironmentTexture());
    Skeleton skeleton = Skeleton::CreateSkeleton(data.rootNode); skeleton.UpdateSkeleton(); PlayAnimation animation; animation.SetSkeleton(&skeleton);
    SkinningObject3d skinned; skinned.SetModel(&model); skinned.SetAnimation(&animation); skinned.Initialize(skinManager);
    skinned.SetTranslate({4, 0, 0}); skinned.SetRotate({0, -1.57079633f, 0}); skinned.SetShadingMode(MaterialShadingMode::Standard);
    skinned.GetMaterial()->shininess = 0; skinned.SetNormalMap("runtime/captures/DxrTests/material-normal.png", 1);
    skinned.SetMetallicRoughnessMap("runtime/captures/DxrTests/material-mr.png"); TextureManager::GetInstance()->FlushUploads();
    Vector3 skinRaster = ReadRasterMaterial(dxCommon, scene, skinned, surfaceCamera, offscreen, postEffects, "material-skinned-raster.png", true);
    skinned.SetCamera(&camera); std::vector<Object3d*> skinRasterObjects = {&receiver}; RaytracingTestSceneInputs skinInputs; skinInputs.shouldDrawSkinnedTarget = false;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, skinRasterObjects, 0, &skinned, nullptr, true, &skinInputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "material-skinned-rt.png"), skinRaster, "Skinned hit material differed from raster");
    report << "PASS: Standard/specular gate, normal/strength/Y flip/mirrored and degenerate UV/nonuniform scale, linear packed G/B/factors/capture, Toon/ShadowToon, GPU skinned material, 144-byte Material and 96-byte records\n";
    objectManager->SetDefaultCamera(nullptr); skinManager->SetDefaultCamera(nullptr);
}

void RunGlobalIlluminationValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene; scene.Initialize(); if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings; sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    DxrGlobalIlluminationRenderer indirect; indirect.Initialize();
    auto settings = indirect.GetSettings();
    Require(!settings.isEnabled && settings.sampleCount == 1, "RTGI default configuration changed");
    settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0; settings.maxDistance = 1000;
    settings.maxRoughness = 0; settings.shouldTraceSunShadows = false;
    auto invalid = settings; invalid.maxRadiance = std::nanf("");
    Require(!indirect.SetSettings(invalid), "Nonfinite RTGI radiance accepted");
    Camera camera; camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    Object3dManager::GetInstance()->SetDefaultCamera(&camera);
    auto* lights = LightManager::GetInstance();
    LightingPreset lighting; lighting.intensity = 0; lighting.ambient = {1, 1, 1, 0.2f}; lighting.pointIntensity = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0); lights->SetLightingComponents(1, 1, 1, 0);
    ModelCommon modelCommon; modelCommon.Initialize(dxCommon);
    ModelData modelData; modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    modelData.rootNode.name = "indirectRoot"; modelData.rootNode.transform = {{1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0}};
    modelData.materials = {{"resources/Textures/white.png"}};
    MeshPrimitive primitive = {}; primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices = {{{-1, -1, 0, 1}, {0, 0}, {0, 0, -1}}, {{-1, 1, 0, 1}, {0, 1}, {0, 0, -1}},
        {{1, -1, 0, 1}, {1, 0}, {0, 0, -1}}, {{1, 1, 0, 1}, {1, 1}, {0, 0, -1}}};
    primitive.indices = {0, 1, 2, 2, 1, 3}; modelData.primitives = {primitive};
    JointWeightData weights; weights.inverseBindPoseMatrix = MatrixMath::MakeIdentity4x4();
    for (uint32_t index = 0; index < 4; ++index) { weights.vertexWeights.push_back({1, index}); }
    modelData.skinClusterData["indirectRoot"] = weights;
    Model model; model.Initialize(&modelCommon, modelData);
    Object3d receiver; receiver.Initialize(Object3dManager::GetInstance()); receiver.SetModel(&model);
    receiver.SetShadingMode(MaterialShadingMode::Standard); receiver.SetCastShadow(false); receiver.SetColor({0.5f, 0.25f, 0.75f, 1});
    receiver.GetMaterial()->roughness = 1; receiver.GetMaterial()->specularStrength = 0; receiver.GetMaterial()->shininess = 0;
    Object3d target; target.Initialize(Object3dManager::GetInstance()); target.SetModel(&model);
    target.SetTranslate({0, 0, -6}); target.SetScale({2000, 2000, 1}); target.SetColor({1, 0, 0, 1});
    target.GetMaterial()->specularStrength = 0; target.GetMaterial()->shininess = 0;
    target.Update();
    for (const auto& vertex : primitive.vertices) {
        auto world = MatrixMath::Transform({vertex.position.x, vertex.position.y, vertex.position.z}, target.GetWorldMatrix());
        auto clip = MatrixMath::Transform(world, camera.GetViewMatrix());
        Require(clip.z < 0, "RTGI source is not behind the camera");
    }
    std::vector<Object3d*> objects = {&receiver, &target};
    TextureManager::GetInstance()->FlushUploads();
    OffscreenRenderer offscreen; offscreen.Initialize(); PostEffectManager postEffects; postEffects.Initialize(dxCommon);
    MotionVectorRenderer motionVectors; motionVectors.Initialize();
    indirect.SetSettings(settings);
    auto* source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    Vector3 baseline = ReadTextureCenter(dxCommon, source, "rtgi-off.png");
    settings.isEnabled = true; settings.strength = 0; indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "lighting-gi-strength-zero.png"), baseline, "GI strength zero changed lighting");
    settings.strength = 1; indirect.SetSettings(settings);
    target.SetColor({0, 0, 0, 1});
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "lighting-gi-black-hit.png"), {0, 0, 0}, "Black GI hit retained ambient diffuse");
    dxCommon->PreDraw(); float blackCoverage = 0;
    ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "lighting-gi-black-coverage.png", nullptr, nullptr, &blackCoverage);
    Require(std::abs(blackCoverage - 1) < 0.005f, "Black hit was classified as a miss");
    target.SetColor({1, 0, 0, 1});
    settings.sampleCount = 16; indirect.SetSettings(settings); target.SetScale({6, 6, 1});
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    Vector3 partialColor = ReadTextureCenter(dxCommon, source, "lighting-gi-partial.png");
    dxCommon->PreDraw(); float partialCoverage = 0;
    Vector3 partialSignal = ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "lighting-gi-partial-coverage.png", nullptr, nullptr, &partialCoverage);
    Require(partialCoverage > 0 && partialCoverage < 1, "Partial GI fixture did not contain both hits and misses");
    RequireColor(partialSignal, {partialCoverage, 0, 0}, "GI coverage and radiance use different sample weights");
    RequireColor(partialColor, baseline * (1 - partialCoverage) + Vector3{0.5f * partialCoverage, 0, 0}, "Partial GI double-counted ambient or discarded miss fallback");
    target.SetScale({2000, 2000, 1}); settings.sampleCount = 1;
    settings.isEnabled = false; indirect.SetSettings(settings);
    receiver.GetMaterial()->specularStrength = 0.5f; lights->SetEnvironmentLighting(0.4f, 0.6f);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    Vector3 environmentBaseline = ReadTextureCenter(dxCommon, source, "lighting-gi-environment-baseline.png");
    dxCommon->PreDraw(); Vector3 capturedIndirect = ReadTextureCenter(dxCommon, offscreen.GetIndirectTexture(), "lighting-gi-indirect.png");
    dxCommon->PreDraw(); Vector3 capturedSpecular = ReadTextureCenter(dxCommon, offscreen.GetReflectionEnvironmentTexture(), "lighting-gi-specular.png");
    Vector3 capturedDiffuse = capturedIndirect - capturedSpecular;
    capturedDiffuse.x = (std::max)(capturedDiffuse.x, 0.0f); capturedDiffuse.y = (std::max)(capturedDiffuse.y, 0.0f); capturedDiffuse.z = (std::max)(capturedDiffuse.z, 0.0f);
    Require(capturedSpecular.z > 0.001f, "Environment preservation fixture has no specular");
    settings.isEnabled = true; indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "lighting-gi-specular-preserved.png"), environmentBaseline - capturedDiffuse + Vector3{0.49f, 0, 0}, "GI removed specular or double-counted diffuse environment");
    receiver.GetMaterial()->specularStrength = 0; lights->SetEnvironmentLighting(0, 0);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "rtgi-color-bleeding.png"), Vector3{0.5f, 0, 0}, "RTGI missed behind-camera source or receiver albedo");
    dxCommon->PreDraw(); RequireColor(ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-raw-irradiance.png"), {1, 0, 0}, "RTGI depends on specular strength or roughness");
    std::vector<Object3d*> giRasterObjects = {&receiver};
    RaytracingTestSceneInputs giSceneInputs; giSceneInputs.raytracingObjects = &objects;
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, giRasterObjects, 0, nullptr, nullptr, true, &giSceneInputs);
    RequireColor(ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-undrawn-source.png"), {1, 0, 0}, "Undrawn source stopped contributing indirect light");
    scene.ReadCompleted(); indirect.ReadCompleted();
    std::ofstream report("runtime/captures/DxrTests/rtgi-result.txt");
    report << "traceMs=" << indirect.GetTraceGpuTimeMs() << " filterMs=" << indirect.GetFilterGpuTimeMs()
        << " compositeMs=" << indirect.GetCompositeGpuTimeMs() << " targetsBytes=" << indirect.GetAllocationBytes() << '\n';
    settings.isDebugVisible = true; indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "rtgi-debug.png"), {0.5f, 0, 0}, "RTGI debug view includes scene or ignores albedo");
    settings.isDebugVisible = false; settings.strength = 0.25f; indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "rtgi-strength.png"), baseline * 0.75f + Vector3{0.125f, 0, 0}, "RTGI strength composition mismatch");
    settings.strength = 1; settings.maxDistance = 1; indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "rtgi-distance-miss.png"), baseline, "RTGI miss replaced base environment/direct lighting");
    settings.maxDistance = 1000; indirect.SetSettings(settings); receiver.GetMaterial()->metallic = 1;
    settings.isDebugVisible = true; indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "rtgi-metallic.png"), {0, 0, 0}, "Metal received diffuse indirect light");
    settings.isDebugVisible = false; indirect.SetSettings(settings); receiver.GetMaterial()->metallic = 0;
    target.SetAlphaCutoff(0.5f); target.SetColor({1, 0, 0, 0.25f});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-alpha-hole.png"), {0, 0, 0}, "RTGI alpha AnyHit failed");
    target.SetAlphaCutoff(0); target.SetColor({1, 0, 0, 1}); target.SetShadingMode(MaterialShadingMode::Standard);
    lighting.intensity = 1; lighting.direction = {0, 0, -1}; lights->ApplyLightingPreset(lighting);
    settings.shouldTraceSunShadows = true; indirect.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-lit-source.png"), {1.2f, 0, 0}, "RTGI hit ignored illumination");
    settings.maxRadiance = 0.5f; indirect.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-radiance-clamp.png"), {0.5f, 0, 0}, "RTGI radiance clamp failed");
    target.SetShadingMode(MaterialShadingMode::Unlit); lighting.intensity = 0; lights->ApplyLightingPreset(lighting);
    settings.maxRadiance = 10; settings.shouldUseTemporalHistory = true; settings.spatialPassCount = 2; indirect.SetSettings(settings);
    for (uint32_t index = 0; index < 3; ++index) {
        DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    }
    Require(indirect.HasUsedHistory(), "Stable RTGI receiver did not reuse history");
    dxCommon->PreDraw();
    Vector3 indirectStatistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "rtgi-history-stable.png");
    Require(indirectStatistics.z >= 3, "Stable diffuse pixel did not accumulate history");
    receiver.SetColor({0.25f, 0.5f, 0.75f, 1});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    indirectStatistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "rtgi-history-receiver-material.png");
    Require(indirectStatistics.z == 1, "Changed receiving material reused incompatible history");
    receiver.SetColor({0.5f, 0.25f, 0.75f, 1});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    camera.SetTranslate({0.01f, 0, -4}); camera.Update(); camera.SetProjectionJitter({0.001f, -0.001f});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    Require(indirect.HasUsedHistory(), "Camera motion/jitter discarded all RTGI history");
    camera.ResetMotionHistory();
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    Require(!indirect.HasUsedHistory(), "RTGI camera cut retained history");
    target.SetColor({0, 1, 0, 1});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    Require(indirect.HasUsedHistory(), "RTGI material change discarded unaffected history globally");
    dxCommon->PreDraw();
    RequireColor(ReadTextureCenter(dxCommon, indirect.GetFilteredTexture(), "rtgi-history-color-change.png"), {0, 1, 0}, "RTGI changed source retained old red light");
    dxCommon->PreDraw();
    indirectStatistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "rtgi-history-color-count.png");
    Require(indirectStatistics.z == 1, "RTGI changed source retained its accumulation count");
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    lighting.ambient.w = 0.3f; lights->ApplyLightingPreset(lighting);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    Require(indirect.HasUsedHistory(), "RTGI lighting change discarded unaffected history globally");
    dxCommon->PreDraw();
    indirectStatistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "rtgi-history-unrelated-light.png");
    Require(indirectStatistics.z >= 3, "Unaffected unlit GI source lost history when ambient changed");
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects, 1); dxCommon->PostDraw();
    Require(!indirect.HasUsedHistory(), "RTGI scene change retained history");
    camera.SetTranslate({0, 0, -4}); camera.Update(); camera.SetProjectionJitter({});
    target.SetColor({1, 0, 0, 1}); target.SetScale({2, 2, 1}); target.SetTranslate({3, 0, -6}); receiver.SetColor({1, 1, 1, 1});
    settings.shouldTraceSunShadows = false; settings.sampleCount = 16; settings.spatialPassCount = 0;
    // Vary samples between frames without temporal blending to build a reference.
    indirect.SetSettings(settings); std::vector<float> reference;
    for (uint32_t index = 0; index < 8; ++index) {
        indirect.ResetHistory(); DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
        std::vector<float> values; ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-reference-sample.png", nullptr, &values);
        if (reference.empty()) { reference.resize(values.size()); }
        for (size_t pixelIndex = 0; pixelIndex < values.size(); ++pixelIndex) { reference[pixelIndex] += values[pixelIndex] / 8; }
    }
    settings.sampleCount = 1; settings.shouldUseTemporalHistory = false; indirect.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    std::vector<float> raw; ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-noise-raw1.png", nullptr, &raw);
    settings.shouldUseTemporalHistory = true; settings.spatialPassCount = 2; indirect.SetSettings(settings);
    for (uint32_t index = 0; index < 32; ++index) {
        DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    }
    dxCommon->PreDraw(); std::vector<float> filtered;
    ReadTextureCenter(dxCommon, indirect.GetFilteredTexture(), "rtgi-noise-filtered.png", nullptr, &filtered);
    double rawError = ReflectionMeanSquareError(raw, reference); double filteredError = ReflectionMeanSquareError(filtered, reference);
    report << "mseRaw=" << rawError << " mseFiltered=" << filteredError << '\n';
    Require(filteredError < rawError * 0.5, "RTGI temporal/spatial filters did not reduce noise");
    auto* skinManager = SkinningObject3dManager::GetInstance(); skinManager->SetDefaultCamera(&camera);
    skinManager->SetBlendMode(kBlendModeNone); skinManager->SetEnvironmentTexture(Object3dManager::GetInstance()->GetEnvironmentTexture());
    Skeleton skeleton = Skeleton::CreateSkeleton(modelData.rootNode); skeleton.UpdateSkeleton();
    PlayAnimation animation; animation.SetSkeleton(&skeleton);
    SkinningObject3d skinned; skinned.SetModel(&model); skinned.SetAnimation(&animation); skinned.Initialize(skinManager);
    skinned.SetTranslate({0, 0, -6}); skinned.SetScale({2000, 2000, 1}); skinned.SetColor({1, 0, 0, 1});
    settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0; indirect.SetSettings(settings);
    const std::vector<Object3d*> kSkinObjects = {&receiver};
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinned);
    RequireColor(ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-skinned-hit.png"), {1, 0, 0}, "RTGI skinned hit failed");
    skeleton.joints[0].transform.translate.z = 20; skeleton.UpdateSkeleton();
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinned);
    Require(scene.GetStatistics().updatedBlasCount == 1, "RTGI skinned geometry did not refit");
    RequireColor(ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-skinned-moved.png"), {0, 0, 0}, "RTGI retained stale skinning geometry");
    Object3d foreground; foreground.Initialize(Object3dManager::GetInstance()); foreground.SetModel(&model);
    foreground.SetTranslate({0, 0, -0.1f}); foreground.SetColor({0, 0, 1, 1}); objects.push_back(&foreground);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "rtgi-unsupported-foreground.png"), {0, 0, 1}, "RTGI modified unsupported foreground");
    DxrGlobalIlluminationInputs missing; missing.colorSrv = offscreen.GetSrvHandleGPU();
    Require(indirect.Draw(missing).ptr == missing.colorSrv.ptr && !indirect.HasValidFrame(), "Missing RTGI inputs retained stale output");
    settings.isEnabled = false; indirect.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects); dxCommon->PostDraw();
    Require(indirect.GetFilteredTexture() == nullptr && indirect.GetTraceGpuTimeMs() == 0, "RTGI OFF retained valid output/timing");
    report << "PASS: behind-camera source, diffuse/specular independence, receiver albedo/metallic, strength/debug, distance/miss, alpha, lit hit, radiance clamp, history/jitter/cut/material/light/scene, noise reduction, actual GPU skinning/refit, foreground rejection, missing inputs, OFF\n";
    skinManager->SetBlendMode(kBlendModeNormal); skinManager->SetDefaultCamera(nullptr); Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}
double ReferenceDiffusePlaneCoverage(double planeDistance, double maxDistance, double fadeRatio) {
    constexpr uint32_t kSteps = 65536;
    double total = 0;
    for (uint32_t index = 0; index < kSteps; ++index) {
        double fraction = (index + 0.5) / kSteps;
        double distance = planeDistance / std::sqrt(1 - fraction);
        if (distance > maxDistance) { continue; }
        double weight = 1;
        if (fadeRatio > 0) {
            double fractionIntoFade = (distance - maxDistance * (1 - fadeRatio)) / (maxDistance * fadeRatio);
            fractionIntoFade = std::clamp(fractionIntoFade, 0.0, 1.0);
            weight = 1 - fractionIntoFade * fractionIntoFade * (3 - 2 * fractionIntoFade);
        }
        total += weight;
    }
    return total / kSteps;
}
double MeasureDiffusePatch(const std::vector<float>& values, ID3D12Resource* texture, double reference, double* mean) {
    auto description = texture->GetDesc(); size_t width = static_cast<size_t>(description.Width);
    int centerX = static_cast<int>(width / 2); int centerY = static_cast<int>(description.Height / 2);
    double total = 0; double error = 0; size_t count = 0;
    for (int row = centerY - 15; row <= centerY + 15; ++row) {
        for (int column = centerX - 15; column <= centerX + 15; ++column) {
            double value = values[static_cast<size_t>(row) * width + column];
            Require(std::isfinite(value) && value >= 0 && value <= 1.001, "Diffuse coverage exceeded unit radiance bounds");
            total += value; error += (value - reference) * (value - reference); ++count;
        }
    }
    *mean = total / count; return error / count;
}
void RunDiffuseQualityValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene; scene.Initialize(); if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings; sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    DxrGlobalIlluminationRenderer indirect; indirect.Initialize(); auto settings = indirect.GetSettings();
    settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0; settings.shouldTraceSunShadows = false;
    settings.sampleCount = 16; settings.maxDistance = 10;
    auto invalid = settings; invalid.indirectDistanceFadeRatio = std::nanf("");
    Require(!indirect.SetSettings(invalid), "Nonfinite distance fade accepted");
    invalid.indirectDistanceFadeRatio = -0.1f; Require(!indirect.SetSettings(invalid), "Negative distance fade accepted");
    invalid.indirectDistanceFadeRatio = 1.1f; Require(!indirect.SetSettings(invalid), "Distance fade above one accepted");
    Camera camera; camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    auto* manager = Object3dManager::GetInstance(); manager->SetDefaultCamera(&camera); manager->SetBlendMode(kBlendModeNone);
    manager->SetShadowRenderer(nullptr); manager->SetLocalShadowRenderer(nullptr);
    auto* lights = LightManager::GetInstance(); lights->ClearDynamicPointLights(); lights->ClearDynamicSpotLights();
    LightingPreset lighting; lighting.intensity = 0; lighting.ambient = {1, 1, 1, 0.2f}; lighting.pointIntensity = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0); lights->SetLightingComponents(1, 1, 1, 0);
    ModelCommon common; common.Initialize(dxCommon); ModelData data;
    data.rootNode.localMatrix = MatrixMath::MakeIdentity4x4(); data.materials = {{"resources/Textures/white.png"}};
    MeshPrimitive plane = {}; plane.mode = PrimitiveMode::Triangles;
    plane.vertices = {{{-1, -1, 0, 1}, {0, 0}, {0, 0, -1}}, {{-1, 1, 0, 1}, {0, 1}, {0, 0, -1}},
        {{1, -1, 0, 1}, {1, 0}, {0, 0, -1}}, {{1, 1, 0, 1}, {1, 1}, {0, 0, -1}}};
    plane.indices = {0, 1, 2, 2, 1, 3}; data.primitives = {plane}; Model model; model.Initialize(&common, data);
    Object3d receiver; receiver.Initialize(manager); receiver.SetModel(&model); receiver.SetShadingMode(MaterialShadingMode::Standard);
    receiver.SetSurfaceProperties(1, 0, 0); receiver.GetMaterial()->shininess = 0;
    Object3d target; target.Initialize(manager); target.SetModel(&model); target.SetShadingMode(MaterialShadingMode::Unlit);
    target.SetScale({1000, 1000, 1}); target.SetTranslate({0, 0, -5}); target.SetColor({1, 0, 0, 1});
    std::vector<Object3d*> rasterObjects = {&receiver}; std::vector<Object3d*> rayObjects = {&target};
    RaytracingTestSceneInputs inputs; inputs.raytracingObjects = &rayObjects;
    OffscreenRenderer offscreen; offscreen.Initialize(); PostEffectManager post; post.Initialize(dxCommon); MotionVectorRenderer motion; motion.Initialize();
    indirect.SetSettings(settings);
    auto* source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, false, &inputs);
    Vector3 baseline = ReadTextureCenter(dxCommon, source, "diffuse-quality-baseline.png");
    std::ofstream report("runtime/captures/DxrTests/diffuse-quality-result.txt"); report.setf(std::ios::unitbuf);
    settings.isEnabled = true;
    const float kDistances[] = {2, 5, 8, 9, 9.9f, 10.01f, 11};
    for (uint32_t index = 0; index < 7; ++index) {
        target.SetTranslate({0, 0, -kDistances[index]}); indirect.SetSettings(settings);
        source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
        std::string capture = "diffuse-distance-composite-" + std::to_string(index) + ".png";
        Vector3 composite = ReadTextureCenter(dxCommon, source, capture.c_str()); dxCommon->PreDraw();
        float coverage = 0; std::vector<float> values;
        capture = "diffuse-distance-raw-" + std::to_string(index) + ".png";
        Vector3 raw = ReadTextureCenter(dxCommon, indirect.GetRawTexture(), capture.c_str(), nullptr, &values, &coverage);
        double reference = ReferenceDiffusePlaneCoverage(kDistances[index] - settings.normalBias, settings.maxDistance, settings.indirectDistanceFadeRatio);
        double mean = 0; MeasureDiffusePatch(values, indirect.GetRawTexture(), reference, &mean);
        report << "distance=" << kDistances[index] << " mean=" << mean << " integral=" << reference << " coverage=" << coverage << '\n';
        Require(std::abs(mean - reference) < 0.015, "Distance fade disagreed with independent cosine hemisphere integral");
        RequireColor(raw, {coverage, 0, 0}, "Distance fade used inconsistent radiance/coverage weights");
        RequireColor(composite, baseline * (1 - coverage) + Vector3{coverage, 0, 0}, "Distance fade did not preserve complementary environment fallback");
    }
    target.SetTranslate({0, 0, -5}); double reference = ReferenceDiffusePlaneCoverage(5 - settings.normalBias, 10, 0.2);
    double errors[2];
    for (uint32_t index = 0; index < 2; ++index) {
        settings.shouldUseLowDiscrepancySampling = index == 0; indirect.SetSettings(settings);
        DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
        std::vector<float> values; std::string capture = "diffuse-sampling-" + std::to_string(index) + ".png";
        ReadTextureCenter(dxCommon, indirect.GetRawTexture(), capture.c_str(), nullptr, &values);
        double mean = 0; errors[index] = MeasureDiffusePatch(values, indirect.GetRawTexture(), reference, &mean);
    }
    report << "mseSobol=" << errors[0] << " mseRandom=" << errors[1] << '\n';
    Require(errors[0] < errors[1] * 0.85, "Low discrepancy sampling failed to reduce fixed-budget plane integration error");
    settings.shouldUseLowDiscrepancySampling = true; settings.indirectDistanceFadeRatio = 0;
    const uint32_t kSampleCounts[] = {1, 3, 5, 16};
    for (uint32_t sampleCount : kSampleCounts) {
        settings.sampleCount = sampleCount; indirect.SetSettings(settings);
        DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
        std::vector<float> values; ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "diffuse-no-fade.png", nullptr, &values);
        reference = 1 - std::pow((5 - settings.normalBias) / 10, 2);
        double mean = 0; MeasureDiffusePatch(values, indirect.GetRawTexture(), reference, &mean);
        report << "samples=" << sampleCount << " mean=" << mean << " noFadeIntegral=" << reference << '\n';
        Require(std::abs(mean - reference) < 0.04, "Non-power-of-two/sample-one diffuse sequence biased the mean");
    }
    settings.sampleCount = 16; settings.indirectDistanceFadeRatio = 1; target.SetColor({0, 0, 0, 1}); indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    Vector3 blackComposite = ReadTextureCenter(dxCommon, source, "diffuse-black-fade.png"); dxCommon->PreDraw(); float blackCoverage = 0;
    RequireColor(ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "diffuse-black-raw.png", nullptr, nullptr, &blackCoverage), {0, 0, 0}, "Black distance hit returned radiance");
    Require(blackCoverage > 0 && blackCoverage < 1, "Black distance hit lost weighted coverage");
    RequireColor(blackComposite, baseline * (1 - blackCoverage), "Black faded hit failed to replace environment with darkness");
    settings.shouldUseTemporalHistory = true; settings.indirectDistanceFadeRatio = 0.2f; indirect.SetSettings(settings);
    for (uint32_t index = 0; index < 3; ++index) {
        DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs); dxCommon->PostDraw();
    }
    Require(indirect.HasUsedHistory(), "Diffuse quality fixture did not accumulate history");
    settings.indirectDistanceFadeRatio = 0.4f; indirect.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs); dxCommon->PostDraw();
    Require(!indirect.HasUsedHistory(), "Distance fade change reused old history");
    settings.shouldUseLowDiscrepancySampling = false; indirect.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs); dxCommon->PostDraw();
    Require(!indirect.HasUsedHistory(), "Sampling distribution change reused old history");
    report << "PASS: independent distance integral, environment replacement, black/miss fallback, Sobol/random equal-budget MSE, 1/3/5/16 samples, full/zero fade, invalid settings, history reset\n";
    manager->SetDefaultCamera(nullptr);
}

void RunMultipleReflectionValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene; scene.Initialize(); if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings; sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    DxrReflectionRenderer reflections; reflections.Initialize(); auto settings = reflections.GetSettings();
    Require(!settings.shouldTraceMultipleReflections && settings.maxReflectionBounces == 2, "Multiple reflection default configuration changed");
    settings.isEnabled = true; settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0;
    settings.shouldTraceSunShadows = false; settings.sampleCount = 1; settings.maxRoughness = 1;
    auto invalid = settings; invalid.maxReflectionBounces = 0; Require(!reflections.SetSettings(invalid), "Zero bounce limit accepted");
    invalid.maxReflectionBounces = 3; Require(!reflections.SetSettings(invalid), "Unsupported recursive bounce limit accepted");
    Camera camera; camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    auto* manager = Object3dManager::GetInstance(); manager->SetDefaultCamera(&camera); manager->SetBlendMode(kBlendModeNone);
    manager->SetShadowRenderer(nullptr); manager->SetLocalShadowRenderer(nullptr);
    auto* lights = LightManager::GetInstance(); lights->ClearDynamicPointLights(); lights->ClearDynamicSpotLights();
    LightingPreset lighting; lighting.intensity = 0; lighting.ambient = {1, 1, 1, 0}; lighting.pointIntensity = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0); lights->SetLightingComponents(1, 1, 1, 0);
    ModelCommon common; common.Initialize(dxCommon); ModelData receiverData;
    receiverData.rootNode.name = "multipleRoot"; receiverData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    receiverData.rootNode.transform = {{1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0}};
    receiverData.materials = {{"resources/Textures/white.png"}}; MeshPrimitive plane = {}; plane.mode = PrimitiveMode::Triangles;
    constexpr float kDiagonal = 0.70710678f;
    plane.vertices = {{{-1, -1, 0, 1}, {0, 0}, {kDiagonal, 0, -kDiagonal}}, {{-1, 1, 0, 1}, {0, 1}, {kDiagonal, 0, -kDiagonal}},
        {{1, -1, 0, 1}, {1, 0}, {kDiagonal, 0, -kDiagonal}}, {{1, 1, 0, 1}, {1, 1}, {kDiagonal, 0, -kDiagonal}}};
    plane.indices = {0, 1, 2, 2, 1, 3}; receiverData.primitives = {plane}; Model receiverModel; receiverModel.Initialize(&common, receiverData);
    ModelData mirrorData = receiverData;
    for (auto& vertex : mirrorData.primitives[0].vertices) { vertex.normal = {0, 0, -1}; }
    JointWeightData weights; weights.inverseBindPoseMatrix = MatrixMath::MakeIdentity4x4();
    for (uint32_t index = 0; index < 4; ++index) { weights.vertexWeights.push_back({1, index}); }
    mirrorData.skinClusterData["multipleRoot"] = weights;
    Model mirrorModel; mirrorModel.Initialize(&common, mirrorData);
    Object3d receiver; receiver.Initialize(manager); receiver.SetModel(&receiverModel); receiver.SetShadingMode(MaterialShadingMode::Standard);
    receiver.SetSurfaceProperties(0, 1, 1); receiver.GetMaterial()->roughness = 0; receiver.GetMaterial()->shininess = 0;
    Object3d mirror; mirror.Initialize(manager); mirror.SetModel(&mirrorModel); mirror.SetShadingMode(MaterialShadingMode::Standard);
    mirror.SetRotate({0, -0.785398163f, 0}); mirror.SetTranslate({4, 0, 0}); mirror.SetSurfaceProperties(0, 1, 1);
    mirror.GetMaterial()->roughness = 0; mirror.GetMaterial()->shininess = 0;
    Object3d target; target.Initialize(manager); target.SetModel(&mirrorModel); target.SetShadingMode(MaterialShadingMode::Unlit);
    target.SetTranslate({4, 0, -4}); target.SetColor({1, 0, 0, 1}); target.SetCastShadow(false);
    std::vector<Object3d*> rasterObjects = {&receiver}; std::vector<Object3d*> rayObjects = {&mirror, &target};
    RaytracingTestSceneInputs inputs; inputs.raytracingObjects = &rayObjects;
    OffscreenRenderer offscreen; offscreen.Initialize(); PostEffectManager post; post.Initialize(dxCommon); MotionVectorRenderer motion; motion.Initialize();
    reflections.SetSettings(settings); TextureManager::GetInstance()->FlushUploads();
    std::ofstream report("runtime/captures/DxrTests/multiple-reflection-result.txt"); report.setf(std::ios::unitbuf);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-off.png"), {0, 0, 0}, "OFF unexpectedly traced the reflected target");
    settings.shouldTraceMultipleReflections = true; settings.maxReflectionBounces = 1; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-limit-one.png"), {0, 0, 0}, "One reflection limit traced another bounce");
    settings.maxReflectionBounces = 2; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    float firstCoverage = 0;
    Vector3 redReflection = ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-red.png", nullptr, nullptr, &firstCoverage);
    report << "mirror inside mirror=" << redReflection.x << "," << redReflection.y << "," << redReflection.z << " first coverage=" << firstCoverage << "\n";
    RequireColor(redReflection, {1, 0, 0}, "Mirror inside mirror did not show the target");
    Require(std::abs(firstCoverage - 1) < 0.005f, "Second bounce changed first-hit coverage");
    settings.maxDistance = 4.5f; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-segment-distance.png"), {1, 0, 0}, "Distance limit was applied to total path instead of each segment");
    settings.maxDistance = 1000; reflections.SetSettings(settings);
    target.SetColor({1, 1, 1, 1}); mirror.SetColor({0.5f, 0.25f, 0.75f, 1});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    float fresnelTail = std::pow(1.0f - kDiagonal, 5.0f);
    Vector3 tintedExpected = Vector3{0.5f, 0.25f, 0.75f} + Vector3{0.5f, 0.75f, 0.25f} * fresnelTail;
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-tinted-metal.png"), tintedExpected, "Secondary metal reflection ignored colored Fresnel");
    mirror.SetColor({1, 1, 1, 1}); mirror.GetMaterial()->metallic = 0;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    float dielectricExpected = 0.04f + 0.96f * fresnelTail;
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-dielectric.png"), {dielectricExpected, dielectricExpected, dielectricExpected}, "Secondary dielectric Fresnel was missing");
    mirror.GetMaterial()->metallic = 1; mirror.GetMaterial()->specularStrength = 0;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-no-specular.png"), {0, 0, 0}, "Non-reflective hit traced secondary radiance");
    mirror.GetMaterial()->specularStrength = 1; mirror.SetShadingMode(MaterialShadingMode::Unlit); mirror.SetColor({0, 1, 0, 1});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-unlit.png"), {0, 1, 0}, "Unlit hit was replaced by a secondary reflection");
    mirror.SetShadingMode(MaterialShadingMode::Standard); mirror.SetColor({1, 1, 1, 1}); target.SetColor({1, 0, 0, 1});
    target.SetAlphaCutoff(0.5f); target.SetColor({1, 0, 0, 0.25f}); lights->SetEnvironmentLighting(0, 1);
    settings.shouldTraceMultipleReflections = false; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    Vector3 environment = ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-environment.png");
    Require(environment.x + environment.y + environment.z > 0.01f, "Secondary environment fixture lacked a fallback");
    settings.shouldTraceMultipleReflections = true; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-alpha-miss.png"), environment, "Secondary alpha miss discarded environment fallback");
    target.SetAlphaCutoff(0); target.SetColor({0, 0, 0, 1});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-black-hit.png"), {0, 0, 0}, "Secondary black hit retained/double-counted environment reflection");
    target.SetTranslate({4, 10, -4});
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-miss.png"), environment, "Secondary geometry miss discarded fallback");
    target.SetTranslate({4, 0, -4}); target.SetColor({1, 0, 0, 1}); lights->SetEnvironmentLighting(0, 0);
    // Last hit is lit and casts a shadow ray at recursion depth three.
    target.SetShadingMode(MaterialShadingMode::Standard); target.SetSurfaceProperties(1, 0, 0); target.GetMaterial()->shininess = 0;
    lighting.intensity = 1; lighting.direction = {0, 0, -1}; lights->ApplyLightingPreset(lighting);
    mirror.SetCastShadow(false); settings.shouldTraceSunShadows = true; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-lit-last-hit.png"), {1, 0, 0}, "Final reflection hit could not shade/trace sun at depth three");
    mirror.SetCastShadow(true); lighting.intensity = 0; lights->ApplyLightingPreset(lighting); settings.shouldTraceSunShadows = false;
    target.SetShadingMode(MaterialShadingMode::Standard); target.SetSurfaceProperties(0, 1, 1); target.GetMaterial()->roughness = 0;
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-mirror-loop.png"), {0, 0, 0}, "Opposing mirrors escaped the reflection depth bound");
    target.SetShadingMode(MaterialShadingMode::Unlit); target.SetColor({1, 1, 1, 1}); target.SetScale({100, 100, 1});
    mirror.GetMaterial()->roughness = 0.6f; settings.sampleCount = 16; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs);
    std::vector<float> values; ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "multiple-rough.png", nullptr, &values);
    double mean = 0;
    double error = MeasureDiffusePatch(values, reflections.GetRawTexture(), 0, &mean);
    Require(mean > 0.05 && error <= 1.001, "Rough secondary reflection was missing/non-finite or amplified unit light");
    report << "rough unit-plane mean=" << mean << " tinted Fresnel=" << tintedExpected.x << ',' << tintedExpected.y << ',' << tintedExpected.z << '\n';
    mirror.GetMaterial()->roughness = 0; target.SetScale({1, 1, 1}); settings.shouldUseTemporalHistory = true; reflections.SetSettings(settings);
    for (uint32_t index = 0; index < 3; ++index) {
        DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs); dxCommon->PostDraw();
    }
    Require(reflections.HasUsedHistory(), "Multiple reflection history did not accumulate");
    settings.maxReflectionBounces = 1; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs); dxCommon->PostDraw();
    Require(!reflections.HasUsedHistory(), "Bounce limit change retained incompatible history");
    settings.shouldTraceMultipleReflections = false; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &inputs); dxCommon->PostDraw();
    Require(!reflections.HasUsedHistory(), "Multiple reflection toggle retained incompatible history");
    report << "PASS: mirror inside mirror, OFF/one/two bounce limit, per-segment distance, Fresnel/metal/specular/unlit, alpha/black/miss fallback, last-hit sun shadow recursion, opposing mirrors, rough bounded light, history reset\n";
    manager->SetDefaultCamera(nullptr);
}

ID3D12Resource* DrawVolumeFrame(DirectXCommon* dxCommon, VolumetricLightRenderer& volume, Camera& camera,
    OffscreenRenderer& scene, OffscreenRenderer& output, D3D12_CPU_DESCRIPTOR_HANDLE outputRtv, PostEffectManager& post,
    uint64_t sceneRevision = 0, Object3d* foreground = nullptr) {
    post.PreDrawDepth(); scene.PreDraw(post.GetDepthDSVHandle()); SrvManager::GetInstance()->PreDraw();
    if (foreground) { foreground->SetCamera(&camera); foreground->Update(); Object3dManager::GetInstance()->PreDraw(); foreground->Draw(); }
    scene.PostDraw(); post.PostDrawDepth(); volume.SetFrameInputs(&camera, nullptr, nullptr, sceneRevision);
    bool hasVolume = volume.Generate(post.GetDepthSrv(), true);
    ID3D12Resource* result = scene.GetColorTexture();
    if (hasVolume) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(output.GetColorTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        auto* commandList = dxCommon->GetCommandList(); commandList->ResourceBarrier(1, &barrier);
        commandList->OMSetRenderTargets(1, &outputRtv, FALSE, nullptr); volume.Composite(scene.GetSrvHandleGPU());
        barrier = CD3DX12_RESOURCE_BARRIER::Transition(output.GetColorTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier); result = output.GetColorTexture();
    }
    dxCommon->PreDraw(); return result;
}
struct TemporalTestInputs {
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 4> textures;
    std::array<uint32_t, 4> srvIndices {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    uint32_t width = 64;
    uint32_t height = 36;
    ~TemporalTestInputs() { for (uint32_t index : srvIndices) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); } } }
    void Initialize(DirectXCommon* dxCommon, uint32_t inputWidth, uint32_t inputHeight) {
        width = inputWidth; height = inputHeight; auto* device = dxCommon->GetDevice();
        D3D12_DESCRIPTOR_HEAP_DESC heapDescription = {}; heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heapDescription.NumDescriptors = 4;
        Require(SUCCEEDED(device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&rtvHeap))), "Temporal test RTV allocation failed");
        D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        const DXGI_FORMAT kFormats[] = {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT};
        for (uint32_t index = 0; index < 4; ++index) {
            auto description = CD3DX12_RESOURCE_DESC::Tex2D(kFormats[index], width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
            Require(SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&textures[index]))), "Temporal test texture allocation failed");
            device->CreateRenderTargetView(textures[index].Get(), nullptr, Rtv(dxCommon, index));
            srvIndices[index] = SrvManager::GetInstance()->Allocate();
            SrvManager::GetInstance()->CreateSRVforTexture2D(srvIndices[index], textures[index].Get(), kFormats[index], 1);
        }
    }
    D3D12_CPU_DESCRIPTOR_HANDLE Rtv(DirectXCommon* dxCommon, uint32_t index) const {
        auto handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); return handle;
    }
    void Fill(DirectXCommon* dxCommon, Camera& camera, uint32_t pattern, uint32_t phase, float depth, float surfaceId, float motionX = 0) {
        const float kBlack[] = {0, 0, 0, 1};
        const float kHdr[] = {4, 2, 1, 1};
        float viewDepth = camera.GetNearClip() * camera.GetFarClip() /
            (camera.GetFarClip() - depth * (camera.GetFarClip() - camera.GetNearClip()));
        float colors[4][4] = {{0, 0, 0, 1}, {depth, 0, 0, 0}, {motionX, 0, 0, 0}, {0, 0, viewDepth, surfaceId}};
        if (pattern == 0) { for (uint32_t channel = 0; channel < 4; ++channel) { colors[0][channel] = kHdr[channel]; } }
        for (uint32_t index = 0; index < 4; ++index) {
            auto before = CD3DX12_RESOURCE_BARRIER::Transition(textures[index].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            dxCommon->GetCommandList()->ResourceBarrier(1, &before);
            dxCommon->GetCommandList()->ClearRenderTargetView(Rtv(dxCommon, index), colors[index], 0, nullptr);
        }
        if (pattern == 1 || pattern == 2) {
            for (uint32_t x = 0; x < width; ++x) {
                float value = float((x + phase) % 2);
                if (pattern == 2) { value = float(x) / float(width - 1) * 4; }
                float color[] = {value, value, value, 1};
                D3D12_RECT rect = {LONG(x), 0, LONG(x + 1), LONG(height)};
                dxCommon->GetCommandList()->ClearRenderTargetView(Rtv(dxCommon, 0), color, 1, &rect);
            }
        }
        if (pattern == 3) { dxCommon->GetCommandList()->ClearRenderTargetView(Rtv(dxCommon, 0), kBlack, 0, nullptr); }
        for (auto& texture : textures) {
            auto after = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            dxCommon->GetCommandList()->ResourceBarrier(1, &after);
        }
    }
    TemporalResolutionFrameInputs Frame(Camera& camera) const {
        TemporalResolutionFrameInputs result; result.camera = &camera;
        result.scene.colorTexture = textures[0].Get(); result.scene.depthTexture = textures[1].Get(); result.scene.motionVectorTexture = textures[2].Get();
        result.scene.colorSrv = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndices[0]);
        result.depthSrv = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndices[1]);
        result.motionSrv = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndices[2]);
        result.reprojectionSrv = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndices[3]); result.reprojectionTexture = textures[3].Get();
        return result;
    }
};
Vector3 ReadTemporalFrame(DirectXCommon* dxCommon, TemporalSuperResolution& temporal, TemporalTestInputs& inputs,
    Camera& camera, SuperResolutionHistoryInputs& history, uint32_t pattern, uint32_t phase, float depth,
    float surfaceId, const char* name, float motionX = 0) {
    temporal.BeginFrame(history); camera.SetProjectionJitter(temporal.GetProjectionJitterNdc());
    inputs.Fill(dxCommon, camera, pattern, phase, depth, surfaceId, motionX);
    temporal.Evaluate(inputs.Frame(camera)); dxCommon->PreDraw();
    Vector3 result = ReadTextureCenter(dxCommon, temporal.GetOutputTexture(), name); temporal.ReadCompleted(); return result;
}
float ExpectedTemporalStripe(const TemporalSuperResolution& temporal, uint32_t width, uint32_t phase) {
    float value = float((width / 2 + phase) % 2);
    float offset = std::abs(temporal.GetProjectionJitterNdc().x * float(width) * 0.5f);
    return value * (1 - offset) + (1 - value) * offset;
}
void RunTemporalResolutionValidation(DirectXCommon* dxCommon) {
    TemporalSuperResolution temporal; TemporalResolutionSettings settings;
    Require(temporal.GetAllocationBytes() == 0, "Disabled TAA allocated history images");
    settings.isEnabled = true; settings.inputWidth = 64; settings.inputHeight = 36; settings.outputWidth = 64; settings.outputHeight = 36;
    settings.shouldUseBicubic = false; Require(temporal.SetSettings(settings), "Temporal settings rejected");
    auto invalid = settings; invalid.inputWidth = 128;
    Require(!temporal.SetSettings(invalid), "Temporal downscaling/inverted size accepted");
    invalid = settings; invalid.historyWeight = std::nanf(""); Require(!temporal.SetSettings(invalid), "Nonfinite temporal weight accepted");
    invalid = settings; invalid.inputHeight = 35; Require(!temporal.SetSettings(invalid), "Temporal aspect mismatch accepted");
    Camera camera; camera.Initialize(); camera.SetAspectRatio(16.0f / 9); camera.Update();
    SuperResolutionHistoryInputs history; history.hasCamera = true; history.cameraId = reinterpret_cast<uintptr_t>(&camera);
    history.cameraHistoryId = camera.GetMotionHistoryId(); history.sceneRevision = 1;
    TemporalTestInputs inputs; inputs.Initialize(dxCommon, 64, 36);
    RequireColor(ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 0, 0, 0.5f, 1, "taa-hdr-first.png"), {4, 2, 1}, "TAA discarded HDR color");
    Require(!temporal.HasUsedHistory(), "First temporal frame used uninitialized history");
    RequireColor(ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 0, 0, 0.5f, 1, "taa-hdr-stable.png"), {4, 2, 1}, "Stable TAA changed HDR color");
    Require(temporal.HasUsedHistory(), "Stable temporal frame did not attempt history reuse");
    temporal.ResetHistory();
    float rawError = 0; float filteredError = 0;
    for (uint32_t frame = 0; frame < 40; ++frame) {
        Vector3 color = ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 1, frame, 0.5f, 1, "taa-stripes.png");
        if (frame >= 8) {
            float current = ExpectedTemporalStripe(temporal, 64, frame);
            rawError += (current - 0.5f) * (current - 0.5f) / 32;
            filteredError += (color.x - 0.5f) * (color.x - 0.5f) / 32;
        }
    }
    std::ofstream report("runtime/captures/DxrTests/taa-result.txt"); report.setf(std::ios::unitbuf);
    report << "stripe MSE current=" << rawError << " temporal=" << filteredError << '\n';
    Require(filteredError < rawError * 0.75f, "Temporal reconstruction did not reduce subpixel stripe flicker");
    uint32_t phase = 1;
    Vector3 changedId = ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 1, phase, 0.5f, 2, "taa-surface-change.png");
    Require(std::abs(changedId.x - ExpectedTemporalStripe(temporal, 64, phase)) < 0.003f, "Changed surface ID retained old history");
    Vector3 changedDepth = ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 1, phase, 0.9f, 2, "taa-disocclusion.png");
    Require(std::abs(changedDepth.x - ExpectedTemporalStripe(temporal, 64, phase)) < 0.003f, "Disoccluded surface retained history");
    Vector3 invalidMotion = ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 1, phase, 0.9f, 2, "taa-outside-motion.png", 4);
    Require(std::abs(invalidMotion.x - ExpectedTemporalStripe(temporal, 64, phase)) < 0.003f, "Outside reprojection sampled screen-edge history");
    ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 3, 0, 0.9f, 2, "taa-reactive-black.png");
    RequireColor(ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 0, 0, 0.9f, 2, "taa-reactive-hdr.png"), {4, 2, 1}, "Abrupt radiance change retained ghost lighting");
    ++history.sceneRevision; ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 0, 0, 0.9f, 2, "taa-scene-reset.png");
    Require(!temporal.HasUsedHistory(), "Scene change retained temporal history");
    ++history.radianceRevision; ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 0, 0, 0.9f, 2, "taa-light-reset.png");
    Require(!temporal.HasUsedHistory(), "Lighting revision retained temporal history");
    camera.ResetMotionHistory(); history.cameraHistoryId = camera.GetMotionHistoryId();
    ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 0, 0, 1, 0, "taa-sky-cut.png");
    Require(!temporal.HasUsedHistory(), "Camera cut retained temporal history");
    RequireColor(ReadTemporalFrame(dxCommon, temporal, inputs, camera, history, 0, 0, 1, 0, "taa-sky-stable.png"), {4, 2, 1}, "Sky reprojection corrupted radiance");
    auto missing = inputs.Frame(camera); missing.scene.motionVectorTexture = nullptr;
    Require(temporal.Evaluate(missing).ptr == missing.scene.colorSrv.ptr && temporal.GetOutputTexture() == nullptr,
        "Missing temporal inputs did not return source/reset history");
    TemporalTestInputs lowInputs; lowInputs.Initialize(dxCommon, 32, 18);
    settings.inputWidth = 32; settings.inputHeight = 18; settings.shouldUseBicubic = true; temporal.SetSettings(settings);
    Vector3 upscaled = ReadTemporalFrame(dxCommon, temporal, lowInputs, camera, history, 2, 0, 0.5f, 1, "taa-upscale-2x.png");
    Require(temporal.GetOutputTexture()->GetDesc().Width == 64 && temporal.GetOutputTexture()->GetDesc().Height == 36,
        "Temporal upscaler did not change output dimensions");
    float expectedRamp = (float(64 / 2) + 0.5f) * 0.5f - 0.5f;
    expectedRamp += temporal.GetProjectionJitterNdc().x * 32 * 0.5f;
    expectedRamp = expectedRamp / 31 * 4;
    Require(std::abs(upscaled.x - expectedRamp) < 0.025f, "Bicubic low-resolution HDR reconstruction differed from linear ramp");
    report << "32x18 -> 64x36 center=" << upscaled.x << " expected=" << expectedRamp << '\n';
    // Measure the real native 720p allocation and resolve cost using borrowed HDR/depth/motion fixtures.
    TemporalTestInputs nativeInputs; nativeInputs.Initialize(dxCommon, 1280, 720);
    settings.inputWidth = 1280; settings.inputHeight = 720; settings.outputWidth = 1280; settings.outputHeight = 720;
    temporal.SetSettings(settings);
    ReadTemporalFrame(dxCommon, temporal, nativeInputs, camera, history, 0, 0, 0.5f, 1, "taa-native-720p.png");
    ReadTemporalFrame(dxCommon, temporal, nativeInputs, camera, history, 0, 0, 0.5f, 1, "taa-native-720p-history.png");
    report << "native GPU ms=" << temporal.GetGpuTimeMs() << " images bytes=" << temporal.GetAllocationBytes() << '\n';
    settings.isEnabled = false; temporal.SetSettings(settings); temporal.BeginFrame(history);
    Require(temporal.GetProjectionJitterNdc().x == 0 && temporal.GetProjectionJitterNdc().y == 0
        && temporal.Evaluate(nativeInputs.Frame(camera)).ptr == nativeInputs.Frame(camera).scene.colorSrv.ptr,
        "Disabled temporal resolve jittered or changed the input");
    report << "PASS: HDR/stable history, stripe flicker reduction, ID/depth/disocclusion/outside motion, reactive changes, camera/scene/light reset, sky, missing/OFF, native and 2x bicubic upscale\n";
    camera.SetProjectionJitter({});
}

Vector3 DrawExposureFrame(DirectXCommon* dxCommon, OffscreenRenderer& source,
    AutoExposureRenderer& exposure, const Vector4& color, float deltaSeconds, const char* name) {
    source.SetClearColor(color); source.PreDraw(dxCommon->GetDSVHandle()); source.PostDraw();
    exposure.Generate(source.GetColorTexture(), source.GetSrvHandleGPU(), deltaSeconds);
    dxCommon->PreDraw();
    Vector3 result = ReadTextureCenter(dxCommon, exposure.GetExposureTexture(), name);
    exposure.ReadCompleted(); return result;
}
void RunHdrValidation(DirectXCommon* dxCommon) {
    AutoExposureRenderer exposure; AutoExposureSettings settings;
    Require(exposure.GetAllocationBytes() == 0, "Disabled auto exposure allocated GPU resources");
    settings.isEnabled = true; Require(exposure.SetSettings(settings), "Auto exposure settings rejected");
    auto invalidSettings = settings; invalidSettings.maxExposure = std::nanf("");
    Require(!exposure.SetSettings(invalidSettings), "Nonfinite exposure accepted");
    invalidSettings = settings; invalidSettings.lowPercentile = 0.96f;
    Require(!exposure.SetSettings(invalidSettings), "Inverted percentiles accepted");
    invalidSettings = settings; invalidSettings.minExposure = 100;
    Require(!exposure.SetSettings(invalidSettings), "Inverted exposure range accepted");
    OffscreenRenderer source; source.Initialize(); PostEffectManager post; post.Initialize(dxCommon);
    TextureManager::GetInstance()->FlushUploads();
    std::ofstream report("runtime/captures/DxrTests/hdr-result.txt"); report.setf(std::ios::unitbuf);
    float neutral = DrawExposureFrame(dxCommon, source, exposure, {0.18f, 0.18f, 0.18f, 1}, 0,
        "exposure-neutral.png").x;
    Require(std::abs(neutral - 1) < 0.08f, "Middle gray exposure was not close to one");
    exposure.ResetHistory();
    float bright = DrawExposureFrame(dxCommon, source, exposure, {2.88f, 2.88f, 2.88f, 1}, 0,
        "exposure-bright.png").x;
    Require(std::abs(bright - 0.0625f) < 0.006f, "Bright scene exposure was not inverse luminance");
    exposure.ResetHistory();
    float dark = DrawExposureFrame(dxCommon, source, exposure, {0.01125f, 0.01125f, 0.01125f, 1}, 0,
        "exposure-dark.png").x;
    Require(std::abs(dark - 16) < 1.2f, "Dark scene exposure was not inverse luminance");
    float frozen = DrawExposureFrame(dxCommon, source, exposure, {2.88f, 2.88f, 2.88f, 1}, 0,
        "exposure-zero-delta.png").x;
    Require(std::abs(frozen - dark) < 0.001f, "Zero delta changed exposure history");
    float adapting = DrawExposureFrame(dxCommon, source, exposure, {2.88f, 2.88f, 2.88f, 1}, 0.1f,
        "exposure-adapting.png").x;
    Require(adapting < dark && adapting > bright, "Exposure adaptation snapped or moved in the wrong direction");
    exposure.ResetHistory();
    DrawExposureFrame(dxCommon, source, exposure, {0.01125f, 0.01125f, 0.01125f, 1}, 0, "exposure-start.png");
    float wholeStep = DrawExposureFrame(dxCommon, source, exposure, {2.88f, 2.88f, 2.88f, 1}, 0.5f, "exposure-whole.png").x;
    exposure.ResetHistory();
    DrawExposureFrame(dxCommon, source, exposure, {0.01125f, 0.01125f, 0.01125f, 1}, 0, "exposure-start.png");
    float splitStep = 0;
    for (uint32_t index = 0; index < 5; ++index) {
        splitStep = DrawExposureFrame(dxCommon, source, exposure, {2.88f, 2.88f, 2.88f, 1}, 0.1f,
            "exposure-split.png").x;
    }
    Require(std::abs(wholeStep - splitStep) < 0.001f, "Exposure depended on frame count rather than elapsed time");
    exposure.ResetHistory();
    Require(std::abs(DrawExposureFrame(dxCommon, source, exposure, {0, 0, 0, 1}, 0, "exposure-black.png").x - 1) < 0.001f,
        "All-black scene exposure was invalid");
    settings.minExposure = 0.1f; settings.maxExposure = 2; exposure.SetSettings(settings);
    Require(std::abs(DrawExposureFrame(dxCommon, source, exposure, {100, 100, 100, 1}, 0, "exposure-min.png").x - 0.1f) < 0.001f,
        "Exposure minimum was not enforced");
    exposure.ResetHistory();
    Require(std::abs(DrawExposureFrame(dxCommon, source, exposure, {0.001f, 0.001f, 0.001f, 1}, 0, "exposure-max.png").x - 2) < 0.001f,
        "Exposure maximum was not enforced");
    settings = AutoExposureSettings(); settings.isEnabled = true; exposure.SetSettings(settings);
    source.SetClearColor({0.18f, 0.18f, 0.18f, 1}); source.PreDraw(dxCommon->GetDSVHandle());
    const float kOutlierColor[] = {60000, 60000, 60000, 1}; D3D12_RECT outlierRect = {0, 0, 32, 32};
    dxCommon->GetCommandList()->ClearRenderTargetView(dxCommon->GetRTVHandle(2), kOutlierColor, 1, &outlierRect);
    source.PostDraw(); exposure.Generate(source.GetColorTexture(), source.GetSrvHandleGPU(), 0); dxCommon->PreDraw();
    float trimmed = ReadTextureCenter(dxCommon, exposure.GetExposureTexture(), "exposure-outlier.png").x;
    Require(std::abs(trimmed - neutral) < 0.001f, "Bright outlier escaped percentile trimming");
    exposure.ReadCompleted();
    report << "neutral=" << neutral << " bright=" << bright << " dark=" << dark << " adapting=" << adapting
        << " whole/split=" << wholeStep << '/' << splitStep << " GPU ms=" << exposure.GetGpuTimeMs()
        << " allocationBytes=" << exposure.GetAllocationBytes() << '\n';
    auto* device = dxCommon->GetDevice();
    D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto description = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, WinApp::kClientWidth,
        WinApp::kClientHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    Microsoft::WRL::ComPtr<ID3D12Resource> output;
    Require(SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&output))), "HDR test target allocation failed");
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescription = {}; rtvDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; rtvDescription.NumDescriptors = 1;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    Require(SUCCEEDED(device->CreateDescriptorHeap(&rtvDescription, IID_PPV_ARGS(&rtvHeap))), "HDR test RTV allocation failed");
    auto rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart(); device->CreateRenderTargetView(output.Get(), nullptr, rtv);
    auto* copy = post.GetCopyImageRenderer(); auto& parameters = copy->GetPostEffectParameter();
    parameters.toneExposure = 1; parameters.toneContrast = 1; parameters.toneSaturation = 1;
    auto* bloom = post.GetBloomRenderer(); auto* bloomSettings = bloom->GetEditableBloomParameter();
    Vector3 toneColors[2];
    for (uint32_t mode = 0; mode < 2; ++mode) {
        source.SetClearColor({4, 2, 1, 1}); source.PreDraw(dxCommon->GetDSVHandle()); source.PostDraw();
        SrvManager::GetInstance()->PreDraw(); post.SetToneMapping(mode, 0);
        auto before = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        dxCommon->GetCommandList()->ResourceBarrier(1, &before); dxCommon->GetCommandList()->OMSetRenderTargets(1, &rtv, false, nullptr);
        copy->SetOutputFormat(DXGI_FORMAT_R16G16B16A16_FLOAT); copy->SetPostEffectType(PostEffectType::ToneMap);
        copy->Draw(source.GetSrvHandleGPU(), source.GetSrvHandleGPU(), source.GetNormalSrvHandleGPU());
        auto after = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxCommon->GetCommandList()->ResourceBarrier(1, &after); dxCommon->PreDraw();
        std::string name = "hdr-tone-" + std::to_string(mode) + ".png";
        toneColors[mode] = ReadTextureCenter(dxCommon, output.Get(), name.c_str());
    }
    Require(std::abs(toneColors[1].y / toneColors[1].x - 0.5f) < 0.002f
        && std::abs(toneColors[1].z / toneColors[1].x - 0.25f) < 0.002f, "Hue-preserving tone map lost highlight RGB ratios");
    Require(toneColors[0].y > toneColors[1].y + 0.2f, "Tone mapping mode did not change highlight compression");
    const PostEffectType kHdrEffects[] = {PostEffectType::AnamorphicFlare, PostEffectType::ArchiveAtmosphere,
        PostEffectType::FilmGrain, PostEffectType::GhostImage, PostEffectType::Glare, PostEffectType::Halo,
        PostEffectType::LensDirt, PostEffectType::LensFlare, PostEffectType::LightShafts, PostEffectType::LightStreak,
        PostEffectType::NeonGlow, PostEffectType::VolumetricLight};
    parameters.lightStrength = 0; parameters.lensDirtStrength = 0; parameters.filmGrainStrength = 0;
    for (PostEffectType type : kHdrEffects) {
        source.SetClearColor({4, 2, 1, 1}); source.PreDraw(dxCommon->GetDSVHandle()); source.PostDraw();
        SrvManager::GetInstance()->PreDraw();
        auto before = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        dxCommon->GetCommandList()->ResourceBarrier(1, &before);
        dxCommon->GetCommandList()->OMSetRenderTargets(1, &rtv, false, nullptr);
        copy->SetOutputFormat(DXGI_FORMAT_R16G16B16A16_FLOAT); copy->SetPostEffectType(type);
        copy->Draw(source.GetSrvHandleGPU(), source.GetSrvHandleGPU(), source.GetNormalSrvHandleGPU());
        auto after = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxCommon->GetCommandList()->ResourceBarrier(1, &after); dxCommon->PreDraw();
        std::string name = "hdr-intermediate-" + std::to_string(static_cast<int>(type)) + ".png";
        Vector3 color = ReadTextureCenter(dxCommon, output.Get(), name.c_str());
        Require(color.x > 1 && std::isfinite(color.x), "Intermediate post effect clipped HDR color to LDR");
    }
    Require(!post.SetToneMapping(2, 0) && !post.SetToneMapping(1, std::nanf("")), "Invalid tone settings accepted");
    bloomSettings->isEnabled = 1; bloomSettings->threshold = 1; bloomSettings->intensity = 1;
    Vector3 bloomColors[3];
    for (uint32_t mode = 0; mode < 3; ++mode) {
        Vector4 color = {4, 2, 1, 1};
        if (mode == 2) { color = {60000, 60000, 60000, 1}; bloomSettings->intensity = 5; }
        bloomSettings->maxRadiance = 65504;
        if (mode == 1) { bloomSettings->maxRadiance = 1; }
        source.SetClearColor(color); source.PreDraw(dxCommon->GetDSVHandle()); source.PostDraw();
        SrvManager::GetInstance()->PreDraw(); bloom->Generate(source.GetSrvHandleGPU());
        auto before = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        dxCommon->GetCommandList()->ResourceBarrier(1, &before);
        dxCommon->GetCommandList()->OMSetRenderTargets(1, &rtv, false, nullptr); bloom->Composite(source.GetSrvHandleGPU());
        auto after = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxCommon->GetCommandList()->ResourceBarrier(1, &after); dxCommon->PreDraw();
        std::string name = "hdr-bloom-" + std::to_string(mode) + ".png";
        bloomColors[mode] = ReadTextureCenter(dxCommon, output.Get(), name.c_str());
    }
    Require(bloomColors[0].x > 6 && bloomColors[0].y > 2, "Bloom lost HDR energy before tone mapping");
    Require(bloomColors[1].x < bloomColors[0].x - 2, "Bloom radiance limit did not suppress bright extraction");
    Require(std::isfinite(bloomColors[2].x) && bloomColors[2].x <= 65504, "HDR Bloom overflowed half float output");
    // Validate the real final-pass integration, not only the standalone meter.
    source.SetClearColor({4, 4, 4, 1}); source.PreDraw(dxCommon->GetDSVHandle()); source.PostDraw();
    bloomSettings->isEnabled = 0; post.SetFxaaEnabled(false);
    auto integratedSettings = post.GetAutoExposureRenderer()->GetSettings(); integratedSettings.isEnabled = true;
    post.GetAutoExposureRenderer()->SetSettings(integratedSettings);
    dxCommon->PreDraw(); SrvManager::GetInstance()->PreDraw(); post.Apply(nullptr, source.GetSrvHandleGPU());
    Vector3 automaticOutput = ReadTextureCenter(dxCommon, dxCommon->GetCurrentBackBuffer(), "hdr-final-auto.png",
        nullptr, nullptr, nullptr, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon->PreDraw();
    float integrated = ReadTextureCenter(dxCommon, post.GetAutoExposureRenderer()->GetExposureTexture(), "exposure-integrated.png").x;
    Require(std::abs(integrated - 0.045f) < 0.005f && parameters.colorFinishSettings.y > 0.5f,
        "Final HDR pipeline did not meter/bind automatic exposure");
    post.ReadCompletedGpuTiming();
    float linearInput = 4 * integrated;
    float expectedLinear = linearInput * (2.43f * linearInput + 0.03f)
        / (linearInput * (2.43f * linearInput + 0.59f) + 0.14f);
    float expectedSrgb = 1.055f * std::pow(expectedLinear, 1.0f / 2.4f) - 0.055f;
    Require(std::abs(automaticOutput.x - expectedSrgb) < 0.01f, "Final automatic exposure/tone/sRGB conversion mismatch");
    post.SetFxaaEnabled(true);
    dxCommon->PreDraw(); SrvManager::GetInstance()->PreDraw(); post.Apply(nullptr, source.GetSrvHandleGPU());
    Vector3 fxaaOutput = ReadTextureCenter(dxCommon, dxCommon->GetCurrentBackBuffer(), "hdr-final-fxaa.png",
        nullptr, nullptr, nullptr, D3D12_RESOURCE_STATE_RENDER_TARGET);
    RequireColor(fxaaOutput, automaticOutput, "FXAA changed the uniform automatic exposure/tone output");
    post.SetToneMapping(1, 1);
    dxCommon->PreDraw(); SrvManager::GetInstance()->PreDraw(); post.Apply(nullptr, source.GetSrvHandleGPU());
    Vector3 compensatedOutput = ReadTextureCenter(dxCommon, dxCommon->GetCurrentBackBuffer(), "hdr-final-compensation.png",
        nullptr, nullptr, nullptr, D3D12_RESOURCE_STATE_RENDER_TARGET);
    Require(compensatedOutput.x > automaticOutput.x + 0.1f, "Exposure compensation EV was not applied in FXAA");
    post.SetToneMapping(1, 0);
    integratedSettings.isEnabled = false; post.GetAutoExposureRenderer()->SetSettings(integratedSettings);
    dxCommon->PreDraw(); SrvManager::GetInstance()->PreDraw(); post.Apply(nullptr, source.GetSrvHandleGPU());
    Vector3 manualOutput = ReadTextureCenter(dxCommon, dxCommon->GetCurrentBackBuffer(), "hdr-final-manual.png",
        nullptr, nullptr, nullptr, D3D12_RESOURCE_STATE_RENDER_TARGET);
    Require(manualOutput.x > automaticOutput.x + 0.2f && parameters.colorFinishSettings.y == 0,
        "Auto exposure OFF did not restore manual final output");
    report << "final automatic=" << automaticOutput.x << " expected sRGB=" << expectedSrgb << " manual=" << manualOutput.x << '\n';
    settings.isEnabled = false; exposure.SetSettings(settings);
    Require(exposure.GetExposureSrv().ptr == 0, "Disabled exposure retained a public output");
    report << "tone legacy=" << toneColors[0].x << ',' << toneColors[0].y << ',' << toneColors[0].z
        << " hue=" << toneColors[1].x << ',' << toneColors[1].y << ',' << toneColors[1].z << '\n';
    report << "Bloom HDR=" << bloomColors[0].x << " limited=" << bloomColors[1].x << " maximum=" << bloomColors[2].x << '\n';
    report << "PASS: inverse luminance, adaptation/time invariance, black/min/max/OFF/invalid, final sRGB/FXAA/EV/manual restore, hue-preserving HDR highlights, 12 intermediate HDR effects, Bloom HDR/limit/finite output\n";
}

void RunVolumetricQualityValidation(DirectXCommon* dxCommon) {
    Camera camera; camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, 0}, {0, 0, 1}); camera.Update();
    auto* manager = Object3dManager::GetInstance(); manager->SetDefaultCamera(&camera); manager->SetBlendMode(kBlendModeNone);
    manager->SetShadowRenderer(nullptr); manager->SetLocalShadowRenderer(nullptr);
    auto* lights = LightManager::GetInstance(); lights->ClearDynamicPointLights(); lights->ClearDynamicSpotLights();
    LightingPreset lighting; lighting.intensity = 0; lighting.ambient = {1, 1, 1, 0}; lighting.pointIntensity = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0);
    OffscreenRenderer output; output.Initialize();
    auto outputInitialBarrier = CD3DX12_RESOURCE_BARRIER::Transition(output.GetColorTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    dxCommon->GetCommandList()->ResourceBarrier(1, &outputInitialBarrier);
    OffscreenRenderer scene; scene.Initialize(); scene.SetClearColor({0.2f, 0.4f, 0.8f, 1});
    PostEffectManager post; post.Initialize(dxCommon);
    auto outputHeap = dxCommon->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    auto outputRtv = outputHeap->GetCPUDescriptorHandleForHeapStart();
    dxCommon->GetDevice()->CreateRenderTargetView(output.GetColorTexture(), nullptr, outputRtv);
    VolumetricLightRenderer volume; Require(volume.Initialize(dxCommon), "Volumetric renderer initialization failed");
    Require(volume.SetQuality(VolumetricQuality::Low) && volume.GetParameters().sampleCount == 16, "Low volume quality mismatch");
    Require(volume.SetQuality(VolumetricQuality::High) && volume.GetParameters().sampleCount == 64, "High volume quality mismatch");
    Require(volume.SetQuality(VolumetricQuality::Medium) && volume.GetParameters().sampleCount == 32, "Medium volume quality mismatch");
    Require(!volume.SetHistoryWeight(std::nanf("")) && !volume.SetHistoryWeight(1), "Invalid volumetric history weight accepted");
    Require(!volume.SetScatteringAlbedo(-0.1f) && !volume.SetScatteringAlbedo(std::nanf("")), "Invalid scattering albedo accepted");
    volume.SetMaxDistance(10); volume.SetLocalFogEnabled(true); volume.SetHeightFog(1000, 0.01f, 0.1f);
    volume.SetFogColor({0.6f, 0.7f, 0.8f}); volume.SetTemporalEnabled(false); volume.SetScatteringAlbedo(0);
    uint64_t rawAllocationBytes = volume.GetAllocationBytes();
    std::ofstream report("runtime/captures/DxrTests/volumetric-quality-result.txt"); report.setf(std::ios::unitbuf);
    auto* source = DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post);
    Vector3 absorbed = ReadTextureCenter(dxCommon, source, "volume-absorption.png"); dxCommon->PreDraw();
    float transmission = ReadTextureCenter(dxCommon, volume.GetRawTransmittanceTexture(), "volume-transmittance.png").x;
    float expectedTransmission = std::exp(-0.01f * 10);
    Require(std::abs(transmission - expectedTransmission) < 0.001f, "Volume transmittance disagreed with Beer absorption law");
    RequireColor(absorbed, Vector3{0.2f, 0.4f, 0.8f} * expectedTransmission, "Pure absorbing fog incorrectly added scattered color");
    volume.SetScatteringAlbedo(1);
    source = DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post);
    RequireColor(ReadTextureCenter(dxCommon, source, "volume-scattering.png"), Vector3{0.2f, 0.4f, 0.8f} * expectedTransmission
        + Vector3{0.6f, 0.7f, 0.8f} * (1 - expectedTransmission), "Fog scattering albedo changed extinction or lost ambient scattering");
    volume.SetScatteringAlbedo(0.5f);
    source = DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post);
    RequireColor(ReadTextureCenter(dxCommon, source, "volume-half-albedo.png"), Vector3{0.2f, 0.4f, 0.8f} * expectedTransmission
        + Vector3{0.6f, 0.7f, 0.8f} * (0.5f * (1 - expectedTransmission)), "Scattering/absorption split did not preserve extinction");
    volume.SetScatteringAlbedo(0); volume.SetHeightFog(0, 0, 0.1f);
    FogVolumeSettings sphere; sphere.isEnabled = true; sphere.center = {0, 0, 5}; sphere.radius = 0.9f; sphere.density = 0.05f; sphere.edgeSoftness = 0.1f;
    Require(volume.SetFogVolume(0, sphere), "Thin volume fixture rejected"); volume.SetSampleCount(64);
    DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post);
    std::vector<float> reference; ReadTextureCenter(dxCommon, volume.GetRawTransmittanceTexture(), "volume-reference64.png", nullptr, &reference);
    volume.SetSampleCount(8); volume.SetTemporalEnabled(true);
    double rawError = 0; double filteredError = 0; std::vector<float> rawValues;
    for (uint32_t frame = 0; frame < 32; ++frame) {
        DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post);
        if (frame == 0) { ReadTextureCenter(dxCommon, volume.GetRawTransmittanceTexture(), "volume-jitter-raw.png", nullptr, &rawValues); }
        else { dxCommon->PostDraw(); }
    }
    Require(volume.HasUsedHistory(), "Stationary fog did not reuse temporal history"); dxCommon->PreDraw();
    std::vector<float> filtered; ReadTextureCenter(dxCommon, volume.GetFilteredTransmittanceTexture(), "volume-filtered.png", nullptr, &filtered);
    size_t width = static_cast<size_t>(volume.GetRawTransmittanceTexture()->GetDesc().Width);
    size_t height = volume.GetRawTransmittanceTexture()->GetDesc().Height;
    size_t count = 0;
    for (size_t row = height / 2 - 15; row <= height / 2 + 15; ++row) {
        for (size_t column = width / 2 - 15; column <= width / 2 + 15; ++column) {
            size_t index = row * width + column;
            Require(std::isfinite(filtered[index]) && filtered[index] >= 0 && filtered[index] <= 1, "Filtered volume transmission left physical bounds");
            rawError += std::pow(rawValues[index] - reference[index], 2); filteredError += std::pow(filtered[index] - reference[index], 2); ++count;
        }
    }
    rawError /= count; filteredError /= count;
    report << "Beer transmission=" << transmission << " expected=" << expectedTransmission << " mseRaw8=" << rawError << " mseFiltered8=" << filteredError << '\n';
    Require(filteredError < rawError * 0.5, "Volumetric temporal filtering failed to reduce fixed-sample integration noise");
    report << "raw allocation bytes=" << rawAllocationBytes << " with history=" << volume.GetAllocationBytes() << '\n';
    volume.ReadCompleted(); report << "raymarchMs=" << volume.GetRaymarchGpuTimeMs() << " temporalMs=" << volume.GetTemporalGpuTimeMs()
        << " compositeMs=" << volume.GetCompositeGpuTimeMs() << '\n';
    ModelCommon common; common.Initialize(dxCommon); ModelData data; data.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    data.materials = {{"resources/Textures/white.png"}}; MeshPrimitive plane = {}; plane.mode = PrimitiveMode::Triangles;
    plane.vertices = {{{-1, -1, 0, 1}, {0, 0}, {0, 0, -1}}, {{-1, 1, 0, 1}, {0, 1}, {0, 0, -1}},
        {{1, -1, 0, 1}, {1, 0}, {0, 0, -1}}, {{1, 1, 0, 1}, {1, 1}, {0, 0, -1}}};
    plane.indices = {0, 1, 2, 2, 1, 3}; data.primitives = {plane}; Model model; model.Initialize(&common, data);
    Object3d foreground; foreground.Initialize(manager); foreground.SetModel(&model); foreground.SetTranslate({0, 0, 2}); foreground.SetShadingMode(MaterialShadingMode::Unlit);
    DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post, 0, &foreground);
    float foregroundTransmission = ReadTextureCenter(dxCommon, volume.GetFilteredTransmittanceTexture(), "volume-depth-rejection.png").x;
    Require(std::abs(foregroundTransmission - 1) < 0.001f, "Fog history leaked through new foreground depth");
    camera.ResetMotionHistory(); DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post); dxCommon->PostDraw();
    Require(!volume.HasUsedHistory(), "Volume camera cut kept history");
    DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post, 2); dxCommon->PostDraw();
    Require(!volume.HasUsedHistory(), "Volume scene revision kept history");
    DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post, 2); dxCommon->PostDraw();
    Require(volume.HasUsedHistory(), "Stable volume scene lost history");
    camera.SetTranslate({0.01f, 0, 0}); camera.Update();
    DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post, 2); dxCommon->PostDraw();
    Require(volume.HasUsedHistory(), "Small camera motion discarded volume history");
    sphere.density = 0.01f; volume.SetFogVolume(0, sphere);
    DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post, 2); dxCommon->PostDraw();
    Require(!volume.HasUsedHistory(), "Density change kept incompatible volume history");
    volume.SetFrameInputs(nullptr, nullptr); Require(!volume.Generate(post.GetDepthSrv(), true), "Missing volume camera was accepted");
    volume.SetEnabled(false); source = DrawVolumeFrame(dxCommon, volume, camera, scene, output, outputRtv, post);
    RequireColor(ReadTextureCenter(dxCommon, source, "volume-disabled.png"), {0.2f, 0.4f, 0.8f}, "Volume OFF modified scene");
    Require(!volume.HasUsedHistory() && volume.GetRaymarchGpuTimeMs() == 0 && volume.GetTemporalGpuTimeMs() == 0, "Volume OFF retained history or timing");
    report << "PASS: Beer transmission, scattering/absorption, quality presets, bounded filtered signal/noise reduction, depth rejection, camera cut/motion, scene/config resets, missing inputs, OFF, GPU timers\n";
    manager->SetDefaultCamera(nullptr);
}

double ReferenceGgxDirectionalAlbedo(double roughness, double viewCosine) {
    // Independent uniform-hemisphere quadrature of D * G1(V) * G1(L) / (4 * N.V).
    // White conductor Fresnel is one; no VNDF sample routine is used here.
    constexpr uint32_t kIntegrationSteps = 512; constexpr double kPi = 3.14159265358979323846;
    double alpha = roughness * roughness; double alphaSquared = alpha * alpha;
    double viewSine = std::sqrt(1 - viewCosine * viewCosine);
    double viewVisibility = 2 * viewCosine / (viewCosine + std::sqrt(alphaSquared + (1 - alphaSquared) * viewCosine * viewCosine));
    double integral = 0;
    for (uint32_t depthIndex = 0; depthIndex < kIntegrationSteps; ++depthIndex) {
        double lightCosine = (depthIndex + 0.5) / kIntegrationSteps;
        double lightSine = std::sqrt(1 - lightCosine * lightCosine);
        double lightVisibility = 2 * lightCosine / (lightCosine + std::sqrt(alphaSquared + (1 - alphaSquared) * lightCosine * lightCosine));
        for (uint32_t angleIndex = 0; angleIndex < kIntegrationSteps; ++angleIndex) {
            double angle = 2 * kPi * (angleIndex + 0.5) / kIntegrationSteps;
            double sumX = viewSine + lightSine * std::cos(angle); double sumY = lightSine * std::sin(angle);
            double sumZ = viewCosine + lightCosine;
            double halfCosine = sumZ / std::sqrt(sumX * sumX + sumY * sumY + sumZ * sumZ);
            double denominator = halfCosine * halfCosine * (alphaSquared - 1) + 1;
            double distribution = alphaSquared / (kPi * denominator * denominator);
            integral += distribution * viewVisibility * lightVisibility / (4 * viewCosine);
        }
    }
    return integral * 2 * kPi / (kIntegrationSteps * kIntegrationSteps);
}
void RunRoughReflectionValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene; scene.Initialize(); if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings; sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    Camera camera; camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    auto* manager = Object3dManager::GetInstance(); manager->SetDefaultCamera(&camera); manager->SetBlendMode(kBlendModeNone);
    manager->SetShadowRenderer(nullptr); manager->SetLocalShadowRenderer(nullptr);
    auto* lights = LightManager::GetInstance(); lights->ClearDynamicPointLights(); lights->ClearDynamicSpotLights();
    LightingPreset lighting; lighting.intensity = 0; lighting.ambient = {1, 1, 1, 0}; lighting.pointIntensity = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0); lights->SetLightingComponents(1, 1, 1, 0);
    ModelCommon common; common.Initialize(dxCommon);
    ModelData receiverData; receiverData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4(); receiverData.materials = {{"resources/Textures/white.png"}};
    MeshPrimitive plane = {}; plane.mode = PrimitiveMode::Triangles;
    plane.vertices = {{{-10, -10, 0, 1}, {0, 0}, {0, 0, -1}}, {{-10, 10, 0, 1}, {0, 1}, {0, 0, -1}},
        {{10, -10, 0, 1}, {1, 0}, {0, 0, -1}}, {{10, 10, 0, 1}, {1, 1}, {0, 0, -1}}};
    plane.indices = {0, 1, 2, 2, 1, 3}; receiverData.primitives = {plane};
    ModelData roomData; roomData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4(); roomData.materials = receiverData.materials;
    for (uint32_t axis = 0; axis < 3; ++axis) {
        for (int side = -1; side <= 1; side += 2) {
            MeshPrimitive face = plane;
            for (auto& vertex : face.vertices) {
                float first = vertex.position.x * 10; float second = vertex.position.y * 10; float distance = side * 100.0f;
                if (axis == 0) { vertex.position = {distance, first, second, 1}; }
                if (axis == 1) { vertex.position = {first, distance, second, 1}; }
                if (axis == 2) { vertex.position = {first, second, distance, 1}; }
            }
            roomData.primitives.push_back(face);
        }
    }
    Model roomModel; roomModel.Initialize(&common, roomData);
    Object3d room; room.Initialize(manager); room.SetModel(&roomModel); room.SetShadingMode(MaterialShadingMode::Unlit); room.SetCastShadow(false);
    std::vector<Object3d*> rayObjects = {&room}; RaytracingTestSceneInputs sceneInputs; sceneInputs.raytracingObjects = &rayObjects;
    OffscreenRenderer offscreen; offscreen.Initialize(); PostEffectManager post; post.Initialize(dxCommon); MotionVectorRenderer motion; motion.Initialize();
    DxrReflectionRenderer reflection; reflection.Initialize(); auto settings = reflection.GetSettings();
    settings.isEnabled = true; settings.sampleCount = 16; settings.maxRoughness = 1; settings.maxDistance = 1000;
    settings.shouldUseTemporalHistory = false; settings.shouldTraceSunShadows = false; settings.spatialPassCount = 0; reflection.SetSettings(settings);
    std::ofstream report("runtime/captures/DxrTests/rough-reflection-result.txt"); report.setf(std::ios::unitbuf);
    const float kViewCosines[] = {1, 0.6f, 0.15f}; const float kRoughnessValues[] = {0.35f, 0.6f, 0.9f, 1};
    for (float viewCosine : kViewCosines) {
        ModelData data = receiverData; float viewSine = std::sqrt(1 - viewCosine * viewCosine);
        for (auto& vertex : data.primitives[0].vertices) { vertex.normal = {viewSine, 0, -viewCosine}; }
        Model model; model.Initialize(&common, data); Object3d receiver; receiver.Initialize(manager); receiver.SetModel(&model);
        receiver.SetShadingMode(MaterialShadingMode::Standard); receiver.GetMaterial()->metallic = 1; receiver.GetMaterial()->specularStrength = 1;
        receiver.GetMaterial()->shininess = 0; std::vector<Object3d*> rasterObjects = {&receiver}; TextureManager::GetInstance()->FlushUploads();
        for (float roughness : kRoughnessValues) {
            receiver.GetMaterial()->roughness = roughness;
            DrawReflectionFrame(dxCommon, scene, reflection, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &sceneInputs);
            std::vector<float> pixels; ReadTextureCenter(dxCommon, reflection.GetRawTexture(), "rough-furnace.png", nullptr, &pixels);
            uint32_t width = WinApp::kClientWidth / 2; uint32_t height = WinApp::kClientHeight / 2;
            double average = 0; constexpr int kPatchRadius = 8; uint32_t count = 0;
            for (int y = -kPatchRadius; y <= kPatchRadius; ++y) {
                for (int x = -kPatchRadius; x <= kPatchRadius; ++x) {
                    float value = pixels[(height / 2 + y) * width + width / 2 + x];
                    Require(std::isfinite(value) && value >= 0 && value <= 1.001f, "GGX reflection amplified unit incoming light or produced invalid radiance");
                    average += value; ++count;
                }
            }
            average /= count;
            double ndcX = ((width / 2 * 2 + 1.5) / WinApp::kClientWidth) * 2 - 1;
            double ndcY = 1 - ((height / 2 * 2 + 1.5) / WinApp::kClientHeight) * 2;
            double viewX = -ndcX * std::tan(0.35) * WinApp::kClientWidth / WinApp::kClientHeight;
            double viewY = -ndcY * std::tan(0.35);
            double actualViewCosine = (viewCosine + viewSine * viewX) / std::sqrt(1 + viewX * viewX + viewY * viewY);
            double reference = ReferenceGgxDirectionalAlbedo(roughness, actualViewCosine);
            report << "roughness=" << roughness << " viewCosine=" << viewCosine << " gpuMean=" << average << " quadrature=" << reference << '\n';
            Require(std::abs(average - reference) < 0.035, "VNDF reflection disagrees with independent GGX BRDF integration");
        }
        receiver.GetMaterial()->roughness = 0;
        DrawReflectionFrame(dxCommon, scene, reflection, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &sceneInputs);
        RequireColor(ReadTextureCenter(dxCommon, reflection.GetRawTexture(), "rough-mirror-limit.png"), {1, 1, 1}, "GGX update changed mirror limit");
        receiver.GetMaterial()->metallic = 0;
        DrawReflectionFrame(dxCommon, scene, reflection, offscreen, post, motion, camera, rasterObjects, 0, nullptr, nullptr, true, &sceneInputs);
        Vector3 dielectric = ReadTextureCenter(dxCommon, reflection.GetRawTexture(), "rough-dielectric-mirror.png");
        double expected = 0.04 + 0.96 * std::pow(1 - viewCosine, 5);
        Require(std::abs(dielectric.x - expected) < 0.01, "Dielectric mirror Fresnel mismatch");
    }
    report << "PASS: unit-radiance furnace at 12 roughness/view combinations, independent hemisphere BRDF quadrature, bounded finite weights, metal/dielectric mirror limits\n";
    manager->SetDefaultCamera(nullptr);
}

void RunReceiverMotionValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene; scene.Initialize(); if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings; sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    Camera camera; camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    auto* objectManager = Object3dManager::GetInstance(); auto* skinManager = SkinningObject3dManager::GetInstance();
    objectManager->SetDefaultCamera(&camera); objectManager->SetBlendMode(kBlendModeNone);
    objectManager->SetShadowRenderer(nullptr); objectManager->SetLocalShadowRenderer(nullptr);
    skinManager->SetDefaultCamera(&camera); skinManager->SetBlendMode(kBlendModeNone);
    skinManager->SetEnvironmentTexture(objectManager->GetEnvironmentTexture());
    auto* lights = LightManager::GetInstance(); lights->ClearDynamicPointLights(); lights->ClearDynamicSpotLights();
    LightingPreset lighting; lighting.intensity = 0; lighting.ambient = {1, 1, 1, 0.2f}; lighting.pointIntensity = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0); lights->SetLightingComponents(1, 1, 1, 0);
    ModelCommon modelCommon; modelCommon.Initialize(dxCommon); ModelData data;
    data.rootNode.name = "receiverMotionRoot"; data.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    data.rootNode.transform = {{1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0}}; data.materials = {{"resources/Textures/white.png"}};
    MeshPrimitive primitive = {}; primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices = {{{-1, -1, 0, 1}, {0, 0}, {0, 0, -1}}, {{-1, 1, 0, 1}, {0, 1}, {0, 0, -1}},
        {{1, -1, 0, 1}, {1, 0}, {0, 0, -1}}, {{1, 1, 0, 1}, {1, 1}, {0, 0, -1}}};
    primitive.indices = {0, 1, 2, 2, 1, 3}; data.primitives = {primitive};
    JointWeightData weights; weights.inverseBindPoseMatrix = MatrixMath::MakeIdentity4x4();
    for (uint32_t index = 0; index < 4; ++index) { weights.vertexWeights.push_back({1, index}); }
    data.skinClusterData["receiverMotionRoot"] = weights; Model model; model.Initialize(&modelCommon, data);
    Object3d receiver; receiver.Initialize(objectManager); receiver.SetModel(&model); receiver.SetShadingMode(MaterialShadingMode::Standard);
    receiver.SetReceiveShadow(true); receiver.SetCastShadow(false); receiver.GetMaterial()->specularStrength = 0; receiver.GetMaterial()->shininess = 0;
    Object3d source; source.Initialize(objectManager); source.SetModel(&model); source.SetTranslate({0, 0, -6}); source.SetScale({2000, 2000, 1});
    source.SetColor({1, 0, 0, 1}); source.SetCastShadow(false);
    std::vector<Object3d*> objects = {&receiver, &source};
    OffscreenRenderer offscreen; offscreen.Initialize(); PostEffectManager postEffects; postEffects.Initialize(dxCommon);
    MotionVectorRenderer motion; motion.Initialize(); TextureManager::GetInstance()->FlushUploads();
    Require(motion.GetReprojectionAllocationBytes() >= 2ull * WinApp::kClientWidth * WinApp::kClientHeight * 16, "Motion metadata allocation mismatch");
    DxrGlobalIlluminationRenderer indirect; indirect.Initialize(); auto settings = indirect.GetSettings();
    settings.isEnabled = true; settings.maxDistance = 1000; settings.shouldTraceSunShadows = false; settings.spatialPassCount = 0;
    indirect.SetSettings(settings); std::ofstream report("runtime/captures/DxrTests/receiver-motion-result.txt"); report.setf(std::ios::unitbuf);
    for (uint32_t frame = 0; frame < 3; ++frame) {
        DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects);
        Vector3 statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-stable.png");
        Require(statistics.z >= frame + 1, "Stationary receiver lost motion history");
    }
    receiver.SetTranslate({0.4f, 0, 0.5f});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects);
    Vector3 statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-translated-history.png");
    Require(statistics.z >= 4, "Fast receiver translation/depth change discarded compatible GI history");
    dxCommon->PreDraw(); float surfaceId = 0;
    Vector3 metadata = ReadTextureCenter(dxCommon, motion.GetReprojectionTexture(), "receiver-motion-translated-metadata.png", nullptr, nullptr, &surfaceId);
    RequireColor(metadata, {1, 1, 4}, "Motion metadata did not contain previous normal/depth"); Require(surfaceId > 0, "Valid receiver history has no surface ID");
    dxCommon->PreDraw(); Vector3 displacement = ReadTextureCenter(dxCommon, motion.GetTexture(), "receiver-motion-translated-vector.png");
    Require(std::abs(displacement.x) * WinApp::kClientWidth > 3, "Moving receiver fixture did not exceed old conservative threshold");
    report << "translationHistory=" << statistics.z << " previousDepth=" << metadata.z << " motionPixels=" << displacement.x * WinApp::kClientWidth << '\n';
    receiver.SetRotate({0, 0.35f, 0});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects);
    statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-rotated-history.png");
    Require(statistics.z >= 5, "Rotated receiver compared current normal against old geometry");
    receiver.SetScale({1.4f, 1.2f, 0.7f});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects);
    statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-scaled-history.png");
    Require(statistics.z >= 6, "Nonuniform receiver scale discarded compatible history");
    WriteMaterialFixture(L"runtime/captures/DxrTests/receiver-motion-normal.png", {204, 204, 204, 255});
    receiver.SetNormalMap("runtime/captures/DxrTests/receiver-motion-normal.png", 1); TextureManager::GetInstance()->FlushUploads(); indirect.ResetHistory();
    for (uint32_t frame = 0; frame < 3; ++frame) {
        DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects);
        statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-normal-map-stable.png");
        Require(statistics.z >= frame + 1, "Normal-mapped stationary receiver lost its compatible history");
    }
    dxCommon->PreDraw(); metadata = ReadTextureCenter(dxCommon, motion.GetReprojectionTexture(), "receiver-motion-normal-map-metadata.png");
    Require(metadata.z < 0, "Normal map did not select conservative shading-normal validation");
    receiver.SetNormalMap("");
    Object3d replacement; replacement.Initialize(objectManager); replacement.SetModel(&model); replacement.SetShadingMode(MaterialShadingMode::Standard);
    replacement.SetTranslate({0.4f, 0, 0.5f}); replacement.SetRotate({0, 0.35f, 0}); replacement.SetScale({1.4f, 1.2f, 0.7f});
    replacement.SetCastShadow(false); replacement.GetMaterial()->specularStrength = 0; replacement.GetMaterial()->shininess = 0;
    objects[0] = &replacement;
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects);
    statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-replacement-history.png");
    Require(statistics.z == 1, "Same-looking replacement object reused another receiver history");
    dxCommon->PreDraw(); float replacementId = 0;
    ReadTextureCenter(dxCommon, motion.GetReprojectionTexture(), "receiver-motion-replacement-id.png", nullptr, nullptr, &replacementId);
    Require(replacementId < 0 && -replacementId != surfaceId, "First-frame validity or unique surface ID failed");
    for (uint32_t frame = 0; frame < 2; ++frame) {
        DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects);
        ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-replacement-stable.png");
    }
    replacement.SetTranslate({20, 0, 0});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects);
    ReadTextureCenter(dxCommon, motion.GetReprojectionTexture(), "receiver-motion-offscreen.png");
    replacement.SetTranslate({0.4f, 0, 0.5f});
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects);
    statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-disocclusion.png");
    Require(statistics.z == 1, "Receiver returning from offscreen reused absent history");
    Skeleton skeleton = Skeleton::CreateSkeleton(data.rootNode); skeleton.UpdateSkeleton(); PlayAnimation animation; animation.SetSkeleton(&skeleton);
    SkinningObject3d skinned; skinned.SetModel(&model); skinned.SetAnimation(&animation); skinned.Initialize(skinManager);
    skinned.SetShadingMode(MaterialShadingMode::Standard); skinned.GetMaterial()->specularStrength = 0; skinned.GetMaterial()->shininess = 0;
    std::vector<Object3d*> skinObjects = {&source}; indirect.ResetHistory();
    for (uint32_t frame = 0; frame < 3; ++frame) {
        DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, skinObjects, 0, &skinned);
        ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-skin-stable.png");
    }
    skeleton.joints[0].transform.translate = {0.4f, 0, 0.5f}; skeleton.UpdateSkeleton();
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, skinObjects, 0, &skinned);
    statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-skin-deformed.png");
    Require(statistics.z >= 4, "GPU skinned receiver did not reproject its previous vertices/depth");
    dxCommon->PreDraw(); metadata = ReadTextureCenter(dxCommon, motion.GetReprojectionTexture(), "receiver-motion-skin-metadata.png");
    RequireColor(metadata, {1, 1, 4}, "Skin metadata used current vertices instead of previous vertices");
    report << "skinnedHistory=" << statistics.z << " previousDepth=" << metadata.z << '\n';
    skeleton.joints[0].transform.rotate = {0, std::sin(0.175f), 0, std::cos(0.175f)}; skeleton.UpdateSkeleton();
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, skinObjects, 0, &skinned);
    statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-skin-rotated.png");
    Require(statistics.z >= 5, "Skinned normal deformation discarded compatible history");
    dxCommon->PreDraw(); metadata = ReadTextureCenter(dxCommon, motion.GetReprojectionTexture(), "receiver-motion-skin-previous-normal.png");
    RequireColor(metadata, {1, 1, 4.5f}, "Skin metadata did not retain the previous deformed normal/depth");
    camera.ResetMotionHistory(); camera.Update();
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, skinObjects, 0, &skinned);
    statistics = ReadTextureCenter(dxCommon, indirect.GetHistoryStatisticsTexture(), "receiver-motion-camera-cut.png");
    Require(statistics.z == 1, "Camera cut retained receiver history");
    DxrShadowRenderer shadows; shadows.Initialize(); auto shadowSettings = shadows.GetSettings();
    shadowSettings.isEnabled = true; shadowSettings.isDenoisingEnabled = true; shadowSettings.sampleCount = 1; shadowSettings.sunAngularRadiusRadians = 0.00465f; shadowSettings.spatialPassCount = 0;
    shadows.SetSettings(shadowSettings); lighting.intensity = 1; lighting.direction = {0, 0, 1}; lights->ApplyLightingPreset(lighting);
    receiver.SetTranslate({0, 0, 0}); receiver.SetRotate({0, 0, 0}); receiver.SetScale({1, 1, 1});
    for (uint32_t frame = 0; frame < 3; ++frame) {
        DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, source, 0, &motion);
        ReadTextureCenter(dxCommon, shadows.GetDenoiseHistoryTexture(), "receiver-motion-shadow-stable.png");
    }
    receiver.SetTranslate({0.4f, 0, 0.5f}); receiver.SetRotate({0, 0.35f, 0});
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, source, 0, &motion);
    Vector3 shadowHistory = ReadTextureCenter(dxCommon, shadows.GetDenoiseHistoryTexture(), "receiver-motion-shadow-moved.png");
    Require(shadowHistory.y >= 4, "Moving/rotated sunlight receiver lost compatible history");
    report << "shadowHistory=" << shadowHistory.y << " metadataBytes=" << motion.GetReprojectionAllocationBytes() << '\n';
    report << "PASS: previous depth/normal, translation over 3 pixels, rotation, nonuniform scale, identity/replacement, disocclusion, actual GPU skinning previous vertices, camera cut, sunlight receiver history\n";
    skinManager->SetBlendMode(kBlendModeNormal); skinManager->SetDefaultCamera(nullptr); objectManager->SetDefaultCamera(nullptr);
}

void RunLocalShadowValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene; scene.Initialize(); if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings; sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    DxrLocalShadowRenderer shadows; shadows.Initialize(); auto settings = shadows.GetSettings();
    settings.emitterRadius = 0; settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0; settings.maxLightCount = 1;
    auto invalid = settings; invalid.maxLightCount = 9; Require(!shadows.SetSettings(invalid), "Unlimited RT local lights accepted");
    invalid = settings; invalid.emitterRadius = std::nanf(""); Require(!shadows.SetSettings(invalid), "Nonfinite emitter accepted");
    Camera camera; camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 0}); camera.Update();
    Object3dManager::GetInstance()->SetDefaultCamera(&camera); Object3dManager::GetInstance()->SetBlendMode(kBlendModeNone);
    Object3dManager::GetInstance()->SetShadowRenderer(nullptr); Object3dManager::GetInstance()->SetLocalShadowRenderer(nullptr);
    auto* lights = LightManager::GetInstance(); lights->ClearDynamicPointLights(); lights->ClearDynamicSpotLights();
    LightingPreset lighting; lighting.intensity = 0; lighting.ambient = {1, 1, 1, 0.2f}; lighting.pointIntensity = 1;
    lighting.pointColor = {1, 0, 0, 1}; lighting.pointPosition = {-2, 0, -3}; lighting.pointDecay = 0;
    lights->ApplyLightingPreset(lighting); lights->SetEnvironmentLighting(0, 0); lights->SetLightingComponents(1, 1, 1, 0);
    lights->SetPointLightShadowEnabled(0, true); lights->SetSpotLightShadowEnabled(0, true);
    auto greenLight = lights->AddPointLight({0, 1, 0, 1}, {2, 0, -3}, 1, 10, 0);
    lights->SetPointLightShadowEnabled(greenLight, false);
    ModelCommon modelCommon; modelCommon.Initialize(dxCommon);
    ModelData data; data.rootNode.localMatrix = MatrixMath::MakeIdentity4x4(); data.rootNode.name = "localShadowRoot";
    data.rootNode.transform = {{1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0}}; data.materials = {{"resources/Textures/white.png"}};
    MeshPrimitive primitive = {}; primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices = {{{-1, -1, 0, 1}, {0, 0}, {0, 0, -1}}, {{-1, 1, 0, 1}, {0, 1}, {0, 0, -1}},
        {{1, -1, 0, 1}, {1, 0}, {0, 0, -1}}, {{1, 1, 0, 1}, {1, 1}, {0, 0, -1}}};
    primitive.indices = {0, 1, 2, 2, 1, 3}; data.primitives = {primitive};
    JointWeightData weights; weights.inverseBindPoseMatrix = MatrixMath::MakeIdentity4x4();
    for (uint32_t index = 0; index < 4; ++index) { weights.vertexWeights.push_back({1, index}); }
    data.skinClusterData["localShadowRoot"] = weights;
    Model model; model.Initialize(&modelCommon, data);
    Object3d receiver; receiver.Initialize(Object3dManager::GetInstance()); receiver.SetModel(&model);
    receiver.SetScale({4, 4, 1}); receiver.SetShadingMode(MaterialShadingMode::Standard); receiver.SetReceiveShadow(true);
    receiver.GetMaterial()->shininess = 0; receiver.GetMaterial()->specularStrength = 0;
    Object3d caster; caster.Initialize(Object3dManager::GetInstance()); caster.SetModel(&model);
    caster.SetScale({0.4f, 0.6f, 1}); caster.SetTranslate({-1, 0, -1.5f}); caster.SetColor({0, 0, 0, 1}); caster.SetCastShadow(true);
    std::vector<Object3d*> objects = {&receiver, &caster}; TextureManager::GetInstance()->FlushUploads();
    OffscreenRenderer offscreen; offscreen.Initialize(); PostEffectManager postEffects; postEffects.Initialize(dxCommon);
    MotionVectorRenderer motion; motion.Initialize(); shadows.SetSettings(settings);
    auto* source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    Vector3 baseline = ReadTextureCenter(dxCommon, source, "local-shadow-off.png");
    Require(baseline.x > 1 && baseline.y > 1 && baseline.z > 0.15f, "Local OFF baseline is not lit");
    settings.isEnabled = true; shadows.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    Vector3 shadowed = ReadTextureCenter(dxCommon, source, "local-shadow-point.png");
    dxCommon->PreDraw(); Vector3 capture = ReadTextureCenter(dxCommon, offscreen.GetLocalLightTexture(), "local-shadow-captured-light.png");
    Require(capture.x > 0.8f && capture.y < 0.01f, "Selected local lighting capture contains another light");
    dxCommon->PreDraw(); ReadTextureCenter(dxCommon, shadows.GetRawTexture(), "local-shadow-point-raw.png");
    RequireColor(shadowed, baseline - capture, "RT local shadow changed ambient/unselected light or failed to block");
    std::vector<Object3d*> localRasterObjects = {&receiver};
    RaytracingTestSceneInputs localSceneInputs; localSceneInputs.raytracingObjects = &objects;
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, localRasterObjects, 0, nullptr, &shadows, true, &localSceneInputs);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-undrawn-caster.png"), baseline - capture, "Undrawn local caster stopped blocking point light");
    scene.ReadCompleted(); shadows.ReadCompleted(); std::ofstream report("runtime/captures/DxrTests/local-shadow-result.txt");
    report.setf(std::ios::unitbuf);
    report << "traceMs=" << shadows.GetTraceGpuTimeMs() << " compositeMs=" << shadows.GetCompositeGpuTimeMs()
        << " targetsBytes=" << shadows.GetAllocationBytes() << " captureBytes=" << offscreen.GetLocalLightAllocationBytes() << '\n';
    LocalShadowRenderer maps; Require(maps.Initialize(dxCommon), "Local shadow map fixture failed"); maps.Prepare(*lights);
    for (uint32_t index = 0; index < maps.GetPassCount(); ++index) {
        auto& pass = maps.GetPass(index); pass.BeginShadowPass(); caster.DrawShadow(pass); pass.EndShadowPass();
    }
    maps.Finish(); Object3dManager::GetInstance()->SetLocalShadowRenderer(&maps); caster.SetCastShadow(false);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-map-replaced.png"), baseline, "RT local lighting retained/doubled mapped shadow");
    dxCommon->PreDraw(); Require(ReadTextureCenter(dxCommon, offscreen.GetLocalLightTexture(), "local-shadow-map-captured.png").x < 0.05f,
        "Shadow map replacement fixture has no mapped shadow");
    Object3dManager::GetInstance()->SetLocalShadowRenderer(nullptr);
    caster.SetCastShadow(false);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-cast-off.png"), baseline, "CastShadow OFF still blocked local light");
    caster.SetCastShadow(true); receiver.SetReceiveShadow(false);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-receive-off.png"), baseline, "ReceiveShadow OFF changed local lighting");
    receiver.SetReceiveShadow(true); caster.SetAlphaCutoff(0.5f); caster.SetColor({0, 0, 0, 0.25f});
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-alpha-hole.png"), baseline, "Local shadow alpha AnyHit failed");
    caster.SetAlphaCutoff(0); caster.SetColor({0, 0, 0, 1}); caster.SetTranslate({-3.333f, 0, -5});
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-behind-light.png"), baseline, "Caster behind point light blocked finite segment");
    caster.SetTranslate({-1, 0, -1.5f}); lights->SetPointLightShadowEnabled(0, false);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows, false);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-no-eligible-light.png"), baseline, "No eligible local lights changed the scene");
    lights->SetPointLightShadowEnabled(0, true); lights->SetPointIntensity(0); lights->RemovePointLight(greenLight);
    lights->SetSpotLightColor({0, 0, 1, 1}); lights->SetSpotLightPosition({-2, 0, -3}); lights->SetSpotLightDirection({2, 0, 3});
    lights->SetSpotLightIntensity(1); lights->SetSpotLightDistance(10); lights->SetSpotLightDecay(0);
    lights->SetSpotLightCosAngle(0.5f); lights->SetSpotLightCosFalloffStart(0.9f);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-spot.png"), {0.2f, 0.2f, 0.2f}, "Spot shadow changed ambient or did not block");
    caster.SetCastShadow(false);
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    Require(ReadTextureCenter(dxCommon, shadows.GetRawTexture(), "local-shadow-spot-lit.png").z > 0.8f, "Spotlight attenuation/cone ignored");
    lights->SetSpotLightDirection({-2, 0, -3});
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetRawTexture(), "local-shadow-outside-cone.png"), {0, 0, 0}, "Outside spotlight cone remained lit");
    lights->SetSpotLightDirection({2, 0, 3}); lights->SetPointIntensity(1); caster.SetCastShadow(true);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    Require(shadows.GetSelectedLightCount() == 1, "Local shadow light cap exceeded");
    Vector3 limited = ReadTextureCenter(dxCommon, source, "local-shadow-light-cap.png");
    Require(limited.x < 0.22f && limited.z > 1, "Light cap shadowed unselected spotlight or skipped selected point light");
    settings.maxLightCount = 2; shadows.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-two-lights.png"), {0.2f, 0.2f, 0.2f}, "Two selected lights were not independently shadowed");
    lights->SetSpotLightIntensity(0); settings.maxLightCount = 1; settings.emitterRadius = 0.8f;
    settings.sampleCount = 16; settings.shouldUseTemporalHistory = true; settings.spatialPassCount = 0; shadows.SetSettings(settings);
    caster.SetScale({0.2f, 0.6f, 1}); std::vector<float> reference;
    for (uint32_t index = 0; index < 8; ++index) {
        shadows.ResetHistory(); DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
        std::vector<float> values; ReadTextureCenter(dxCommon, shadows.GetRawTexture(), "local-shadow-reference-sample.png", nullptr, &values);
        if (reference.empty()) { reference.resize(values.size()); }
        for (size_t pixelIndex = 0; pixelIndex < values.size(); ++pixelIndex) { reference[pixelIndex] += values[pixelIndex] / 8; }
    }
    settings.sampleCount = 1; settings.shouldUseTemporalHistory = false; shadows.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    std::vector<float> raw; ReadTextureCenter(dxCommon, shadows.GetRawTexture(), "local-shadow-noise-raw1.png", nullptr, &raw);
    settings.shouldUseTemporalHistory = true; settings.spatialPassCount = 2; shadows.SetSettings(settings);
    for (uint32_t index = 0; index < 32; ++index) {
        DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows); dxCommon->PostDraw();
    }
    Require(shadows.HasUsedHistory(), "Stable local shadows did not use history");
    dxCommon->PreDraw(); std::vector<float> filtered;
    ReadTextureCenter(dxCommon, shadows.GetFilteredTexture(), "local-shadow-noise-filtered.png", nullptr, &filtered);
    scene.ReadCompleted(); shadows.ReadCompleted();
    double rawError = ReflectionMeanSquareError(raw, reference); double filteredError = ReflectionMeanSquareError(filtered, reference);
    report << "mseRaw=" << rawError << " mseFiltered=" << filteredError << " softTraceMs=" << shadows.GetTraceGpuTimeMs()
        << " filterMs=" << shadows.GetFilterGpuTimeMs() << '\n';
    Require(filteredError < rawError * 0.5, "Local shadow denoising did not reduce error");
    settings.emitterRadius = 0; settings.spatialPassCount = 0; shadows.SetSettings(settings);
    caster.SetScale({0.4f, 0.6f, 1}); caster.SetTranslate({-1, 0, -1.5f});
    for (uint32_t frameIndex = 0; frameIndex < 4; ++frameIndex) {
        DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows); dxCommon->PostDraw();
    }
    caster.SetTranslate({100, 100, 100});
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    Vector3 localStatistics = ReadTextureCenter(dxCommon, shadows.GetHistoryStatisticsTexture(), "local-shadow-history-caster-count.png");
    Require(localStatistics.z == 1, "Moving local caster left accumulated occlusion at a changed pixel");
    dxCommon->PreDraw();
    Vector3 freshLocalLighting = ReadTextureCenter(dxCommon, shadows.GetRawTexture(), "local-shadow-history-caster-raw.png");
    dxCommon->PreDraw();
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetFilteredTexture(), "local-shadow-history-caster-filtered.png"), freshLocalLighting, "Moving local caster left a shadow trail");
    caster.SetTranslate({101, 100, 100});
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    localStatistics = ReadTextureCenter(dxCommon, shadows.GetHistoryStatisticsTexture(), "local-shadow-history-unrelated-caster.png");
    Require(localStatistics.z >= 2, "Unrelated local caster discarded stable pixel history");
    lights->SetPointIntensity(2);
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    localStatistics = ReadTextureCenter(dxCommon, shadows.GetHistoryStatisticsTexture(), "local-shadow-history-light-count.png");
    Require(localStatistics.z == 1, "Changed local light retained its old accumulation count");
    dxCommon->PreDraw();
    freshLocalLighting = ReadTextureCenter(dxCommon, shadows.GetRawTexture(), "local-shadow-history-light-raw.png");
    dxCommon->PreDraw();
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetFilteredTexture(), "local-shadow-history-light-filtered.png"), freshLocalLighting, "Changed local light retained old brightness");
    lights->SetPointIntensity(1); caster.SetTranslate({-1, 0, -1.5f}); caster.SetScale({0.2f, 0.6f, 1});
    settings.emitterRadius = 0.8f; settings.spatialPassCount = 2; shadows.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows); dxCommon->PostDraw();
    camera.SetTranslate({0.01f, 0, -4}); camera.Update(); camera.SetProjectionJitter({0.001f, -0.001f});
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows); dxCommon->PostDraw();
    Require(shadows.HasUsedHistory(), "Local shadow camera/jitter lost global history");
    caster.SetTranslate({-1.1f, 0, -1.5f});
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows); dxCommon->PostDraw();
    Require(shadows.HasUsedHistory(), "Moving local caster discarded unaffected history globally");
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows); dxCommon->PostDraw();
    lights->SetPointPosition({-2.1f, 0, -3});
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows); dxCommon->PostDraw();
    Require(shadows.HasUsedHistory(), "Moving local light discarded unaffected history globally");
    camera.ResetMotionHistory();
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows); dxCommon->PostDraw();
    Require(!shadows.HasUsedHistory(), "Local shadow camera cut retained history");
    camera.SetTranslate({0, 0, -4}); camera.Update(); camera.SetProjectionJitter({}); lights->SetPointPosition({-2, 0, -3});
    settings.emitterRadius = 0; settings.shouldUseTemporalHistory = false; settings.spatialPassCount = 0; shadows.SetSettings(settings);
    auto* skinManager = SkinningObject3dManager::GetInstance(); skinManager->SetDefaultCamera(&camera); skinManager->SetBlendMode(kBlendModeNone);
    skinManager->SetEnvironmentTexture(Object3dManager::GetInstance()->GetEnvironmentTexture());
    Skeleton skeleton = Skeleton::CreateSkeleton(data.rootNode); skeleton.UpdateSkeleton(); PlayAnimation animation; animation.SetSkeleton(&skeleton);
    SkinningObject3d skinned; skinned.SetModel(&model); skinned.SetAnimation(&animation); skinned.Initialize(skinManager);
    skinned.SetTranslate({-1, 0, -1.5f}); skinned.SetScale({0.4f, 0.6f, 1}); skinned.SetColor({0, 0, 0, 1}); skinned.SetCastShadow(true);
    const std::vector<Object3d*> kSkinObjects = {&receiver};
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, kSkinObjects, 0, &skinned, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetRawTexture(), "local-shadow-skinned-caster.png"), {0, 0, 0}, "Skinned local caster failed");
    skeleton.joints[0].transform.translate.y = 4; skeleton.UpdateSkeleton();
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, kSkinObjects, 0, &skinned, &shadows);
    Require(scene.GetStatistics().updatedBlasCount == 1, "Local shadow skinning did not refit");
    Require(ReadTextureCenter(dxCommon, shadows.GetRawTexture(), "local-shadow-skinned-moved.png").x > 0.8f, "Skinned local shadow retained old pose");
    skeleton.joints[0].transform.translate.y = 0; skeleton.UpdateSkeleton();
    skinned.SetTranslate({0, 0, 0}); skinned.SetScale({4, 4, 1}); skinned.SetColor({1, 1, 1, 1}); skinned.SetReceiveShadow(true);
    skinned.SetShadingMode(MaterialShadingMode::Standard); skinned.GetMaterial()->shininess = 0; skinned.GetMaterial()->specularStrength = 0;
    caster.SetTranslate({-1, 0, -1.5f}); caster.SetScale({0.4f, 0.6f, 1}); const std::vector<Object3d*> kCasterOnly = {&caster};
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, kCasterOnly, 0, &skinned, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-skinned-receiver.png"), {0.2f, 0.2f, 0.2f}, "Skinned receiver did not replace selected lighting");
    skinned.SetReceiveShadow(false);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, kCasterOnly, 0, &skinned, &shadows);
    Require(ReadTextureCenter(dxCommon, source, "local-shadow-skinned-receive-off.png").x > 1, "Skinned ReceiveShadow OFF ignored");
    Object3d foreground; foreground.Initialize(Object3dManager::GetInstance()); foreground.SetModel(&model);
    foreground.SetTranslate({0, 0, -0.1f}); foreground.SetColor({0, 0, 1, 1}); objects.push_back(&foreground);
    source = DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, source, "local-shadow-unsupported-foreground.png"), {0, 0, 1}, "Local shadows modified unsupported foreground");
    // A reflected surface receives a point light; a blocker is outside the primary mirror ray.
    auto diagonalData = data; for (auto& vertex : diagonalData.primitives[0].vertices) { vertex.normal = {0.70710678f, 0, -0.70710678f}; }
    Model diagonalModel; diagonalModel.Initialize(&modelCommon, diagonalData);
    receiver.SetModel(&diagonalModel); receiver.SetScale({1, 1, 1}); receiver.GetMaterial()->metallic = 1; receiver.GetMaterial()->specularStrength = 1; receiver.GetMaterial()->roughness = 0;
    Object3d reflectedTarget; reflectedTarget.Initialize(Object3dManager::GetInstance()); reflectedTarget.SetModel(&model);
    reflectedTarget.SetTranslate({4, 0, 0}); reflectedTarget.SetRotate({0, 1.57079633f, 0}); reflectedTarget.SetShadingMode(MaterialShadingMode::Standard);
    reflectedTarget.GetMaterial()->shininess = 0; reflectedTarget.GetMaterial()->specularStrength = 0; reflectedTarget.SetReceiveShadow(true);
    caster.SetRotate({0, 1.57079633f, 0}); caster.SetTranslate({3, 1, 0}); caster.SetScale({0.4f, 0.4f, 1});
    lighting.ambient.w = 0; lights->ApplyLightingPreset(lighting); lights->SetPointPosition({2, 2, 0}); lights->SetPointLightShadowEnabled(0, true);
    objects = {&receiver, &reflectedTarget, &caster}; DxrReflectionRenderer reflections; reflections.Initialize();
    auto reflectionSettings = reflections.GetSettings(); reflectionSettings.isEnabled = true; reflectionSettings.shouldUseTemporalHistory = false;
    reflectionSettings.spatialPassCount = 0; reflectionSettings.shouldTraceSunShadows = false; reflections.SetSettings(reflectionSettings);
    settings.isEnabled = false; shadows.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    Vector3 litReflection = ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "local-shadow-reflection-off.png");
    Require(litReflection.x > 0.6f, "Reflected local light setup is not lit");
    settings.isEnabled = true; shadows.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "local-shadow-reflection-on.png"), {0, 0, 0}, "Reflection hit local shadow did not block");
    reflectedTarget.SetReceiveShadow(false);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "local-shadow-reflected-receive-off.png"), litReflection, "Reflected ReceiveShadow OFF ignored");
    reflectedTarget.SetReceiveShadow(true); reflectedTarget.SetScale({4, 4, 1});
    caster.SetTranslate({3, 5, 0}); caster.SetScale({3, 3, 1}); lights->SetPointPosition({2, 10, 0}); lights->SetPointRadius(30);
    DxrGlobalIlluminationRenderer indirect; indirect.Initialize(); auto giSettings = indirect.GetSettings();
    giSettings.isEnabled = true; giSettings.sampleCount = 16; giSettings.maxDistance = 100; giSettings.shouldUseTemporalHistory = false;
    giSettings.spatialPassCount = 0; giSettings.shouldTraceSunShadows = false; indirect.SetSettings(giSettings);
    settings.isEnabled = false; shadows.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    Vector3 giLit = ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "local-shadow-rtgi-off.png");
    settings.isEnabled = true; shadows.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows);
    Vector3 giShadowed = ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "local-shadow-rtgi-on.png");
    Require(giLit.x > 0.001f && giShadowed.x < giLit.x * 0.8f, "RTGI local shadow did not reduce bounced radiance");
    DxrLocalShadowInputs missing; missing.colorSrv = offscreen.GetSrvHandleGPU();
    Require(shadows.Draw(missing).ptr == missing.colorSrv.ptr && !shadows.HasValidFrame(), "Missing local inputs reused stale output");
    settings.isEnabled = false; shadows.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, shadows, offscreen, postEffects, motion, camera, objects, 0, nullptr, &shadows); dxCommon->PostDraw();
    Require(shadows.GetFilteredTexture() == nullptr && shadows.GetTraceGpuTimeMs() == 0, "Local shadows OFF retained output/timing");
    report << "PASS: point/spot, colored light isolation, actual shadow-map replacement, finite segments, caster/receiver flags, alpha, cone/range, cap/two lights, soft shadows/denoising, history/jitter/cut/geometry/light, GPU skinning caster/refit/receiver/receive flag, foreground rejection, reflected local lighting/receive flag, RTGI local lighting, missing inputs, OFF\n";
    skinManager->SetDefaultCamera(nullptr); skinManager->SetBlendMode(kBlendModeNormal); Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}
void RunSkinningValidation(DirectXCommon* dxCommon) {
    DxrRenderer scene;
    scene.Initialize();
    if (!scene.IsSupported()) { return; }
    DxrSettings sceneSettings;
    sceneSettings.isEnabled = true; scene.SetSettings(sceneSettings);
    DxrShadowRenderer shadows;
    shadows.Initialize();
    DxrShadowSettings settings;
    settings.isEnabled = true; settings.sunAngularRadiusRadians = 0; settings.isDenoisingEnabled = false;
    shadows.SetSettings(settings);
    Camera camera;
    camera.Initialize(); camera.SetFovY(0.7f); camera.LookAt({0, 0, -4}, {0, 0, 5}); camera.Update();
    Object3dManager::GetInstance()->SetDefaultCamera(&camera);
    auto* skinManager = SkinningObject3dManager::GetInstance();
    skinManager->Initialize(dxCommon); skinManager->SetDefaultCamera(&camera);
    skinManager->SetBlendMode(kBlendModeNone);
    skinManager->SetEnvironmentTexture(Object3dManager::GetInstance()->GetEnvironmentTexture());
    LightingPreset lighting;
    lighting.direction = {1, 0, 1}; lighting.intensity = 1; lighting.ambient = {1, 1, 1, 0.2f};
    LightManager::GetInstance()->ApplyLightingPreset(lighting);
    LightManager::GetInstance()->SetEnvironmentLighting(0, 0);
    ModelCommon modelCommon;
    modelCommon.Initialize(dxCommon);
    ModelData modelData;
    modelData.rootNode.name = "root";
    modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    modelData.rootNode.transform.scale = {1, 1, 1};
    modelData.rootNode.transform.rotate = {0, 0, 0, 1};
    modelData.rootNode.transform.translate = {0, 0, 0};
    modelData.materials = {{"resources/Textures/white.png"}, {"dxr-test-green"}};
    MeshPrimitive primitive = {};
    primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices = {{{-1, -1, 0, 1}, {0, 0}, {0, 0, -1}}, {{-1, 1, 0, 1}, {0, 1}, {0, 0, -1}},
        {{1, -1, 0, 1}, {1, 0}, {0, 0, -1}}, {{1, 1, 0, 1}, {1, 1}, {0, 0, -1}}};
    primitive.indices = {0, 1, 2, 2, 1, 3};
    MeshPrimitive upperPrimitive = primitive;
    upperPrimitive.materialIndex = 1;
    for (VertexData& vertex : upperPrimitive.vertices) { vertex.position.y += 5; }
    modelData.primitives = {primitive, upperPrimitive};
    JointWeightData weights;
    weights.inverseBindPoseMatrix = MatrixMath::MakeIdentity4x4();
    for (uint32_t index = 0; index < 8; ++index) { weights.vertexWeights.push_back({1, index}); }
    modelData.skinClusterData["root"] = weights;
    Model model;
    model.Initialize(&modelCommon, modelData);
    Skeleton firstSkeleton = Skeleton::CreateSkeleton(modelData.rootNode);
    Skeleton secondSkeleton = Skeleton::CreateSkeleton(modelData.rootNode);
    firstSkeleton.UpdateSkeleton(); secondSkeleton.UpdateSkeleton();
    PlayAnimation firstAnimation;
    firstAnimation.SetSkeleton(&firstSkeleton);
    PlayAnimation secondAnimation;
    secondAnimation.SetSkeleton(&secondSkeleton);
    SkinningObject3d first;
    first.SetModel(&model); first.SetAnimation(&firstAnimation); first.Initialize(skinManager);
    first.SetScale({0.6f, 0.6f, 1}); first.SetTranslate({-2, 0, 3});
    first.SetShadingMode(MaterialShadingMode::Standard); first.SetCastShadow(true); first.GetMaterial()->shininess = 0;
    Require(first.SetAlphaCutoff(0.5f), "Skinned alpha cutoff rejected");
    SkinningObject3d second;
    second.SetModel(&model); second.SetAnimation(&secondAnimation); second.Initialize(skinManager);
    second.SetTranslate({30, 0, 3}); second.SetShadingMode(MaterialShadingMode::Standard); second.SetCastShadow(true);
    std::vector<SkinningObject3d*> objects = {&first, &second};
    Object3d receiver;
    receiver.Initialize(Object3dManager::GetInstance()); receiver.SetModel(&model);
    receiver.SetScale({4, 4, 1}); receiver.SetTranslate({0, 0, 5});
    receiver.SetShadingMode(MaterialShadingMode::Standard); receiver.SetReceiveShadow(true); receiver.GetMaterial()->shininess = 0;
    Object3d occluder;
    occluder.Initialize(Object3dManager::GetInstance()); occluder.SetModel(&model);
    occluder.SetScale({0.6f, 0.6f, 1}); occluder.SetTranslate({30, 0, 3});
    occluder.SetShadingMode(MaterialShadingMode::Standard); occluder.GetMaterial()->shininess = 0;
    TextureManager::GetInstance()->FlushUploads();
    OffscreenRenderer offscreen;
    offscreen.Initialize();
    PostEffectManager postEffects;
    postEffects.Initialize(dxCommon);
    MotionVectorRenderer motionVectors;
    motionVectors.Initialize();
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    Require(scene.GetStatistics().dynamicBlasCount == 2 && scene.GetStatistics().builtBlasCount == 3, "GPU-skinned meshes were not registered independently");
    uint64_t dynamicBufferBytes = scene.GetStatistics().dynamicBufferBytes;
    uint64_t shadowRevision = scene.GetShadowSceneRevision();
    Vector3 initialMask = ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "skinned-caster-initial.png");
    dxCommon->PreDraw();
    ReadTextureCenter(dxCommon, offscreen.GetColorTexture(), "skinned-initial-scene.png");
    dxCommon->PreDraw();
    ReadTextureCenter(dxCommon, offscreen.GetDirectionalLightTexture(), "skinned-initial-direct.png");
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> validationQueue;
    dxCommon->GetDevice()->QueryInterface(IID_PPV_ARGS(&validationQueue));
    CheckValidationMessages(validationQueue.Get());
    RequireColor(initialMask, {0, 0, 0}, "GPU-skinned caster did not shadow receiver");
    scene.ReadCompleted(); shadows.ReadCompleted();
    std::ofstream report("runtime/captures/DxrTests/skinning-result.txt");
    report << "initialBuildMs=" << scene.GetBuildGpuTimeMs() << " dynamicBufferBytes=" << dynamicBufferBytes
        << " dynamicAllocationBytes=" << scene.GetStatistics().dynamicAllocationBytes << '\n';
    first.GetMaterial()->color.w = 0.25f;
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "skinned-alpha-hole.png"), {1, 1, 1}, "Skinned AnyHit ignored material alpha or vertex synchronization");
    first.GetMaterial()->color.w = 1;
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "skinned-alpha-opaque.png"), {0, 0, 0}, "Skinned alpha mask failed to restore opaque shadow");
    firstSkeleton.joints[0].transform.translate.x = 8; firstSkeleton.UpdateSkeleton();
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    Require(scene.GetStatistics().updatedBlasCount == 1 && scene.GetStatistics().builtBlasCount == 0
        && scene.GetStatistics().dynamicBufferBytes == dynamicBufferBytes, "Skinned pose refit rebuilt buffers or updated unchanged pose");
    Require(scene.GetShadowSceneRevision() != shadowRevision, "GPU pose change retained stale shadow revision");
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "skinned-caster-moved.png"), {1, 1, 1}, "Animated caster left stale shadow bounds");
    scene.ReadCompleted(); shadows.ReadCompleted();
    report << "refitAndTlasMs=" << scene.GetBuildGpuTimeMs() << '\n';
    shadowRevision = scene.GetShadowSceneRevision();
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    Require(scene.GetStatistics().updatedBlasCount == 0 && scene.GetShadowSceneRevision() == shadowRevision,
        "Repeated unchanged skinning dispatch invalidated history or refitted BLAS");
    dxCommon->PostDraw(); scene.ReadCompleted(); shadows.ReadCompleted();
    firstSkeleton.joints[0].transform.translate.x = 0; firstSkeleton.UpdateSkeleton();
    first.SetCastShadow(false);
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    RequireColor(ReadTextureCenter(dxCommon, shadows.GetMaskTexture(), "skinned-cast-off.png"), {1, 1, 1}, "Skinned CastShadow OFF still occluded");
    // The skinned mesh becomes the receiver; a rigid mesh casts onto its current GPU pose.
    receiver.SetTranslate({30, 0, 5});
    first.SetScale({4, 4, 1}); first.SetTranslate({0, 0, 5}); first.SetReceiveShadow(true);
    occluder.SetTranslate({-2, 0, 3}); occluder.SetCastShadow(true);
    settings.isEnabled = false; shadows.SetSettings(settings);
    auto* source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    Vector3 baseline = ReadTextureCenter(dxCommon, source, "skinned-receiver-off.png");
    dxCommon->PreDraw();
    Vector3 sun = ReadTextureCenter(dxCommon, offscreen.GetDirectionalLightTexture(), "skinned-receiver-sun.png");
    Require(sun.x > 0.4f, "Skinned receiver did not capture sunlight");
    settings.isEnabled = true; shadows.SetSettings(settings);
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "skinned-receiver-on.png"), baseline - sun, "Skinned receiver composition changed indirect light");
    first.SetShadingMode(MaterialShadingMode::Toon);
    settings.isEnabled = false; shadows.SetSettings(settings);
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    baseline = ReadTextureCenter(dxCommon, source, "skinned-toon-off.png");
    dxCommon->PreDraw(); sun = ReadTextureCenter(dxCommon, offscreen.GetDirectionalLightTexture(), "skinned-toon-sun.png");
    settings.isEnabled = true; shadows.SetSettings(settings);
    source = DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "skinned-toon-on.png"), baseline - sun, "Skinned Toon sunlight composition failed");
    settings.sunAngularRadiusRadians = 0.1f; settings.sampleCount = 1; settings.isDenoisingEnabled = true;
    shadows.SetSettings(settings);
    for (uint32_t index = 0; index < 3; ++index) {
        DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
        dxCommon->PostDraw(); scene.ReadCompleted(); shadows.ReadCompleted();
    }
    Require(shadows.HasUsedShadowHistory(), "Stable skinned receiver did not reuse shadow history");
    firstSkeleton.joints[0].transform.translate.x = 0.01f; firstSkeleton.UpdateSkeleton();
    first.SetCastShadow(true);
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    dxCommon->PostDraw(); scene.ReadCompleted(); shadows.ReadCompleted();
    Require(shadows.HasUsedShadowHistory(), "Animated skinned caster discarded unaffected history globally");
    skinManager->SetBlendMode(kBlendModeNormal);
    DrawShadowFrame(dxCommon, scene, shadows, offscreen, postEffects, camera, receiver, occluder, 0, &motionVectors, &objects);
    Require(scene.GetStatistics().dynamicBlasCount == 0, "Blended skinning entered opaque RT path");
    dxCommon->PostDraw();
    report << "PASS: actual GPU skinning, multi-primitive poses, alpha AnyHit/material alpha, caster/refit, frozen reuse, CastShadow OFF, Standard/Toon receiver, sunlight-only composition, motion-copy barriers, denoising history, blended exclusion\n";
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr); skinManager->SetDefaultCamera(nullptr);
}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int) {
    int exitCode = 0;
    Logger::Initialize();
    try {
        std::filesystem::create_directories("runtime/captures/DxrTests");
        std::filesystem::remove("runtime/captures/DxrTests/failure.txt");
        std::filesystem::remove("runtime/captures/DxrTests/native-failure.txt");
        RunSceneBoundsValidation();
        Microsoft::WRL::ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) { debug->EnableDebugLayer(); }
        WinApp::GetInstance()->initialize();
        ShowWindow(WinApp::GetInstance()->GetHwnd(), SW_HIDE);
        auto* dxCommon = DirectXCommon::GetInstance();
        dxCommon->Initialize(WinApp::GetInstance());
        SrvManager::GetInstance()->Initialize(dxCommon);
        TextureManager::GetInstance()->Initialize(dxCommon, SrvManager::GetInstance());
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
        dxCommon->GetDevice()->QueryInterface(IID_PPV_ARGS(&infoQueue));
        if (infoQueue) {
            infoQueue->ClearStoredMessages();
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
        }
        if (std::strcmp(commandLine, "--taa") == 0) {
            RunTemporalResolutionValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--hdr") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunHdrValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--sky-lighting") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunMaterialValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--volumetric") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunVolumetricQualityValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--multiple-reflections") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunMultipleReflectionValidation(dxCommon); RunReflectionValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--diffuse-quality") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunDiffuseQualityValidation(dxCommon); RunGlobalIlluminationValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--texture-mips") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunTextureMipValidation(dxCommon); RunMaterialValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--rough-reflections") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunRoughReflectionValidation(dxCommon); RunReflectionValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--receiver-motion") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunReceiverMotionValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--lighting") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunGlobalIlluminationValidation(dxCommon); RunMaterialValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--materials") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunMaterialValidation(dxCommon);
        } else if (std::strcmp(commandLine, "--local-shadows") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunLocalShadowValidation(dxCommon);
        } else {
            RunValidation(dxCommon); RunDeformedValidation(dxCommon); RunShadowValidation(dxCommon); RunSkinningValidation(dxCommon);
            RunAlphaValidation(dxCommon); RunReflectionValidation(dxCommon); RunGlobalIlluminationValidation(dxCommon); RunLocalShadowValidation(dxCommon);
            RunMaterialValidation(dxCommon); RunReceiverMotionValidation(dxCommon); RunRoughReflectionValidation(dxCommon); RunTextureMipValidation(dxCommon); RunDiffuseQualityValidation(dxCommon); RunMultipleReflectionValidation(dxCommon); RunVolumetricQualityValidation(dxCommon); RunHdrValidation(dxCommon); RunTemporalResolutionValidation(dxCommon);
        }
        CheckValidationMessages(infoQueue.Get());
    } catch (const std::exception& error) {
        Logger::Error(error.what());
        std::ofstream report("runtime/captures/DxrTests/failure.txt");
        report << error.what();
        exitCode = 1;
    }
    SkyBoxManager::Finalize();
    Object3dManager::Finalize();
    SkinningObject3dManager::Finalize();
    LightManager::Finalize();
    TextureManager::GetInstance()->Finalize();
    SrvManager::GetInstance()->Finalize();
    DirectXCommon::Finalize();
    WinApp::GetInstance()->Finalize();
    Logger::Finalize();
    return exitCode;
}
