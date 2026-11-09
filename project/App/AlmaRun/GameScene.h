#pragma once
#include "Background.h"
#include "BurstParticles.h"
#include "CameraController.h"
#include "DeathParticles.h"
#include "Enemy.h"
#include "Enemy2.h"
#include "Fade.h"
#include "FallingSpike.h"
#include "GameHUD.h"
#include "HitEffect.h"
#include "KamataEngine.h"
#include "MapChipField.h"
#include "Math.h"
#include "Player.h"
#include "RisingSpike.h"
#include "ScoreManager.h"
#include "Skydome.h"
#include <array>
#include <list>
#include <set>
#include <string>
#include <vector>

//--------------------------------------------------
// ゲームシーン
//--------------------------------------------------
class AlmaGameScene {
public:
	~AlmaGameScene();

	// ===== ライフサイクル =====
	void Initialize(); // 初期化
	void Update();     // 更新
	void Draw();       // 描画

	// ===== 補助処理 =====
	void UpdateDynamicBlocks(); // 動的にブロックを生成・破棄する
	void CheckAllCollisions();  // 全衝突判定

	// ===== 状態確認 =====
	bool IsFinished() const { return finished_; }
	bool IsClear() const { return isClear_; }
	bool IsReturnToTitle() const { return returnToTitle_; }

private:
	//--------------------------------------------------
	// ブロック1マス分の描画情報（本体＋草の天面＋奥行きの列）
	//--------------------------------------------------
	struct AlmaBlockCell {
		KamataEngine::AlmaWorldTransform main;
		KamataEngine::AlmaWorldTransform top;
		std::array<KamataEngine::AlmaWorldTransform, 2> back;
		std::array<KamataEngine::AlmaWorldTransform, 2> backTop;
		bool hasTop = false;
		bool hasBack = false;
		int biome = 0;
	};

	// ニアミス判定の候補
	struct AlmaNearMissCandidate {
		int key = 0;
		float rightX = 0.0f;
		int damageStamp = 0;
	};

	//--------------------------------------------------
	// 内部処理
	//--------------------------------------------------
	void ChangePhese();
	void UpdatePlay();
	void UpdateSpeedLines();
	void UpdateObjectsVisual();
	void UpdateHudState();
	void CheckEnemyCollisions(const AlmaAABB& playerAABB);
	void CheckCoinCollisions(const AlmaAABB& playerAABB);
	void CheckBoostPads(const AlmaAABB& playerAABB);
	void CheckNearMiss();
	void CheckSlide();
	void HandleBrokenBlocks();
	void HandleDamageFeedback();
	void OnEnemySmashed(const KamataEngine::AlmaVector3& position);
	void AwardPopup(const std::string& label, int base, const KamataEngine::AlmaVector3& worldPos, const KamataEngine::AlmaVector4& color);
	void StartHitstop(int frames) { hitstopFrames_ = (std::max)(hitstopFrames_, frames); }
	KamataEngine::AlmaVector2 ToScreen(const KamataEngine::AlmaVector3& p) const;
	AlmaBlockCell* CreateBlockCell(uint32_t x, uint32_t y);
	void PurgeColumn(uint32_t x);
	float GetDistance() const;

	//--------------------------------------------------
	// リソース（モデル / テクスチャ）
	//--------------------------------------------------
	KamataEngine::AlmaModel* model_ = nullptr;
	KamataEngine::AlmaModel* modelRolling = nullptr;
	KamataEngine::AlmaModel* modelBlock_ = nullptr;
	KamataEngine::AlmaModel* enemyModel_ = nullptr;
	KamataEngine::AlmaModel* enemyModel2_ = nullptr;
	KamataEngine::AlmaModel* dethParticleModel = nullptr;
	KamataEngine::AlmaModel* spikeModel_ = nullptr;
	KamataEngine::AlmaModel* springModel_ = nullptr;
	KamataEngine::AlmaModel* fireModel_ = nullptr;
	KamataEngine::AlmaModel* coinModel_ = nullptr;
	KamataEngine::AlmaModel* particleModel_ = nullptr;
	KamataEngine::AlmaModel* hitEffectModel_ = nullptr;

	// READY / GO!! 演出モデル
	KamataEngine::AlmaModel* readyModel_ = nullptr;
	KamataEngine::AlmaModel* goModel_ = nullptr;

	// テクスチャ
	std::array<uint32_t, Biome::kCount> blockBodyTex_{};
	std::array<uint32_t, Biome::kCount> blockTopTex_{};
	uint32_t breakableTex_ = 0;
	uint32_t boostTex_ = 0;
	uint32_t textureHandlePose_ = 0;
	uint32_t soundhandleGo_ = 0;
	uint32_t soundHandleCoin_ = 0;
	uint32_t soundHandleSmash_ = 0;
	uint32_t soundHandleBreak_ = 0;

	// 色（環境色 × 意味のある色）
	KamataEngine::AlmaObjectColor colorFront_;
	KamataEngine::AlmaObjectColor colorTop_;
	std::array<KamataEngine::AlmaObjectColor, 2> colorBack_;
	KamataEngine::AlmaObjectColor colorBreakable_;
	KamataEngine::AlmaObjectColor colorBoost_;
	KamataEngine::AlmaObjectColor colorSpike_;
	KamataEngine::AlmaObjectColor colorSpring_;
	KamataEngine::AlmaObjectColor colorFire_;
	KamataEngine::AlmaObjectColor colorCoin_;

	//--------------------------------------------------
	// 変換行列 / カメラ
	//--------------------------------------------------
	KamataEngine::AlmaWorldTransform worldTransformReady_; // Ready用
	KamataEngine::AlmaWorldTransform worldTransformGo_;    // Go用

	KamataEngine::AlmaCamera* camera_ = nullptr;           // メインカメラ
	KamataEngine::AlmaDebugCamera* debugCamera_ = nullptr; // デバッグカメラ

	//--------------------------------------------------
	// ゲームオブジェクト
	//--------------------------------------------------
	AlmaPlayer* player_ = nullptr;

	std::list<AlmaEnemy*> enemies_;   // 敵1リスト
	std::list<AlmaEnemy2*> enemies2_; // 敵2リスト

	// マップチップ由来の配置オブジェクト [行][列]
	std::vector<std::vector<AlmaBlockCell*>> blockCells_;                        // ブロック
	std::vector<std::vector<KamataEngine::AlmaWorldTransform*>> worldTransformSpikes_;     // スパイク
	std::vector<std::vector<KamataEngine::AlmaWorldTransform*>> worldTransformSprings_;    // バネ
	std::vector<std::vector<KamataEngine::AlmaWorldTransform*>> worldTransformFires_;      // 火
	std::vector<std::vector<KamataEngine::AlmaWorldTransform*>> worldTransformCoins_;      // コイン
	std::vector<std::vector<KamataEngine::AlmaWorldTransform*>> worldTransformBreakables_; // ヒビ割れブロック
	std::vector<std::vector<KamataEngine::AlmaWorldTransform*>> worldTransformBoosts_;     // ダッシュパネル

	//--------------------------------------------------
	// マップ / カメラ
	//--------------------------------------------------
	AlmaMapChipField* mapChipField_ = nullptr;
	AlmaCameraController* cController_ = nullptr;
	bool isDebugCameraActive_ = false; // デバッグカメラON/OFF
	int generatedUntil_ = -1;          // ここまでの列はオブジェクト生成済み
	int purgedUntil_ = -1;             // ここまでの列は破棄済み

	//--------------------------------------------------
	// 演出系
	//--------------------------------------------------
	AlmaDeathParticles* deathParticles_ = nullptr; // デス演出
	std::list<AlmaHitEffect*> hitEffects_;         // ヒットエフェクト
	AlmaBurstParticles burst_;                     // 汎用パーティクル
	AlmaBackground background_;                    // 多重スクロール背景
	AlmaGameHUD hud_;                              // HUD
	AlmaHudState hudState_;
	AlmaScoreManager score_;                       // スコア・コンボ

	// READY-GO!! 管理
	float readyTimer_ = 0.0f;
	bool playedReadySound_ = false;

	// フェーズ管理
	enum class AlmaPhase {
		kFadeIn,
		kReady,   // READY-GO!! 演出
		kPlay,    // プレイ中
		kDeath,   // 死亡演出
		kFadeOut, // シーン終了
		kPause,
	};
	AlmaPhase phase_ = AlmaPhase::kFadeIn;

	// フェード
	AlmaFade* fade_ = nullptr;

	//--------------------------------------------------
	// 状態フラグ
	//--------------------------------------------------
	bool finished_ = false; // シーン終了フラグ
	bool isClear_ = false;  // ゴールクリアフラグ
	bool returnToTitle_ = false;

	// ===== 追加ゲーム性の状態 =====
	float totalTime_ = 0.0f;      // シーン経過時間
	float playTime_ = 0.0f;       // プレイ時間
	float fireTime_ = 0.0f;       // 火の上下運動用
	int speedLevel_ = 0;          // 距離で上がるスピードレベル
	int lastArea_ = 0;            // 直前のエリア番号
	int hitstopFrames_ = 0;       // ヒットストップ
	int damageStamp_ = 0;         // 被弾回数（ニアミス無効化用）
	float fov_ = 45.0f;           // 現在のFOV（度）
	bool sliding_ = false;        // 天井下スライディング中
	int slideDamageStamp_ = 0;
	int lastBoostColumn_ = -1;    // 同じパネルで連続加速しない
	float niceCooldown_ = 0.0f;   // NICE! の連続表示を防ぐ
	int windowStart_ = 0;         // 描画・更新する列範囲
	int windowEnd_ = -1;
	std::vector<AlmaNearMissCandidate> nearMissCandidates_;
	std::set<int> nearMissAwarded_;

	//--------------------------------------------------
	// UI（2Dスプライト）
	//--------------------------------------------------
	KamataEngine::AlmaSprite* spritePose_ = nullptr;

	// ===== スピードライン =====
	struct AlmaSpeedLine {
		KamataEngine::AlmaSprite* sprite = nullptr;
		KamataEngine::AlmaVector2 position;
		float speed;
		float width;
		float alpha;
		float hue;
	};
	std::list<AlmaSpeedLine> speedLines_;
	uint32_t speedLineTextureHandle_ = 0;

	// ===== 落下プレス機 / 飛び出すトゲ =====
	std::list<AlmaFallingSpike*> fallingSpikes_;
	std::list<AlmaRisingSpike*> risingSpikes_;
};
