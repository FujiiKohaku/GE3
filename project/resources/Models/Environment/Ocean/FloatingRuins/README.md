# 浮遊する土の地面と遺跡壁：試作A

草の上面、露出した土層、岩、垂れた根、ツタ、欠けた石壁、実際に開口したアーチを持つ静的モデル。

- `floating_earth_wall.obj` / `.mtl`：全体の交換用モデル。Y上方向。画像を相対パスで参照。
- `floating_earth_wall.glb`：画像を内包する交換用データ。
- `floating_earth_wall.blend`：編集・レンダリング用。カメラ・ライト・海面を含む。
- `previews/floating_earth_wall.png`：正面寄りのプレビュー。
- `previews/floating_earth_wall_back.png`：裏側のプレビュー。
- OBJは全体4,810三角形、10マテリアル。壁単体は144三角形・1マテリアル。画像は各128×128。
- Blender上の寸法：約12.62×5.13×7.99。OBJではY上方向への変換により高さと奥行きの軸が入れ替わる。
- プレビュー用のカメラ・ライト・海面はOBJ/GLBに含めない。

生成元：`project/Tools/ModelGeneration/CreateFloatingEarthWall.py`。Blender 4.4で実行。
STAGE1の最初のゲート（進行方向240）に試験配置済み。他のゲート・壁は変更していない。

設置モデルは `floating_earth_wall_ground.obj`（土・草・岩・根・ツタ）と `floating_earth_wall_masonry.obj`（連続した壁面）の2オブジェクト。壁は1回の描画で、専用PS `resources/Shaders/Object3D/FloatingRuins/Render.PS.hlsl` がUVから石の色の違い、細い目地、縁の法線、粗さ・反射の違いを生成する。細かすぎる目地は画面微分で薄める。石を重ねた面は持たない。

シーン座標で位置 `[0, 240, -7]`、スケール `[6, 2, 4]`。壁には17個の箱（左右5個＋アーチ上部12個）、地面には1個の箱を設定。中央の穴は最大幅16.8、アーチ頂点の高さ6.64。地面の判定上面は高さ-3.4。判定は箱による近似で、曲線との間には小さな余裕がある。

`CreateFloatingEarthWall.py` と補助 `CreateSolidRuinsWall.py` でモデルを生成し、`PlaceFloatingEarthWall.py` でゲート配置・判定を更新する。配置スクリプトは再実行可能で、他のシーンオブジェクトが変わらないことと、アーチ内に箱が入らないことを検査する。

プレビュー画像はBlenderのBrick/Bumpノードで同じ意図の模様を再現した参考画像。ゲーム内の専用PSそのもののスクリーンショットではない。OBJ/GLB単体の表示ではゲームのPSによるタイル表現は適用されない。

## 最初のスラローム壁

進行方向760の `Slalom01_LeftWall` を、穴のない石壁 `floating_slalom_wall_masonry.obj` に置き換えている。1メッシュ・1マテリアル・52三角形。アーチと同じPSを利用し、上端の欠けだけを形状で表す。

土・草・根・ツタは既存の `floating_earth_wall_ground.obj` を共有する。壁の判定は上端の段差に合わせた6箱、地面は1箱。元の右側通路（X=-2より右）に新しい判定が張り出さないことを配置時に検査する。壁と地面はシーン座標 `[-12, 760, -12]`、スケール `[1.55, 2, 3]`。

生成・設置元：`project/Tools/ModelGeneration/CreateFloatingSlalomWall.py`。編集用は `floating_slalom_wall.blend`、参考プレビューは `previews/floating_slalom_wall.png`。Blenderファイルのオブジェクトには配置時の縦横比を再現するスケールを設定している。ゲーム用OBJはそのスケールを適用する前に出力し、シーン側で大きさを指定する。

## STAGE1の壁への展開

`project/Tools/ModelGeneration/ReplaceStage01Walls.py` で、残り19枚のスラローム壁・ゲートの側面と上下・トンネルの床と天井を同じ石壁モデルへ置換。元の箱の範囲に合わせてスケールと高さを設定し、6箱ずつで上端の形状を近似する。天井4枚には土台を追加せず、それ以外の15枚には共有の土台モデルを追加している。

壁の判定は元の箱の内部に収め、土台は元の箱の底面以下、横幅・奥行きは元の箱以下に制限。元のアーチ、最初のスラローム壁、柱・壊せる岩・レーザー・敵配置は維持。変更前の壁寸法は `stage01_wall_replacements.json` に保存し、再実行時もその寸法から配置を生成する。

今回置換・追加したモデルの描画三角形は19×52＋15×4,666＝70,978。モデルと画像の読み込み・GPUデータは共有されるが、各配置の描画は増える。追加の壁判定114箱・土台判定15箱で、置換前19箱から差し引くと110箱増。

## 専用の門モデル

Gate02・OffsetGate・FinalGate・SecondActGateは、現在は `floating_gate_*.obj` の専用モデルへ置換済み。壁用パネル3枚を描く構成をやめ、元の3箱の和集合の境界だけを1メッシュ・1マテリアルで出力している。Gate02・FinalGate・SecondActGateは各60三角形、OffsetGateは84三角形。内部面を生成せず、閉じた形状・重複面ゼロを生成時に検査する。

スケールは全ゲート等倍で、UVの石サイズは幅2.5・高さ1.25のワールド単位。柱と横梁で模様のサイズが変わらない。当たり判定は元の箱3個を使用し、開口を維持する。左右の土台は維持し、下側横梁の下に追加していた全幅の土台2個は取り外した。

生成・設置は `project/Tools/ModelGeneration/CreateFloatingGates.py`。`ReplaceStage01Walls.py` の後に実行する。再実行可能で、門以外のシーンオブジェクトを維持することも検査する。`gate_asset_info.json` に配置と検査結果、`floating_gates.blend` に編集用データ、`previews/floating_gate.png` にGate02の参考画像を保存する。ゲーム内の水・影を含めたちらつきの動作確認は別途必要。
