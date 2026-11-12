#include "App/Game/Player/Aim/PlayerShotPointCalculator.h"
#include "Engine/Camera/Camera.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/math/MatrixMath.h"

//初期化
void PlayerShotPointCalculator::Initialize()
{
    aimDistance_ = kAimConvergenceDistance;
    aimPoint_ = {};
    aimRayOrigin_ = {};
}


// 画面上の照準を通って奥へ伸びるレイを作り、引数のaimRayに結果を書き込む。
// 簡単に言うと判定に使う目に見えない線を作成する関数
void PlayerShotPointCalculator::CreateAimRay(Ray& aimRay, const Camera& activeCamera, const Vector2& aimScreenPosition) const
{


    // 現在の照準位置を取得する
    float mouseX = aimScreenPosition.x;
    float mouseY = aimScreenPosition.y;

    // 画面の幅と高さを取得する
    float screenWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    float screenHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());

    // ピクセル座標をNDCへ変換する。画面中央は(0, 0)、左上は(-1, 1)。
    // 画面座標は下向きが正なので、Yを反転する。
    float ndcX = (2.0f * mouseX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / screenHeight);

    // 描画時の座標変換を逆にたどるため、投影行列とビュー行列の逆行列を用意する。
    Matrix4x4 inverseProjection = MatrixMath::Inverse(activeCamera.GetProjectionMatrix());
    Matrix4x4 inverseView = MatrixMath::Inverse(activeCamera.GetViewMatrix());

    // 同じ照準位置のニア面とファー面に点を置く。Zの0と1はNDCの奥行き。
    Vector3 nearPoint = { ndcX, ndcY, 0.0f };
    Vector3 farPoint = { ndcX, ndcY, 1.0f };

    // 逆投影で、NDCからカメラ基準の3D座標へ戻す。
    nearPoint = MatrixMath::Transform(nearPoint, inverseProjection);
    farPoint = MatrixMath::Transform(farPoint, inverseProjection);

    // 逆ビュー変換で、カメラ基準の座標からワールド座標へ戻す。
    nearPoint = MatrixMath::Transform(nearPoint, inverseView);
    farPoint = MatrixMath::Transform(farPoint, inverseView);

    // ニア面の点を始点にする。方向は手前から奥へ向け、長さを1に揃える。
    aimRay.origin = nearPoint;
    aimRay.direction = Normalize(farPoint - nearPoint);
}
void PlayerShotPointCalculator::UpdateFromRay(const Ray& aimRay, float fallbackDistance)
{

    // 何も当たらなければ基準距離を使い、当たれば命中点までの距離を使う。
    float targetDistance = fallbackDistance;
    RaycastHit hit{};
    if (CollisionManager::GetInstance()->Raycast(aimRay, hit)) {
        targetDistance = hit.distance;
    }

    aimDistance_ = targetDistance;
    aimRayOrigin_ = aimRay.origin;
    aimPoint_ = aimRay.origin + aimRay.direction * aimDistance_;
}
// 照準方向で最も近くに当たる対象までの距離と狙い先を更新する。
void PlayerShotPointCalculator::Update(const Camera& activeCamera, const Vector2& aimScreenPosition, float /*deltaTimeSeconds*/)
{
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera, aimScreenPosition);
    UpdateFromRay(aimRay, kAimConvergenceDistance);
}



// 最後のUpdateで計算・保存した狙い先を返す。
Vector3 PlayerShotPointCalculator::GetAimPoint() const
{
    return aimPoint_;
}

// 最後のUpdateで使用したRayの始点を返す。
Vector3 PlayerShotPointCalculator::GetAimRayOrigin() const
{
    return aimRayOrigin_;
}
