#include "App/Game/Player/PlayerWeaponController.h"
#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include <cmath>
#include "App/Game/Common/CombatTarget.h"
#include "App/Game/Common/GameTiming.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include <algorithm>
#include "Engine/audio/SoundManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/Input/Input.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Winapp/WinApp.h"
#include <numbers>
#include "Engine/math/MatrixMath.h"
#include "Engine/3D/Object3dManager.h"
#include <cassert>

// PlayerBullet
// プレイヤー弾に共通する敵への命中処理を定義し、敵弾とは別の型として扱う。

// 弾種固有の命中演出を追加するための共通の呼び出し口。標準では何も行わない。
void PlayerBullet::OnHitEnemy(const Vector3& position)
{
}

// NormalBullet
// プレイヤーの通常弾の描画設定と、敵への命中時のエフェクトを扱う。
// 通常弾の色・シェーダー・霧や影の設定を適用し、機体から見やすい弾にする。
void NormalBullet::Initialize(Model* model)
{
	// ここでは、BaseBulletのInitialize関数を呼び出して、モデルの設定やオブジェクトの初期化を行います。
	BaseBullet::Initialize(model);
	SetEnableLighting(false);
	SetColor({ 0.12f, 0.48f, 1.0f, 1.0f });
	SetPixelShaderPath("resources/Shaders/Object3D/AssaultBullet/Render.PS.hlsl");
	SetFogEnabled(false);
	SetTransparent(true);
	SetTransparentDepthWrite(false);
	SetCastShadow(false);
	constexpr Vector3 kAssaultTracerScale { 0.36f, 0.36f, 0.50f };
	transform_.scale = kAssaultTracerScale;
	transform_.rotate = { 0.0f, 0.0f, 0.0f };
	transform_.translate = { 0.0f, 0.0f, 0.0f };
}

void NormalBullet::AlignToFlight(const Vector3& velocity)
{
	const float horizontalSpeed = std::hypot(velocity.x, velocity.z);
	transform_.rotate = {
		-std::atan2(velocity.y, horizontalSpeed),
		-std::atan2(velocity.x, velocity.z),
		0.0f
	};
}

// 共通の移動と寿命処理を使い、通常弾を進める。
void NormalBullet::Update()
{
	BaseBullet::Update();
}

// 武器切替・冷却・ミサイル間隔を更新し、選択中の武器へ射撃入力を渡す。
void PlayerWeaponController::UpdateInput(Input* input, bool isDebugMode)
{
	// デバッグカメラ操作中でなければ、武器切替の入力を処理する
	if (!isDebugMode) {
		UpdateWeaponSwitch(input);
	}
	const float dt = TimeManager::GetInstance()->GetDeltaTime();
	UpdateCooldowns(dt);//ミサイルとホーミングミサイルのクールダウン処理

	// ホーミングを選択中か
	const bool isHoming = currentWeapon_ == kWeaponHomingMissile;
	// アサルトを選択中か
	const bool isAssault = currentWeapon_ == kWeaponAssault;
	// デバッグ中かホーミング以外ならロック解除
	if (isDebugMode || !isHoming) { ResetHomingLock(); }

	// 射撃の基本条件を満たしているか
	const bool canFire = CanFire(isDebugMode);

	// 射撃入力を取得
	const FireInput fireInput = ReadFireInput(input);

	// 射撃状態に応じて熱量を更新
	UpdateAssaultHeat(dt, canFire && isAssault && fireInput.isHeld);

	
	if (!canFire) {
		ResetHomingLock();
		return;
	}

	switch (currentWeapon_) {
	case kWeaponAssault:
		UpdateAssaultInput(dt, fireInput.isHeld, fireInput.isTriggered);
		break;
	case kWeaponMissileBullet:
		UpdateMissileInput(fireInput.isTriggered);
		break;
	case kWeaponHomingMissile:
		UpdateHomingInput(fireInput.isHeld);
		break;
	}
}

// ボタン状態と入力禁止を確認
PlayerWeaponController::FireInput PlayerWeaponController::ReadFireInput(Input* input)
{
	// 射撃ボタンを押しているか
	const bool isPhysicalFireHeld = IsPhysicalFireHeld(input);
	// メニュー操作後は一度離すまで待つ
	if (isFireBlocked_) {
		if (!isPhysicalFireHeld) { isFireBlocked_ = false; }
		return {};
	}
	// いずれかの射撃ボタンを押した瞬間
	const bool isTriggered = input->IsKeyTrigger(DIK_SPACE)|| input->IsMouseTrigger(0)|| input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_A)|| input->IsGamepadRightTriggerTrigger();

	// 長押しと押した瞬間をまとめて返す
	return { isPhysicalFireHeld, isTriggered };
}

// アサルトは押した瞬間と、押し続けた時間に応じて連射する。
void PlayerWeaponController::UpdateAssaultInput(float deltaTimeSeconds, bool isFireHeld, bool isTriggered)
{
	const int requestedShotCount = GetRequestedShotCount(isFireHeld, isTriggered, deltaTimeSeconds);
	for (int shotIndex = 0; shotIndex < requestedShotCount; ++shotIndex) {
		FireBullet(*camera_);
	}
}

// ミサイルはボタンを押した瞬間に発射を要求する。
void PlayerWeaponController::UpdateMissileInput(bool isTriggered)
{
	if (isTriggered) {
		FireBullet(*camera_);
	}
}

// ホーミングは押している間にロックし、離した瞬間に発射する。
void PlayerWeaponController::UpdateHomingInput(bool isFireHeld)
{
	const bool shouldFire = UpdateHomingFireInput(isFireHeld, IsWeaponReady(),
		camera_, transform_.translate, player_.GetFlightForward(), player_.GetAimScreenPosition());
	if (shouldFire) {
		const bool isFired = FireBullet(*camera_);
		if (isFired) {
			lockedTargets_.clear();
		}
	}
}

// 通常弾の命中位置へヒットエフェクトを出す。
void NormalBullet::OnHitEnemy(const Vector3& position)
{
	EffectManager::GetInstance()->PlayEffect("NormalBulletHit",position);

	EffectManager::GetInstance()->PlayEffect("NormalBulletImpactFlash",position);
}

// MissileBullet
// ミサイルの描画・威力と航跡エフェクトを扱う。消滅時には航跡も停止する。


// ミサイルの見た目と威力を設定し、追従する航跡エフェクトを開始する。
void MissileBullet::Initialize(Model* model)
{
	PlayerBullet::Initialize(model);
	SetEnableLighting(false);
	SetColor({ 0.12f, 0.48f, 1.0f, 1.0f });
	SetPixelShaderPath("resources/Shaders/Object3D/PlayerBullet/Render.PS.hlsl");
	SetFogEnabled(false);
	SetTransparent(true);
	SetTransparentDepthWrite(false);
	SetCastShadow(false);

	SetDamage(10);

	transform_.scale = {
		scale_,
		scale_,
		scale_
	};

	CreateTrailEffect();
}

// 移動と寿命を更新し、航跡を追従させる。消滅した場合は航跡を停止する。
void MissileBullet::Update()
{
	PlayerBullet::Update();

	UpdateTrailPosition();

	if (!IsAlive()) {
		StopTrailEffect();
	}
}

// 本体の位置を設定し、航跡の追従先も同じ位置へ更新する。
void MissileBullet::SetTranslate(const Vector3& translate)
{
	PlayerBullet::SetTranslate(translate);
	UpdateTrailPosition();
}

// 弾を消滅扱いにし、その場に航跡だけが残り続けないよう停止する。
void MissileBullet::SetDead()
{
	PlayerBullet::SetDead();

	StopTrailEffect();
}

// 命中位置にミサイル用の演出を出す。
void MissileBullet::OnHitEnemy(const Vector3& position)
{
	EffectManager::GetInstance()->PlayEffect(
		"MissileExplosion",
		position);
	EffectManager::GetInstance()->PlayEffect(
		"MissileExplosionParticles",
		position);
	EffectManager::GetInstance()->PlayEffect(
		"MissileExplosionRing",
		position);
	EffectManager::GetInstance()->PlayEffect(
		"MissileExplosionFlash",
		position);
}

PlayerBulletExplosionSettings MissileBullet::GetExplosionSettingsFromConfig(const nlohmann::json& config)
{
	if (!config.is_object() || !config.contains("Particle") || !config.at("Particle").is_object() ||
		!config.contains("Render") || !config.at("Render").is_object()) {
		return {};
	}
	const auto& render = config.at("Render");
	if (!render.contains("MeshType") || render.at("MeshType") != "Sphere") { return {}; }
	if (render.contains("Type") && render.at("Type") != "Mesh") { return {}; }
	const auto& particle = config.at("Particle");
	if (!particle.contains("EndScale") || !particle.at("EndScale").is_number()) { return {}; }
	const float radius = particle.at("EndScale").get<float>();
	if (!std::isfinite(radius) || radius <= 0.0f) { return {}; }
	return { radius, kExplosionDamage };
}

PlayerBulletExplosionSettings MissileBullet::GetExplosionSettings() const
{
	nlohmann::json config;
	std::string error;
	// 読み込まれていない演出の半径を推測して、見えない爆風を発生させない。
	if (!EffectManager::GetInstance()->GetEffectConfig("MissileExplosion", config, error)) { return {}; }
	return GetExplosionSettingsFromConfig(config);
}

// ミサイルの位置を追いかけるループエフェクトを生成する。
void MissileBullet::CreateTrailEffect()
{
	trailPosition_ = std::make_shared<Vector3>(transform_.translate);

	UpdateTrailPosition();

	std::shared_ptr<Vector3> trailPosition = trailPosition_;

	trailEffectHandle_ = EffectManager::GetInstance()->AttachEffect(
		"MissileTrail",
		[trailPosition]() {
			return *trailPosition;
		});
}

// 弾の位置に航跡の発生位置を追従させる。
void MissileBullet::UpdateTrailPosition()
{
	if (!trailPosition_) {
		return;
	}

	Vector3 trailPosition = transform_.translate;

	float velocityLength = std::sqrt(
		velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);

	if (velocityLength > 0.001f) {
		trailPosition.x -= velocity_.x / velocityLength * 2.0f;
		trailPosition.y -= velocity_.y / velocityLength * 2.0f;
		trailPosition.z -= velocity_.z / velocityLength * 2.0f;
	}

	*trailPosition_ = trailPosition;
}

// 再生中の航跡を停止し、エフェクトのハンドルを無効化する。
void MissileBullet::StopTrailEffect()
{
	if (trailEffectHandle_ == kInvalidEffectHandle) {
		return;
	}

	EffectManager::GetInstance()->StopEffect(
		trailEffectHandle_);

	trailEffectHandle_ = kInvalidEffectHandle;
}

// HomingMissileBullet
// 生存しているロック対象へ進行方向を曲げ、目標を通り過ぎた回数も管理するミサイル。

void HomingMissileBullet::OnHitEnemy(const Vector3& position)
{
	EffectManager::GetInstance()->PlayEffect("NormalBulletImpactFlash", position);
}

// ロック対象と候補一覧を記録する。候補一覧は弱参照で保持し、寿命を引き延ばさない。
void HomingMissileBullet::SetTarget(CombatTarget* target, const std::shared_ptr<const std::vector<CombatTarget*>>& activeTargets)
{
	target_ = target;
	activeTargets_ = activeTargets;
	targetPassCount_ = 0;
}

// 対象の生存を確認して進行方向を曲げ、接近時の補正と通過回数によって追尾を制御する。
void HomingMissileBullet::Move()
{
	const std::shared_ptr<const std::vector<CombatTarget*>> activeTargets = activeTargets_.lock();
	const bool targetIsActive = activeTargets != nullptr &&
		std::find(activeTargets->begin(), activeTargets->end(), target_) != activeTargets->end();

	if (targetIsActive && target_ != nullptr && !target_->IsDead()) {
		const Vector3 targetPosition = target_->GetPosition();
		const Vector3 toTarget = targetPosition - transform_.translate;
		const float distanceSquared = Dot(toTarget, toTarget);
		if (distanceSquared > 0.0001f) {
			const float speed = std::sqrt(Dot(velocity_, velocity_));
			const Vector3 desiredVelocity = Normalize(toTarget) * speed;
			const float frameScale = TimeManager::GetInstance()->GetDeltaTime() * 60.0f;
			const float distance = std::sqrt(distanceSquared);
			const float closeRatio = std::clamp(
				1.0f - distance / kCloseHomingDistance,
				0.0f,
				1.0f);
			const float steeringStrength =
				homingStrength_ +
				(kCloseHomingStrength - homingStrength_) * closeRatio;
			const float blend = GameTiming::CalculateFollowBlend(steeringStrength, TimeManager::GetInstance()->GetDeltaTime());
			velocity_ = Normalize(velocity_ * (1.0f - blend) + desiredVelocity * blend) * speed;

			const Vector3 frameMovement = velocity_ * frameScale;
			const Vector3 afterMoveToTarget = toTarget - frameMovement;
			const bool passesTargetThisFrame =
				Dot(toTarget, velocity_) > 0.0f &&
				Dot(afterMoveToTarget, velocity_) <= 0.0f;
			if (passesTargetThisFrame) {
				const float movementSquared = Dot(frameMovement, frameMovement);
				float closestDistanceSquared = distanceSquared;
				if (movementSquared > 0.0001f) {
					const float closestTime = std::clamp(
						Dot(toTarget, frameMovement) / movementSquared,
						0.0f,
						1.0f);
					const Vector3 closestOffset =
						toTarget - frameMovement * closestTime;
					closestDistanceSquared = Dot(closestOffset, closestOffset);
				}

				if (closestDistanceSquared <=
					kTargetSnapDistance * kTargetSnapDistance) {
					transform_.translate = targetPosition;
					return;
				}

				++targetPassCount_;
				if (targetPassCount_ >= kMaxTargetPassCount) {
					SetDead();
					return;
				}
			}
		}
	}
	else {
		target_ = nullptr;
	}

	MissileBullet::Move();
}

// PlayerWeaponController
// 選択中の武器を切り替え、発射・発熱・ロックオンと生成済みのプレイヤー弾を管理する。



// 前回の戦闘状態を破棄し、プレイヤー弾の表示に使うモデルを読み込む。
void PlayerWeaponController::Initialize()
{
	ResetCombatState();
	bulletModel_ = ModelManager::GetInstance()->Load("Enemy/Bullet/LowPolySphere/lowPolySphere.obj");
}
// クールダウンの残り時間を減らし、0未満になった場合は0に揃える。
void PlayerWeaponController::UpdateCooldowns(float deltaTimeSeconds)
{
	missileCooldownRemainingSeconds_ -= deltaTimeSeconds;
	if (missileCooldownRemainingSeconds_ < 0.0) {
		missileCooldownRemainingSeconds_ = 0.0;
	}
	homingCooldownRemainingSeconds_ -= deltaTimeSeconds;
	if (homingCooldownRemainingSeconds_ < 0.0) {
		homingCooldownRemainingSeconds_ = 0.0;
	}
}

// 選択中の武器が発射可能かどうかを返す。クールダウン中はfalse。
bool PlayerWeaponController::IsWeaponReady() const
{
	constexpr float kCooldownToleranceSeconds = 0.000001f;
	if (currentWeapon_ == kWeaponMissileBullet) {
		if (missileCooldownRemainingSeconds_ > kCooldownToleranceSeconds) {
			return false;
		}
		return true;
	}
	if (currentWeapon_ == kWeaponHomingMissile) {
		if (homingCooldownRemainingSeconds_ > kCooldownToleranceSeconds) {
			return false;
		}
		return true;
	}
	return true;
}

// 発射後に、選択中の武器のクールダウンを開始する。
void PlayerWeaponController::StartCooldown()
{
	if (currentWeapon_ == kWeaponMissileBullet) { missileCooldownRemainingSeconds_ = kMissileFireIntervalSeconds; }
	if (currentWeapon_ == kWeaponHomingMissile) { homingCooldownRemainingSeconds_ = kHomingFireIntervalSeconds; }
}

float PlayerWeaponController::GetMissileReloadRatio() const
{
	if (IsWeaponReady()) { return 1.0f; }
	if (currentWeapon_ == kWeaponMissileBullet) {
		return std::clamp(static_cast<float>(1.0 - missileCooldownRemainingSeconds_ / kMissileFireIntervalSeconds), 0.0f, 1.0f);
	}
	return std::clamp(static_cast<float>(1.0 - homingCooldownRemainingSeconds_ / kHomingFireIntervalSeconds), 0.0f, 1.0f);
}

int PlayerWeaponController::GetRequestedShotCount(bool isHeld, bool isTriggered, float deltaTimeSeconds)
{
	if (currentWeapon_ != kWeaponAssault) {
		if (isTriggered) { return 1; }
		return 0;
	}
	int requestedShotCount = 0;
	if (isTriggered) {
		assaultCooldownSeconds_ = 0.0f;
		++requestedShotCount;
	}
	if (isHeld) {
		assaultCooldownSeconds_ += deltaTimeSeconds;
		while (assaultCooldownSeconds_ + 0.000001f >= kAssaultFireIntervalSeconds) {
			assaultCooldownSeconds_ = (std::max)(0.0f, assaultCooldownSeconds_ - kAssaultFireIntervalSeconds);
			++requestedShotCount;
		}
	}
	return requestedShotCount;
}
// 機体の姿勢と状態を参照する。参照先のPlayerは、この管理クラスより長く生存する必要がある。
PlayerWeaponController::PlayerWeaponController(const Player& player)
	: transform_(player.transform_), player_(player)
{
}



void PlayerWeaponController::ResetCombatState()
{
	// ミサイルの航跡など、弾種固有の終了処理を実行してから破棄する。
	for (const auto& bullet : bullets_) {
		bullet->SetDead();
	}
	bullets_.clear();
	ClearHomingTargets();
	heatGauge_ = 0.0f;
	isOverheated_ = false;
	graceRemainingSeconds_ = 0.0f;
	shotsFired_ = 0;
	isFireBlocked_ = false;
	currentWeapon_ = kWeaponAssault;
	assaultCooldownSeconds_ = 0.0f;
	missileCooldownRemainingSeconds_ = 0.0;
	isNextAssaultShotLeft_ = true;
	homingCooldownRemainingSeconds_ = 0.0;
#ifdef _DEBUG
	shouldDrawDebugLines_ = false;
	debugAimRayOrigin_ = {};
	debugAimPoint_ = {};
	debugMuzzlePosition_ = {};
	debugDrawRayOrigin_ = {};
	debugDrawAimPoint_ = {};
#endif
}

// 新しいカメラを記録し、すでに発射済みの弾にも同じカメラを設定する。
void PlayerWeaponController::SetCamera(Camera* camera)
{
	camera_ = camera;
	for (const auto& bullet : bullets_) {
		bullet->SetCamera(camera);
	}
}

void PlayerWeaponController::BlockFireUntilRelease()
{
	isFireBlocked_ = true;
	ResetHomingLock();
}
//撃てるかどうかを判定する関数。
bool PlayerWeaponController::CanFire(bool isDebugMode) const
{
	//プレイヤーが生きているかつバレットモデルがあるときかつカメラがあるかつデバッグモードではない場合
	return !player_.IsDead()&& bulletModel_ != nullptr&& camera_ != nullptr&& !isDebugMode;
}

bool PlayerWeaponController::IsPhysicalFireHeld(Input* input) const
{
	return input->IsKeyPressed(DIK_SPACE)
		|| input->IsMousePressed(0)
		|| input->IsGamepadButtonPressed(XINPUT_GAMEPAD_A)
		|| input->IsGamepadRightTriggerPressed();
}

// 追尾ミサイル選択中だけ、ロック表示に必要な敵の位置を書き出す。
void PlayerWeaponController::GetHomingLockPositions(std::vector<Vector3>& positions) const
{
	positions.clear();
	if (IsHomingMissileSelected()) {
		GetLockedPositions(positions);
	}
}

void PlayerWeaponController::DrawBullets()
{
	for (const auto& bullet : bullets_) {
		bullet->Draw();
	}
}

// 描画対象をレイトレーシング用のシーンへ登録する。
void PlayerWeaponController::SubmitRaytracing(DxrRenderer& renderer) {
	for (const auto& bullet : bullets_) {
		if (bullet) { bullet->SubmitRaytracing(renderer); }
	}
}

// 現在の武器へ発射を要求し、その武器固有の過熱やクールダウン判定を使う。
bool PlayerWeaponController::FireBullet(const Camera& activeCamera)
{
	if (player_.IsDead() || bulletModel_ == nullptr || camera_ == nullptr) { return false; }
	const bool isAssault = currentWeapon_ == kWeaponAssault;
	const bool isHoming = IsHomingMissileSelected();
	if (isAssault && IsOverheated()) { return false; }
	if (!IsWeaponReady()) { return false; }
	if (isAssault) {
		SoundManager::GetInstance()->Play("AssaultShot");
	}
	else {
		SoundManager::GetInstance()->Play("MissileShot");
	}
	if (isHoming && !lockedTargets_.empty()) {
		for (CombatTarget* target : lockedTargets_) {
			FireSingleBullet(activeCamera, target);
		}
	}
	else {
		FireSingleBullet(activeCamera, nullptr);
	}
	StartCooldown();
	if (isAssault) {
		isNextAssaultShotLeft_ = !isNextAssaultShotLeft_;
		AddShotHeat();
	}
	return true;
}
// 武器に弾を生成させ、熱量の色・銃口位置・照準方向・機体速度を反映して所有する。
void PlayerWeaponController::FireSingleBullet(const Camera& activeCamera, CombatTarget* homingTarget)
{

	float shotSpeed = bulletSpeed_;
	std::unique_ptr<PlayerBullet> bullet;
	NormalBullet* assaultBullet = nullptr;
	if (currentWeapon_ == kWeaponAssault) {
		auto normalBullet = std::make_unique<NormalBullet>();
		normalBullet->Initialize(bulletModel_);
		normalBullet->SetDamage(GetShotDamage(kAssaultBaseDamage));
		assaultBullet = normalBullet.get();
		bullet = std::move(normalBullet);
	}
	else if (currentWeapon_ == kWeaponMissileBullet) {
		auto missileBullet = std::make_unique<MissileBullet>();
		shotSpeed = missileBullet->GetSpeed() / 60.0f;
		missileBullet->Initialize(bulletModel_);
		bullet = std::move(missileBullet);
	}
	else {
		auto homingBullet = std::make_unique<HomingMissileBullet>();
		shotSpeed = homingBullet->GetSpeed() / 60.0f;
		homingBullet->Initialize(bulletModel_);
		homingBullet->SetTarget(homingTarget, homingTargets_);
		bullet = std::move(homingBullet);
	}
	constexpr Vector4 kColdBulletColor = { 0.12f, 0.48f, 1.0f, 1.0f };
	constexpr Vector4 kHotBulletColor = { 0.85f, 0.015f, 0.008f, 1.0f };
	const float heatRatio = std::clamp(GetHeatRatio(), 0.0f, 1.0f);
	const Vector4 bulletColor = {
		kColdBulletColor.x + (kHotBulletColor.x - kColdBulletColor.x) * heatRatio,
		kColdBulletColor.y + (kHotBulletColor.y - kColdBulletColor.y) * heatRatio,
		kColdBulletColor.z + (kHotBulletColor.z - kColdBulletColor.z) * heatRatio,
		1.0f
	};
	bullet->SetColor(bulletColor);
	bullet->SetCamera(camera_);
	Vector3 muzzlePosition = CalculateShotMuzzlePosition();
	EffectManager::GetInstance()->PlayEffect("ShotBullet", muzzlePosition);

	// 照準更新時に求めた目標点へ銃口を向ける。
	Vector3 aimPoint = player_.GetAimPoint();


#ifdef _DEBUG
	shouldDrawDebugLines_ = true;
	debugAimRayOrigin_ = player_.GetAimRayOrigin();
	debugAimPoint_ = aimPoint;
	debugMuzzlePosition_ = muzzlePosition;

	debugDrawRayOrigin_ = player_.GetAimRayOrigin();
	debugDrawAimPoint_ = player_.GetAimPoint();
#endif

	Vector3 bulletDirection = Normalize(aimPoint - muzzlePosition);
	Vector3 worldPlayerVelocity = player_.GetWorldVelocity();

	Vector3 bulletVelocity;
	bulletVelocity.x = bulletDirection.x * shotSpeed + worldPlayerVelocity.x;
	bulletVelocity.y = bulletDirection.y * shotSpeed + worldPlayerVelocity.y;
	bulletVelocity.z = bulletDirection.z * shotSpeed + worldPlayerVelocity.z;

	bullet->SetVelocity(bulletVelocity);
	if (assaultBullet != nullptr) {
		assaultBullet->AlignToFlight(bulletVelocity);
	}
	bullet->SetTranslate(muzzlePosition);

	// 発射後の弾は管理側が所有し、以後の更新・衝突判定・削除の対象にする。
	bullets_.push_back(std::move(bullet));
	++shotsFired_;
}

// 機体に対する銃口の相対位置を、現在の姿勢でワールド位置へ変換する。
Vector3 PlayerWeaponController::CalculateMuzzlePosition() const
{
	const Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	return MatrixMath::Transform({ 0.0f, bulletSpawnOffsetY_, bulletSpawnOffsetZ_ }, worldMatrix);
}

Vector3 PlayerWeaponController::CalculateShotMuzzlePosition() const
{
	if (currentWeapon_ == kWeaponAssault && player_.flightVisual_) {
		// 羽の内側寄りの付け根を使い、見た目の傾きやアニメーションにも追従する。
		const Skeleton& skeleton = *player_.flightVisual_->GetSkeleton();
		const char* jointName = isNextAssaultShotLeft_ ? "Wing_Fin_L_02" : "Wing_Fin_R_02";
		const auto joint = skeleton.GetJointMap().find(jointName);
		if (joint != skeleton.GetJointMap().end()) {
			const Vector3 modelPosition = MatrixMath::Transform(
				Vector3{}, skeleton.GetJoints()[joint->second].GetSkeletonSpaceMatrix());
			return MatrixMath::Transform(modelPosition, player_.flightVisual_->GetObject()->GetWorldMatrix());
		}
	}
	Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	Vector3 localMuzzle = { 0.0f, bulletSpawnOffsetY_, bulletSpawnOffsetZ_ };
	if (currentWeapon_ == kWeaponAssault) {
		// 羽の骨を持たないモデルでも左右交互に発射する。
		localMuzzle.x = isNextAssaultShotLeft_ ? -1.25f : 1.25f;
	}
	return MatrixMath::Transform(localMuzzle, worldMatrix);
}

// ホイール・LB/RB・数字キーの入力を武器の選択番号へ反映する。
void PlayerWeaponController::UpdateWeaponSwitch(Input* input)
{
	if (input->GetMouseWheel() > 0 || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
		currentWeapon_ = (currentWeapon_ + 1) % kWeaponCount;
	}

	if (input->GetMouseWheel() < 0 || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
		currentWeapon_ = (currentWeapon_ + kWeaponCount - 1) % kWeaponCount;
	}

	if (input->IsKeyTrigger(DIK_1)) currentWeapon_ = kWeaponAssault;
	if (input->IsKeyTrigger(DIK_2)) currentWeapon_ = kWeaponMissileBullet;
	if (input->IsKeyTrigger(DIK_3)) currentWeapon_ = kWeaponHomingMissile;
}

const char* PlayerWeaponController::GetCurrentWeaponName() const
{
	if (currentWeapon_ == kWeaponMissileBullet) { return "ミサイル"; }
	if (currentWeapon_ == kWeaponHomingMissile) { return "ホーミングミサイル"; }
	return "アサルト";
}
// 所有しているプレイヤー弾を一フレームずつ更新する。
void PlayerWeaponController::UpdateBullets()
{
	for (std::unique_ptr<PlayerBullet>& bullet : bullets_) {
		bullet->Update();
	}
}

// 命中や寿命で消滅した弾を一覧から外し、所有していたオブジェクトを破棄する。
void PlayerWeaponController::RemoveDeadBullets()
{
	for (uint32_t i = 0; i < bullets_.size();) {
		if (!bullets_[i]->IsAlive()) {
			bullets_.erase(bullets_.begin() + i);
		}
		else {
			++i;
		}
	}

}

#ifdef _DEBUG
// 照準レイと銃口からの射線を線で表示し、狙いのずれを確認できるようにする。
void PlayerWeaponController::DrawAimDebugLines()
{
	if (shouldDrawDebugLines_) {
		// AimCameraから飛ぶRay (青)
		DebugRenderer::GetInstance()->AddLine(debugAimRayOrigin_, debugAimPoint_, { 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f);

		// Playerのマズルから飛ぶ実際の弾の進行方向 (赤)
		DebugRenderer::GetInstance()->AddLine(debugMuzzlePosition_, debugAimPoint_, { 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f);

		// 描画用Cameraから飛ぶRay (黄色 - デバッグ比較専用)
		DebugRenderer::GetInstance()->AddLine(debugDrawRayOrigin_, debugDrawAimPoint_, { 1.0f, 1.0f, 0.0f, 1.0f }, 3.0f);
	}
}
#endif

// 発熱と冷却の共通処理。
// 熱量が高いほど威力を増やす。発射時点の熱量でその弾の威力を決める。
int PlayerWeaponController::GetShotDamage(int baseDamage) const
{
	// 発射時の熱量で威力を確定する。0%で等倍、100%で2倍。
	if (baseDamage <= 0) { return 0; }
	return static_cast<int>(std::lround(baseDamage * (1.0f + heatGauge_)));
}

// 連射中の過熱猶予と、射撃停止中・過熱中の冷却を進める。
void PlayerWeaponController::UpdateAssaultHeat(float deltaTimeSeconds, bool isAssaultFireActive)
{
	if (deltaTimeSeconds <= 0.0f) {
		return;
	}

	// 過熱中は冷却し、熱量0で解除
	if (isOverheated_) {
		heatGauge_ = (std::max)(0.0f,heatGauge_ - kOverheatCoolingPerSecond * deltaTimeSeconds);
		if (heatGauge_ <= 0.0f) {
			isOverheated_ = false;
		}
		return;
	}

	// 射撃停止中は猶予を終了して冷却
	if (!isAssaultFireActive) {
		graceRemainingSeconds_ = 0.0f;
		heatGauge_ = (std::max)(0.0f,heatGauge_ - kCoolingPerSecond * deltaTimeSeconds);
		return;
	}

	// 満タンで撃ち続けると猶予が減り、時間切れで過熱
	if (heatGauge_ >= 1.0f) {
		graceRemainingSeconds_ -= deltaTimeSeconds;
		if (graceRemainingSeconds_ <= 0.0f) {
			isOverheated_ = true;
		}
	}
}

// アサルト一発分の熱量を加え、満タンになったときの猶予を準備する。
void PlayerWeaponController::AddShotHeat()
{
	if (isOverheated_) {
		return;
	}
	//ヒートゲージ追加 100パーセント(0～1)を超えないようにminを使用
	heatGauge_ = (std::min)(1.0f, heatGauge_ + kAssaultShotHeat);
	// 熱量が100％以上で、猶予が0秒以下なら0.5秒の猶予を設定
	if (heatGauge_ >= 1.0f && graceRemainingSeconds_ <= 0.0f) {
		graceRemainingSeconds_ = kGraceDurationSeconds;
	}
}

// ロック対象の管理と発射タイミング。
namespace {
	struct LockCandidate {
		CombatTarget* enemy;
		float screenDistanceSquared;
	};

	bool CompareLockCandidates(const LockCandidate& left, const LockCandidate& right)
	{
		return left.screenDistanceSquared < right.screenDistanceSquared;
	}
}

// 現在の候補一覧を共有データへ反映し、弾が対象の生存を確認できるようにする。
void PlayerWeaponController::SetHomingTargets(const std::vector<CombatTarget*>& targets)
{
	*homingTargets_ = targets;
	for (auto targetIterator = lockedTargets_.begin(); targetIterator != lockedTargets_.end();) {
		if (std::find(targets.begin(), targets.end(), *targetIterator) == targets.end()) {
			targetIterator = lockedTargets_.erase(targetIterator);
		}
		else {
			++targetIterator;
		}
	}
}

void PlayerWeaponController::ClearHomingTargets()
{
	homingTargets_->clear();
	ResetHomingLock();
}

// ロック対象と前回の押下状態を消し、次の射撃入力を新しく受け付ける。
void PlayerWeaponController::ResetHomingLock()
{
	lockedTargets_.clear();
	wasHomingFireHeld_ = false;
}

// 押下中のロック更新と、ボタンを離した瞬間の発射要求を判定する。
bool PlayerWeaponController::UpdateHomingFireInput(bool isFireHeld, bool isEnabled, Camera* camera,
	const Vector3& position, const Vector3& forward, const Vector2& aimPosition)
{
	if (!isEnabled) {
		ResetHomingLock();
		return false;
	}
	if (isFireHeld && !wasHomingFireHeld_) {
		lockedTargets_.clear();
	}
	UpdateHomingTargets(isFireHeld, camera, position, forward, aimPosition);
	const bool shouldFire = !isFireHeld && wasHomingFireHeld_ && camera != nullptr;
	wasHomingFireHeld_ = isFireHeld;
	return shouldFire;
}

// 前方距離と画面上の照準との近さを調べ、条件を満たす敵を上限数までロックする。
void PlayerWeaponController::UpdateHomingTargets(bool isLocking, Camera* camera,
	const Vector3& position, const Vector3& forward, const Vector2& aimPosition)
{
	if (!isLocking ||
		camera == nullptr || lockedTargets_.size() >= kMaxLockCount) {
		return;
	}

	const std::size_t previousLockCount = lockedTargets_.size();
	constexpr float kLockRadiusPixels = 120.0f;
	std::vector<LockCandidate> candidates;
	for (CombatTarget* enemy : *homingTargets_) {
		if (enemy == nullptr || enemy->IsDead()) {
			continue;
		}
		const Vector3 toEnemy = enemy->GetPosition() - position;
		const float forwardDistance = Dot(toEnemy, forward);
		if (forwardDistance <= 0.0f ||
			forwardDistance > kMaxForwardDistance) {
			continue;
		}
		const Vector2 screenPosition = camera->WorldToScreen(enemy->GetPosition());
		const float differenceX = screenPosition.x - aimPosition.x;
		const float differenceY = screenPosition.y - aimPosition.y;
		const float distanceSquared = differenceX * differenceX + differenceY * differenceY;
		if (distanceSquared <= kLockRadiusPixels * kLockRadiusPixels) {
			candidates.push_back({ enemy, distanceSquared });
		}
	}

	std::sort(candidates.begin(), candidates.end(), CompareLockCandidates);
	for (const LockCandidate& candidate : candidates) {
		if (std::find(
			lockedTargets_.begin(),
			lockedTargets_.end(),
			candidate.enemy) != lockedTargets_.end()) {
			continue;
		}
		lockedTargets_.push_back(candidate.enemy);
		if (lockedTargets_.size() >= kMaxLockCount) {
			break;
		}
	}
	if (lockedTargets_.size() > previousLockCount) {
		SoundManager::GetInstance()->Play("HomingLock");
	}
}

// 生存中のロック対象の位置を集め、画面のロックマーカー表示に使う。
void PlayerWeaponController::GetLockedPositions(std::vector<Vector3>& positions) const
{
	positions.clear();
	for (CombatTarget* target : lockedTargets_) {
		if (target != nullptr && !target->IsDead()) {
			positions.push_back(target->GetPosition());
		}
	}
}

