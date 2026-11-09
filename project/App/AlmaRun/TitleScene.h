#pragma once
#include "Fade.h"
#include "KamataEngine.h"
using namespace KamataEngine;

//--------------------------------------------------
// タイトルシーン
// 02_12 スライド 19枚目
//--------------------------------------------------
class AlmaTitleScene {
public:
	~AlmaTitleScene();

	// ライフサイクル
	void Initialize();
	void Update();
	void Draw();

	// シーンが終了したか
	bool IsFinished() const { return finished_; }

	// シーンのフェーズ
	// 02_12 スライド 27枚目
	enum class AlmaPhase {
		kFadeIn,  // フェードイン
		kMain,    // メイン
		kFadeOut, // フェードアウト
	};

private:
	//--------------------------------------------------
	// 定数
	//--------------------------------------------------
	static inline const float kTimeTitleMove = 2.0f;

	//--------------------------------------------------
	// カメラ・ワールド変換
	//--------------------------------------------------
	AlmaCamera camera_;
	AlmaWorldTransform worldTransformTitle_;     // タイトルロゴ
	AlmaWorldTransform worldTransformPlayer_;    // プレイヤー
	AlmaWorldTransform worldTransformSun_;       // 太陽
	AlmaWorldTransform worldTransformBack_;      // 背景
	AlmaWorldTransform worldTransformPushSpace_; // 「Push Space」

	//--------------------------------------------------
	// モデル・テクスチャ
	//--------------------------------------------------
	AlmaModel* modelPlayer_ = nullptr;
	AlmaModel* modelTitle_ = nullptr;
	AlmaModel* sun_ = nullptr;
	AlmaModel* backGround_ = nullptr;
	AlmaModel* pushSpace_ = nullptr;
	uint32_t BackGroundTexture_ = 0;

	//--------------------------------------------------
	// フェード・演出
	//--------------------------------------------------
	AlmaFade* fade_ = nullptr;
	AlmaPhase phase_ = AlmaPhase::kFadeIn;
	float counter_ = 0.0f; // 汎用タイマー
	float angle_ = 0.0f;   // 演出回転用

	//--------------------------------------------------
	// 状態管理
	//--------------------------------------------------
	bool finished_ = false;

	//--------------------------------------------------
	// サウンド
	//--------------------------------------------------
	uint32_t soundHandleSelecct = 0;

	//--------------------------------------------------
	// ベスト記録（セーブデータから読み込み）
	//--------------------------------------------------
	int bestDistance_ = 0;
	int highScore_ = 0;
};
