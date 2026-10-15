# STAGE1用・壊せる浮遊岩

砂岩色の6つの岩塊、深い亀裂、青緑の結晶、周囲の6つの小さな破片で構成。

- `floating_rock.obj` / `.mtl`：ゲーム用。Y上方向、原点は全体の中心、最大寸法1。
- `textures/`：専用の64×64テクスチャ6枚。MTLから相対パスで参照。
- `floating_rock.glb`：画像を内包したプレビュー・交換用データ。
- `floating_rock.blend`：編集用。プレビュー用カメラ・ライト・床を含む。
- `previews/floating_rock.png`：完成プレビュー。
- 1,002三角形。OBJ/GLBにはプレビュー用の床・カメラ・ライトを含めない。

生成スクリプト：`project/Tools/ModelGeneration/CreateFloatingRock.py`。
Blender 4.4をバックグラウンド実行して再生成できる。乱数シード固定。
STAGE1の壊せる岩6個から参照。位置・拡縮・当たり判定・HP・破壊処理は従来のまま。
結晶の発光設定はBlender/GLB用。エンジンでの発光表現は使用シェーダーに依存する。
