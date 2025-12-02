#pragma once
#include "Camera.h"
#include "Input.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "TextureManager.h"
class GamePlayScene {
public:
    void Initialize();

    void Finalize();

    void Update();

    void Draw2D();
    void Draw3D();
    void DrawImGui();

private:
    // ------------------------------
    // サウンド
    // ------------------------------
    SoundData bgm;
    Sprite* sprite_ = nullptr;
    std::vector<Sprite*> sprites_;
    Object3d* player2_;
};
