# Turret model preview

単砲身のローポリ砲台。参考①の方向性を基に作成した独自のモデル。
ゲームへの組み込みは `project/App/Game/Enemy/TurretEnemy` を参照。

- `Turret.blend`: 編集用。土台・旋回台・砲身と発射点の親子構造、プレビュー用カメラ・照明を含む。
- `TurretPreview.png`: プレビュー画像。
- `TurretFeedback.gif`: Stage1での発射演出・弾の尾・速度調整後の確認録画（5秒）。
- `models/Turret.glb`: モデルだけの親子構造付きデータ。中立姿勢。
- `models/TurretBase.obj`: 土台。原点は設置位置。
- `models/TurretYaw.obj`: 左右旋回部。原点は旋回軸。
- `models/TurretBarrel.obj`: 砲身。原点は上下照準の軸。
- `.mtl`: 各OBJの色指定。

三角形数は合計1,440。テクスチャ画像は使わず、材質色で塗り分けている。
OBJは各パーツの回転軸を原点としたローカル座標で書き出し済み。

## 座標

Blender: 上が+Z、砲身の正面は-Y。
OBJ: 上が+Y、砲身の正面は+Z。

ゲーム側の親子Transform用オフセット（OBJ座標）:

- 土台 → 旋回台: `(0, 0.62, 0)`
- 旋回台 → 砲身: `(0, 0.50, 0)`
- 砲身 → 発射位置: `(0, 0.03, 2.28)`

土台の位置・向き、旋回台のY回転、砲身のX回転を独立して設定できる。
