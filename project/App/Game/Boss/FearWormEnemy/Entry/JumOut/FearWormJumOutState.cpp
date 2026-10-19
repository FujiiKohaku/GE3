#include "FearWormJumOutState.h"
#include "../FearWormEntryState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"

FearWormJumOutState::FearWormJumOutState() = default;
FearWormJumOutState::~FearWormJumOutState() = default;

void FearWormJumOutState::Enter(FearWormEntryState& entry, FearWormEnemy& boss)
{
    if (boss.player_ == nullptr) {
        return;
    }
    boss.orbitAngle_ = kInitialOrbitAngle;
    boss.ChangeMovementPattern(FearWormEnemy::MovementPattern::Orbit);
    boss.movementPatternTimer_ = 0.0f;
    // 経過時間0秒の水中配置と水面の予兆を、登場開始時から反映する。
    Update(entry, boss);
}

void FearWormJumOutState::Update(FearWormEntryState& entry, FearWormEnemy& boss)
{
    // 経過時間を更新する
    if (entry.IsHeadFlightFinished()) {
        entry.ChangePhase(FearWormEntryState::EntryPhase::WaitForBody, boss);
        return;
    }
    // セグメントが存在する場合、プレイヤーの位置に基づいて各セグメントの位置を計算する
    if (!boss.segments_.empty()) {
        // プレイヤーの現在位置を、出現位置と到着位置を決める基準にする。
        const Vector3 playerPosition = boss.player_->GetTranslate();
        // 飛び出しの出発点となる水面上の位置を求める。
        const Vector3 surfacePosition = entry.EntrySurfacePosition(playerPosition);
        // 登場後につながる戦闘中の移動先を、軌道の到着位置として求める。
        const Vector3 battlePosition = boss.MovementTargetPosition(playerPosition);
        // 頭から尾まで、部位番号に応じて開始時刻をずらした軌道上へ配置する。
        for (size_t index = 0; index < boss.segments_.size(); ++index) {
            // 登場全体の経過時間を使い、状態が変わっても部位ごとの時間差を保つ。
            boss.segments_[index].position = FearWormEntryState::CalculateEntryPosition(
                surfacePosition, battlePosition, entry.elapsedSeconds_, index, boss.segmentSpacing_);
        }
        // ボス本体の代表位置を、先頭の部位（頭）の位置に合わせる。
        boss.transform_.translate = boss.segments_[0].position;
    }
    // 頭が水面に達したら、水しぶきを一度だけ再生する。
    PlaySplashIfNeeded(entry, boss);
    // 飛び出し中も水面の影・波紋を更新し、水しぶき再生後は影を消す。
    UpdateSurface(entry, boss);
}

// セグメントが水面を突破したときに一度だけ水しぶきエフェクトを再生する
void FearWormJumOutState::PlaySplashIfNeeded(FearWormEntryState& entry, FearWormEnemy& boss)
{

    // すでに水しぶきエフェクトを再生済み、またはセグメントが存在しない、または最初のセグメントが水面より下にある場合は何もしない
    if (entry.hasPlayedSplash_ || boss.segments_.empty()|| boss.segments_[0].position.y < entry.waterHeight_) {
        return;
    }
    // 水しぶきエフェクトを再生する位置を計算する
    Vector3 splashPosition = boss.segments_[0].position;
    splashPosition.y = entry.waterHeight_ + kSplashHeightOffset;
    EffectManager::GetInstance()->PlayEffect("FearWormBreachSplash", splashPosition);
    entry.hasPlayedSplash_ = true;
}


void FearWormJumOutState::Initialize()
{
    surface_ = std::make_unique<Object3d>();
    surface_->Initialize(Object3dManager::GetInstance());
    surface_->SetModel(ModelManager::GetInstance()->CreatePlane());
    surface_->SetEnableLighting(false);
    surface_->SetMaterial("resources/Shaders/Object3D/FearWormEntrySurface");
    surface_->SetTransparent(true);
    surface_->SetTransparentDepthWrite(false);
    surface_->SetCastShadow(false);
    surface_->SetReceiveShadow(false);
    surface_->SetRaytracingEnabled(false);
    surface_->SetRotate({ kSurfaceRotationRadians, 0.0f, 0.0f });
    surface_->SetScale(kSurfaceScale);
}

void FearWormJumOutState::UpdateSurface(FearWormEntryState& entry, FearWormEnemy& boss)
{
    if (!surface_ || !boss.player_) { return; }
    Vector3 surfacePosition = entry.EntrySurfacePosition(boss.player_->GetTranslate());
    // 海面の波頂より上に置き、深度の競合を避ける。
    surfacePosition.y += kSurfaceHeightOffset;
    surfacePosition.z += kSurfaceOffsetZ;
    surface_->SetTranslate(surfacePosition);
    float shadowVisibility = 1.0f;
    if (entry.hasPlayedSplash_) {
        shadowVisibility = 0.0f;
    }
    surface_->SetColor({ entry.elapsedSeconds_, FearWormEntryState::CalculateEntryDurationSeconds(boss.segments_.size()), shadowVisibility, 1.0f });
    surface_->Update();
}

void FearWormJumOutState::Draw()
{
    if (surface_) {
        surface_->Draw();
    }
}
