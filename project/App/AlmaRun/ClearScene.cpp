#define NOMINMAX
#include "ClearScene.h"
#include <numbers>

//==================================================
// 初期化
//==================================================
void AlmaClearScene::Initialize() {
	// フェード
	fade_ = new AlmaFade();
	fade_->Initialize();
	fade_->Start(AlmaFade::AlmaStatus::FadeIn, 1.0f); // フェードイン開始

	// カメラ
	camera_ = new AlmaCamera();
	camera_->Initialize();

	// スカイドーム
	modelSkyDome_ = AlmaModel::CreateFromOBJ("skydome", true);
	worldTransformSky_.Initialize();

	// 「GAME CLEAR」文字
	clearFont_ = AlmaModel::CreateFromOBJ("clear", true);
	worldTransformClear_.Initialize();
	worldTransformClear_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	worldTransformClear_.rotation_.y = std::numbers::pi_v<float>;
	worldTransformClear_.translation_.z = -40.0f;

	// 「Press Enter」文字
	retryFont_ = AlmaModel::CreateFromOBJ("pushToEnter", true);
	worldTransformPushTo_.Initialize();
	worldTransformPushTo_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	worldTransformPushTo_.rotation_.y = std::numbers::pi_v<float>;
	worldTransformPushTo_.translation_.z = -40.0f;
	worldTransformPushTo_.translation_.y = -2.0f;

	// 状態初期化
	state_ = AlmaState::WaitInput;

	soundHandleClear_ = AlmaAudio::GetInstance()->LoadWave("clear.wav");
	AlmaAudio::GetInstance()->PlayWave(soundHandleClear_);
	soundHandleSelect_ = AlmaAudio::GetInstance()->LoadWave("serect.wav");
}

//==================================================
// 更新
//==================================================
void AlmaClearScene::Update() {
	// フェード更新
	if (fade_) {
		fade_->Update();
	}
	static float timer = 0.0f;
	timer += 1.0f / 60.0f; // 経過時間

	// 時間設定
	float launchTime = 1.0f; // 打ち上げ時間(秒)
	float waitTime = 0.5f;   // 中央で待つ時間
	float expandTime = 1.5f; // 拡大演出の時間

	// 初期位置と最終位置
	float startY = -3.0f; // 画面下から
	float centerY = 0.0f; // 画面中央
	float offsetX = 0.0f; // ← 左に寄せたい量（マイナスで左、プラスで右）

	// 打ち上げ
	if (timer < launchTime) {
		float t = timer / launchTime; // 0→1 の補間
		worldTransformClear_.translation_.x = offsetX;
		worldTransformClear_.translation_.y = startY + (centerY - startY) * t;
		worldTransformClear_.scale_ = {0.3f, 0.3f, 0.3f};
	}
	// 停止
	else if (timer < launchTime + waitTime) {
		worldTransformClear_.translation_.x = offsetX;
		worldTransformClear_.translation_.y = centerY;
		worldTransformClear_.scale_ = {0.3f, 0.3f, 0.3f};
	}
	// 拡大（花火っぽく）
	else if (timer < launchTime + waitTime + expandTime) {
		float t = (timer - (launchTime + waitTime)) / expandTime;
		worldTransformClear_.translation_.x = offsetX;
		worldTransformClear_.translation_.y = centerY;
		float s = 0.3f + t * 1.5f;
		worldTransformClear_.scale_ = {s, s, s};
	}
	// 終了後はそのまま表示
	else {
		worldTransformClear_.translation_.x = offsetX;
		worldTransformClear_.translation_.y = centerY;
		worldTransformClear_.scale_ = {1.8f, 1.8f, 1.8f};
	}

	//==========================================
	// 状態管理
	//==========================================
	switch (state_) {
	case AlmaState::WaitInput:
		// Enter押されたらフェードアウト開始
		if (AlmaInput::GetInstance()->TriggerKey(DIK_RETURN)) {
			fade_->Start(AlmaFade::AlmaStatus::FadeOut, 1.0f);
			AlmaAudio::GetInstance()->PlayWave(soundHandleSelect_);
			state_ = AlmaState::FadeOut;
		}
		break;

	case AlmaState::FadeOut:
		// フェード完了でシーン終了
		if (fade_->IsFinished()) {
			result_ = AlmaResult::Title;
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
	AlmaWorldTransformUpdate(worldTransformClear_);
	AlmaWorldTransformUpdate(worldTransformPushTo_);
	camera_->TransferMatrix();
}

//==================================================
// 描画
//==================================================
void AlmaClearScene::Draw() {
	AlmaDirectXCommon* dxCommon = AlmaDirectXCommon::GetInstance();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	// 3Dモデル描画開始
	AlmaModel::PreDraw(commandList);

	// 背景
	if (modelSkyDome_) {
		modelSkyDome_->Draw(worldTransformSky_, *camera_);
	}

	// 「GAME CLEAR」文字
	if (clearFont_) {
		clearFont_->Draw(worldTransformClear_, *camera_);
	}

	// 「Press Enter」文字
	if (retryFont_) {
		retryFont_->Draw(worldTransformPushTo_, *camera_);
	}

	// 3Dモデル描画終了
	AlmaModel::PostDraw();

	// フェード描画（最後に）
	if (fade_) {
		fade_->Draw();
	}
}

AlmaClearScene::~AlmaClearScene(){delete fade_;delete camera_;delete modelSkyDome_;delete clearFont_;delete retryFont_;}
