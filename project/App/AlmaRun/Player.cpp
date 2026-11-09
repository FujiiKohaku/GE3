#define NOMINMAX
#include "Player.h"
#include "Enemy.h"
#include "FxUtil.h"
#include "externals/imgui/imgui.h"
#include <cassert>
#include <cmath>
using namespace KamataEngine;

//==================================================
// コンストラクタ・デストラクタ
//==================================================
AlmaPlayer::AlmaPlayer() {}
AlmaPlayer::~AlmaPlayer() { delete dustModel_; }

//==================================================
// 初期化処理
//==================================================
void AlmaPlayer::Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaModel* modelRollling, KamataEngine::AlmaCamera* camera, const KamataEngine::AlmaVector3& position) {
	assert(model);

	model_ = model;

	camera_ = camera;

	modelRollling_ = modelRollling;

	hp_ = kMaxHp;
	isInvincible_ = false;
	invincibleTimer_ = 0.0f;
	isDead_ = false;

	// ワールド変換の初期化
	worldTransform_.Initialize();
	worldTransform_.translation_ = position;

	// 初期向き：右向き
	worldTransform_.rotation_.y = std::numbers::pi_v<float> / 2.0f;

	dustModel_ = AlmaModel::CreateFromOBJ("particle", true);

	soundHandleDeath = AlmaAudio::GetInstance()->LoadWave("desu.wav");
	soundHandleSpring = AlmaAudio::GetInstance()->LoadWave("jumpbane.wav");
	soundHandleChange = AlmaAudio::GetInstance()->LoadWave("changeMode.wav");
	dust_.Initialize(dustModel_, camera_);

	// ===== 新規演出初期化 =====
	landDust_.Initialize(dustModel_, camera_);
	spark_.Initialize(dustModel_, camera_);

	trailColors_.clear();
	for (int i = 0; i < kMaxTrails; i++) {
		auto color = std::make_unique<AlmaObjectColor>();
		color->Initialize();
		trailColors_.push_back(std::move(color));
	}
	for (auto& transform : trailTransforms_) {
		transform.Initialize();
	}
	trails_.clear();
	trailEmitTimer_ = 0.0f;
	sparkEmitTimer_ = 0.0f;

	// ===== 追加ゲーム性の初期化 =====
	baseSpeed_ = kLimitRunSpeed;
	boostSpeed_ = 0.0f;
	visualScale_ = {1.0f, 1.0f, 1.0f};
	rollAngle_ = 0.0f;
	brokenBlocks_.clear();
}

#pragma region 入力処理ジャンプ関数化
//==================================================
// 入力処理（移動・ジャンプ）
//==================================================
void AlmaPlayer::InputMove() {
	// 自動で右へ走り続ける（距離で上がる基本速度 + 一時加速）
	float speed = baseSpeed_ + boostSpeed_;
	if (state_ == AlmaPlayerState::Rolling) {
		speed *= kRollSpeedMultiplier; // 丸まると転がって加速！
	}
	velocity_.x = std::min(speed, kMaxRunSpeed);
	boostSpeed_ = std::max(0.0f, boostSpeed_ - 0.0025f);

	// ===== ジャンプ処理（二段ジャンプ対応、上矢印キー） =====
	if (AlmaInput::GetInstance()->TriggerKey(DIK_UP)) {
		if (onGround_ || jumpCount_ < kMaxJumpCount) {
			// 丸まり中はジャンプが低い（リスクとリターン）
			float jumpScale = (state_ == AlmaPlayerState::Rolling) ? kRollJumpMultiplier : 1.0f;
			velocity_.y = kJumpInitialVelocity * jumpScale; // 初速を与える
			onGround_ = false;
			jumpCount_++;
			// 伸び（ストレッチ）
			visualScale_ = {0.78f, 1.28f, 0.78f};
		}
	}

	//  ジャンプ中の長押し補助（上昇中だけ、上矢印キー）
	if (AlmaInput::GetInstance()->PushKey(DIK_UP) && velocity_.y > 0) {
		velocity_.y += kJumpBoost / 60.0f;
	}

	//  重力を加える
	velocity_.y -= kGravityAcceleration / 60.0f;
	velocity_.y = std::max(velocity_.y, -kLimitFallSpeed); // 落下速度制限

	// ===== 地面に着いたらジャンプ回数リセット =====
	if (onGround_) {
		jumpCount_ = 0;
	}
}

#pragma endregion

//==================================================
// 四隅の座標を返す
//==================================================
AlmaVector3 AlmaPlayer::CornerPosition(const AlmaVector3& center, Corner corner) {
	AlmaVector3 offsetTable[kNumCorner] = {
	    {+kWidth / 2.0f, -kHeight / 2.0f, 0}, // 右下
	    {-kWidth / 2.0f, -kHeight / 2.0f, 0}, // 左下
	    {+kWidth / 2.0f, +kHeight / 2.0f, 0}, // 右上
	    {-kWidth / 2.0f, +kHeight / 2.0f, 0}  // 左上
	};
	return center + offsetTable[static_cast<uint32_t>(corner)];
}

#pragma region プレイヤーの当たり判定まとめ
//==================================================
// マップ衝突判定の統合呼び出し
//==================================================
void AlmaPlayer::CheckMapCollision(AlmaCollisionMapInfo& info) {
	CheckMapCollisionUp(info);
	CheckMapCollisionDown(info);
	CheckMapCollisionRight(info);
	CheckMapCollisionLeft(info);
}
#pragma endregion

#pragma region 天井
//--------------------------------------------------
// 天井との衝突判定
//--------------------------------------------------
void AlmaPlayer::CheckMapCollisionUp(AlmaCollisionMapInfo& info) {
	if (info.move.y <= 0)
		return; // 上昇してなければ不要

	// 四隅の座標
	std::array<AlmaVector3, kNumCorner> positionNew;
	for (uint32_t i = 0; i < positionNew.size(); ++i) {
		positionNew[i] = CornerPosition(worldTransform_.translation_ + info.move, static_cast<Corner>(i));
	}

	bool hit = false;
	AlmaMapChipType mapChipType, mapChipTypeNext;
	AlmaMapChipField::AlmaIndexSet indexSet;

	// 左上
	indexSet = mapChipField_->GetMapChipIndexSetByposition(positionNew[kLeftTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex + 1);
	if (IsSolidChip(mapChipType) && !IsSolidChip(mapChipTypeNext))
		hit = true;

	// 右上
	indexSet = mapChipField_->GetMapChipIndexSetByposition(positionNew[kRightTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex + 1);
	if (IsSolidChip(mapChipType) && !IsSolidChip(mapChipTypeNext))
		hit = true;

	if (hit) {
		AlmaVector3 playerTopPos = worldTransform_.translation_ + AlmaVector3(0, +kHeight / 2.0f, 0);
		indexSet = mapChipField_->GetMapChipIndexSetByposition(playerTopPos);
		AlmaMapChipField::AlmaRect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);

		info.move.y = std::max(0.0f, rect.bottom - worldTransform_.translation_.y - (kHeight / 2.0f + kBlank));
		info.isHitCeiling = true;
	}
}
#pragma endregion

#pragma region 床との衝突判定
//--------------------------------------------------
// 床との衝突判定
//--------------------------------------------------
void AlmaPlayer::CheckMapCollisionDown(AlmaCollisionMapInfo& info) {
	if (info.move.y >= 0)
		return; // 下降してなければ不要

	std::array<AlmaVector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + info.move, static_cast<Corner>(i));
	}

	bool hit = false;
	float highestFloorTop = -1000.0f;
	AlmaMapChipField::AlmaIndexSet bestIndexSet = {};

	// 左下と右下をチェック
	for (Corner corner : {kLeftBottom, kRightBottom}) {
		auto indexSetTemp = mapChipField_->GetMapChipIndexSetByposition(positionsNew[corner]);

		// そのブロックの天面高さをチェック
		AlmaVector3 blockPos = mapChipField_->GetMapChipPositionByIndex(indexSetTemp.xIndex, indexSetTemp.yIndex);
		float blockTopY = blockPos.y + AlmaMapChipField::kBlockHeight / 2.0f;
		float playerBottomY = worldTransform_.translation_.y - kHeight / 2.0f;

		// ブロックの天面が、プレイヤーの足元よりも明らかに高いなら、それは床ではなく「壁」なので無視する
		if (blockTopY > playerBottomY + 0.2f) {
			continue;
		}

		AlmaMapChipType mapChip = mapChipField_->GetMapChipTypeByIndex(indexSetTemp.xIndex, indexSetTemp.yIndex);

		// トゲならダメージ（無敵中はスルー）
		if (mapChip == AlmaMapChipType::kSpike) {
			if (!isInvincible_) {
				ApplyDamage(30.0f);
			}
			return;
		}

		// ブロック判定
		if (IsSolidChip(mapChip)) {
			hit = true;

			// 衝突したブロックの中から、最も高い床を探す
			int xIndex = indexSetTemp.xIndex;
			int yIndex = indexSetTemp.yIndex;

			while (yIndex < 20) {
				AlmaVector3 tempPos = mapChipField_->GetMapChipPositionByIndex(xIndex, yIndex);
				float tempTopY = tempPos.y + AlmaMapChipField::kBlockHeight / 2.0f;
				AlmaMapChipType type = mapChipField_->GetMapChipTypeByIndex(xIndex, yIndex);

				if (IsSolidChip(type) && tempTopY <= playerBottomY + 0.2f) {
					if (tempTopY > highestFloorTop) {
						highestFloorTop = tempTopY;
						bestIndexSet = { (uint32_t)xIndex, (uint32_t)yIndex };
					}
					break;
				}
				yIndex++;
			}
		}
	}

	if (hit && highestFloorTop > -900.0f) {
		AlmaMapChipField::AlmaRect rect = mapChipField_->GetRectByIndex(bestIndexSet.xIndex, bestIndexSet.yIndex);
		info.move.y = std::min(0.0f, rect.top - worldTransform_.translation_.y + (kHeight / 2.0f + kBlank));
		info.isHitLanding = true;
	}
}
#pragma endregion

#pragma region 右側の衝突判定
//--------------------------------------------------
// 右側との衝突判定
//--------------------------------------------------
void AlmaPlayer::CheckMapCollisionRight(AlmaCollisionMapInfo& info) {
	if (info.move.x <= 0)
		return;

	std::array<AlmaVector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + info.move, static_cast<Corner>(i));
	}

	bool hit = false;
	AlmaMapChipType mapChip = AlmaMapChipType::kBlank;
	for (Corner corner : {kRightTop, kRightBottom}) {
		auto index = mapChipField_->GetMapChipIndexSetByposition(positionsNew[corner]);
		mapChip = mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex);

		// ===== ヒビ割れブロック：丸まり体当たりなら粉砕 =====
		if (mapChip == AlmaMapChipType::kBreakable && state_ == AlmaPlayerState::Rolling) {
			BreakColumn(index.xIndex);
			continue;
		}

		// ===== ブロックなら通常の壁衝突 =====
		if (IsSolidChip(mapChip)) {
			if (!isInvincible_) {
				ApplyDamage(20.0f); // 壁激突ダメージ
			}

			if (isInvincible_) {
				// 無敵中ならすり抜ける（当たりにしない）
			} else {
				hit = true;
				break;
			}
		}

		// ===== トゲならダメージ =====
		if (mapChip == AlmaMapChipType::kSpike) {
			if (!isInvincible_) {
				ApplyDamage(30.0f);
			}
			return;
		}
	}

	if (hit) {
		AlmaVector3 playerRightPosNext = worldTransform_.translation_ + info.move + AlmaVector3(+kWidth / 2.0f, 0, 0);
		auto index = mapChipField_->GetMapChipIndexSetByposition(playerRightPosNext);
		auto rect = mapChipField_->GetRectByIndex(index.xIndex, index.yIndex);

		info.move.x = std::max(0.0f, rect.left - worldTransform_.translation_.x - (kWidth / 2.0f + kBlank));
		info.isHitWall = true;
	}
}

#pragma endregion

#pragma region 左側の衝突判定
//--------------------------------------------------
// 左側との衝突判定
//--------------------------------------------------

//--------------------------------------------------
// 左側との衝突判定
//--------------------------------------------------
void AlmaPlayer::CheckMapCollisionLeft(AlmaCollisionMapInfo& info) {
	if (info.move.x >= 0)
		return;

	std::array<AlmaVector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + info.move, static_cast<Corner>(i));
	}

	bool hit = false;
	for (Corner corner : {kLeftTop, kLeftBottom}) {
		auto index = mapChipField_->GetMapChipIndexSetByposition(positionsNew[corner]);
		auto mapChip = mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex);

		// ===== ブロックなら衝突扱い =====
		if (IsSolidChip(mapChip)) {
			if (!isInvincible_) {
				ApplyDamage(20.0f);
			}

			if (isInvincible_) {
				// 無敵中ならすり抜ける
			} else {
				hit = true;
			}
		}

		// ===== トゲならダメージ =====
		if (mapChip == AlmaMapChipType::kSpike) {
			if (!isInvincible_) {
				ApplyDamage(30.0f);
			}
			return;
		}
	}

	if (hit) {
		AlmaVector3 leftPosAfterMove = worldTransform_.translation_ + info.move + AlmaVector3(-kWidth / 2.0f, 0, 0);
		auto index = mapChipField_->GetMapChipIndexSetByposition(leftPosAfterMove);
		auto rect = mapChipField_->GetRectByIndex(index.xIndex, index.yIndex);

		float overlap = leftPosAfterMove.x - rect.right;
		if (overlap < 0.0f) {
			info.move.x -= overlap + kBlank; // 微小な隙間を空ける
		}
		info.isHitWall = true;
	}
}

#pragma endregion

#pragma region 接地状態の更新
//==================================================
// 接地状態の更新
//==================================================
void AlmaPlayer::UpdateOnGround(const AlmaCollisionMapInfo& info) {
	if (onGround_) {
		if (velocity_.y > 0.0f) {
			onGround_ = false; // ジャンプ開始
		} else {
			// 落下判定
			std::array<AlmaVector3, kNumCorner> positionNew;
			for (uint32_t i = 0; i < positionNew.size(); ++i) {
				positionNew[i] = CornerPosition(worldTransform_.translation_ + info.move, static_cast<Corner>(i));
			}

			bool hit = false;
			for (Corner corner : {kLeftBottom, kRightBottom}) {
				auto index = mapChipField_->GetMapChipIndexSetByposition(positionNew[corner] + AlmaVector3(0, -kGroundSearchHeight, 0));
				auto mapChip = mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex);
				if (IsSolidChip(mapChip))
					hit = true;
			}
			if (!hit)
				onGround_ = false; // 落下開始
		}
	} else {
		if (info.isHitLanding) {
			onGround_ = true; // 着地
			velocity_.x *= (1.0f - kAttenuationLanding);
			// 着地イベント（落下速度に応じて演出の強さを変える）
			justLanded_ = true;
			landingSpeed_ = -velocity_.y;
			float squash = std::clamp(landingSpeed_ / kLimitFallSpeed, 0.3f, 1.0f);
			visualScale_ = {1.0f + 0.35f * squash, 1.0f - 0.3f * squash, 1.0f + 0.35f * squash};
			velocity_.y = 0.0f;

			// 着地煙を発生させる
			AlmaVector3 footPos = worldTransform_.translation_ + AlmaVector3(0.0f, -kHeight / 2.0f, 0.0f);
			landDust_.EmitLand(footPos);
		}
	}
}
#pragma endregion

#pragma region 壁衝突時
//==================================================
// 壁衝突時の処理
// 壁にぶつかったら横方向の速度をリセットする
//==================================================
void AlmaPlayer::UpdateOnWall(const AlmaCollisionMapInfo& info) {
	if (info.isHitWall) {
		// 無敵中なら壁すり抜けが有効なので速度リセットしない
		if (!isInvincible_) {
			velocity_.x = 0.0f;
		}
	}
}
#pragma endregion

//==================================================
// 更新処理
//==================================================
void AlmaPlayer::Update() {
	// ===== 1フレームのイベントをリセット =====
	damagedThisFrame_ = false;
	justLanded_ = false;
	brokenBlocks_.clear();

	//==================================================
	// 無敵タイマーの更新と安全措置
	//==================================================
	if (isInvincible_) {
		invincibleTimer_ -= 1.0f / 60.0f;
		if (invincibleTimer_ <= 0.0f) {
			// 安全措置：壁の中にめり込んでいる場合は、壁の外に脱出するまで無敵終了を保留
			bool inBlock = false;
			auto index = mapChipField_->GetMapChipIndexSetByposition(worldTransform_.translation_);
			if (IsSolidChip(mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex))) {
				inBlock = true;
			}

			if (!inBlock) {
				isInvincible_ = false;
			} else {
				invincibleTimer_ = 0.1f; // 0.1秒維持して次のフレームで再確認
			}
		}
	}

	//==================================================
	// 状態ごとの更新
	//==================================================
	InputMove();   // 移動・ジャンプ入力
	UpdateState(); // 状態遷移チェック（Normal⇔Rolling）

	//==================================================
	// パーティクル（砂ぼこり）
	//==================================================
	if (onGround_ && fabs(velocity_.x) > 0.1f) {
		dustEmitTimer_ += 1.0f / 60.0f; // 経過時間を加算
		if (dustEmitTimer_ >= 0.1f) {   // 0.1秒ごとに発生
			AlmaVector3 footPos = worldTransform_.translation_ + AlmaVector3(0, -kHeight / 2.0f, 0);
			dust_.Emit(footPos);
			dustEmitTimer_ = 0.0f;
		}
	} else {
		dustEmitTimer_ = 0.0f; // 停止時はリセット
	}
	dust_.Update();

	// ===== 着地煙の更新 =====
	landDust_.Update();

	// ===== スライディング摩擦火花の発生と更新 =====
	if (state_ == AlmaPlayerState::Rolling && onGround_ && fabs(velocity_.x) > 0.1f) {
		sparkEmitTimer_ += 1.0f / 60.0f;
		if (sparkEmitTimer_ >= 0.05f) { // 0.05秒ごとに発生
			AlmaVector3 footPos = worldTransform_.translation_ + AlmaVector3(0.0f, -kHeight / 2.0f, 0.0f);
			spark_.Emit(footPos);
			sparkEmitTimer_ = 0.0f;
		}
	} else {
		sparkEmitTimer_ = 0.0f;
	}
	spark_.Update();

	// ===== 残像の記録 =====
	bool shouldEmitTrail = false;
	if (state_ == AlmaPlayerState::Rolling) {
		shouldEmitTrail = true;
	} else if (!onGround_ && velocity_.y > 0.05f) {
		shouldEmitTrail = true;
	}

	trailEmitTimer_ += 1.0f / 60.0f;
	if (shouldEmitTrail && trailEmitTimer_ >= 0.06f) { // 約4フレームごと
		AlmaTrail newTrail;
		newTrail.translation = worldTransform_.translation_;
		newTrail.rotation = worldTransform_.rotation_;
		newTrail.scale = worldTransform_.scale_;

		newTrail.state = state_;
		newTrail.alpha = 0.6f; // 初期アルファ

		trails_.push_front(newTrail);
		if (trails_.size() > kMaxTrails) {
			trails_.pop_back();
		}
		trailEmitTimer_ = 0.0f;
	}

	// 残像のフェードアウトと削除
	for (auto it = trails_.begin(); it != trails_.end(); ) {
		it->alpha -= 0.03f; // 更新速度に合わせて減衰
		if (it->alpha <= 0.0f) {
			it = trails_.erase(it);
		} else {
			++it;
		}
	}

	//==================================================
	// 移動・衝突判定
	//==================================================
	AlmaCollisionMapInfo collisionMapInfo;
	collisionMapInfo.move = velocity_;

	CheckMapCollision(collisionMapInfo);                   // マップ衝突判定
	worldTransform_.translation_ += collisionMapInfo.move; // 移動反映

	if (collisionMapInfo.isHitCeiling) {
		velocity_.y = 0; // 天井に当たったらY速度リセット
	}

	UpdateOnGround(collisionMapInfo); // 接地判定
	UpdateOnWall(collisionMapInfo);   // 壁判定

	//==================================================
	// 旋回処理
	//==================================================
	if (turnTimer_ > 0.0f) {
		turnTimer_ = std::max(turnTimer_ - (1.0f / 60.0f), 0.0f);

		float destinationRotationYTable[] = {std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float> * 3.0f / 2.0f};
		float destinationRotationY = destinationRotationYTable[static_cast<uint32_t>(lrDirection_)];
		worldTransform_.rotation_.y = AlmaEaseInOut(turnFirstRotationY_, destinationRotationY, 1.0f - turnTimer_ / kTimeTrun);
	}

	//==================================================
	// 穴落下死亡判定
	//==================================================
	if (worldTransform_.translation_.y < -5.0f) { //
		uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleDeath);
		AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);
		isDead_ = true;
	}

	//==================================================
	// 見た目：転がり回転・伸び縮み
	//==================================================
	if (state_ == AlmaPlayerState::Rolling) {
		// 進んだ距離に合わせてボールのように回転（半径 0.4）
		rollAngle_ += velocity_.x / 0.4f;
		rollAngle_ = std::fmod(rollAngle_, std::numbers::pi_v<float> * 2.0f);
	} else {
		rollAngle_ = 0.0f;
	}
	worldTransform_.rotation_.x = rollAngle_;

	visualScale_.x += (1.0f - visualScale_.x) * 0.2f;
	visualScale_.y += (1.0f - visualScale_.y) * 0.2f;
	visualScale_.z += (1.0f - visualScale_.z) * 0.2f;
	worldTransform_.scale_ = visualScale_;

	//==================================================
	// 行列更新（つぶれた分だけ足元に合わせて下げる）
	//==================================================
	AlmaVector3 savedTranslation = worldTransform_.translation_;
	worldTransform_.translation_.y -= (1.0f - visualScale_.y) * kHeight / 2.0f;
	AlmaWorldTransformUpdate(worldTransform_);
	worldTransform_.translation_ = savedTranslation;
}

//==================================================
// 描画処理
//==================================================
void AlmaPlayer::Draw() {
	// 無敵時間中の点滅（0.06秒ごとに表示/非表示をトグル）※スーパー中は点滅しない
	bool visible = true;
	if (isInvincible_) {
		if (static_cast<int>(invincibleTimer_ * 15.0f) % 2 == 0) {
			visible = false; // 本体の描画をスキップ
		}
	}

	if (visible) {
		if (state_ == AlmaPlayerState::Rolling) {
			modelRollling_->Draw(worldTransform_, *camera_);
		} else {
			model_->Draw(worldTransform_, *camera_);
		}
	}

	// ===== 残像の描画 =====
	size_t colorIndex = 0;
	for (auto& trail : trails_) {
		if (colorIndex >= trailColors_.size())
			break;

		// 事前確保された AlmaWorldTransform にパラメータをコピーして更新
		auto& wt = trailTransforms_[colorIndex];
		wt.translation_ = trail.translation;
		wt.rotation_ = trail.rotation;
		wt.scale_ = trail.scale;
		AlmaWorldTransformUpdate(wt);

		// シアン調の半透明カラーを設定
		AlmaVector4 trailColor = {0.3f, 0.7f, 1.0f, trail.alpha};
		trailColors_[colorIndex]->SetColor(trailColor);

		if (trail.state == AlmaPlayerState::Rolling) {
			modelRollling_->Draw(wt, *camera_, trailColors_[colorIndex].get());
		} else {
			model_->Draw(wt, *camera_, trailColors_[colorIndex].get());
		}
		colorIndex++;
	}

	// 各パーティクルの描画
	dust_.Draw();
	landDust_.Draw();
	spark_.Draw();
}

//==================================================
// ユーティリティ
//==================================================

AlmaAABB AlmaPlayer::GetAABB() {
	// 見た目の伸び縮みオフセットに影響されないよう、論理座標を使う
	AlmaVector3 worldPos = worldTransform_.translation_;
	return {
	    {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f},
        {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f}
    };
}

void AlmaPlayer::OnCollision(const AlmaEnemy* enemy) {
	// --- 敵がすでに倒れているなら何もしない ---
	if (enemy->IsDefeated()) {
		return;
	}

	// --- 接触したら死亡 ---
	isDead_ = true;
	uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleDeath);
	AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);
}

void AlmaPlayer::OnCollision(const AlmaEnemy2* enemy) {
	// --- 敵がすでに倒れているなら何もしない ---
	if (enemy->IsDefeated()) {
		return;
	}

	// --- 接触したら死亡 ---
	isDead_ = true;
	uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleDeath);
	AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);
}

KamataEngine::AlmaVector3 AlmaPlayer::GetWorldPosition() {
	// ワールド座標を入れる変数
	KamataEngine::AlmaVector3 worldPos;
	// ワールド行列の平行移動成分を取得
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];
	return worldPos;
}

void AlmaPlayer::UpdateState() {
	// --- 下矢印キー（DIK_DOWN）によるホールド式しゃがみ ---
	if (AlmaInput::GetInstance()->PushKey(DIK_DOWN)) {
		if (state_ == AlmaPlayerState::Normal) {
			// しゃがむ（通常 → ローリング）
			uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleChange);
			AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);
			state_ = AlmaPlayerState::Rolling;

			// 【足元維持オフセット】：立ち（高さ1.5f, 半分0.75f）からしゃがみ（高さ0.8f, 半分0.4f）に変わるため、
			// 中心Y座標を 0.35f 下げて足元の位置を完全にキープする
			worldTransform_.translation_.y -= 0.35f;
		}
	} else {
		if (state_ == AlmaPlayerState::Rolling) {
			// 立ち上がる（ローリング → 通常）前に、立ち上がった後の頭上（Y + 0.35f + 0.75f = Y + 1.1f）にブロックがないかチェック
			AlmaVector3 topPos = worldTransform_.translation_ + AlmaVector3(0, 1.1f, 0);

			auto index = mapChipField_->GetMapChipIndexSetByposition(topPos);
			auto mapChip = mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex);

			if (!IsSolidChip(mapChip)) {
				uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleChange);
				AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);
				state_ = AlmaPlayerState::Normal; // 立ち上がる

				// 【足元維持オフセット】：しゃがみから立ちに戻るため、
				// 中心Y座標を 0.35f 上げて足元の位置を完全にキープする
				worldTransform_.translation_.y += 0.35f;
			}
		}
	}

	// --- 高さを設定 ---
	if (state_ == AlmaPlayerState::Rolling) {
		kHeight = 0.8f;
	} else {
		kHeight = 1.5f;
	}

	// --- 下の判定（地面に埋まらないように補正） ---
	if (state_ == AlmaPlayerState::Normal) {
		float highestFloorTop = -1000.0f;
		bool foundFloor = false;
		AlmaMapChipField::AlmaIndexSet bestIndexSet = {};

		// 左右の足元の座標をチェック
		for (float offsetX : { -kWidth / 2.0f, +kWidth / 2.0f }) {
			AlmaVector3 checkPos = worldTransform_.translation_ + AlmaVector3(offsetX, -kHeight / 2.0f, 0);
			auto index = mapChipField_->GetMapChipIndexSetByposition(checkPos);

			int xIndex = index.xIndex;
			int yIndex = index.yIndex;
			float playerBottomY = worldTransform_.translation_.y - kHeight / 2.0f;

			// 足元より明らかに高い（0.2f超）ブロックは「床」ではなく「壁」なので無視し、下を探す
			while (yIndex < 20) {
				AlmaVector3 tempPos = mapChipField_->GetMapChipPositionByIndex(xIndex, yIndex);
				float tempTopY = tempPos.y + AlmaMapChipField::kBlockHeight / 2.0f;
				AlmaMapChipType type = mapChipField_->GetMapChipTypeByIndex(xIndex, yIndex);

				if (IsSolidChip(type) && tempTopY <= playerBottomY + 0.2f) {
					if (tempTopY > highestFloorTop) {
						highestFloorTop = tempTopY;
						bestIndexSet = { (uint32_t)xIndex, (uint32_t)yIndex };
						foundFloor = true;
					}
					break;
				}
				yIndex++;
			}
		}

		if (foundFloor) {
			auto rect = mapChipField_->GetRectByIndex(bestIndexSet.xIndex, bestIndexSet.yIndex);
			float targetY = rect.top + kHeight / 2.0f + kBlank;
			float diffY = targetY - worldTransform_.translation_.y;
			// わずかな埋まり（0.4f未満）のときのみ補正する（壁の天面へのワープを防ぐ）
			if (diffY > 0.0f && diffY < 0.4f) {
				worldTransform_.translation_.y = targetY;
			}
		}
	}

	// --- 上の判定（天井にぶつかったら補正） ---
	if (state_ == AlmaPlayerState::Normal) {
		AlmaVector3 topPos = worldTransform_.translation_ + AlmaVector3(0, +kHeight / 2.0f, 0);
		auto index = mapChipField_->GetMapChipIndexSetByposition(topPos);
		auto mapChip = mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex);

		if (IsSolidChip(mapChip)) {
			auto rect = mapChipField_->GetRectByIndex(index.xIndex, index.yIndex);
			float targetY = rect.bottom - kHeight / 2.0f - kBlank;
			float diffY = worldTransform_.translation_.y - targetY;
			// わずかなめり込み（0.4f未満）のときのみ補正する
			if (diffY > 0.0f && diffY < 0.4f) {
				worldTransform_.translation_.y = targetY;
				velocity_.y = 0.0f; // 天井にぶつかったのでリセット
			}
		}
	}
}


void AlmaPlayer::CheckSpringCollision(const std::vector<std::vector<AlmaWorldTransform*>>& springs) {
	// すでに上昇中ならバネ判定をスキップし、衝突SEの重複（チャララランと連続で鳴る現象）を防ぐ
	if (velocity_.y > 0.0f)
		return;

	AlmaAABB playerAABB = GetAABB();
	float playerX = worldTransform_.translation_.x;

	for (auto& line : springs) {
		for (auto& spring : line) {
			if (!spring)
				continue;

			// 画面外なら判定をスキップ
			if (std::abs(playerX - spring->translation_.x) > 13.0f)
				continue;

			// バネの AlmaAABB を計算
			AlmaAABB springAABB = {
			    {spring->translation_.x - AlmaMapChipField::kBlockWidth / 2.0f, spring->translation_.y - AlmaMapChipField::kBlockHeight / 2.0f, -0.5f},
			    {spring->translation_.x + AlmaMapChipField::kBlockWidth / 2.0f, spring->translation_.y + AlmaMapChipField::kBlockHeight / 2.0f, +0.5f}
            };

			// 当たり判定
			if (AlmaIsCollision(playerAABB, springAABB)) {
				//  バネに触れたら強制大ジャンプ (4マスの高壁を確実に超えられるよう、通常ジャンプの2.2倍に強化)
				velocity_.y = kJumpInitialVelocity * 2.2f; 
				onGround_ = false;
				uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleSpring);
				AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);
				return; // 1つヒットしたら終わり
			}
		}
	}
}
void AlmaPlayer::CheckFireCollision(const std::vector<std::vector<AlmaWorldTransform*>>& fires) {
	if (isInvincible_)
		return; // 無敵中なら判定をスキップ

	AlmaAABB playerAABB = GetAABB();
	float playerX = worldTransform_.translation_.x;

	for (auto& line : fires) {
		for (auto& fire : line) {
			if (!fire)
				continue;

			// 画面外なら判定をスキップ
			if (std::abs(playerX - fire->translation_.x) > 13.0f)
				continue;

			// 火の AlmaAABB
			AlmaAABB fireAABB = {
			    {fire->translation_.x - AlmaMapChipField::kBlockWidth / 2.0f, fire->translation_.y - AlmaMapChipField::kBlockHeight / 2.0f, -0.5f},
			    {fire->translation_.x + AlmaMapChipField::kBlockWidth / 2.0f, fire->translation_.y + AlmaMapChipField::kBlockHeight / 2.0f, +0.5f}
            };

			if (AlmaIsCollision(playerAABB, fireAABB)) {
				// 火に触れたらダメージ
				ApplyDamage(30.0f);
				return;
			}
		}
	}
}

void AlmaPlayer::ApplyDamage(float damage) {
	if (isInvincible_ || isDead_)
		return;

	hp_ -= damage;
	damagedThisFrame_ = true;
	if (hp_ <= 0.0f) {
		hp_ = 0.0f;
		isDead_ = true;
		uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleDeath);
		AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);
	} else {
		// 被弾無敵の開始
		isInvincible_ = true;
		invincibleTimer_ = kInvincibleDuration;

		// 被弾時の変身SE
		uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleChange);
		AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);
	}
}

//==================================================
// 追加ゲーム性
//==================================================
void AlmaPlayer::ApplyBoost(float amount) { boostSpeed_ = std::max(boostSpeed_, amount); }

void AlmaPlayer::Heal(float amount) { hp_ = std::min(kMaxHp, hp_ + amount); }

void AlmaPlayer::BreakColumn(uint32_t xIndex) {
	for (uint32_t y = 0; y < 20; ++y) {
		if (mapChipField_->GetMapChipTypeByIndex(xIndex, y) == AlmaMapChipType::kBreakable) {
			mapChipField_->SetMapChipType(xIndex, y, AlmaMapChipType::kBlank);
			brokenBlocks_.push_back(mapChipField_->GetMapChipPositionByIndex(xIndex, y));
		}
	}
}


