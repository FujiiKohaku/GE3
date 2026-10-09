#include "Engine/Raytracing/DxrRenderer.h"
#include "Engine/Raytracing/DxrShadowRenderer.h"
#include "Engine/Raytracing/DxrReflectionRenderer.h"
#include "Engine/Raytracing/DxrGlobalIlluminationRenderer.h"
#include "Engine/Raytracing/DxrLocalShadowRenderer.h"
#include "Engine/Lighting/ScreenSpaceGlobalIllumination.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkinningObject3d.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/Shadow/ShadowMapRenderer.h"
#include "Engine/Shadow/LocalShadowRenderer.h"
#include "Engine/Light/LightManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
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
Vector3 ReadTextureCenter(DirectXCommon* dxCommon, ID3D12Resource* source, const char* captureName,
    size_t* fractionalPixels = nullptr, std::vector<float>* values = nullptr) {
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
    auto before = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
    dxCommon->GetCommandList()->ResourceBarrier(1, &before);
    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource = readback.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = layout;
    D3D12_TEXTURE_COPY_LOCATION origin = {};
    origin.pResource = source;
    origin.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dxCommon->GetCommandList()->CopyTextureRegion(&destination, 0, 0, 0, &origin, nullptr);
    auto after = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
    if (description.Format == DXGI_FORMAT_R32G32B32A32_FLOAT) { pixelStrideBytes = 16; }
    auto* centerBytes = pixels + layout.Footprint.RowPitch * (description.Height / 2) + (description.Width / 2) * pixelStrideBytes;
    Vector3 result = {};
    if (description.Format == DXGI_FORMAT_R16G16_FLOAT) {
        auto* center = reinterpret_cast<const DirectX::PackedVector::HALF*>(centerBytes);
        result = {DirectX::PackedVector::XMConvertHalfToFloat(center[0]), DirectX::PackedVector::XMConvertHalfToFloat(center[1]), 0};
    } else if (description.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
        auto* center = reinterpret_cast<const DirectX::PackedVector::HALF*>(centerBytes);
        result = {DirectX::PackedVector::XMConvertHalfToFloat(center[0]),
            DirectX::PackedVector::XMConvertHalfToFloat(center[1]), DirectX::PackedVector::XMConvertHalfToFloat(center[2])};
    } else {
        auto* center = reinterpret_cast<const float*>(centerBytes);
        result = {center[0], center[0], center[0]};
        if (description.Format == DXGI_FORMAT_R32G32B32A32_FLOAT) { result = {center[0], center[1], center[2]}; }
    }
    if (values != nullptr) {
        values->resize(static_cast<size_t>(description.Width) * description.Height);
        for (uint32_t row = 0; row < description.Height; ++row) {
            for (uint32_t column = 0; column < description.Width; ++column) {
                auto* valueBytes = pixels + row * layout.Footprint.RowPitch + column * pixelStrideBytes;
                float value = 0;
                if (description.Format == DXGI_FORMAT_R16G16_FLOAT || description.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
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
    Require(SUCCEEDED(DirectX::Convert(image, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0, converted)), "Capture conversion failed");
    std::filesystem::path capturePath = std::filesystem::path("runtime/captures/DxrTests") / captureName;
    Require(SUCCEEDED(DirectX::SaveToWICFile(*converted.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
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
    Object3d* additionalOccluder = nullptr) {
    scene.BeginFrame();
    postEffects.PreDrawDepth();
    offscreen.PreDraw(postEffects.GetDepthDSVHandle(), true);
    scene.SetDirectionalShadowCapture(offscreen.IsDirectionalCaptureActive());
    if (motionVectors != nullptr) { motionVectors->BeginFrame(); }
    SrvManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->UpdateClusters(&camera);
    Object3dManager::GetInstance()->PreDraw();
    receiver.Update(); receiver.Draw(); occluder.Update(); occluder.Draw();
    if (skinnedObjects != nullptr) {
        for (SkinningObject3d* object : *skinnedObjects) {
            if (shouldUpdateSkin) { object->Update(); }
            SkinningObject3dManager::GetInstance()->PreDraw();
            object->Draw();
        }
    }
    if (additionalOccluder != nullptr) { Object3dManager::GetInstance()->PreDraw(); additionalOccluder->Update(); additionalOccluder->Draw(); }
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
    if (motionVectors != nullptr) { inputs.motionVectorSrv = motionVectors->GetSrvHandle(); }
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
template<class LightingPass>
ID3D12Resource* DrawReflectionFrame(DirectXCommon* dxCommon, DxrRenderer& scene, LightingPass& reflections,
    OffscreenRenderer& offscreen, PostEffectManager& postEffects, MotionVectorRenderer& motionVectors, Camera& camera,
    const std::vector<Object3d*>& objects, uint64_t sceneRevision = 0, SkinningObject3d* skinnedTarget = nullptr, DxrLocalShadowRenderer* localShadows = nullptr, bool shouldExpectValidFrame = true) {
    scene.BeginFrame(); motionVectors.BeginFrame();
    bool shouldCaptureLocalShadows = false;
    if (localShadows) { localShadows->Prepare(scene); shouldCaptureLocalShadows = localShadows->GetSelectedLightCount() > 0; }
    postEffects.PreDrawDepth(); offscreen.PreDraw(postEffects.GetDepthDSVHandle(), false, true, shouldCaptureLocalShadows);
    Require(offscreen.IsReflectionCaptureActive(), "Reflection material capture not available");
    scene.SetReflectionCapture(true);
    scene.SetLocalShadowCapture(offscreen.IsLocalShadowCaptureActive());
    SrvManager::GetInstance()->PreDraw(); LightManager::GetInstance()->UpdateClusters(&camera);
    Object3dManager::GetInstance()->PreDraw();
    for (auto* object : objects) { object->Update(); object->Draw(); }
    if (skinnedTarget != nullptr) {
        skinnedTarget->Update(); SkinningObject3dManager::GetInstance()->PreDraw(); skinnedTarget->Draw();
    }
    scene.EndFrame(&camera, false); Require(scene.HasValidScene(), scene.GetStatus().c_str());
    motionVectors.EndFrame(postEffects.GetDepthDSVHandle()); postEffects.PostDrawDepth(); offscreen.PostDraw();
    DxrReflectionInputs inputs;
    inputs.scene = &scene; inputs.camera = &camera; inputs.colorSrv = offscreen.GetSrvHandleGPU();
    inputs.depthTexture = postEffects.GetDepthTexture(); inputs.depthSrv = postEffects.GetDepthSrv();
    inputs.surfaceTexture = offscreen.GetReflectionSurfaceTexture(); inputs.surfaceSrv = offscreen.GetReflectionSurfaceSrv();
    inputs.environmentTexture = offscreen.GetReflectionEnvironmentTexture(); inputs.environmentSrv = offscreen.GetReflectionEnvironmentSrv();
    inputs.materialTexture = offscreen.GetMaterialTexture(); inputs.materialSrv = offscreen.GetMaterialSrvHandleGPU();
    inputs.localLightTexture = offscreen.GetLocalLightTexture(); inputs.localLightSrv = offscreen.GetLocalLightSrv();
    inputs.motionVectorSrv = motionVectors.GetSrvHandle(); inputs.sceneRevision = sceneRevision;
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
    lights->SetEnvironmentLighting(0, 0);
    target.SetTranslate({4, 4, 0});
    source = DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    baseline = ReadTextureCenter(dxCommon, source, "reflection-miss.png");
    dxCommon->PreDraw(); RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-miss-raw.png"), {0, 0, 0}, "Reflection miss retained an old target");
    RequireColor(baseline, {0.2f, 0.2f, 0.2f}, "Reflection miss changed base lighting");
    target.SetTranslate({4, 0, 0}); target.SetShadingMode(MaterialShadingMode::Standard);
    target.GetMaterial()->specularStrength = 0; target.GetMaterial()->shininess = 0;
    lighting.intensity = 1; lighting.direction = {0, 0, 1}; lighting.ambient.w = 0; lights->ApplyLightingPreset(lighting);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-lit-hit.png"), {kDiagonal, 0, 0}, "Reflection hit ignored material lighting");
    blocker.SetCastShadow(true); objects.push_back(&blocker);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-hit-shadowed.png"), {0, 0, 0}, "Sun shadow ray at reflected hit missed caster");
    settings.shouldTraceSunShadows = false; reflections.SetSettings(settings);
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, reflections.GetRawTexture(), "reflection-hit-shadows-off.png"), {kDiagonal, 0, 0}, "Disabling reflection-hit shadows did not restore sunlight");
    settings.shouldTraceSunShadows = true; reflections.SetSettings(settings); objects.pop_back();
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
    skeleton.joints[0].transform.translate.x = 4; skeleton.UpdateSkeleton();
    DrawReflectionFrame(dxCommon, scene, reflections, offscreen, postEffects, motionVectors, camera, kSkinObjects, 0, &skinnedTarget);
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
    settings.isEnabled = true; indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "rtgi-color-bleeding.png"), baseline + Vector3{0.5f, 0, 0}, "RTGI missed behind-camera source or receiver albedo");
    dxCommon->PreDraw(); RequireColor(ReadTextureCenter(dxCommon, indirect.GetRawTexture(), "rtgi-raw-irradiance.png"), {1, 0, 0}, "RTGI depends on specular strength or roughness");
    scene.ReadCompleted(); indirect.ReadCompleted();
    std::ofstream report("runtime/captures/DxrTests/rtgi-result.txt");
    report << "traceMs=" << indirect.GetTraceGpuTimeMs() << " filterMs=" << indirect.GetFilterGpuTimeMs()
        << " compositeMs=" << indirect.GetCompositeGpuTimeMs() << " targetsBytes=" << indirect.GetAllocationBytes() << '\n';
    settings.isDebugVisible = true; indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "rtgi-debug.png"), {0.5f, 0, 0}, "RTGI debug view includes scene or ignores albedo");
    settings.isDebugVisible = false; settings.strength = 0.25f; indirect.SetSettings(settings);
    source = DrawReflectionFrame(dxCommon, scene, indirect, offscreen, postEffects, motionVectors, camera, objects);
    RequireColor(ReadTextureCenter(dxCommon, source, "rtgi-strength.png"), baseline + Vector3{0.125f, 0, 0}, "RTGI strength composition mismatch");
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
        if (std::strcmp(commandLine, "--local-shadows") == 0) {
            Object3dManager::GetInstance()->Initialize(dxCommon); SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            RunLocalShadowValidation(dxCommon);
        } else {
            RunValidation(dxCommon); RunDeformedValidation(dxCommon); RunShadowValidation(dxCommon); RunSkinningValidation(dxCommon);
            RunAlphaValidation(dxCommon); RunReflectionValidation(dxCommon); RunGlobalIlluminationValidation(dxCommon); RunLocalShadowValidation(dxCommon);
        }
        CheckValidationMessages(infoQueue.Get());
    } catch (const std::exception& error) {
        Logger::Error(error.what());
        std::ofstream report("runtime/captures/DxrTests/failure.txt");
        report << error.what();
        exitCode = 1;
    }
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
