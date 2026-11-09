#include "Fade.h"
#include <algorithm>

AlmaFade::~AlmaFade() { delete sprite_; }

void AlmaFade::Initialize() {
	// 02_13_page10
	// スプライト生成

	sprite_ = KamataEngine::AlmaSprite::Create(0, KamataEngine::AlmaVector2{});
	// サイズ指定
	sprite_->SetSize(KamataEngine::AlmaVector2(1280, 720));
	// 色指定
	sprite_->SetColor(KamataEngine::AlmaVector4(0, 0, 0, 1));
}

void AlmaFade::Update() {
	// フェード状態による分岐
	switch (status_) {
	case AlmaFade::AlmaStatus::None:
		// 何もしない
		break;
	case AlmaFade::AlmaStatus::FadeIn:
		// フェードイン中の更新処理
		// 1フレーム分の秒数をカウントアップ
		counter_ += 1.0f / 60.0f;
		// フェード継続時間に達したら打ち止め
		if (counter_ >= duration_) {
			counter_ = duration_;
		}
		// 0.0fから1.0fの間で、経過時間がフェード継続時間に近づくほどアルファ値を大きくする
		sprite_->SetColor(KamataEngine::AlmaVector4(0, 0, 0, std::clamp(1.0f - counter_ / duration_, 0.0f, 1.0f)));

		break;
	case AlmaFade::AlmaStatus::FadeOut:
		// フェードアウト中の更新処理
		// 1frame分の秒数をカウントアップ
		counter_ += 1.0f / 60.0f;
		// フェードが継続時間に達したら打ち止め
		if (counter_ >= duration_) {
			counter_ = duration_;
		}
		// 0.0fから1.0fの間で経過時間がフェード継続時間に近づくほどアルファ値を大きくする
		sprite_->SetColor(KamataEngine::AlmaVector4(0, 0, 0, std::clamp(counter_ / duration_, 0.0f, 1.0f)));
		break;
	default:
		break;
	}
}

void AlmaFade::Draw() {

	// 02_13 24枚目
	if (status_ == AlmaStatus::None) {
		return;
	}
	// よくわからんがなんかPSOセットしてシグネチャセットして設定をコマンドリストに反映
	KamataEngine::AlmaSprite::PreDraw(KamataEngine::AlmaDirectXCommon::GetInstance()->GetCommandList());

	sprite_->Draw();

	KamataEngine::AlmaSprite::PostDraw();
}
void AlmaFade::Start(AlmaStatus status, float duration) {
	status_ = status;
	duration_ = duration;
	counter_ = 0.0f;
}
// フェード停止関数
void AlmaFade::Stop() { status_ = AlmaStatus::None; }

bool AlmaFade::IsFinished() const {

	switch (status_) {
	case AlmaStatus::FadeIn:
	case AlmaFade::AlmaStatus::FadeOut:

		if (counter_ >= duration_) {
			return true;
		} else {

			return false;
		}
	}

	return true;
}
