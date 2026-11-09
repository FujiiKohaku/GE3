#pragma once
#define NOMINMAX
#include "KamataEngine.h"
#include "Math.h"
#include "Player.h"
#include <Windows.h>

class AlmaPlayer; // 前方宣言

class AlmaCameraController {
public:
	// 初期化
	void Initialize(KamataEngine::AlmaCamera* camera);

	// 毎フレーム更新（追従処理）
	void Update();

	// ターゲット（プレイヤー）を登録
	void SetTarget(AlmaPlayer* target) { target_ = target; }

	// 位置をリセット（ターゲットに即追従）
	void Reset();

	// 矩形（カメラの移動できる範囲）
	struct AlmaRect {
		float left = 0.0f;   // Xの最小
		float right = 1.0f;  // Xの最大
		float bottom = 0.0f; // Yの最小
		float top = 1.0f;    // Yの最大
	};

	// 移動可能範囲を設定
	void SetMovableArea(const AlmaRect& area) { movableArea_ = area; }

	// 画面揺れ（シェイク）を開始
	void StartShake(float duration, float intensity);

private:
	// カメラ本体
	KamataEngine::AlmaCamera* camera_ = nullptr;

	// 追従対象（プレイヤー）
	AlmaPlayer* target_ = nullptr;

	// プレイヤーからの相対位置（後ろに15.0f下がった位置がデフォルト）
	KamataEngine::AlmaVector3 targetOffset_ = {0.0f, 3.0f, -15.0f};

	// 移動可能範囲（デフォルト: 0〜100）
	// → 実際は AlmaGameScene 側で設定するので初期値は保険
	AlmaRect movableArea_ = {0, 100, 0, 100};

	// 目的地（補間でゆったり追従するために使う）
	KamataEngine::AlmaVector3 destination_;

	// 補間率（0.1f = 毎フレーム10%だけ目的地に近づく）
	static inline const float kinterpolationRate = 0.1f;

	// 速度のバイアス（プレイヤーの進行方向に先行してカメラを動かす量）
	static inline const float kVelocityBias = 20.0f;

	// プレイヤーを画面内に収めるためのマージン
	// → 左右: ±9.0f, 上下: ±5.0f
	static inline const AlmaRect targetMargin = {-9.0f, 9.0f, -5.0f, 5.0f};

	// 画面揺れ用変数
	float shakeTimer_ = 0.0f;
	float shakeDuration_ = 0.0f;
	float shakeIntensity_ = 0.0f;
	KamataEngine::AlmaVector3 shakeOffset_ = {0.0f, 0.0f, 0.0f};

	// 横方向の先読み量（速度に応じてなめらかに変化）
	float lead_ = 0.0f;
};
