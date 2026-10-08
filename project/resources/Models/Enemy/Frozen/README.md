# Frozenステージの道中敵

Stage3限定で通常・移動・装甲・ペイント・群れの5種類へ適用する。
GamePlayScene::ConfigureFrozenEnemyAppearanceが初期配置、群れ出現、レベル再読み込みの生成経路から呼ばれる。
他ステージとボスのモデル、敵のHP・攻撃・移動・当たり判定は変更しない。

モデルはModelManagerで種類ごとにキャッシュ・共有する。
テクスチャ本来の色を表示するため、装甲の着色と群れの隊形色をStage3では白に戻す。
群れのフェードアウトも白を基準にする。

編集元：output/enemy_models_v1/EnemyModels.blend。
変換スクリプト：output/enemy_models_v1/export_game_models.py。
Y上・Z軸方向のゲーム用OBJへ変換し、モデル半径を既存の機体サイズへ合わせている。

| 種類 | 三角形数 | 材質メッシュ数 |
|---|---:|---:|
| 通常 | 800 | 3 |
| 移動 | 1030 | 3 |
| 装甲 | 840 | 3 |
| ペイント | 1142 | 4 |
| 群れ | 70 | 3 |

Blenderの透過・発光は再現していない。ゲームでは既存のShadowStandardと色テクスチャで表示する。
通常Release/x64ビルド成功：generated/FrozenEnemyModels-Release.log。
エンジンと同じAssimpインポート設定で全モデルのメッシュ・法線・テクスチャ参照を検証済み。
実プレイによる表示とGPU負荷は未確認。
