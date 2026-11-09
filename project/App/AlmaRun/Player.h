#pragma once
#include "DustParticles.h"
#include "SparkParticles.h"
#include "Enemy2.h"
#include "KamataEngine.h"
#include "MapChipField.h"
#include "Math.h"
#include <algorithm>
#include <array>
#include <numbers>
#include <list>
#include <memory>
// 前方宣言
class AlmaMapChipField;
class AlmaEnemy; // 02_10 21枚目

//==================================================
// プレイヤークラス
//==================================================
class AlmaPlayer {
public:
	// コンストラクタ / デストラクタ
	AlmaPlayer();
	~AlmaPlayer();

	// 初期化
	void Initialize(KamataEngine::AlmaModel* modelNormal, KamataEngine::AlmaModel* modelRolling, KamataEngine::AlmaCamera* camera, const KamataEngine::AlmaVector3& position);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	void CheckSpringCollision(const std::vector<std::vector<AlmaWorldTransform*>>& springs);
	// 火の当たり判定
	void CheckFireCollision(const std::vector<std::vector<AlmaWorldTransform*>>& fires);

	//==================================================
	// Getter / Setter
	//==================================================
	// ワールド変換を返す（描画などに利用）
	const KamataEngine::AlmaWorldTransform& GetWorldTransform() const { return worldTransform_; }

	// 現在の速度を返す
	const KamataEngine::AlmaVector3& GetVelocity() const { return velocity_; }

	// マップチップフィールドを外部から設定
	void SetMapChipField(AlmaMapChipField* mapChipField) { mapChipField_ = mapChipField; }

	// ワールド座標を取得（02_10 10枚目）
	AlmaVector3 GetWorldPosition();

	// AABBを取得（02_10 13枚目）
	AlmaAABB GetAABB();

	// 敵との衝突応答（02_10 21枚目）

	void OnCollision(const AlmaEnemy* enemy);
	void OnCollision(const AlmaEnemy2* enemy2);
	AlmaVector3 GetPosition() const { return worldTransform_.translation_; }
	//==================================================
	// 入力処理
	//==================================================
	// 移動入力処理（AL3_02_07 スライド10枚目）
	void InputMove();

	// キャラクター矩形の角を示す列挙型（02_07 スライド16枚目）
	enum Corner {
		kRightBottom, // 右下
		kLeftBottom,  // 左下
		kRightTop,    // 右上
		kLeftTop,     // 左上
		kNumCorner    // 要素数
	};
	struct AlmaCollisionMapInfo {
		bool isHitCeiling = false;
		bool isHitLanding = false;
		bool isHitWall = false;
		AlmaVector3 move;
	};
	enum class AlmaPlayerState { Normal, Rolling };
	AlmaPlayerState state_ = AlmaPlayerState::Normal;
	bool IsDead() const { return isDead_; }

	// HPと無敵関連
	float hp_ = 100.0f;
	const float kMaxHp = 100.0f;
	bool isInvincible_ = false;
	float invincibleTimer_ = 0.0f;
	const float kInvincibleDuration = 1.5f;

	void ApplyDamage(float damage);
	float GetHp() const { return hp_; }
	float GetMaxHp() const { return kMaxHp; }
	bool IsInvincible() const { return isInvincible_; }

	//==================================================
	// 追加ゲーム性：速度・丸まり・スーパー・イベント
	//==================================================
	// 基本走行速度（距離に応じて AlmaGameScene が上げていく）
	void SetBaseSpeed(float speed) { baseSpeed_ = speed; }
	// ダッシュパネルなどによる一時加速
	void ApplyBoost(float amount);
	bool IsRolling() const { return state_ == AlmaPlayerState::Rolling; }
	bool IsOnGround() const { return onGround_; }
	float GetCurrentSpeed() const { return velocity_.x; }
	void Heal(float amount);

	// 1フレーム内に起きた出来事（AlmaGameScene が演出に使う）
	bool TookDamage() const { return damagedThisFrame_; }
	bool JustLanded() const { return justLanded_; }
	float GetLandingSpeed() const { return landingSpeed_; }
	const std::vector<KamataEngine::AlmaVector3>& GetBrokenBlocks() const { return brokenBlocks_; }

private:
	//==================================================
	// 内部関数
	//==================================================
	// マップとの衝突判定処理（AL3_02_07 p13）
	void CheckMapCollision(AlmaCollisionMapInfo& info);
	void CheckMapCollisionUp(AlmaCollisionMapInfo& info);
	void CheckMapCollisionDown(AlmaCollisionMapInfo& info);
	void CheckMapCollisionRight(AlmaCollisionMapInfo& info);
	void CheckMapCollisionLeft(AlmaCollisionMapInfo& info);

	// キャラクター矩形の角座標を算出（AL3_02_07 page17）
	AlmaVector3 CornerPosition(const AlmaVector3& center, Corner corner);

	// 接地状態の更新（02_08 page14）
	void UpdateOnGround(const AlmaCollisionMapInfo& info);

	// 壁衝突時の処理（02_08 page27）
	void UpdateOnWall(const AlmaCollisionMapInfo& info);

	void UpdateState();

	//==================================================
	// メンバ変数
	//==================================================
	// AlmaTransform / Rendering
	KamataEngine::AlmaWorldTransform worldTransform_;
	KamataEngine::AlmaModel* model_ = nullptr;
	KamataEngine::AlmaModel* modelRollling_ = nullptr;
	uint32_t textureHandle_ = 0u;
	KamataEngine::AlmaCamera* camera_ = nullptr;

	// 物理
	KamataEngine::AlmaVector3 velocity_ = {};  // 現在の速度
	bool onGround_ = true;                 // 接地フラグ
	AlmaMapChipField* mapChipField_ = nullptr; // マップフィールド

	//==================================================
	// キャラ挙動用パラメータ
	//==================================================
	// 移動パラメータ
	static inline float kAccelaration_ = 0.01f; // 加速度
	static inline float kAttenuation = 0.05f;   // 減衰率
	static inline float kLimitRunSpeed = 0.2f;  // 最大走行速度

	// ジャンプ / 重力
	static inline const float kGravityAcceleration = 0.98f; // 重力加速度
	static inline const float kLimitFallSpeed = 0.5f;       // 最大落下速度
	static inline const float kJumpAcceleration = 20.0f;    // ジャンプ初速

	// プレイヤー
	float kWidth = 0.8f;                      // 横幅
	float kHeight = 1.5f;                     // 高さ
	static inline const float kBlank = 0.04f; // 判定の余白

	// 接地 / 壁パラメータ
	static inline const float kAttenuationLanding = 0.0f;  // 着地時減衰
	static inline const float kAttenuationWall = 0.2f;     // 壁接触時減衰
	static inline const float kGroundSearchHeight = 0.06f; // 微小高さ

	// 旋回関連
	enum class AlmaLRDorection { kRight, kLeft };
	AlmaLRDorection lrDirection_ = AlmaLRDorection::kRight;
	float turnFirstRotationY_ = 0.0f;           // 旋回開始角度
	float turnTimer_ = 0.0f;                    // 旋回タイマー
	static inline const float kTimeTrun = 0.3f; // 旋回にかける時間（秒）

	bool isDead_ = false;

	AlmaDustParticles dust_;

	AlmaModel* dustModel_ = nullptr;

	float dustEmitTimer_;
	const float kJumpInitialVelocity = 0.25f; // 最低ジャンプ高さ
	const float kJumpBoost = 0.35f;           // 長押し補助の強さ

	int jumpCount_ = 0;          // 今何回ジャンプしたか
	const int kMaxJumpCount = 2; // 最大ジャンプ回数（二段ジャンプなら2）

	uint32_t soundHandleDeath = 0;

	uint32_t soundHandleSpring = 0;

	uint32_t soundHandleChange = 0;

	// ===== 演出追加要素 =====
	// 着地煙
	AlmaDustParticles landDust_;

	// 摩擦火花
	AlmaSparkParticles spark_;
	float sparkEmitTimer_ = 0.0f;

	// 残像エフェクト
	static inline const int kMaxTrails = 5;
	struct AlmaTrail {
		KamataEngine::AlmaVector3 translation;
		KamataEngine::AlmaVector3 rotation;
		KamataEngine::AlmaVector3 scale;
		AlmaPlayerState state;
		float alpha;
	};
	std::list<AlmaTrail> trails_;
	std::array<KamataEngine::AlmaWorldTransform, kMaxTrails> trailTransforms_;
	std::vector<std::unique_ptr<KamataEngine::AlmaObjectColor>> trailColors_;
	float trailEmitTimer_ = 0.0f;

	// ===== 追加ゲーム性 =====
	float baseSpeed_ = 0.2f;   // 基本走行速度
	float boostSpeed_ = 0.0f;  // 一時加速（徐々に減衰）
	static inline const float kRollSpeedMultiplier = 1.2f;  // 丸まり中の速度倍率
	static inline const float kRollJumpMultiplier = 0.82f;  // 丸まり中はジャンプが低い
	static inline const float kMaxRunSpeed = 0.9f;          // すり抜け防止の上限（1マス未満）

	// 1フレームのイベント
	bool damagedThisFrame_ = false;
	bool justLanded_ = false;
	float landingSpeed_ = 0.0f;
	std::vector<KamataEngine::AlmaVector3> brokenBlocks_;

	// ヒビ割れブロックの縦一列を壊す
	void BreakColumn(uint32_t xIndex);

	// ===== 見た目 =====
	KamataEngine::AlmaVector3 visualScale_ = {1.0f, 1.0f, 1.0f}; // 伸び縮み（スクワッシュ＆ストレッチ）
	float rollAngle_ = 0.0f;                                  // 転がり回転
};
