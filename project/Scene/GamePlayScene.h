#pragma once
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
#include "../Animation/PlayAnimation.h"
#include"../Skeleton/Skeleton.h"
#include"../3D/SphereObject.h"
#include"../3D/SkinningObject3d.h"
#include"../3D/SkinningObject3dManager.h"
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
	Vector3 sphereScale = { 1.0f, 1.0f, 1.0f };
	float lightIntensity = 1.0f;
	Vector3 lightDir = { 0.0f, -1.0f, 0.0f };
	SkinningObject3d* animationPlayer_;
	// ------------------------------
	/// その他
	// ------------------------------
	PlayAnimation* playAnim_;
	Animation animation_;
	Skeleton skeleton_;
	SphereObject* jointSphere_ = nullptr;
	std::vector<SphereObject> jointSpheres_;
	float r = 0;
};
