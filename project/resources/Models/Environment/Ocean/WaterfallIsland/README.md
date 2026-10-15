# STAGE1 山島と大滝・モデル試作

生成画像を参考に作成した実際の3Dモデル。海から立ち上がる段状の岩山、苔の天端、石造の水路橋と塔、段瀑、中央を貫通する飛行用トンネルで構成。

- `waterfall_island.blend`：編集用。4つのモデルとプレビュー用の海面・照明・カメラを含む。
- `waterfall_island.glb`：テクスチャを内包する交換用。プレビュー用オブジェクトは含まない。
- `WaterfallIsland_Terrain.obj`：地形・苔、1,016三角形。
- `WaterfallIsland_Ruins.obj`：遺跡、1,192三角形。
- `WaterfallIsland_Water.obj`：滝、2,400三角形。
- `WaterfallIsland_Foam.obj`：着水部の泡、720三角形。
- `previews/waterfall_island.png`：斜め上からのモデルプレビュー。
- `previews/waterfall_island_approach.png`：進入方向からのモデルプレビュー。

合計5,328三角形、4メッシュ、10マテリアル。PNGは各16×16。マテリアル単位で描画が分かれる場合、4メッシュでも4描画にはならない。影用の描画と滝の透過描画は別途負荷になる。

Blender座標で横幅約160、高さ約110。中央の穴は幅28、アーチ頂点の高さ26で、奥行き方向に貫通している。BlenderはZ上方向、OBJはY上方向。既存エンジンのOBJインポートはXを反転するため、設置時に進行方向と左右を確認する。

滝は独立した静的メッシュ。プレビューでは半透明を使用している。ゲーム側の水シェーダー、流れるアニメーション、飛沫パーティクル、当たり判定、STAGE1への設置は未実装。穴の手前には水の面があるため、地形と水に同じ当たり判定を付けない。

生成元：`project/Tools/ModelGeneration/CreateWaterfallIsland.py`。Blender 4.4で実行。STAGE1のJSONやゲームコードは変更しない。
