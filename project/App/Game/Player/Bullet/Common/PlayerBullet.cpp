// プレイヤー弾に共通する敵への命中処理を定義し、敵弾とは別の型として扱う。
#include "App/Game/Player/Bullet/Common/PlayerBullet.h"

// 弾種固有の命中演出を追加するための共通の呼び出し口。標準では何も行わない。
void PlayerBullet::OnHitEnemy(const Vector3& position)
{
}
