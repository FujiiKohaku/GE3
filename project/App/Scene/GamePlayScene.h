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

#include "Engine/Effect/EffectManager.h"

#include "Engine/audio/SoundManager.h"

#include "Engine/2D/Sprite.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include <numbers>

#include "Engine/Animation/AnimationActor.h"

#include "../Game/Player.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/postEffect/CopyImageRenderer.h"
#include <memory>
#include <vector>

#include "../../Engine/EditorManager/EditorManager.h"

#include "../../Engine/SceneObjectManager/SceneObjectManager.h"

#include"../Game/BaseEnemy.h"

#include "../Game/NormalEnemy.h"    
class GamePlayScene : public BaseScene {
public:
    void Initialize() override;

    void Finalize() override;

    void Update() override;

    void Draw2D() override;
    void Draw3D() override;
    void DrawImGui() override;

private:
    void CheckCollision();
    std::unique_ptr<SceneObjectManager> sceneObjectManager_;

    int test_ = 0;
    std::unique_ptr<EditorManager> editorManager_;
    // ------------------------------
    // 繧ｫ繝｡繝ｩ
    // ------------------------------
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<DebugCameraController> debugCameraController_;
    // ------------------------------
    // 3D繧ｪ繝悶ず繧ｧ繧ｯ繝茨ｼ域緒逕ｻ荳ｻ菴難ｼ・
    // ------------------------------

    std::unique_ptr<SphereObject> sphere_;
    // std::unique_ptr<Object3d> terrain_;
    // std::unique_ptr<Object3d> plane_;
    std::unique_ptr<Object3d> droneObj_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<SkinningObject3d> skinningPlayer_;
    std::unique_ptr<AnimationActor> animationActor_;
    std::vector<std::unique_ptr<Object3d>> levelObjects_;
    // 繝励Ξ繧､繝､繝ｼ繧ｯ繝ｩ繧ｹ
    std::unique_ptr<Player> player_;

    // ------------------------------
    // 繧ｹ繝励Λ繧､繝茨ｼ・I / 2D・・
    // ------------------------------
    std::unique_ptr<Sprite> testSprite_;
    std::unique_ptr<Sprite> aimSprite_;
    // ------------------------------
    // 繧ｵ繧ｦ繝ｳ繝・
    // ------------------------------
    SoundData bgm;

    // ------------------------------
    // 繝代・繝・ぅ繧ｯ繝ｫ
    // ------------------------------
    // ParticleEmitter emitter_;
    // ParticleSystem particleSystem_;
    // ------------------------------
    // 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ / 繧ｹ繧ｱ繝ｫ繝医Φ
    // ------------------------------

    // ------------------------------
    // 繝ｩ繧､繝医・謠冗判繝代Λ繝｡繝ｼ繧ｿ
    // ------------------------------
    bool sphereLighting = true;
    float lightIntensity = 1.0f;
    Vector3 lightDir = { 0.0f, -1.0f, 0.0f };

    // ------------------------------
    // 繧ｹ繝輔ぅ繧｢ Transform
    // ------------------------------
    Vector3 spherePos = { 0.0f, 0.0f, 0.0f };
    Vector3 sphereRotate = { 0.0f, 0.0f, 0.0f };
    Vector3 sphereScale = { 1.0f, 1.0f, 1.0f };

    // ------------------------------
    // Terrain Transform (ImGui逕ｨ)
    // ------------------------------
    Vector3 terrainPos = { 0.0f, -10.0f, 0.0f };
    Vector3 terrainRotate = { 0.0f, 0.0f, 0.0f };
    Vector3 terrainScale = { 1.0f, 1.0f, 1.0f };
    // ------------------------------
    // Plane Transform (ImGui逕ｨ)
    // ------------------------------
    Vector3 planePos = { 0.0f, 0.0f, 0.0f };
    Vector3 planeRotate = { 0.0f, std::numbers::pi_v<float>, 0.0f };
    Vector3 planeScale = { 1.0f, 1.0f, 1.0f };
    // ------------------------------
    // 縺昴・莉・
    // ------------------------------
    float r = 0.0f;

    Vector3 cameraRotate_ = { 0.0f, 0.0f, 0.0f };
    EffectHandle playerJetHandle_ = kInvalidEffectHandle;

    // 謨ｵ縺ｮ邂｡逅・
    std::vector<std::unique_ptr<BaseEnemy>> enemies_;
};
