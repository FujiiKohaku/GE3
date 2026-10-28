# Stage01 海底オブジェクト

8種類のスタイライズド3Dモデル。Stage01の海底ルートに40か所、42オブジェクトを配置しています。
ゲーム側のコード変更はありません。

| 番号 | ファイル名 | 内容 |
| --- | --- | --- |
| 01 | broken_arch | 片側が欠けたアーチ、貝、サンゴ、落下した石 |
| 02 | crystal_cluster | 青緑の発光結晶と岩の土台 |
| 03 | buried_colossus | 砂に埋もれた石像、発光する目、付着したサンゴ |
| 04 | sunken_rotor | 分割された石輪、発光象嵌、支持台 |
| 05 | giant_ribcage | 7対の肋骨と背骨からなるトンネル |
| 06 | hydrothermal_vents | 3本の噴出口、鉱物の縁、気泡のプレビュー形状 |
| 07 | tall_kelp | 高さと曲がり方が異なる15枚の海藻 |
| 08 | collapsed_pillars | 高さの異なる柱と倒れた柱、瓦礫、サンゴ |

## ファイル

- `previews/underwater_props_contact_sheet.png`: 日本語ラベル付き一覧。
- `previews/01_*.png`〜`08_*.png`: 個別レンダリング。
- `underwater_props.blend`: 全モデルと照明・カメラを含む編集用ファイル。
- `01_*.glb`〜`08_*.glb`: マテリアルを内包する個別モデル。
- `01_*.obj`〜`08_*.obj`、同名のMTL、`textures/`: 静的モデル。
- `asset_info.json`: 三角形数とBlender座標での寸法。

石輪は1〜241フレームで一回転、海藻は1〜121フレームで往復するキーを設定しています（30fps）。
Blender/GLBのアニメーションです。OBJは1フレーム目の静止形状です。
配置した石輪は既存のROTATIONギミックで毎秒0.18ラジアン回転します。台座は静止します。
ゲーム内の海藻は静的なOBJです。発光シェーダー設定、当たり判定、噴出口の熱水・気泡エフェクト、ダメージ処理は未設定です。
気泡はプレビュー用の不透明な形状で、OBJには含めていません。
プレビューはBlenderの照明で描画しているため、ゲームでの見え方は環境光や霧の設定によって変わります。

生成: `project/Tools/ModelGeneration/CreateUnderwaterProps.py` をBlenderで実行。
一覧: `project/Tools/ModelGeneration/ComposeUnderwaterPropsPreview.py` をPillowのあるPythonで実行。

## Stage01への配置

- `Runtime/`: 材質を1枚のパレットにまとめたゲーム用OBJ（9モデル）。各モデルは1材質です。
- `placement_info.json`: 配置座標・個数・全配置分の三角形数（47,060）。
- `placement_verification.json`: 中央通路の形状サンプリング結果。全面的な衝突検証ではありません。
- `previews/placement_*.png`: 実際の配置JSONとゲーム用OBJから描画したBlenderプレビュー。ゲーム内のキャプチャではありません。
- `stage01_underwater_placement.blend`: 配置後の確認用Blenderファイル。

配置: `project/Tools/ModelGeneration/PlaceUnderwaterProps.py` をPillowのあるPythonで実行。
既存の砂地形をサンプリングして高さを合わせ、`LowerRoute_Prop_`の配置だけを更新します。
配置プレビュー: `project/Tools/ModelGeneration/PreviewUnderwaterPlacement.py` をBlenderで実行。
`-- --verify-only` を付けると形状チェックだけを実行します。
