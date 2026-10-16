// 敵味方の弾に共通する位置・速度・寿命・描画設定を扱う。具体的な移動は派生クラスで変更できる。
#include "App/Game/Common/Bullet/BaseBullet.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include <cassert>
// 弾の描画オブジェクトを作り、モデル・寿命・初期位置を準備する。
void BaseBullet::Initialize(Model* model)
{
    assert(model != nullptr);
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetFogEnabled(fogEnabled_);
    object_->SetEnableLighting(true);
    object_->SetModel(model);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

// 前回位置を保存して移動し、経過時間が寿命を超えたら弾を消滅扱いにする。
void BaseBullet::Update()
{
    if (!isAlive_) {
        return; // 弾が生存していない場合は更新処理を行わない
    }

    previousPosition_ = transform_.translate;
    Move(); // 弾の移動処理を行う

    lifeTime_ += TimeManager::GetInstance()->GetDeltaTime();

    if (lifeTime_ >= maxLifeTime_) { // 最大寿命時間を超えた場合は弾を消滅させる
        SetDead();
    }

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    object_->Update();
}

// 現在表示するモデルや追加パーツを描画する。
void BaseBullet::Draw()
{
    if (!isAlive_) {
        return;
    }

    object_->Draw();
}

// 描画対象をレイトレーシング用のシーンへ登録する。
void BaseBullet::SubmitRaytracing(DxrRenderer& renderer) {
    if (isAlive_ && object_) { object_->SubmitRaytracing(renderer); }
}

void BaseBullet::SetScale(const Vector3& scale)
{
    transform_.scale = scale;

    if (object_ == nullptr) {
        return;
    }

    object_->SetScale(transform_.scale);
    object_->Update();
}

void BaseBullet::SetColor(const Vector4& color)
{
    if (object_ == nullptr) {
        return;
    }

    object_->SetColor(color);
    object_->Update();
}

// 配置位置を変更し、初回の移動区間が不自然にならないよう前回位置も合わせる。
void BaseBullet::SetTranslate(const Vector3& translate)
{
    transform_.translate = translate;
    previousPosition_ = translate;

    if (object_ == nullptr) {
        return;
    }

    // Spawn Sync
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
    object_->Update();
}

// 速度に従って位置を進める。特殊な軌道を持つ弾はこの処理を上書きする。
void BaseBullet::Move()
{
    const float timeScale =GetTimeScale() * TimeManager::GetInstance()->GetDeltaTime() * 60.0f;
    transform_.translate.x += velocity_.x * timeScale;
    transform_.translate.y += velocity_.y * timeScale;
    transform_.translate.z += velocity_.z * timeScale;
}

// 弾を消滅扱いにする。実際の所有オブジェクトの削除は管理側が行う。
void BaseBullet::SetDead()
{
    isAlive_ = false;
}

// 弾を霧の影響に含めるかを設定し、描画オブジェクトへ反映する。
void BaseBullet::SetFogEnabled(bool enabled)
{
    fogEnabled_ = enabled;
    if (object_) object_->SetFogEnabled(enabled);
}
void BaseBullet::SetCamera(Camera* camera)
{
    camera_ = camera;

    if (object_ != nullptr) {
        object_->SetCamera(camera_);
    }
}

void BaseBullet::SetEnableLighting(bool enable)
{
    if (object_ != nullptr) {
        object_->SetEnableLighting(enable);
    }
}

// 弾の見た目に使うピクセルシェーダーを切り替える。
void BaseBullet::SetPixelShaderPath(const std::string& pixelShaderPath)
{
    if (object_ != nullptr) {
        object_->SetPixelShaderPath(pixelShaderPath);
    }
}

void BaseBullet::SetTransparent(bool transparent)
{
    if (object_ != nullptr) {
        object_->SetTransparent(transparent);
    }
}

// 半透明の弾が深度を書き込むかを変更し、他の描画との重なり方を調整する。
void BaseBullet::SetTransparentDepthWrite(bool enable)
{
    if (object_ != nullptr) {
        object_->SetTransparentDepthWrite(enable);
    }
}

void BaseBullet::SetCastShadow(bool enabled)
{
    if (object_ != nullptr) {
        object_->SetCastShadow(enabled);
    }
}

