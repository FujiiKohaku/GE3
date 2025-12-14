#pragma once
#include "BaseScene.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "TextureManager.h"
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
};
