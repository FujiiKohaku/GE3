#pragma once
#define NOMINMAX
#include "KamataEngine.h"
#include "Math.h"

class AlmaPlayer;
class AlmaMapChipField;

class AlmaEnemy2 {
public:
	void Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera, const KamataEngine::AlmaVector3& position, AlmaMapChipField* mapChipField);

	void Update();
	void Draw();

	AlmaAABB GetAABB();
	KamataEngine::AlmaVector3 GetWorldPosition();

	void OnCollision(const AlmaPlayer* player);

	bool IsDead() const { return isDead_; }
	bool IsDefeated() const;

	// 丸まり体当たりで吹っ飛ばされる
	void Smash();

private:
	KamataEngine::AlmaWorldTransform worldTransform_;
	KamataEngine::AlmaModel* model_ = nullptr;
	KamataEngine::AlmaCamera* camera_ = nullptr;
	AlmaMapChipField* mapChipField_ = nullptr;

	static inline const float kMoveSpeed = 0.02f;
	static inline const float kWidth = 0.8f;
	static inline const float kHeight = 0.8f;

	KamataEngine::AlmaVector3 velocity_ = {};

	enum class AlmaBehavior { kNormal, kDefeated };
	AlmaBehavior behavior_ = AlmaBehavior::kNormal;

	bool isDead_ = false;
	bool isCollisionDisabled_ = false;
	int deathTimer_ = 0;

	// 吹っ飛び演出
	bool smashed_ = false;
	KamataEngine::AlmaVector3 knockVelocity_ = {};

	// 色（危険＝赤みの脈動 / 撃破時は白フラッシュ）
	KamataEngine::AlmaObjectColor color_;
	float colorTime_ = 0.0f;
};
