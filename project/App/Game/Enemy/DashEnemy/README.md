# DASHENEMY

Stage-independent cube enemy. Its spawn identifier is exactly `DASHENEMY`.
The model is a white-textured 4-unit cube made with ModelManager::CreateCube.
It uses the player's flight basis rather than a fixed world axis or water level.

## Behavior

Waiting -> Entry -> Tracking -> Locked -> Dash -> Recovery -> Exit -> Finished.

- Entry: 0.5 s, approaches from the side.
- Tracking: 0.55 s, red dashed path follows the target.
- Locked: 0.45 s, the central red line becomes solid and its direction freezes.
- Dash: 220 units/s along that fixed direction, at most 160 units.
- Recovery: 0.65 s, then 0.4 s of lateral exit. One charge per spawn.
- HP 4. Damage 2, at most once per charge. Other enemies' contact behavior is unchanged.

Aim predicts automatic player movement through both the locked delay and travel
time. It does not predict lateral dodge input or steer after locking.
The two thin red side markers bound a slightly generous horizontal danger lane.
All prediction graphics are ordinary game objects, including in Release builds.

## Collision

DashEnemyMotion records movement segments with their fractions of the frame.
GameplayCollisionSystem clips attacking segments against stage colliders before
checking player bullets. Relative movement is used for both player contact and
bullets against the moving cube. Stationary frames and phase boundaries remain
part of the bullet hit interval. A thin-wall occlusion check prevents contact
damage between spheres whose centers are separated by a wall.

The dash attack uses a 2-unit-radius sphere; bullet hits use the visible cube's
4x4x4 box. Warning lines have no attack collider. Impacts stop the dash and start
recovery; obstacles are not destroyed. Missed charges leave without awarding score.

## Placement

Two test spawns are in Stage01: surface at engine (-15,-8,1010), lower route at
(15,-78,2960). No stage-specific logic is required by the enemy itself.
Settings are optional; omitted values use the defaults above. Scene coordinates
follow the existing JSON order [horizontal, forward, height].

```json
{
  "name": "Enemy_Dash_Example",
  "type": "EnemySpawn",
  "file_name": "DASHENEMY",
  "transform": {"translation": [-15, 1010, -8], "rotation": [0, 0, 0]},
  "dashEnemy": {
    "activationDistance": 160,
    "forwardOffset": 100,
    "trackingSeconds": 0.55,
    "lockSeconds": 0.45,
    "dashSpeed": 220,
    "dashDistance": 160,
    "hp": 4,
    "damage": 2
  }
}
```

`entrySeconds`, `recoverySeconds` and `exitSeconds` can also be configured.
Invalid values fall back to defaults in LevelDataLoader.

## Verification

- `GameplayCollisionTests.exe --player-bullets-only`: includes DashEnemyTests.cpp,
  checks locked headings, 30/60/120 FPS timing, moving player contact, repeat-hit
  suppression, moving-cube bullet hits, wall stops, warning length and data loading.
- `KohakuEngine.exe --verify-dash-enemy`: automatically starts near the surface
  spawn and captures tracking/locked/dash/recovery phases, then exits.
- Add `--dash-underwater` to verify the same enemy in the lower route.
- Screenshots: `project/generated/Screenshots`; logs: `project/generated/logs`.
