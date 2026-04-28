#pragma once
#include "Engine/3D/SkinningObject3d.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/3D/SphereObject.h"

#include "Engine/Animation/PlayAnimation.h"
#include "Engine/Skeleton/Skeleton.h"

#include "App/Scene/BaseScene.h"

#include "Engine/Camera/Camera.h"
#include "Engine/debugcamera/DebugCameraController.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"

#include "Engine/Particle/ParticleEmitter.h"
#include "Engine/Particle/ParticleManager.h"

#include "Engine/audio/SoundManager.h"

#include "Engine/2D/Sprite.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include <numbers>

#include "Engine/Animation/AnimationActor.h"

#include <memory>

#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/Particle/ParticleSystem.h"
#include "../Game/Player.h"
class GamePlayScene : public BaseScene {
public:
    void Initialize() override;

    void Finalize() override;

    void Update() override;

    void Draw2D() override;
    void Draw3D() override;
    void DrawImGui() override;

private:
    // ------------------------------
    // カメラ
    // ------------------------------
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<DebugCameraController> debugCameraController_;
    // ------------------------------
    // 3Dオブジェクト（描画主体）
    // ------------------------------

    std::unique_ptr<SphereObject> sphere_;
    std::unique_ptr<Object3d> terrain_;
    std::unique_ptr<Object3d> plane_;
    std::unique_ptr<Object3d> droneObj_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<SkinningObject3d> skinningPlayer_;
    std::unique_ptr<AnimationActor> animationActor_;

    // プレイヤークラス
    std::unique_ptr<Player> player_;

    // ------------------------------
    // スプライト（UI / 2D）
    // ------------------------------
    std::unique_ptr<Sprite> testSprite_;

    // ------------------------------
    // サウンド
    // ------------------------------
    SoundData bgm;

    // ------------------------------
    // パーティクル
    // ------------------------------
    ParticleEmitter emitter_;
    ParticleSystem particleSystem_;
    // ------------------------------
    // アニメーション / スケルトン
    // ------------------------------

    // ------------------------------
    // ライト・描画パラメータ
    // ------------------------------
    bool sphereLighting = true;
    float lightIntensity = 1.0f;
    Vector3 lightDir = { 0.0f, -1.0f, 0.0f };

    // ------------------------------
    // スフィア Transform
    // ------------------------------
    Vector3 spherePos = { 0.0f, 0.0f, 0.0f };
    Vector3 sphereRotate = { 0.0f, 0.0f, 0.0f };
    Vector3 sphereScale = { 1.0f, 1.0f, 1.0f };

    // ------------------------------
    // Terrain Transform (ImGui用)
    // ------------------------------
    Vector3 terrainPos = { 0.0f, 5.0f, 0.0f };
    Vector3 terrainRotate = { 0.0f, 0.0f, 0.0f };
    Vector3 terrainScale = { 1.0f, 1.0f, 1.0f };
    // ------------------------------
    // Plane Transform (ImGui用)
    // ------------------------------
    Vector3 planePos = { 0.0f, 0.0f, 0.0f };
    Vector3 planeRotate = { 0.0f, std::numbers::pi_v<float>, 0.0f };
    Vector3 planeScale = { 1.0f, 1.0f, 1.0f };
    // ------------------------------
    // その他
    // ------------------------------
    float r = 0.0f;
};
