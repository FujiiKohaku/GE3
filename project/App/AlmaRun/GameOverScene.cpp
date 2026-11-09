#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "GameOverScene.h"
#include "BitmapFont.h"
#include "FxUtil.h"
#include "ScoreManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace {
std::string ToStr(int v) { return std::to_string(v); }
} // namespace

AlmaGameOverScene::~AlmaGameOverScene() {
	delete fade_;
	delete camera_;
	delete modelSkyDome_;
	delete gameOverFont_;
	delete retryFont_;
	delete panel_;
	delete line_;
}

//==================================================
// 初期化
//==================================================
void AlmaGameOverScene::Initialize() {
	// フェード
	fade_ = new AlmaFade();
	fade_->Initialize();
	fade_->Start(AlmaFade::AlmaStatus::FadeIn, 1.0f);

	// カメラ
	camera_ = new AlmaCamera();
	camera_->Initialize();

	// スカイドーム
	modelSkyDome_ = AlmaModel::CreateFromOBJ("skydome", true);
	worldTransformSky_.Initialize();

	// 「GAME OVER」文字（リザルトを表示するため画面上部へ）
	gameOverFont_ = AlmaModel::CreateFromOBJ("gameover", true);
	worldTransformGameover_.Initialize();
	worldTransformGameover_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	worldTransformGameover_.rotation_.y = std::numbers::pi_v<float>;
	worldTransformGameover_.translation_.z = -40.0f;

	// 「Press Enter」文字（操作説明はテキストで表示するため描画はしない）
	retryFont_ = AlmaModel::CreateFromOBJ("pushToEnter", true);
	worldTransformPushTo_.Initialize();
	worldTransformPushTo_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	worldTransformPushTo_.rotation_.y = std::numbers::pi_v<float>;
	worldTransformPushTo_.translation_.z = -40.0f;
	worldTransformPushTo_.translation_.y = -2.0f;

	// 状態初期化
	state_ = AlmaState::WaitInput;

	soundHandleGameOver_ = AlmaAudio::GetInstance()->LoadWave("gameover.wav");
	AlmaAudio::GetInstance()->PlayWave(soundHandleGameOver_);

	soundHandleSelect_ = AlmaAudio::GetInstance()->LoadWave("serect.wav");

	// リザルト用の板
	uint32_t white = AlmaTextureManager::Load("white1x1.png");
	panel_ = AlmaSprite::Create(white, {0, 0});
	line_ = AlmaSprite::Create(white, {0, 0});
	resultTimer_ = 0.0f;
}

//==================================================
// 更新
//==================================================
void AlmaGameOverScene::Update() {
	// フェード更新
	if (fade_) {
		fade_->Update();
	}
	resultTimer_ += 1.0f / 60.0f;

	//==========================================
	// ゲームオーバー文字のアニメーション（ぴょんぴょん）
	//==========================================
	static float timer = 0.0f;
	timer += 1.0f / 60.0f;

	float baseY = 2.3f;
	float jump = 0.35f * fabsf(sinf(timer * 3.0f)); // ぴょんぴょん動く
	worldTransformGameover_.translation_.y = baseY + jump;

	//==========================================
	// 状態管理
	//==========================================
	switch (state_) {
	case AlmaState::WaitInput:
		// Enter: タイトルへ / R: もう一度
		if (AlmaInput::GetInstance()->TriggerKey(DIK_RETURN)) {
			AlmaAudio::GetInstance()->PlayWave(soundHandleSelect_);
			fade_->Start(AlmaFade::AlmaStatus::FadeOut, 1.0f);
			pendingResult_ = AlmaResult::Title;
			state_ = AlmaState::FadeOut;
		} else if (AlmaInput::GetInstance()->TriggerKey(DIK_R)) {
			AlmaAudio::GetInstance()->PlayWave(soundHandleSelect_);
			fade_->Start(AlmaFade::AlmaStatus::FadeOut, 0.6f);
			pendingResult_ = AlmaResult::Retry;
			state_ = AlmaState::FadeOut;
		}
		break;

	case AlmaState::FadeOut:
		// フェード完了でシーン終了
		if (fade_->IsFinished()) {
			result_ = pendingResult_;
			finished_ = true;
			state_ = AlmaState::Finish;
		}
		break;

	case AlmaState::Finish:
		// もう何もしない
		break;
	}

	// ワールド・カメラ更新
	AlmaWorldTransformUpdate(worldTransformSky_);
	AlmaWorldTransformUpdate(worldTransformGameover_);
	AlmaWorldTransformUpdate(worldTransformPushTo_);
	camera_->TransferMatrix();
}

//==================================================
// 描画
//==================================================
void AlmaGameOverScene::Draw() {
	AlmaDirectXCommon* dxCommon = AlmaDirectXCommon::GetInstance();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
	AlmaBitmapFont* font = AlmaBitmapFont::GetInstance();
	font->BeginFrame();

	// 3Dモデル描画開始
	AlmaModel::PreDraw(commandList);

	// 背景
	if (modelSkyDome_) {
		modelSkyDome_->Draw(worldTransformSky_, *camera_);
	}

	// 「GAME OVER」文字
	if (gameOverFont_) {
		gameOverFont_->Draw(worldTransformGameover_, *camera_);
	}

	// 3Dモデル描画終了
	AlmaModel::PostDraw();

	//==========================================
	// リザルト（2D）
	//==========================================
	AlmaSprite::PreDraw(commandList);
	{
		using AlmaAlign = AlmaBitmapFont::AlmaAlign;
		const AlmaVector4 white = {1.0f, 1.0f, 1.0f, 1.0f};
		const AlmaVector4 label = {0.8f, 0.85f, 0.95f, 1.0f};
		const AlmaVector4 gold = {1.0f, 0.86f, 0.3f, 1.0f};

		// 板
		panel_->SetPosition({330.0f, 250.0f});
		panel_->SetSize({620.0f, 360.0f});
		panel_->SetColor({0.02f, 0.03f, 0.08f, 0.72f});
		panel_->Draw();

		// カウントアップ演出
		float t = std::clamp(resultTimer_ / 1.2f, 0.0f, 1.0f);
		auto counted = [t](int v) { return static_cast<int>(static_cast<float>(v) * t); };

		font->Draw("RESULT", 640.0f, 262.0f, 2.4f, gold, AlmaAlign::kCenter);

		struct Row {
			const char* name;
			std::string value;
		};
		Row rows[] = {
		    {"DISTANCE", ToStr(counted(AlmaGameResult::distance)) + " m"},
		    {"SCORE", ToStr(counted(AlmaGameResult::score))},
		    {"COINS", ToStr(counted(AlmaGameResult::coins))},
		    {"MAX COMBO", ToStr(AlmaGameResult::maxCombo)},
		    {"SMASHED", ToStr(AlmaGameResult::smashed)},
		};
		float y = 318.0f;
		for (const Row& row : rows) {
			font->Draw(row.name, 370.0f, y, 2.0f, label, AlmaAlign::kLeft);
			font->Draw(row.value, 910.0f, y, 2.0f, white, AlmaAlign::kRight);
			y += 42.0f;
		}

		line_->SetPosition({360.0f, 534.0f});
		line_->SetSize({560.0f, 2.0f});
		line_->SetColor({1.0f, 1.0f, 1.0f, 0.4f});
		line_->Draw();

		font->Draw("BEST " + ToStr(AlmaGameResult::bestDistance) + " m    HIGH SCORE " + ToStr(AlmaGameResult::highScore), 640.0f, 546.0f, 1.5f, label, AlmaAlign::kCenter);

		// 新記録
		if ((AlmaGameResult::newHighScore || AlmaGameResult::newBestDistance) && t >= 1.0f) {
			float blink = 0.6f + 0.4f * std::sin(resultTimer_ * 8.0f);
			std::string text = AlmaGameResult::newHighScore ? "NEW HIGH SCORE!" : "NEW BEST DISTANCE!";
			font->Draw(text, 640.0f, 576.0f, 1.8f, FxUtil::Rainbow(resultTimer_ * 0.8f, blink), AlmaAlign::kCenter);
		}

		// 操作説明
		float blink = 0.55f + 0.45f * std::sin(resultTimer_ * 4.0f);
		font->Draw("ENTER : TITLE        R : RETRY", 640.0f, 640.0f, 2.0f, {1.0f, 1.0f, 1.0f, blink}, AlmaAlign::kCenter);
	}
	AlmaSprite::PostDraw();

	// フェード描画（最後に）
	if (fade_) {
		fade_->Draw();
	}
}
