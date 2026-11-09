#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "GameScene.h"
#include "BitmapFont.h"
#include "FxUtil.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <random>
#include <string>
using namespace KamataEngine;

namespace {
// 描画・更新する列の範囲（プレイヤーの列からの相対）
constexpr int kWindowBehind = 15;
constexpr int kWindowAhead = 36;
constexpr uint32_t kRows = 20;
constexpr float kDt = 1.0f / 60.0f;
constexpr float kInitialRunSpeed = 0.12f;

const AlmaVector4 kColorWhite{1.0f, 1.0f, 1.0f, 1.0f};
const AlmaVector4 kColorGold{1.0f, 0.86f, 0.25f, 1.0f};
const AlmaVector4 kColorCyan{0.4f, 0.95f, 1.0f, 1.0f};
const AlmaVector4 kColorOrange{1.0f, 0.6f, 0.2f, 1.0f};
const AlmaVector4 kColorRed{1.0f, 0.25f, 0.2f, 1.0f};
const AlmaVector4 kColorGreen{0.5f, 1.0f, 0.5f, 1.0f};
const AlmaVector4 kColorPink{1.0f, 0.5f, 0.9f, 1.0f};

// グリッドを必要な大きさまで拡張
template <class T> void ResizeGrid(std::vector<std::vector<T*>>& grid, size_t rows, size_t cols) {
	if (grid.size() < rows) {
		grid.resize(rows);
	}
	for (auto& row : grid) {
		if (row.size() < cols) {
			row.resize(cols, nullptr);
		}
	}
}

// グリッドの全要素を解放
template <class T> void DeleteGrid(std::vector<std::vector<T*>>& grid) {
	for (auto& row : grid) {
		for (auto*& p : row) {
			delete p;
			p = nullptr;
		}
	}
	grid.clear();
}

// 範囲チェック付き取得
template <class T> T* GridAt(const std::vector<std::vector<T*>>& grid, uint32_t y, uint32_t x) {
	if (y >= grid.size() || x >= grid[y].size()) {
		return nullptr;
	}
	return grid[y][x];
}

// 範囲チェック付き解放
template <class T> void GridDelete(std::vector<std::vector<T*>>& grid, uint32_t y, uint32_t x) {
	if (y >= grid.size() || x >= grid[y].size()) {
		return;
	}
	delete grid[y][x];
	grid[y][x] = nullptr;
}

AlmaVector4 ScaleRGB(const AlmaVector4& c, float s) { return {c.x * s, c.y * s, c.z * s, c.w}; }
} // namespace

//--------------------------------------------------
// デストラクタ
//--------------------------------------------------
AlmaGameScene::~AlmaGameScene() {
	// ====== オブジェクト解放 ======
	delete player_;

	// ====== 敵の解放 ======
	for (AlmaEnemy* enemy : enemies_) {
		delete enemy;
	}
	enemies_.clear();
	for (AlmaEnemy2* enemy : enemies2_) {
		delete enemy;
	}
	enemies2_.clear();

	// ====== ヒットエフェクト解放 ======
	for (AlmaHitEffect* effect : hitEffects_) {
		delete effect;
	}
	hitEffects_.clear();

	// ====== マップ由来オブジェクトの解放 ======
	DeleteGrid(blockCells_);
	DeleteGrid(worldTransformSpikes_);
	DeleteGrid(worldTransformSprings_);
	DeleteGrid(worldTransformFires_);
	DeleteGrid(worldTransformCoins_);
	DeleteGrid(worldTransformBreakables_);
	DeleteGrid(worldTransformBoosts_);

	// ====== 落下プレス機の解放 ======
	for (AlmaFallingSpike* fs : fallingSpikes_) {
		delete fs;
	}
	fallingSpikes_.clear();

	// ====== 飛び出すトゲの解放 ======
	for (AlmaRisingSpike* rs : risingSpikes_) {
		delete rs;
	}
	risingSpikes_.clear();

	// ====== スピードラインの解放 ======
	for (auto& line : speedLines_) {
		delete line.sprite;
	}
	speedLines_.clear();

	delete spritePose_;
	delete deathParticles_;
	delete fade_;
	delete cController_;
	delete mapChipField_;
	delete debugCamera_;
	delete camera_;

	// ====== モデル解放 ======
	delete model_;
	delete modelRolling;
	delete modelBlock_;
	delete enemyModel_;
	delete enemyModel2_;
	delete dethParticleModel;
	delete spikeModel_;
	delete springModel_;
	delete fireModel_;
	delete coinModel_;
	delete particleModel_;
	delete readyModel_;
	delete goModel_;
	AlmaHitEffect::SetModel(nullptr);
	delete hitEffectModel_;
}

//==================================================
// 初期化
//==================================================
void AlmaGameScene::Initialize() {
	fallingSpikes_.clear();
	risingSpikes_.clear();

	// モデル生成
	model_ = AlmaModel::CreateFromOBJ("playermax", true);
	modelRolling = AlmaModel::CreateFromOBJ("roll", true);
	modelBlock_ = AlmaModel::CreateFromOBJ("tileBlock", true);
	enemyModel_ = AlmaModel::CreateFromOBJ("enemyBody", true);
	enemyModel2_ = AlmaModel::CreateFromOBJ("enemyBody", true);
	dethParticleModel = AlmaModel::CreateFromOBJ("deathParticle");
	spikeModel_ = AlmaModel::CreateFromOBJ("spike", true);
	springModel_ = AlmaModel::CreateFromOBJ("spring", true);
	fireModel_ = AlmaModel::CreateFromOBJ("fire", true);
	coinModel_ = AlmaModel::CreateFromOBJ("coin", false);
	particleModel_ = AlmaModel::CreateFromOBJ("particle", true);
	// Ready/Go モデル
	readyModel_ = AlmaModel::CreateFromOBJ("rReady", true);
	goModel_ = AlmaModel::CreateFromOBJ("Go!", true);

	// カメラ
	camera_ = new AlmaCamera();
	camera_->Initialize();
	debugCamera_ = new AlmaDebugCamera(1080, 720);

	// ===== ヒットエフェクトにモデルとカメラを渡す =====
	hitEffectModel_ = AlmaModel::CreateFromOBJ("AttackEffect", true);
	AlmaHitEffect::SetModel(hitEffectModel_);
	AlmaHitEffect::SetCamera(camera_);

	// Ready モデル
	worldTransformReady_.Initialize();
	worldTransformReady_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	worldTransformReady_.rotation_.y = std::numbers::pi_v<float>;
	worldTransformReady_.translation_.z = 30.0f; // カメラの正面
	worldTransformReady_.translation_.x = 2.5f;
	worldTransformReady_.translation_.y = 0.0f;
	worldTransformReady_.scale_ = {9.0f, 9.0f, 9.0f};

	// Go モデル
	worldTransformGo_.Initialize();
	worldTransformGo_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	worldTransformGo_.rotation_.y = std::numbers::pi_v<float>;
	worldTransformGo_.translation_.z = 30.0f;
	worldTransformGo_.translation_.x = 4.0f;
	worldTransformGo_.translation_.y = 0.0f;
	worldTransformGo_.scale_ = {9.0f, 9.0f, 9.0f};

	// テクスチャ（エリアごとのブロック）
	for (int i = 0; i < Biome::kCount; ++i) {
		blockBodyTex_[i] = AlmaTextureManager::Load("blocks/body_" + std::to_string(i) + ".png");
		blockTopTex_[i] = AlmaTextureManager::Load("blocks/top_" + std::to_string(i) + ".png");
	}
	breakableTex_ = AlmaTextureManager::Load("blocks/breakable.png");
	boostTex_ = AlmaTextureManager::Load("blocks/boost.png");

	// 色
	colorFront_.Initialize();
	colorTop_.Initialize();
	for (auto& c : colorBack_) {
		c.Initialize();
	}
	colorBreakable_.Initialize();
	colorBoost_.Initialize();
	colorSpike_.Initialize();
	colorSpring_.Initialize();
	colorFire_.Initialize();
	colorCoin_.Initialize();

	// マップ
	mapChipField_ = new AlmaMapChipField();
	mapChipField_->ResetMapChipData();

	// プレイヤー生成
	AlmaVector3 playerposition = mapChipField_->GetMapChipPositionByIndex(5, 10);
	player_ = new AlmaPlayer();
	player_->Initialize(model_, modelRolling, camera_, playerposition);
	player_->SetMapChipField(mapChipField_);

	// カメラコントローラー
	cController_ = new AlmaCameraController();
	cController_->Initialize(camera_);
	cController_->SetTarget(player_);
	cController_->Reset();

	AlmaCameraController::AlmaRect cameraArea = {12.0f, 1000000.0f, 6.0f, 6.0f};
	cController_->SetMovableArea(cameraArea);

	// 初期動的生成（プレス機がカメラを揺らせるよう、カメラコントローラー生成後に行う）
	UpdateDynamicBlocks();

	// フェーズ初期化
	phase_ = AlmaPhase::kFadeIn;
	fade_ = new AlmaFade();
	fade_->Initialize();
	fade_->Start(AlmaFade::AlmaStatus::FadeIn, 1.0f);

	// Ready-Go タイマー
	readyTimer_ = 0.0f;

	textureHandlePose_ = AlmaTextureManager::Load("po-zu.jpg");
	spritePose_ = KamataEngine::AlmaSprite::Create(textureHandlePose_, {90, 0});
	soundhandleGo_ = AlmaAudio::GetInstance()->LoadWave("GO.wav");
	soundHandleCoin_ = AlmaAudio::GetInstance()->LoadWave("serect.wav");
	soundHandleSmash_ = AlmaAudio::GetInstance()->LoadWave("kiru.wav");
	soundHandleBreak_ = AlmaAudio::GetInstance()->LoadWave("mokugyo.wav");

	// ===== スピードラインの初期化 =====
	speedLines_.clear();
	speedLineTextureHandle_ = AlmaTextureManager::Load("white1x1.png");

	// ===== 追加要素の初期化 =====
	burst_.Initialize(particleModel_, modelBlock_, camera_, 360);
	background_.Initialize();
	hud_.Initialize();
	score_.Initialize();
	fov_ = 45.0f;
	lastArea_ = 0;
	speedLevel_ = 0;
	player_->SetBaseSpeed(kInitialRunSpeed);
}

//==================================================
// 更新
//==================================================
void AlmaGameScene::Update() {
	totalTime_ += kDt;
	ChangePhese();
	AlmaWorldTransformUpdate(worldTransformReady_);
	AlmaWorldTransformUpdate(worldTransformGo_);

	switch (phase_) {
	case AlmaPhase::kFadeIn: {
		if (fade_) {
			fade_->Update();
			if (fade_->IsFinished()) {
				phase_ = AlmaPhase::kReady;
				readyTimer_ = 0.0f;
			}
		}
		cController_->Update();
	} break;
	case AlmaPhase::kReady: {
		readyTimer_ += kDt;

		if (readyTimer_ < 1.0f) {
			float t = readyTimer_;
			float scale = 12.0f - 3.0f * t;
			worldTransformReady_.scale_ = {scale, scale, scale};

		} else {
			float t = (readyTimer_ - 1.0f) * 2.0f * 3.14159f;
			float scale = 9.0f + 1.0f * sinf(t * 4.0f);
			worldTransformGo_.scale_ = {scale, scale, scale};
			// 一度だけ鳴らす
			if (!playedReadySound_) {
				uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundhandleGo_);
				AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);
				playedReadySound_ = true;
			}
		}

		if (readyTimer_ > 1.5f) {
			phase_ = AlmaPhase::kPlay;
			const AlmaBiomePalette& palette = Biome::Get(0);
			hud_.ShowBanner(palette.name, "AREA 1", {1.0f, 0.95f, 0.6f, 1.0f});
		}
		cController_->Update();
	} break;

	case AlmaPhase::kPlay: {
		UpdatePlay();
	} break;

	case AlmaPhase::kDeath: {
		if (deathParticles_ && deathParticles_->IsFinished()) {
			phase_ = AlmaPhase::kFadeOut;
			if (fade_)
				fade_->Start(AlmaFade::AlmaStatus::FadeOut, 1.0f);
		}
		cController_->Update();
		for (AlmaEnemy* enemy : enemies_)
			enemy->UpDate();
		for (AlmaEnemy2* enemy : enemies2_)
			enemy->Update();
		if (deathParticles_)
			deathParticles_->Update();
	} break;

	case AlmaPhase::kFadeOut: {
		if (fade_) {
			fade_->Update();
			if (fade_->IsFinished())
				finished_ = true;
		}
		cController_->Update();
		for (AlmaEnemy* enemy : enemies_)
			enemy->UpDate();
		for (AlmaEnemy2* enemy : enemies2_)
			enemy->Update();
	} break;
	case AlmaPhase::kPause:
		if (AlmaInput::GetInstance()->TriggerKey(DIK_ESCAPE)) {
			phase_ = AlmaPhase::kPlay;
		}

		// ===== Enterでタイトルに戻る =====
		if (AlmaInput::GetInstance()->TriggerKey(DIK_RETURN)) {
			score_.Finish();
			fade_->Start(AlmaFade::AlmaStatus::FadeOut, 1.0f);
			phase_ = AlmaPhase::kFadeOut;
			returnToTitle_ = true;
		}
		break;
	}

	// ====== フェーズ共通で敵削除処理 ======
	float playerX = player_->GetPosition().x;
	enemies_.remove_if([this, playerX](AlmaEnemy* enemy) {
		if (enemy->IsDead()) {
			// ヒットエフェクト生成
			hitEffects_.push_back(AlmaHitEffect::Create(enemy->GetWorldPosition()));
			delete enemy;
			return true;
		}
		// 置き去りにした敵は破棄
		if (enemy->GetWorldPosition().x < playerX - 20.0f) {
			delete enemy;
			return true;
		}
		return false;
	});
	enemies2_.remove_if([this, playerX](AlmaEnemy2* enemy) {
		if (enemy->IsDead()) {
			hitEffects_.push_back(AlmaHitEffect::Create(enemy->GetWorldPosition()));
			delete enemy;
			return true;
		}
		if (enemy->GetWorldPosition().x < playerX - 20.0f) {
			delete enemy;
			return true;
		}
		return false;
	});

	// ====== ヒットエフェクト更新 ======
	for (auto it = hitEffects_.begin(); it != hitEffects_.end();) {
		(*it)->Update();
		if ((*it)->IsFinished()) {
			delete *it;
			it = hitEffects_.erase(it);
		} else {
			++it;
		}
	}

	if (phase_ != AlmaPhase::kPause) {
		burst_.Update();
		background_.Update(camera_->translation_.x, GetDistance(), totalTime_);
	}

	// ===== スピードラインの更新 =====
	UpdateSpeedLines();

	// ===== HUD =====
	UpdateHudState();
	if (phase_ != AlmaPhase::kPause) {
		hud_.Update(hudState_);
	}
}

//==================================================
// プレイ中の更新
//==================================================
void AlmaGameScene::UpdatePlay() {
	// ===== ESCキーでポーズに切り替え =====
	if (AlmaInput::GetInstance()->TriggerKey(DIK_ESCAPE)) {
		phase_ = AlmaPhase::kPause;
		return;
	}

	playTime_ += kDt;

	// ===== ヒットストップ（当たった瞬間だけ時間を止めて手応えを出す） =====
	if (hitstopFrames_ > 0) {
		hitstopFrames_--;
		cController_->Update(); // 画面揺れだけは進める
		return;
	}

	UpdateDynamicBlocks(); // 動的にブロックを生成・破棄

	// ===== 距離でスピードアップ（200mごとに緩やかに上昇） =====
	float distance = GetDistance();
	int level = std::min(static_cast<int>(distance / 200.0f), 9);
	if (level > speedLevel_) {
		speedLevel_ = level;
		hud_.ShowBanner("SPEED UP!", "SPEED LV " + std::to_string(speedLevel_ + 1), kColorCyan);
	}
	player_->SetBaseSpeed(kInitialRunSpeed + 0.01f * static_cast<float>(speedLevel_));

	// ===== エリア切り替え（500mごと） =====
	int area = Biome::AreaNumberAt(distance);
	if (area != lastArea_) {
		lastArea_ = area;
		const AlmaBiomePalette& palette = Biome::Get(area % Biome::kCount);
		hud_.ShowBanner(palette.name, "AREA " + std::to_string(area + 1), {1.0f, 0.95f, 0.6f, 1.0f});
	}

	// ===== スコア・コンボ =====
	score_.Update(kDt);
	niceCooldown_ = std::max(0.0f, niceCooldown_ - kDt);

	// ===== 速度に応じて視野角を広げる（スピード感） =====
	float speed = player_->GetCurrentSpeed();
	float targetFov = 45.0f + std::clamp((speed - 0.2f) * 30.0f, 0.0f, 14.0f);
	fov_ += (targetFov - fov_) * 0.05f;
	camera_->fovAngleY = fov_ * std::numbers::pi_v<float> / 180.0f;

	cController_->Update();
	player_->Update();
	for (AlmaEnemy* enemy : enemies_) {
		enemy->UpDate();
	}
	for (AlmaEnemy2* enemy : enemies2_) {
		enemy->Update();
	}

	// ===== デバッグカメラ切り替え =====
#ifdef _DEBUG
	if (AlmaInput::GetInstance()->TriggerKey(DIK_G)) {
		isDebugCameraActive_ = !isDebugCameraActive_;
	}
#endif
	if (isDebugCameraActive_) {
		debugCamera_->Update();
		camera_->matView = debugCamera_->GetCamera().matView;
		camera_->matProjection = debugCamera_->GetCamera().matProjection;
		camera_->TransferMatrix();
	} else {
		camera_->UpdateMatrix();
	}

	// ===== 配置物の見た目更新 =====
	UpdateObjectsVisual();

	// ===== 落下プレス機・飛び出すトゲ更新 =====
	float playerX = player_->GetPosition().x;
	for (AlmaFallingSpike* fs : fallingSpikes_) {
		fs->Update(playerX);
	}
	for (AlmaRisingSpike* rs : risingSpikes_) {
		rs->Update(playerX);
	}

	player_->CheckSpringCollision(worldTransformSprings_);
	player_->CheckFireCollision(worldTransformFires_);
	CheckAllCollisions();
	HandleBrokenBlocks();
	CheckNearMiss();
	CheckSlide();
	HandleDamageFeedback();

	// ===== 着地演出 =====
	if (player_->JustLanded() && player_->GetLandingSpeed() > 0.35f) {
		cController_->StartShake(0.12f, 0.08f);
	}


	// ===== 距離スコア =====
	score_.SetDistance(GetDistance());
}

//==================================================
// 配置物の見た目（コインの回転・火の上下など）
//==================================================
void AlmaGameScene::UpdateObjectsVisual() {
	fireTime_ += kDt;
	if (windowEnd_ < windowStart_) {
		return;
	}
	for (uint32_t y = 0; y < kRows; ++y) {
		for (int xi = windowStart_; xi <= windowEnd_; ++xi) {
			uint32_t x = static_cast<uint32_t>(xi);

			// ===== コイン：回転＋ふわふわ =====
			if (AlmaWorldTransform* coin = GridAt(worldTransformCoins_, y, x)) {
				coin->rotation_.y += 0.06f;
				float baseY = mapChipField_->GetMapChipPositionByIndex(x, y).y;
				coin->translation_.y = baseY + 0.08f * std::sin(fireTime_ * 3.0f + static_cast<float>(x) * 0.6f);
				AlmaWorldTransformUpdate(*coin);
			}

			// ===== 火の更新 =====
			if (AlmaWorldTransform* fire = GridAt(worldTransformFires_, y, x)) {
				float amplitude = 0.8f; // 上下の振れ幅（0.4fから拡大）
				float speed = 3.0f;     // 上下の速さ（2.0fから加速）

				// 座標蓄積バグを防ぐため、マップチップから元々の初期Y座標を取得
				float baseY = mapChipField_->GetMapChipPositionByIndex(x, y).y;
				fire->translation_.y = baseY + amplitude * sinf(fireTime_ * speed);
				fire->rotation_.y += 0.05f;
				AlmaWorldTransformUpdate(*fire);
			}
		}
	}
}

//==================================================
// スピードライン
//==================================================
void AlmaGameScene::UpdateSpeedLines() {
	// ポーズ中、フェードアウト中、死亡中はスピードラインを更新しない（停止）
	if (phase_ == AlmaPhase::kPause || phase_ == AlmaPhase::kFadeOut || phase_ == AlmaPhase::kDeath) {
		return;
	}
	float spawnProbability = 0.05f + 0.015f * static_cast<float>(speedLevel_); // 通常時（速いほど増える）
	float lineSpeedMin = 15.0f;
	float lineSpeedMax = 30.0f;

	// プレイヤーの状態によって発生頻度と速度をブースト
	if (phase_ == AlmaPhase::kPlay && player_ && !player_->IsDead()) {
		if (player_->IsRolling()) {
			spawnProbability = 0.25f; // スライディング中は25%
			lineSpeedMin = 30.0f;
			lineSpeedMax = 50.0f;
		} else if (player_->GetVelocity().y > 0.05f) {
			spawnProbability = 0.15f; // バネジャンプ中などは15%
			lineSpeedMin = 25.0f;
			lineSpeedMax = 40.0f;
		}
	} else if (player_ && player_->IsDead()) {
		spawnProbability = 0.0f; // 死亡時は発生しない
	}

	static std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> probDist(0.0f, 1.0f);
	std::uniform_real_distribution<float> yDist(50.0f, 670.0f);     // 画面縦の範囲
	std::uniform_real_distribution<float> widthDist(80.0f, 300.0f); // 線の長さ
	std::uniform_real_distribution<float> heightDist(1.0f, 2.5f);   // 線の太さ（細いほどスタイリッシュ）
	std::uniform_real_distribution<float> speedDist(lineSpeedMin, lineSpeedMax);
	std::uniform_real_distribution<float> alphaDist(0.1f, 0.35f); // 不透明度

	if (probDist(rng) < spawnProbability) {
		AlmaSpeedLine newLine;
		newLine.position = {1280.0f, yDist(rng)};
		newLine.speed = speedDist(rng);
		newLine.width = widthDist(rng);
		newLine.alpha = alphaDist(rng);
		newLine.hue = 0.0f;

		float height = heightDist(rng);
		AlmaVector4 color = {1.0f, 1.0f, 1.0f, newLine.alpha};

		newLine.sprite = AlmaSprite::Create(speedLineTextureHandle_, newLine.position, color, {0.0f, 0.5f});
		newLine.sprite->SetSize({newLine.width, height});

		speedLines_.push_back(newLine);
	}

	// 移動と削除
	for (auto it = speedLines_.begin(); it != speedLines_.end();) {
		it->position.x -= it->speed;
		it->sprite->SetPosition(it->position);

		if (it->position.x + it->width < 0.0f) {
			delete it->sprite;
			it = speedLines_.erase(it);
		} else {
			++it;
		}
	}
}

//==================================================
// HUD に渡す値をまとめる
//==================================================
void AlmaGameScene::UpdateHudState() {
	int distance = static_cast<int>(GetDistance());
	hudState_.hp = player_->GetHp();
	hudState_.maxHp = player_->GetMaxHp();
	hudState_.coins = score_.GetCoins();
	hudState_.distance = distance;
	hudState_.bestDistance = std::max(score_.GetBestDistance(), distance);
	hudState_.score = score_.GetScore();
	hudState_.combo = score_.GetCombo();
	hudState_.multiplier = score_.GetMultiplier();
	hudState_.comboTimeRatio = score_.GetComboTimeRatio();
	hudState_.speedLevel = speedLevel_;
	hudState_.paused = (phase_ == AlmaPhase::kPause);
	hudState_.visible = true;

	// 操作説明：READY中とプレイ開始から約9秒間だけ表示
	if (phase_ == AlmaPhase::kReady) {
		hudState_.hintAlpha = 1.0f;
	} else if (phase_ == AlmaPhase::kPlay || phase_ == AlmaPhase::kPause) {
		hudState_.hintAlpha = std::clamp((9.0f - playTime_) / 1.5f, 0.0f, 1.0f);
	} else {
		hudState_.hintAlpha = 0.0f;
	}
}

//==================================================
// 描画
//==================================================
void AlmaGameScene::Draw() {
	KamataEngine::AlmaDirectXCommon* dxcommon = KamataEngine::AlmaDirectXCommon::GetInstance();
	AlmaBitmapFont::GetInstance()->BeginFrame();

	// ===== 環境色（昼夜・エリア）を各色に反映 =====
	const AlmaVector4& tint = background_.GetWorldTint();
	float pulse = 0.5f + 0.5f * std::sin(totalTime_ * 6.0f);
	colorFront_.SetColor(tint);
	colorTop_.SetColor(tint);
	colorBack_[0].SetColor(ScaleRGB(tint, 0.74f));
	colorBack_[1].SetColor(ScaleRGB(tint, 0.52f));
	colorBreakable_.SetColor({tint.x * 1.1f, tint.y * (0.9f + 0.15f * pulse), tint.z * 0.8f, 1.0f});
	colorBoost_.SetColor({0.5f + 0.5f * pulse, 1.2f, 1.3f, 1.0f}); // 安全・加速 = 光るシアン
	float hazardLight = std::max(tint.x, 0.7f);
	colorSpike_.SetColor({1.0f * hazardLight, 0.4f * hazardLight, 0.35f * hazardLight, 1.0f}); // 危険 = 赤
	colorSpring_.SetColor({0.6f, 1.15f, 0.6f, 1.0f});                                         // 安全 = 緑
	colorFire_.SetColor({1.4f, 0.85f + 0.2f * pulse, 0.5f, 1.0f});
	colorCoin_.SetColor({1.25f, 1.15f, 0.85f, 1.0f});

	// ==============================
	// 背景（2D）→ 深度クリア
	// ==============================
	AlmaSprite::PreDraw(dxcommon->GetCommandList());
	KamataEngine::backgroundDrawing = true;
	background_.Draw();
	KamataEngine::backgroundDrawing = false;
	AlmaSprite::PostDraw();
	dxcommon->ClearDepthBuffer();

	// ==============================
	// 3D描画開始
	// ==============================
	AlmaModel::PreDraw(dxcommon->GetCommandList());

	if (windowEnd_ >= windowStart_) {
		for (uint32_t y = 0; y < kRows; ++y) {
			for (int xi = windowStart_; xi <= windowEnd_; ++xi) {
				uint32_t x = static_cast<uint32_t>(xi);

				// --- ブロック（本体＋草の天面＋奥行き） ---
				if (AlmaBlockCell* cell = GridAt(blockCells_, y, x)) {
					uint32_t body = blockBodyTex_[cell->biome];
					uint32_t top = blockTopTex_[cell->biome];
					if (cell->hasBack) {
						for (int k = 1; k >= 0; --k) {
							modelBlock_->Draw(cell->back[k], *camera_, body, &colorBack_[k]);
							if (cell->hasTop) {
								modelBlock_->Draw(cell->backTop[k], *camera_, top, &colorBack_[k]);
							}
						}
					}
					modelBlock_->Draw(cell->main, *camera_, body, &colorFront_);
					if (cell->hasTop) {
						modelBlock_->Draw(cell->top, *camera_, top, &colorTop_);
					}
				}

				// --- ヒビ割れブロック ---
				if (AlmaWorldTransform* wt = GridAt(worldTransformBreakables_, y, x)) {
					modelBlock_->Draw(*wt, *camera_, breakableTex_, &colorBreakable_);
				}

				// --- ダッシュパネル ---
				if (AlmaWorldTransform* wt = GridAt(worldTransformBoosts_, y, x)) {
					modelBlock_->Draw(*wt, *camera_, boostTex_, &colorBoost_);
				}

				// --- スパイク（危険色） ---
				if (AlmaWorldTransform* wt = GridAt(worldTransformSpikes_, y, x)) {
					spikeModel_->Draw(*wt, *camera_, &colorSpike_);
				}

				// --- バネ（安全色） ---
				if (AlmaWorldTransform* wt = GridAt(worldTransformSprings_, y, x)) {
					springModel_->Draw(*wt, *camera_, &colorSpring_);
				}

				// --- 動く火 ---
				if (AlmaWorldTransform* wt = GridAt(worldTransformFires_, y, x)) {
					fireModel_->Draw(*wt, *camera_, &colorFire_);
				}

				// --- コイン ---
				if (AlmaWorldTransform* wt = GridAt(worldTransformCoins_, y, x)) {
					coinModel_->Draw(*wt, *camera_, &colorCoin_);
				}
			}
		}
	}

	// --- 敵描画 ---
	for (AlmaEnemy* enemy : enemies_) {
		enemy->Draw();
	}
	for (AlmaEnemy2* enemy2 : enemies2_) {
		enemy2->Draw();
	}

	// --- 落下プレス機描画 ---
	for (AlmaFallingSpike* fs : fallingSpikes_) {
		fs->Draw();
	}

	// --- 飛び出すトゲ描画 ---
	for (AlmaRisingSpike* rs : risingSpikes_) {
		rs->Draw();
	}

	// --- プレイヤー ---
	if (!player_->IsDead()) {
		player_->Draw();
	}

	// --- パーティクル ---
	burst_.Draw();

	// --- デスパーティクル ---
	if (deathParticles_) {
		deathParticles_->Draw();
	}

	// --- ヒットエフェクト ---
	for (auto* effect : hitEffects_) {
		effect->Draw();
	}

	// --- Ready/Go 演出 ---
	if (phase_ == AlmaPhase::kReady) {
		if (readyTimer_ < 1.0f) {
			readyModel_->Draw(worldTransformReady_, *camera_);
		} else {
			goModel_->Draw(worldTransformGo_, *camera_);
		}
	}

	// --- 落下プレス機の警告ライン描画 ---
	for (AlmaFallingSpike* fs : fallingSpikes_) {
		fs->DrawWarningLines();
	}

	AlmaModel::PostDraw();
	// ==============================
	// 3D描画終了
	// ==============================

	// ==============================
	// 2D描画開始 (UI, フェード, スプライト)
	// ==============================
	AlmaSprite::PreDraw(dxcommon->GetCommandList());

	// --- スピードライン描画 ---
	for (auto& line : speedLines_) {
		line.sprite->Draw();
	}

	// --- HUD ---
	hud_.Draw(hudState_);

	if (phase_ == AlmaPhase::kPause) {
		spritePose_->Draw();
	}

#ifdef _DEBUG
	// --- FPSの計算と表示 (左下に表示) ---
	static auto lastTime = std::chrono::steady_clock::now();
	static int frameCount = 0;
	static float fps = 0.0f;

	frameCount++;
	auto currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<float> elapsed = currentTime - lastTime;

	if (elapsed.count() >= 1.0f) {
		fps = static_cast<float>(frameCount) / elapsed.count();
		frameCount = 0;
		lastTime = currentTime;
	}

	char fpsString[32];
	snprintf(fpsString, sizeof(fpsString), "FPS: %.2f", fps);
	AlmaDebugText::GetInstance()->Print(fpsString, 10, 690, 1.0f);
#endif

	// デバッグテキスト描画
	AlmaDebugText::GetInstance()->DrawAll();

	AlmaSprite::PostDraw();
	// ==============================
	// 2D描画終了
	// ==============================

	// --- フェードは最前面に描画 ---
	if (fade_) {
		fade_->Draw();
	}
}

//==================================================
// 衝突判定
//==================================================
void AlmaGameScene::CheckAllCollisions() {
	AlmaAABB aabb1 = player_->GetAABB();

	CheckEnemyCollisions(aabb1);
	CheckCoinCollisions(aabb1);
	CheckBoostPads(aabb1);

	// ===== 落下プレス機との当たり判定 =====
	for (AlmaFallingSpike* fs : fallingSpikes_) {
		// 落下中または揺れ中のものだけ、プレイヤーと直撃（潰される）判定を行う
		// (着地後はマップチップが Block に書き換わるため、通常の壁・床衝突システムで処理される)
		if (fs->GetState() == AlmaFallingSpike::AlmaState::kFalling || fs->GetState() == AlmaFallingSpike::AlmaState::kShaking) {
			AlmaAABB fsAABB = fs->GetAABB();
			if (AlmaIsCollision(aabb1, fsAABB)) {
				// 落下中のプレス機に直撃したら 30 ダメージ
				player_->ApplyDamage(30.0f);
			}
		}
	}

	// ===== 飛び出すトゲとの当たり判定 =====
	for (AlmaRisingSpike* rs : risingSpikes_) {
		if (rs->IsDamageActive()) {
			AlmaAABB rsAABB = rs->GetAABB();
			if (AlmaIsCollision(aabb1, rsAABB)) {
				// 飛び出すトゲに被弾したら 20 ダメージ
				player_->ApplyDamage(20.0f);
			}
		}
	}
}

//--------------------------------------------------
// 敵：丸まり/スーパー中なら体当たりで撃破、それ以外はダメージ
//--------------------------------------------------
void AlmaGameScene::CheckEnemyCollisions(const AlmaAABB& playerAABB) {
	float playerX = player_->GetPosition().x;
	bool attacking = player_->IsRolling();

	for (AlmaEnemy* enemy : enemies_) {
		if (enemy->IsDefeated())
			continue;
		// 画面外なら衝突判定をスキップ
		if (std::abs(playerX - enemy->GetWorldPosition().x) > 13.0f)
			continue;
		if (!AlmaIsCollision(playerAABB, enemy->GetAABB()))
			continue;

		if (attacking) {
			enemy->Smash();
			OnEnemySmashed(enemy->GetWorldPosition());
		} else if (!player_->IsInvincible()) {
			player_->ApplyDamage(25.0f); // 敵衝突で25ダメージ
		}
	}

	for (AlmaEnemy2* enemy2 : enemies2_) {
		if (enemy2->IsDefeated())
			continue;
		if (std::abs(playerX - enemy2->GetWorldPosition().x) > 13.0f)
			continue;
		if (!AlmaIsCollision(playerAABB, enemy2->GetAABB()))
			continue;

		if (attacking) {
			enemy2->Smash();
			OnEnemySmashed(enemy2->GetWorldPosition());
		} else if (!player_->IsInvincible()) {
			player_->ApplyDamage(25.0f);
		}
	}
}

//--------------------------------------------------
// 敵撃破の報酬と演出
//--------------------------------------------------
void AlmaGameScene::OnEnemySmashed(const AlmaVector3& position) {
	score_.AddSmash();
	score_.AddCombo();
	AwardPopup("SMASH!", 300, position, kColorOrange);
	hud_.PulseCombo();
	StartHitstop(4);
	cController_->StartShake(0.15f, 0.25f);

	AlmaBurstParticles::AlmaEmitParams p;
	p.position = position;
	p.count = 14;
	p.color = kColorOrange;
	p.speedMin = 0.08f;
	p.speedMax = 0.22f;
	p.scale = 0.22f;
	p.life = 0.6f;
	p.gravity = 0.008f;
	burst_.Emit(p);

	p.count = 6;
	p.color = kColorWhite;
	p.scale = 0.3f;
	p.life = 0.3f;
	p.gravity = 0.0f;
	burst_.Emit(p);

	uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleSmash_);
	AlmaAudio::GetInstance()->SetVolume(handle, 0.12f);
}

//--------------------------------------------------
// コイン
//--------------------------------------------------
void AlmaGameScene::CheckCoinCollisions(const AlmaAABB& playerAABB) {
	if (windowEnd_ < windowStart_) {
		return;
	}
	float playerX = player_->GetPosition().x;
	for (uint32_t y = 0; y < kRows; ++y) {
		for (int xi = windowStart_; xi <= windowEnd_; ++xi) {
			uint32_t x = static_cast<uint32_t>(xi);
			AlmaWorldTransform* coin = GridAt(worldTransformCoins_, y, x);
			if (!coin)
				continue;

			// 画面外なら判定をスキップ
			if (std::abs(playerX - coin->translation_.x) > 13.0f)
				continue;

			// コインの AlmaAABB（少し大きめにして取りやすく）
			AlmaAABB coinAABB = {
			    {coin->translation_.x - 0.45f, coin->translation_.y - 0.45f, -0.5f},
			    {coin->translation_.x + 0.45f, coin->translation_.y + 0.45f, 0.5f }
            };

			if (!AlmaIsCollision(playerAABB, coinAABB))
				continue;

			// コイン獲得！
			AlmaVector3 coinPos = coin->translation_;
			bool heal = score_.AddCoin();
			score_.AddCombo();
			int points = score_.AddBonus(50);
			hud_.AddPopup("+" + std::to_string(points), ToScreen(coinPos + AlmaVector3(0.0f, 0.6f, 0.0f)), kColorGold, 1.5f);
			hud_.PulseCoin();
			hud_.PulseScore();

			// キラキラ
			AlmaBurstParticles::AlmaEmitParams p;
			p.position = coinPos;
			p.count = 8;
			p.color = kColorGold;
			p.speedMin = 0.03f;
			p.speedMax = 0.1f;
			p.scale = 0.12f;
			p.life = 0.4f;
			p.gravity = 0.0f;
			burst_.Emit(p);

			// 50枚ごとに回復
			if (heal) {
				player_->Heal(20.0f);
				hud_.ShowBanner("HP +20", std::to_string(AlmaScoreManager::kCoinsPerHeal) + " COINS BONUS", kColorGreen);
				p.count = 16;
				p.color = kColorGreen;
				p.position = player_->GetPosition();
				p.speedMax = 0.16f;
				burst_.Emit(p);
			}

			// SE再生 (音量 0.1)
			uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleCoin_);
			AlmaAudio::GetInstance()->SetVolume(handle, 0.1f);

			// メモリ解放とマップデータ消去
			GridDelete(worldTransformCoins_, y, x);
			mapChipField_->SetMapChipType(x, y, AlmaMapChipType::kBlank);
		}
	}
}

//--------------------------------------------------
// ダッシュパネル（踏むと一時的に加速）
//--------------------------------------------------
void AlmaGameScene::CheckBoostPads(const AlmaAABB& playerAABB) {
	AlmaVector3 foot = {(playerAABB.min.x + playerAABB.max.x) * 0.5f, playerAABB.min.y + 0.2f, 0.0f};
	AlmaMapChipField::AlmaIndexSet index = mapChipField_->GetMapChipIndexSetByposition(foot);
	if (mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex) != AlmaMapChipType::kBoostPad) {
		return;
	}
	int column = static_cast<int>(index.xIndex);
	if (column == lastBoostColumn_) {
		return;
	}
	bool newLane = column > lastBoostColumn_ + 1;
	lastBoostColumn_ = column;

	// 丸まっていると加速量アップ
	player_->ApplyBoost(player_->IsRolling() ? 0.2f : 0.12f);

	AlmaBurstParticles::AlmaEmitParams p;
	p.position = foot;
	p.count = 8;
	p.color = kColorCyan;
	p.speedMin = 0.04f;
	p.speedMax = 0.12f;
	p.scale = 0.14f;
	p.life = 0.4f;
	p.gravity = 0.0f;
	p.bias = {-0.08f, 0.04f, 0.0f};
	burst_.Emit(p);

	if (newLane) {
		AwardPopup(player_->IsRolling() ? "ROLL BOOST!" : "BOOST!", 50, player_->GetPosition(), kColorCyan);
		cController_->StartShake(0.1f, 0.06f);
	}
}

//--------------------------------------------------
// ニアミス（トゲ・火の真上スレスレを無傷で通過）
//--------------------------------------------------
void AlmaGameScene::CheckNearMiss() {
	AlmaAABB a = player_->GetAABB();
	float bottom = a.min.y;
	uint32_t x0 = mapChipField_->GetMapChipIndexSetByposition({a.min.x, 0.0f, 0.0f}).xIndex;
	uint32_t x1 = mapChipField_->GetMapChipIndexSetByposition({a.max.x, 0.0f, 0.0f}).xIndex;

	auto addCandidate = [this](int key, float rightX) {
		if (nearMissAwarded_.count(key)) {
			return;
		}
		for (const auto& c : nearMissCandidates_) {
			if (c.key == key) {
				return;
			}
		}
		nearMissCandidates_.push_back({key, rightX, damageStamp_});
	};

	if (!player_->IsOnGround()) {
		for (uint32_t x = x0; x <= x1; ++x) {
			for (uint32_t y = 8; y < 18; ++y) {
				float hazardTop = 0.0f;
				bool hazard = false;
				if (mapChipField_->GetMapChipTypeByIndex(x, y) == AlmaMapChipType::kSpike) {
					hazardTop = mapChipField_->GetMapChipPositionByIndex(x, y).y + 0.5f;
					hazard = true;
				} else if (AlmaWorldTransform* fire = GridAt(worldTransformFires_, y, x)) {
					hazardTop = fire->translation_.y + 0.5f;
					hazard = true;
				}
				if (!hazard) {
					continue;
				}
				float gap = bottom - hazardTop;
				if (gap > 0.0f && gap < 0.6f) {
					addCandidate(static_cast<int>(x) * 32 + static_cast<int>(y), static_cast<float>(x) + 0.5f);
				}
			}
		}
		// 飛び出すトゲ
		for (AlmaRisingSpike* rs : risingSpikes_) {
			if (!rs->IsDamageActive()) {
				continue;
			}
			AlmaAABB r = rs->GetAABB();
			if (r.max.x < a.min.x || r.min.x > a.max.x) {
				continue;
			}
			float gap = bottom - r.max.y;
			if (gap > 0.0f && gap < 0.6f) {
				uint32_t rx = mapChipField_->GetMapChipIndexSetByposition(rs->GetPosition()).xIndex;
				addCandidate(static_cast<int>(rx) * 32 + 31, r.max.x);
			}
		}
	}

	// 通り過ぎたら判定（その間に被弾していなければ NICE!）
	for (auto it = nearMissCandidates_.begin(); it != nearMissCandidates_.end();) {
		if (a.min.x > it->rightX) {
			nearMissAwarded_.insert(it->key);
			if (it->damageStamp == damageStamp_ && niceCooldown_ <= 0.0f) {
				score_.AddCombo();
				AwardPopup("NICE!", 150, player_->GetPosition(), kColorPink);
				niceCooldown_ = 0.6f;
			}
			it = nearMissCandidates_.erase(it);
		} else {
			++it;
		}
	}

	// 古い記録は捨てる
	int pruneKey = (static_cast<int>(x0) - 24) * 32;
	while (!nearMissAwarded_.empty() && *nearMissAwarded_.begin() < pruneKey) {
		nearMissAwarded_.erase(nearMissAwarded_.begin());
	}
}

//--------------------------------------------------
// スライディング（低い天井の下を丸まって無傷で通過）
//--------------------------------------------------
void AlmaGameScene::CheckSlide() {
	bool underCeiling = false;
	if (player_->IsRolling() && player_->IsOnGround()) {
		AlmaVector3 head = player_->GetPosition() + AlmaVector3(0.0f, 0.9f, 0.0f);
		AlmaMapChipField::AlmaIndexSet index = mapChipField_->GetMapChipIndexSetByposition(head);
		underCeiling = IsSolidChip(mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex));
	}

	if (underCeiling && !sliding_) {
		sliding_ = true;
		slideDamageStamp_ = damageStamp_;
	} else if (!underCeiling && sliding_) {
		sliding_ = false;
		if (slideDamageStamp_ == damageStamp_ && player_->IsRolling()) {
			score_.AddCombo();
			AwardPopup("SLIDE!", 100, player_->GetPosition(), kColorCyan);
		}
	}
}

//--------------------------------------------------
// 壊したヒビ割れブロック
//--------------------------------------------------
void AlmaGameScene::HandleBrokenBlocks() {
	const std::vector<AlmaVector3>& broken = player_->GetBrokenBlocks();
	if (broken.empty()) {
		return;
	}
	for (const AlmaVector3& pos : broken) {
		AlmaMapChipField::AlmaIndexSet index = mapChipField_->GetMapChipIndexSetByposition(pos);
		GridDelete(worldTransformBreakables_, index.yIndex, index.xIndex);

		// 破片（ブロック形状・ヒビ割れテクスチャ）
		AlmaBurstParticles::AlmaEmitParams p;
		p.position = pos;
		p.count = 5;
		p.color = background_.GetWorldTint();
		p.speedMin = 0.08f;
		p.speedMax = 0.2f;
		p.scale = 0.22f;
		p.life = 0.9f;
		p.gravity = 0.012f;
		p.bias = {0.12f, 0.06f, 0.0f};
		p.texture = breakableTex_;
		p.useAltModel = true;
		p.spin = 0.3f;
		burst_.Emit(p);
	}

	score_.AddCombo();
	AwardPopup("BREAK!", 100 * static_cast<int>(broken.size()), broken.front(), kColorOrange);
	cController_->StartShake(0.12f, 0.2f);
	StartHitstop(2);
	uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleBreak_);
	AlmaAudio::GetInstance()->SetVolume(handle, 0.15f);
}

//--------------------------------------------------
// 被弾時の演出（赤フラッシュ・揺れ・ヒットストップ・コンボリセット）
//--------------------------------------------------
void AlmaGameScene::HandleDamageFeedback() {
	if (!player_->TookDamage()) {
		return;
	}
	damageStamp_++;
	score_.ResetCombo();
	hud_.Flash({1.0f, 0.0f, 0.0f, 0.45f}, 0.35f);
	cController_->StartShake(0.3f, 0.4f);
	StartHitstop(5);

	AlmaBurstParticles::AlmaEmitParams p;
	p.position = player_->GetPosition();
	p.count = 10;
	p.color = kColorRed;
	p.speedMin = 0.06f;
	p.speedMax = 0.16f;
	p.scale = 0.16f;
	p.life = 0.5f;
	burst_.Emit(p);
}

//--------------------------------------------------
// 加点＋ポップアップ表示
//--------------------------------------------------
void AlmaGameScene::AwardPopup(const std::string& label, int base, const AlmaVector3& worldPos, const AlmaVector4& color) {
	int points = score_.AddBonus(base);
	hud_.AddPopup(label + " +" + std::to_string(points), ToScreen(worldPos + AlmaVector3(0.0f, 1.0f, 0.0f)), color, 2.0f);
	hud_.PulseScore();
}

AlmaVector2 AlmaGameScene::ToScreen(const AlmaVector3& p) const { return FxUtil::WorldToScreen(p, *camera_); }

float AlmaGameScene::GetDistance() const { return std::max(0.0f, player_->GetPosition().x - Biome::kStartOffset); }

//==================================================
// フェーズ切替
//==================================================
void AlmaGameScene::ChangePhese() {
	switch (phase_) {
	case AlmaPhase::kPlay:
		if (player_->IsDead()) {
			phase_ = AlmaPhase::kDeath;
			score_.SetDistance(GetDistance());
			score_.Finish();
			const AlmaVector3& deathParticlesPosition = player_->GetWorldPosition();
			delete deathParticles_;
			deathParticles_ = new AlmaDeathParticles;
			deathParticles_->Initialize(dethParticleModel, camera_, deathParticlesPosition);
			hud_.Flash({1.0f, 0.2f, 0.2f, 0.6f}, 0.6f);
			cController_->StartShake(0.5f, 0.5f);
		}
		break;
	default:
		break;
	}
}

//==================================================
// ブロック1マス（本体＋草の天面＋奥行き）を生成
//==================================================
AlmaGameScene::AlmaBlockCell* AlmaGameScene::CreateBlockCell(uint32_t x, uint32_t y) {
	AlmaBlockCell* cell = new AlmaBlockCell();
	AlmaVector3 pos = mapChipField_->GetMapChipPositionByIndex(x, y);
	cell->biome = Biome::IndexAtColumn(x);

	cell->main.Initialize();
	cell->main.translation_ = pos;
	AlmaWorldTransformUpdate(cell->main);

	// 上が空いていれば天面（草・砂など）を乗せる
	bool exposed = (y == 0) || !IsSolidChip(mapChipField_->GetMapChipTypeByIndex(x, y - 1));
	const AlmaVector3 topScale = {1.03f, 0.16f, 1.03f};
	const float topOffset = 0.5f - 0.07f;
	if (exposed) {
		cell->hasTop = true;
		cell->top.Initialize();
		cell->top.translation_ = pos + AlmaVector3(0.0f, topOffset, 0.0f);
		cell->top.scale_ = topScale;
		AlmaWorldTransformUpdate(cell->top);
	}

	// 地面は奥に2列並べて立体感を出す
	if (y >= 18) {
		cell->hasBack = true;
		for (int k = 0; k < 2; ++k) {
			float z = 1.0f + static_cast<float>(k);
			cell->back[k].Initialize();
			cell->back[k].translation_ = pos + AlmaVector3(0.0f, 0.0f, z);
			AlmaWorldTransformUpdate(cell->back[k]);
			if (exposed) {
				cell->backTop[k].Initialize();
				cell->backTop[k].translation_ = pos + AlmaVector3(0.0f, topOffset, z);
				cell->backTop[k].scale_ = topScale;
				AlmaWorldTransformUpdate(cell->backTop[k]);
			}
		}
	}
	return cell;
}

//==================================================
// 列ごとの破棄
//==================================================
void AlmaGameScene::PurgeColumn(uint32_t x) {
	for (uint32_t y = 0; y < kRows; ++y) {
		GridDelete(blockCells_, y, x);
		GridDelete(worldTransformSpikes_, y, x);
		GridDelete(worldTransformSprings_, y, x);
		GridDelete(worldTransformFires_, y, x);
		GridDelete(worldTransformCoins_, y, x);
		GridDelete(worldTransformBreakables_, y, x);
		GridDelete(worldTransformBoosts_, y, x);
	}
}

//==================================================
// 動的生成・破棄（新しく見えてきた列だけ生成する）
//==================================================
void AlmaGameScene::UpdateDynamicBlocks() {
	int playerXIndex = static_cast<int>(mapChipField_->GetMapChipIndexSetByposition(player_->GetPosition()).xIndex);

	// 必要な列数まで vector を拡張
	size_t neededColumns = static_cast<size_t>(playerXIndex + kWindowAhead + 1);
	ResizeGrid(blockCells_, kRows, neededColumns);
	ResizeGrid(worldTransformSpikes_, kRows, neededColumns);
	ResizeGrid(worldTransformSprings_, kRows, neededColumns);
	ResizeGrid(worldTransformFires_, kRows, neededColumns);
	ResizeGrid(worldTransformCoins_, kRows, neededColumns);
	ResizeGrid(worldTransformBreakables_, kRows, neededColumns);
	ResizeGrid(worldTransformBoosts_, kRows, neededColumns);

	// 描画・更新範囲
	windowStart_ = std::max(0, playerXIndex - kWindowBehind);
	windowEnd_ = playerXIndex + kWindowAhead;

	// 新しい列だけ生成
	int genStart = std::max(windowStart_, generatedUntil_ + 1);
	for (int xi = genStart; xi <= windowEnd_; ++xi) {
		uint32_t x = static_cast<uint32_t>(xi);
		for (uint32_t y = 0; y < kRows; ++y) {
			AlmaMapChipType type = mapChipField_->GetMapChipTypeByIndex(x, y);
			AlmaVector3 pos = mapChipField_->GetMapChipPositionByIndex(x, y);

			switch (type) {
			case AlmaMapChipType::kBlock:
				blockCells_[y][x] = CreateBlockCell(x, y);
				break;
			case AlmaMapChipType::kSpike: {
				AlmaWorldTransform* wt = new AlmaWorldTransform();
				wt->Initialize();
				wt->translation_ = pos;
				wt->scale_ = {0.5f, 0.5f, 0.5f};
				wt->rotation_.y = -std::numbers::pi_v<float> / 2.0f;
				AlmaWorldTransformUpdate(*wt);
				worldTransformSpikes_[y][x] = wt;
			} break;
			case AlmaMapChipType::kSpring: {
				AlmaWorldTransform* wt = new AlmaWorldTransform();
				wt->Initialize();
				wt->scale_ = {0.5f, 0.5f, 0.5f};
				wt->translation_ = pos;
				AlmaWorldTransformUpdate(*wt);
				worldTransformSprings_[y][x] = wt;
			} break;
			case AlmaMapChipType::kMovingFire: {
				AlmaWorldTransform* wt = new AlmaWorldTransform();
				wt->Initialize();
				wt->translation_ = pos;
				wt->scale_ = {0.5f, 0.5f, 0.5f};
				AlmaWorldTransformUpdate(*wt);
				worldTransformFires_[y][x] = wt;
			} break;
			case AlmaMapChipType::kCoin: {
				AlmaWorldTransform* wt = new AlmaWorldTransform();
				wt->Initialize();
				wt->scale_ = {0.4f, 0.4f, 0.4f};
				wt->translation_ = pos;
				wt->rotation_.y = static_cast<float>(rand()) / RAND_MAX * 3.14159265f * 2.0f;
				AlmaWorldTransformUpdate(*wt);
				worldTransformCoins_[y][x] = wt;
			} break;
			case AlmaMapChipType::kBreakable: {
				AlmaWorldTransform* wt = new AlmaWorldTransform();
				wt->Initialize();
				wt->translation_ = pos;
				AlmaWorldTransformUpdate(*wt);
				worldTransformBreakables_[y][x] = wt;
			} break;
			case AlmaMapChipType::kBoostPad: {
				// 床に貼り付いた薄い光るパネル
				AlmaWorldTransform* wt = new AlmaWorldTransform();
				wt->Initialize();
				wt->translation_ = pos + AlmaVector3(0.0f, -0.5f + 0.05f, 0.0f);
				wt->scale_ = {0.96f, 0.1f, 0.96f};
				AlmaWorldTransformUpdate(*wt);
				worldTransformBoosts_[y][x] = wt;
			} break;
			case AlmaMapChipType::kEnemy: {
				AlmaEnemy* newEnemy = new AlmaEnemy();
				newEnemy->Initialize(enemyModel_, camera_, pos, mapChipField_);
				enemies_.push_back(newEnemy);
				mapChipField_->SetMapChipType(x, y, AlmaMapChipType::kBlank); // 重複生成防止
			} break;
			case AlmaMapChipType::kEnemy2: {
				AlmaEnemy2* newEnemy2 = new AlmaEnemy2();
				newEnemy2->Initialize(enemyModel2_, camera_, pos, mapChipField_);
				enemies2_.push_back(newEnemy2);
				mapChipField_->SetMapChipType(x, y, AlmaMapChipType::kBlank); // 重複生成防止
			} break;
			case AlmaMapChipType::kFallingSpike: {
				// プレス機はマップチップ1つにつき1台（幅はマップ生成側で決める）
				AlmaFallingSpike* fs = new AlmaFallingSpike();
				fs->Initialize(modelBlock_, camera_, pos, mapChipField_, cController_);
				fallingSpikes_.push_back(fs);
				mapChipField_->SetMapChipType(x, y, AlmaMapChipType::kBlank); // 重複生成防止
			} break;
			case AlmaMapChipType::kRisingSpike: {
				AlmaRisingSpike* rs = new AlmaRisingSpike();
				rs->Initialize(spikeModel_, camera_, pos, mapChipField_);
				risingSpikes_.push_back(rs);
				mapChipField_->SetMapChipType(x, y, AlmaMapChipType::kBlank); // 重複生成防止
			} break;
			default:
				break;
			}
		}
	}
	generatedUntil_ = std::max(generatedUntil_, windowEnd_);

	// 範囲外のオブジェクトを破棄してメモリ解放
	int purgeEndX = playerXIndex - (kWindowBehind + 1);
	for (int xi = purgedUntil_ + 1; xi <= purgeEndX; ++xi) {
		PurgeColumn(static_cast<uint32_t>(xi));
	}
	purgedUntil_ = std::max(purgedUntil_, purgeEndX);

	// 画面外の落下プレス機を破棄
	fallingSpikes_.remove_if([purgeEndX, this](AlmaFallingSpike* fs) {
		auto index = mapChipField_->GetMapChipIndexSetByposition(fs->GetPosition());
		if (static_cast<int>(index.xIndex) <= purgeEndX) {
			delete fs;
			return true;
		}
		return false;
	});

	// 画面外の飛び出すトゲを破棄
	risingSpikes_.remove_if([purgeEndX, this](AlmaRisingSpike* rs) {
		auto index = mapChipField_->GetMapChipIndexSetByposition(rs->GetPosition());
		if (static_cast<int>(index.xIndex) <= purgeEndX) {
			delete rs;
			return true;
		}
		return false;
	});
}
