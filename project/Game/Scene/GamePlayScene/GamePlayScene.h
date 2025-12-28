#pragma once
#include "../BaceScene/BaseScene.h"

#include "../../../Engine/Graphics/2D/SpriteManager.h"
#include "../../../Engine/Graphics/3D/ModelManager.h"
#include "../../../Engine/Graphics/3D/Object3dManager.h"
#include "../../../Engine/Graphics/Particle/ParticleEmitter.h"
#include "../../../Engine/Graphics/Particle/ParticleManager.h"
#include "../../../Engine/System/TextureManager/TextureManager.h"
#include "../../../Engine/audio/SoundManager.h"


class Camera;
class Sprite;
class Object3d;
class SphereObject;
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
    // サウンド
    // ------------------------------
    SoundData bgm;
    Sprite* sprite_ = nullptr;
    std::vector<Sprite*> sprites_;
    Object3d* player2_;

    ParticleEmitter emitter_;

    // ------------------------------
    // メッシュ
    // ------------------------------
    SphereObject* sphere_ = nullptr;

    Camera* camera_;

    bool sphereLighting = true;
    Vector3 spherePos = { 0.0f, 0.0f, 0.0f };
    Vector3 sphereRotate = { 0.0f, 0.0f, 0.0f }; // ラジアン想定

    float lightIntensity = 1.0f;
    Vector3 lightDir = { 0.0f, -1.0f, 0.0f };
};
