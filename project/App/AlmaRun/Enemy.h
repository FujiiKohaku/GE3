#pragma once
#define NOMINMAX
#include "KamataEngine.h"
#include "Math.h"

class AlmaPlayer;
class AlmaMapChipField; // 前方宣言

class AlmaEnemy {
public:
	// 初期化
	void Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera, const KamataEngine::AlmaVector3& position, AlmaMapChipField* mapChipField);

	// 更新
	void UpDate();

	// 描画
	void Draw();

	// 当たり判定
	AlmaAABB GetAABB();
	KamataEngine::AlmaVector3 GetWorldPosition();

	// プレイヤー衝突時
	void OnCollision(const AlmaPlayer* player);

	// 生存判定
	bool IsDead() const { return isDead_; }
	bool IsDefeated() const;

	// 丸まり体当たりで吹っ飛ばされる
	void Smash();

private:
	KamataEngine::AlmaWorldTransform worldTransform_;
	KamataEngine::AlmaModel* model_ = nullptr;
	KamataEngine::AlmaCamera* camera_ = nullptr;

	AlmaMapChipField* mapChipField_ = nullptr; 

	// 歩行速度
	static inline const float kWalkSpeed = 0.01f;

	// サイズ
	static inline const float kWidth = 0.8f;
	static inline const float kHeight = 0.8f;

	// 速度
	KamataEngine::AlmaVector3 velocity_ = {};

	// 行動状態
	enum class AlmaBehavior { kNormal, kDefeated };
	AlmaBehavior behavior_ = AlmaBehavior::kNormal;

	// 管理フラグ
	bool isDead_ = false;
	bool isCollisionDisabled_ = false;

	// デス演出用
	int deathTimer_ = 0;

	// 吹っ飛び演出
	bool smashed_ = false;
	KamataEngine::AlmaVector3 knockVelocity_ = {};

	// 色（危険＝赤みの脈動 / 撃破時は白フラッシュ）
	KamataEngine::AlmaObjectColor color_;
	float colorTime_ = 0.0f;
};
