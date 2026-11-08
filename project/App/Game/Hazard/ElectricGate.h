#pragma once

#include "Engine/math/object3Dstruct.h"
#include "ElectricGateCycleSettings.h"
#include <memory>

class Model;
class ModelCommon;
class Object3d;

// 周期的に上下2段の放電を切り替えるゲート。
class ElectricGate {
public:
    ElectricGate();
    ~ElectricGate();
    void Initialize(const Vector3& position, float yawRadians, float scale,
        const ElectricGateCycleSettings& cycleSettings = {});
    void Update(const Vector3& playerPosition, float deltaTimeSeconds);
    void Draw();
    bool IsDischarging() const { return cycle_.GetState() == ElectricGateState::On; }
    static constexpr int kContactDamage = 1;
    bool CheckCollision(const Vector3& previousPlayerPosition, const Vector3& playerPosition,
        float playerRadius = 1.0f) const;
    static bool CheckDischargeCollision(const Vector3& gatePosition, float yawRadians, float scale,
        const Vector3& previousPlayerPosition, const Vector3& playerPosition, float playerRadius,
        const ElectricGateCycle* cycle = nullptr);

private:
    void UpdateDischarge();
    void UpdateEmitterColor();
    void WriteSegment(const Vector3& start, const Vector3& end, float width, size_t segmentIndex);
    Vector3 position_ = {};
    float yawRadians_ = 0.0f;
    float scale_ = 1.0f;
    float elapsedSeconds_ = 0.0f;
    bool isVisible_ = false;
    ElectricGateCycle cycle_;
    std::unique_ptr<ModelCommon> modelCommon_;
    std::unique_ptr<Model> dischargeModel_;
    std::unique_ptr<Object3d> body_;
    std::unique_ptr<Object3d> emitters_;
    std::unique_ptr<Object3d> discharge_;
    VertexData* dischargeVertices_ = nullptr;
};
