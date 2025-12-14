#pragma once
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "ParticleEmitter.h"
#include "BaseScene.h"
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
};
