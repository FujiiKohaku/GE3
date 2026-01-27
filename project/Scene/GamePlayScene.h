#pragma once
#include "../3D/SkinningObject3d.h"
#include "../3D/SkinningObject3dManager.h"
#include "../3D/SphereObject.h"
#include "../Animation/PlayAnimation.h"
#include "../Skeleton/Skeleton.h"
#include "BaseScene.h"
#include "Camera.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include <numbers>
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
    Camera* camera_;

    // ------------------------------
    // 3Dオブジェクト（描画主体）
    // ------------------------------
    Object3d* player2_;
    SphereObject* sphere_ = nullptr;
    Object3d* terrain_;
    Object3d* plane_;
    // node00
    Object3d* nodeObject00_;


    // animationSkin
    SkinningObject3d* animationSkin00_;
  


    SkinningObject3d* skinningPlayer_;
    // ------------------------------
    // スプライト（UI / 2D）
    // ------------------------------
    Sprite* sprite_ = nullptr;
    std::vector<Sprite*> sprites_;

    // ------------------------------
    // サウンド
    // ------------------------------
    SoundData bgm;

    // ------------------------------
    // パーティクル
    // ------------------------------
    ParticleEmitter emitter_;

    // ------------------------------
    // アニメーション / スケルトン
    // ------------------------------
    // node
    PlayAnimation nodePlayAnim00_;
    Animation nodeAnimation00_;
    
    // skin
    PlayAnimation* skinPlay00_;
    Animation skinAnimation00_;
    Skeleton animationSkinSkeleton00_;
    
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
    Vector3 terrainPos = { 0.0f, 0.0f, 0.0f };
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
