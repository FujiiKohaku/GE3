#pragma once

#include "Engine/math/EngineStruct.h"
#include <memory>

class Object3d;
class Text;

struct SavePointSettings {
    Vector3 position = {};
    Vector3 scale = { 10.0f, 12.0f, 10.0f };
    float yawRadians = 0.0f;
};

// 通過式チェックポイントの表示と、一度だけの到達演出を管理する。
// 進行状況の永続保存は、このクラスでは行わない。
class SavePoint {
public:
    SavePoint();
    ~SavePoint();
    void Initialize(const SavePointSettings& settings, const Vector3& playerPosition);
    bool Update(const Vector3& playerPosition, float deltaTimeSeconds);
    void Draw3D();
    // TextRenderer::PreDraw後に呼ぶ。
    void Draw2D();
    bool HasActivated() const { return hasActivated_; }
    static bool IsPassageCrossed(const SavePointSettings& settings,
        const Vector3& previousPosition, const Vector3& currentPosition);

private:
    SavePointSettings settings_;
    Vector3 previousPlayerPosition_ = {};
    std::unique_ptr<Object3d> body_;
    std::unique_ptr<Object3d> glow_;
    std::unique_ptr<Object3d> boundary_;
    std::unique_ptr<Text> arrivalText_;
    float animationTimeSeconds_ = 0.0f;
    float arrivalRemainingSeconds_ = 0.0f;
    bool hasActivated_ = false;
};
