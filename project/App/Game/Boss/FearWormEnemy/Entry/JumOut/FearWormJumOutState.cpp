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
    isSurfaceVisible_ = false;
    if (boss.player_ == nullptr) {
        return;
    }
    isSurfaceVisible_ = true;
    boss.battleState_.PrepareEntryMovement(boss, kInitialOrbitAngle);
    // 経過時間0秒の水中配置と水面の予兆を、登場開始時から反映する。
    entry.UpdateSegmentPositions(boss);
    UpdateSurface(entry, boss);
}

void FearWormJumOutState::Update(FearWormEntryState& entry, FearWormEnemy& boss)
{
    entry.UpdateSegmentPositions(boss);
    // 頭が水面に達したら、水しぶきを一度だけ再生する。
    PlaySplashIfNeeded(entry, boss);
    // 飛び出し中も水面の影・波紋を更新し、水しぶき再生後は影を消す。
    UpdateSurface(entry, boss);

    // 時間が大きく進んだ場合も、位置更新と水しぶき判定を終えてから次へ移る。
    if (entry.IsHeadFlightFinished()) {
        entry.ChangePhase(FearWormEntryState::EntryPhase::WaitForBody, boss);
    }
}

// 次の段階への遷移や登場の中断時に、水面の予兆・波紋を描画対象から外す。
void FearWormJumOutState::Exit(FearWormEntryState&, FearWormEnemy&)
{
    isSurfaceVisible_ = false;
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
    if (isSurfaceVisible_ && surface_) {
        surface_->Draw();
    }
}
