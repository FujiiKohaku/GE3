#include "ElectricGate.h"

#include "Engine/3D/Model.h"
#include "Engine/3D/ModelCommon.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include <array>
#include <cmath>

namespace {
constexpr size_t kMainSegmentCount = 18;
constexpr size_t kSegmentsPerArc = kMainSegmentCount + 6;
constexpr size_t kTotalSegmentCount = kSegmentsPerArc * 2;
constexpr size_t kVerticesPerSegment = 8;
constexpr float kShapeIntervalSeconds = 0.06f;
constexpr float kLowerArcHeight = 2.2f;
constexpr float kArcHeightInterval = 1.35f;
constexpr float kArcHalfWidth = 2.05f;
constexpr float kArcJitterHeight = 0.16f;
constexpr float kArcVisualWidth = 0.14f;

float CalculateJitter(float pointIndex, float shapeIndex, float arcIndex)
{
    const float value = std::sin(pointIndex * 127.1f + shapeIndex * 311.7f + arcIndex * 74.7f) * 43758.5453f;
    return (value - std::floor(value)) * 2.0f - 1.0f;
}
}

ElectricGate::ElectricGate() = default;
ElectricGate::~ElectricGate() = default;

void ElectricGate::Initialize(const Vector3& position, float yawRadians, float scale,
    const ElectricGateCycleSettings& cycleSettings)
{
    cycle_ = ElectricGateCycle{};
    cycle_.Initialize(cycleSettings);
    position_ = position;
    yawRadians_ = yawRadians;
    scale_ = scale;
    elapsedSeconds_ = 0.0f;
    isVisible_ = false;
    body_ = std::make_unique<Object3d>();
    body_->Initialize(Object3dManager::GetInstance());
    body_->SetModel(ModelManager::GetInstance()->Load("Environment/ElectricGate/ElectricGateBody.obj"));
    body_->SetScale({ scale, scale, scale });
    body_->SetTranslate(position);
    body_->SetRotate({ 0.0f, yawRadians, 0.0f });
    body_->SetSurfaceProperties(0.4f, 0.25f, 0.5f);
    body_->Update();
    emitters_ = std::make_unique<Object3d>();
    emitters_->Initialize(Object3dManager::GetInstance());
    emitters_->SetModel(ModelManager::GetInstance()->Load("Environment/ElectricGate/ElectricGateEmitters.obj"));
    emitters_->SetScale({ scale, scale, scale });
    emitters_->SetTranslate(position);
    emitters_->SetRotate({ 0.0f, yawRadians, 0.0f });
    emitters_->SetEnableLighting(false);
    UpdateEmitterColor();
    emitters_->Update();

    // 放電2段と枝分かれを1つの固定サイズバッファへまとめる。
    ModelData modelData {};
    modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    modelData.rootNode.transform.scale = { 1.0f, 1.0f, 1.0f };
    modelData.rootNode.transform.rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
    MeshPrimitive primitive {};
    primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices.resize(kTotalSegmentCount * kVerticesPerSegment);
    for (size_t segmentIndex = 0; segmentIndex < kTotalSegmentCount; ++segmentIndex) {
        const uint32_t vertexOffset = static_cast<uint32_t>(segmentIndex * kVerticesPerSegment);
        const uint32_t indices[] = { 0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6 };
        for (uint32_t index : indices) { primitive.indices.push_back(vertexOffset + index); }
    }
    modelData.primitives.push_back(std::move(primitive));
    modelData.materials.push_back({ "resources/Textures/white.png" });
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(DirectXCommon::GetInstance());
    dischargeModel_ = std::make_unique<Model>();
    dischargeModel_->Initialize(modelCommon_.get(), modelData);
    dischargeModel_->GetModelData().primitives[0].vertexResource->Map(
        0, nullptr, reinterpret_cast<void**>(&dischargeVertices_));
    discharge_ = std::make_unique<Object3d>();
    discharge_->Initialize(Object3dManager::GetInstance());
    discharge_->SetModel(dischargeModel_.get());
    discharge_->SetScale({ scale, scale, scale });
    discharge_->SetTranslate(position);
    discharge_->SetRotate({ 0.0f, yawRadians, 0.0f });
    discharge_->SetPixelShaderPath("resources/Shaders/Object3D/ElectricGate/Render.PS.hlsl");
    discharge_->SetEnableLighting(false);
    discharge_->SetTransparent(true);
    discharge_->SetTransparentDepthWrite(false);
    discharge_->SetCastShadow(false);
    discharge_->SetReceiveShadow(false);
    discharge_->SetRaytracingEnabled(false);
    discharge_->SetFrustumCullingEnabled(false);
    discharge_->SetFogEnabled(false);
    UpdateDischarge();
    discharge_->Update();
}

void ElectricGate::WriteSegment(const Vector3& start, const Vector3& end, float width, size_t segmentIndex)
{
    const Vector3 direction = Normalize(end - start);
    const Vector3 side = Normalize(Cross(direction, { 0.0f, 0.0f, 1.0f })) * width * 0.5f;
    const Vector3 positions[] = { start - side, start + side, end + side, end - side };
    const Vector2 texcoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
    for (size_t sideIndex = 0; sideIndex < 2; ++sideIndex) {
        float normalZ = -1.0f;
        if (sideIndex == 1) { normalZ = 1.0f; }
        for (size_t cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
            VertexData& vertex = dischargeVertices_[segmentIndex * kVerticesPerSegment + sideIndex * 4 + cornerIndex];
            const Vector3& position = positions[cornerIndex];
            vertex.position = { position.x, position.y, position.z, 1.0f };
            vertex.texcoord = texcoords[cornerIndex];
            vertex.normal = { 0.0f, 0.0f, normalZ };
        }
    }
}

void ElectricGate::UpdateDischarge()
{
    const float shapeIndex = std::floor(elapsedSeconds_ / kShapeIntervalSeconds);
    for (size_t arcIndex = 0; arcIndex < 2; ++arcIndex) {
        const float height = kLowerArcHeight + static_cast<float>(arcIndex) * kArcHeightInterval;
        std::array<Vector3, kMainSegmentCount + 1> points {};
        for (size_t pointIndex = 0; pointIndex <= kMainSegmentCount; ++pointIndex) {
            const float progress = static_cast<float>(pointIndex) / static_cast<float>(kMainSegmentCount);
            float jitter = 0.0f;
            if (pointIndex > 0 && pointIndex < kMainSegmentCount) {
                jitter = CalculateJitter(static_cast<float>(pointIndex), shapeIndex, static_cast<float>(arcIndex));
            }
            points[pointIndex] = { -kArcHalfWidth + progress * kArcHalfWidth * 2.0f,
                height + jitter * kArcJitterHeight, 0.0f };
        }
        const size_t firstSegment = arcIndex * kSegmentsPerArc;
        for (size_t segmentIndex = 0; segmentIndex < kMainSegmentCount; ++segmentIndex) {
            WriteSegment(points[segmentIndex], points[segmentIndex + 1], kArcVisualWidth, firstSegment + segmentIndex);
        }
        for (size_t branchIndex = 0; branchIndex < 3; ++branchIndex) {
            const Vector3 start = points[4 + branchIndex * 5];
            const float jitter = CalculateJitter(static_cast<float>(branchIndex), shapeIndex, static_cast<float>(arcIndex) + 5.0f);
            const Vector3 middle = start + Vector3 { 0.18f, jitter * 0.2f, 0.0f };
            const Vector3 end = start + Vector3 { 0.45f, jitter * 0.4f, 0.0f };
            WriteSegment(start, middle, 0.055f, firstSegment + kMainSegmentCount + branchIndex * 2);
            WriteSegment(middle, end, 0.035f, firstSegment + kMainSegmentCount + branchIndex * 2 + 1);
        }
    }
}

void ElectricGate::Update(const Vector3& playerPosition, float deltaTimeSeconds)
{
    cycle_.Update(deltaTimeSeconds);
    if (std::isfinite(deltaTimeSeconds) && deltaTimeSeconds > 0.0f) {
        constexpr float kAnimationPeriodSeconds = 60.0f;
        elapsedSeconds_ = std::fmod(elapsedSeconds_ + std::fmod(deltaTimeSeconds,
            kAnimationPeriodSeconds), kAnimationPeriodSeconds);
    }
    const float distance = Vector3Length(position_ - playerPosition);
    isVisible_ = distance < 750.0f && playerPosition.z < position_.z + 100.0f;
    if (!isVisible_) { return; }
    UpdateEmitterColor();
    emitters_->Update();
    if (!IsDischarging()) { return; }
    UpdateDischarge();
    discharge_->SetColor({ 1.0f, 0.16f, 0.015f, 0.9f + 0.1f * std::sin(elapsedSeconds_ * 73.0f) });
    discharge_->Update();
}

void ElectricGate::UpdateEmitterColor()
{
    float brightness = 0.08f;
    if (cycle_.GetState() == ElectricGateState::Warning) {
        brightness = 0.4f + 2.6f * cycle_.GetWarningProgress();
    } else if (IsDischarging()) {
        brightness = 4.0f;
    }
    emitters_->SetColor({ brightness, brightness * 0.5f, brightness * 0.25f, 1.0f });
}

void ElectricGate::Draw()
{
    if (!isVisible_) { return; }
    body_->Draw();
    emitters_->Draw();
    if (IsDischarging()) { discharge_->Draw(); }
}

bool ElectricGate::CheckCollision(const Vector3& previousPlayerPosition, const Vector3& playerPosition,
    float playerRadius) const
{
    return CheckDischargeCollision(position_, yawRadians_, scale_, previousPlayerPosition,
        playerPosition, playerRadius, &cycle_);
}

bool ElectricGate::CheckDischargeCollision(const Vector3& gatePosition, float yawRadians, float scale,
    const Vector3& previousPlayerPosition, const Vector3& playerPosition, float playerRadius,
    const ElectricGateCycle* cycle)
{
    if (cycle != nullptr && !cycle->HasDischargeDuringFrameInterval(0.0, 1.0)) { return false; }
    if (!ValueValidation::IsFinite(gatePosition) || !ValueValidation::IsFinite(previousPlayerPosition) ||
        !ValueValidation::IsFinite(playerPosition) || !std::isfinite(yawRadians) ||
        !std::isfinite(scale) || scale <= 0.0f || !std::isfinite(playerRadius) || playerRadius < 0.0f) {
        return false;
    }
    const Sphere playerSphere { previousPlayerPosition, playerRadius };
    const Vector3 movement = playerPosition - previousPlayerPosition;
    // 揺れる主放電が収まる2本の固定帯で判定する。枝分かれの火花は演出のみ。
    const Vector3 size { kArcHalfWidth * 2.0f * scale,
        (kArcJitterHeight * 2.0f + kArcVisualWidth) * scale, kArcVisualWidth * scale };
    for (size_t arcIndex = 0; arcIndex < 2; ++arcIndex) {
        Vector3 center = gatePosition;
        center.y += (kLowerArcHeight + static_cast<float>(arcIndex) * kArcHeightInterval) * scale;
        const OBB dischargeBox = CollisionManager::MakeOBB(center, size, { 0.0f, yawRadians, 0.0f });
        const SweepHit entryHit = CollisionManager::SweepSphere(playerSphere, movement, dischargeBox);
        if (!entryHit.isHit) { continue; }
        if (cycle == nullptr) { return true; }
        // 凸な帯への進入・退出時刻を両方向のSweepから求める。
        // 周期数に関係なく、1本につき最大2回のSweepで放電時間との重なりを判定する。
        const SweepHit exitHit = CollisionManager::SweepSphere(
            Sphere{ playerPosition, playerRadius }, movement * -1.0f, dischargeBox);
        if (exitHit.isHit && cycle->HasDischargeDuringFrameInterval(
            entryHit.time, 1.0 - static_cast<double>(exitHit.time))) { return true; }
    }
    return false;
}
