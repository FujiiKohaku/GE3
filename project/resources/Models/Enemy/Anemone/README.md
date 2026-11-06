# Anemone enemy prototype

Stationary sea-anemone enemy for Stage01's lower route. The current gameplay
prototype uses the open OBJ pose at scale 3.5, HP 8, and fires five orange bullets
in a 28-degree fan every 2.4 seconds. The first volley follows 0.9 seconds in range.
It attacks only underwater targets ahead of the player and within 180 units.
The middle bullet predicts the player's automatic forward motion; lateral input
still lets the player dodge. Bullets use EnemyBulletManager and the existing
obstacle/player collision processing. The enemy can be shot and destroyed.

Placements in `resources/Scenes/stage01.json` (engine X/Y/Z):

| Name | Position |
| --- | --- |
| Enemy_Anemone_01 | -25, -116.07488, 2280 |
| Enemy_Anemone_02 | 25, -116.39793, 2550 |
| Enemy_Anemone_03 | -25, -113.85682, 3420 |

The bases are aligned with the existing sand mesh. The model faces the approaching
player using a 180-degree Y rotation. This is an open-pose shooting prototype;
closing, guarding and attack telegraphs are not installed yet.

## Assets

- `AnemoneEnemy.blend`: editable geometry, preview studio, shape-key animation.
- `AnemoneEnemy.glb`: animated model, open at frame 1, closed at frame 75.
- `AnemoneOpen.obj`, `AnemoneClosed.obj`: static single-material poses, Y up.
- `textures/AnemonePalette.png`: shared diffuse atlas used by both OBJ poses.
- `previews/AnemoneOpen.png`, `previews/AnemoneClosed.png`: Blender renders.
- `asset_info.json`: topology and animation summary (13,444 triangles).

Generate with Blender and `Tools/ModelGeneration/CreateAnemoneEnemy.py`.
The generator changes only model assets; gameplay behavior and placement remain
in the C++ enemy and stage layout.

## Verify

`generated/outputs/Debug/GameplayCollisionTests.exe --player-bullets-only` from the
repository root checks the three spawns, predictive aim, five directions, timing
at 30/60/120 FPS, and existing bullet/obstacle collision behavior.

Launch `KohakuEngine.exe --verify-anemone` to start near the first underwater enemy,
save three game screenshots, and exit automatically. It uses the normal stage
loading/rendering and logs `[AnemoneEnemy] Fired five-shot fan` when firing.
Screenshots go to `project/generated/Screenshots`.
