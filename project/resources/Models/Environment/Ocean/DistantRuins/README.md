# 海底の遠景用ランドマーク

巨大な海底神殿、断崖、沈んだ塔群の3モデルです。
Stage01の海中ルートに神殿1基、断崖6区画、塔群4組を配置しています。

| モデル | 内容 |
| --- | --- |
| distant_temple | 大きなアーチ状の入口、段状の基壇、階段、柱廊、発光する象嵌 |
| distant_cliff_wall | 大きな岩の面、張り出した岩棚、突き出した岩峰 |
| distant_tower_cluster | 高さの違う5本の塔、壊れた連絡橋、少量の発光部 |

- `underwater_distant_ruins.blend`: 3モデルと遠景プレビューの配置・照明・霧を含む編集用ファイル。
- 各モデルの `.glb`: マテリアルとテクスチャを内包します。
- 各モデルの `.obj`、`.mtl`、`textures/`: 静的モデル。OBJはY-upです。
- `asset_info.json`: 三角形数、寸法、プレビュー条件。
- `previews/distant_ruins_preview_sheet.png`: 遠景と個別モデルの一覧。
- `previews/distant_ruins_atmosphere.png`: 手前の既存アーチ越しに見る構図。
- `previews/distant_ruins_open_view.png`: 手前のアーチを外した構図。
- `previews/distant_*.png`: 各モデルの形状確認用画像。

プレビューの神殿はカメラから前方へ約660ユニットの位置にあります。
手前のアーチ・床・照明・霧はプレビュー用で、個別モデルには含めていません。
画像はBlender Cyclesで描画したもので、ゲーム内のキャプチャではありません。
ゲーム側の霧設定やライティングにより実際の見え方は変わります。
発光はBlender/GLBに設定しています。OBJをゲームへ導入する際は材質の設定が必要です。

## ゲームへの配置

- `Runtime/`: 既存の海底用パレットを共有する1材質のゲーム用モデル。
- `placement_info.json`: 全11オブジェクトの位置、三角形数、共有パレットの対応。
- 神殿は進行距離2580の中央。ゲーム用モデルは奥壁と奥の扉飾りを除き、通り抜けられる構造にしています。
- 断崖は左右210の位置、進行距離1800・2600・3400に配置しています。
- 塔群は左右110の位置、進行距離2380・3140に配置しています。
- 全モデルは背景として配置し、新しい当たり判定は追加していません。

ゲーム用材質は既存の海底パレットに近い色を使用します。画像用の発光材質とは異なります。
配置スクリプト: `project/Tools/ModelGeneration/PlaceUnderwaterDistantRuins.py`。

生成: `project/Tools/ModelGeneration/CreateUnderwaterDistantRuins.py` をBlenderで実行。
一覧: `project/Tools/ModelGeneration/ComposeDistantRuinsPreview.py` をPillowのあるPythonで実行。
