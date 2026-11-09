#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "TitleScene.h"
#include "BitmapFont.h"
#include "Math.h"
#include "ScoreManager.h"
#include <numbers>
#include <string>

//--------------------------------------------------
// デストラクタ
//--------------------------------------------------
AlmaTitleScene::~AlmaTitleScene() {
	delete fade_;
	delete modelTitle_;
	delete modelPlayer_;
	delete sun_;
	delete backGround_;
	delete pushSpace_;
}

//==================================================
// 初期化
//==================================================
void AlmaTitleScene::Initialize() {

	//--------------------------------------------------
	// モデル生成
	//--------------------------------------------------
	modelTitle_ = AlmaModel::CreateFromOBJ("aru", true);
	modelPlayer_ = AlmaModel::CreateFromOBJ("playermax");
	sun_ = AlmaModel::CreateFromOBJ("isLand", true);
	backGround_ = AlmaModel::CreateFromOBJ("sora", true);
	pushSpace_ = AlmaModel::CreateFromOBJ("pushSpace", true);

	BackGroundTexture_ = AlmaTextureManager::Load("mario.jpg");

	//--------------------------------------------------
	// カメラ初期化
	//--------------------------------------------------
	camera_.Initialize();

	//--------------------------------------------------
	// タイトル文字 AlmaTransform
	//--------------------------------------------------
	const float kPlayerTitle = 2.0f;
	worldTransformTitle_.Initialize();
	worldTransformTitle_.scale_ = {kPlayerTitle, kPlayerTitle, kPlayerTitle};

	//--------------------------------------------------
	// プレイヤー AlmaTransform
	//--------------------------------------------------
	const float kPlayerScale = 3.0f;
	worldTransformPlayer_.Initialize();
	worldTransformPlayer_.scale_ = {kPlayerScale, kPlayerScale, kPlayerScale};
	worldTransformPlayer_.rotation_.y = std::numbers::pi_v<float> / 2.0f;

	//--------------------------------------------------
	// Sun AlmaTransform
	//--------------------------------------------------
	worldTransformSun_.Initialize();
	worldTransformSun_.rotation_.y = std::numbers::pi_v<float> / 2.0f;
	worldTransformSun_.scale_ = {9.0f, 9.0f, 9.0f};

	//--------------------------------------------------
	// 背景 AlmaTransform
	//--------------------------------------------------
	worldTransformBack_.Initialize();
	const float bgScale = 30.0f;
	worldTransformBack_.scale_ = {bgScale, bgScale, bgScale};
	worldTransformBack_.translation_.z = 60.0f;
	worldTransformBack_.rotation_.y = std::numbers::pi_v<float> / 2.0f;

	//--------------------------------------------------
	// PushSpace AlmaTransform
	//--------------------------------------------------
	worldTransformPushSpace_.Initialize();
	worldTransformPushSpace_.scale_ = {3.0f, 3.0f, 3.0f};
	worldTransformPushSpace_.translation_.z = -10.0f;
	worldTransformPushSpace_.translation_.y = -10.0f;
	worldTransformPushSpace_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	worldTransformPushSpace_.rotation_.y = std::numbers::pi_v<float>;

	//-------------------------------------------------
	// タイトル
	//-------------------------------------------------
	worldTransformTitle_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	worldTransformTitle_.rotation_.y = std::numbers::pi_v<float>;
	worldTransformTitle_.translation_.z = -5.0f;

	//--------------------------------------------------
	// フェード生成
	//--------------------------------------------------
	fade_ = new AlmaFade();
	fade_->Initialize();
	fade_->Start(AlmaFade::AlmaStatus::FadeIn, 1.0f);

	//--------------------------------------------------
	// フェーズ初期化
	//--------------------------------------------------
	phase_ = AlmaPhase::kFadeIn;

	soundHandleSelecct = AlmaAudio::GetInstance()->LoadWave("serect.wav");

	// ベスト記録
	AlmaScoreManager::LoadBest(bestDistance_, highScore_);
}

//==================================================
// 更新
//==================================================
void AlmaTitleScene::Update() {

	//--------------------------------------------------
	// フェーズ管理
	//--------------------------------------------------
	switch (phase_) {
	case AlmaPhase::kFadeIn: {
		fade_->Update();
		if (fade_->IsFinished()) {
			phase_ = AlmaPhase::kMain;
		}
	} break;

	case AlmaPhase::kMain: {
		if (AlmaInput::GetInstance()->PushKey(DIK_SPACE)) {
			fade_->Start(AlmaFade::AlmaStatus::FadeOut, 1.0f);
			AlmaAudio::GetInstance()->PlayWave(soundHandleSelecct);
			phase_ = AlmaPhase::kFadeOut;
		}
	} break;

	case AlmaPhase::kFadeOut: {
		fade_->Update();
		if (fade_->IsFinished()) {
			finished_ = true;
		}
	} break;
	}

	//--------------------------------------------------
	// タイトル文字の上下動アニメーション
	//--------------------------------------------------
	counter_ += 1.0f / 60.0f;
	counter_ = std::fmod(counter_, kTimeTitleMove);

	float angleTitle = counter_ / kTimeTitleMove * 2.0f * std::numbers::pi_v<float>;
	worldTransformTitle_.translation_.y = std::sin(angleTitle) + 10.0f;

	//--------------------------------------------------
	// プレイヤーを円運動＋ふわふわさせる
	//--------------------------------------------------
	angle_ += 0.01f;            // 回転速度
	const float radius = 30.0f; // 回転半径

	// 中心座標
	AlmaVector3 center = {0.0f, -10.0f, 0.0f};

	// 円運動（XZ平面）
	worldTransformPlayer_.translation_.x = center.x + radius * std::cos(angle_);
	worldTransformPlayer_.translation_.z = center.z + radius * std::sin(angle_);

	// ふわふわ（Y座標をsin波で変化）
	const float baseHeight = 5.0f;     // 基準高さ
	const float floatAmplitude = 2.0f; // 振幅
	const float floatSpeed = 0.05f;    // 速度
	worldTransformPlayer_.translation_.y = baseHeight + floatAmplitude * std::sin(angle_ * (1.0f / floatSpeed));

	// プレイヤーの向き
	worldTransformPlayer_.rotation_.y = angle_ + std::numbers::pi_v<float> / 2.0f;

	//--------------------------------------------------
	// カメラ更新
	//--------------------------------------------------
	camera_.TransferMatrix();

	//--------------------------------------------------
	// ワールド行列更新
	//--------------------------------------------------
	AlmaWorldTransformUpdate(worldTransformTitle_);
	AlmaWorldTransformUpdate(worldTransformPlayer_);
	AlmaWorldTransformUpdate(worldTransformSun_);
	AlmaWorldTransformUpdate(worldTransformBack_);
	AlmaWorldTransformUpdate(worldTransformPushSpace_);
}

//==================================================
// 描画
//==================================================
void AlmaTitleScene::Draw() {
	AlmaDirectXCommon* dxCommon_ = AlmaDirectXCommon::GetInstance();
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	AlmaModel::PreDraw(commandList);

	// モデル描画
	modelTitle_->Draw(worldTransformTitle_, camera_);
	modelPlayer_->Draw(worldTransformPlayer_, camera_);
	sun_->Draw(worldTransformSun_, camera_);
	backGround_->Draw(worldTransformBack_, camera_);
	pushSpace_->Draw(worldTransformPushSpace_, camera_);

	AlmaModel::PostDraw();

	// ベスト記録
	AlmaBitmapFont* font = AlmaBitmapFont::GetInstance();
	font->BeginFrame();
	AlmaSprite::PreDraw(commandList);
	if (bestDistance_ > 0 || highScore_ > 0) {
		std::string best = "BEST " + std::to_string(bestDistance_) + " m     HIGH SCORE " + std::to_string(highScore_);
		font->Draw(best, 640.0f, 660.0f, 2.0f, {1.0f, 0.9f, 0.45f, 1.0f}, AlmaBitmapFont::AlmaAlign::kCenter);
	}
	AlmaSprite::PostDraw();

	// フェード描画
	fade_->Draw();
}
