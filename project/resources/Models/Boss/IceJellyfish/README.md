# Ice jellyfish visual prototype

Stage03 creates this passive actor separately from combat boss encounters.
Its bell and tentacles guard player bullets; the core takes damage.
It does not attack, damage the player on contact, or trigger stage clear yet.

## Assets

- `IceJellyfishBell.obj`: hollow ice dome, 44 units wide, scalloped icicle rim.
- `IceJellyfishSegment.obj`: reusable beveled tentacle link.
- `IceJellyfishTip.obj`: pointed final link.
- `IceJellyfishCore.obj`: faceted core (the runtime applies an unlit HDR color).
- `IceJellyfishAssembled.obj`: static assembled reference; not loaded by gameplay.
- `IceJellyfish.mtl`: ice uses the existing `resources/Textures/ice_texture.jpg`.
- `CoreWhite.png`: white base for the core.

OBJ coordinates are Y-up. Each link is 1 unit long along local negative Y;
its root/pivot is at the origin. Normals and UV coordinates are included.
Six tentacles each use six regular links plus one tip, scaled progressively
smaller. Models are shared between Object3d instances.
Link width scales from root to tip are 6.6, 5.6, 4.6, 3.6, 2.6, 1.6, 0.6.
The broad root is twice the first prototype's width; the tip is half as wide.
Link lengths and joint gaps are 1.5 times the first prototype's values.
Attachment positions, widths, and the bell are unchanged by this length adjustment.

## Placement and animation

`IceJellyfish.cpp` implements independent object transforms: vertical bob,
gentle body tilt, subtle bell pulse, and a delayed wave along each tentacle.
Link endpoints determine the next link's root, leaving a 0.18-unit joint gap.
Animation uses elapsed seconds and pauses with the gameplay update.

Initial placement uses Stage03's `boss.position` (currently 0, 20, 3000).
The height was raised by 8 units to keep the longer tentacles above the ice floor.
On this straight stage, the actor stays at least 150 units ahead of the rail
position once approached, so it remains visible at the stage endpoint.
The stage's existing boss type, rail length, enemy layout and clear logic
are unchanged. This is a passive collision prototype, not a complete boss encounter.

## Collision

Each of the 42 tentacle links has one OBB following its full world transform.
The hollow bell uses 192 thin surface OBBs and 16 rim OBBs, leaving its interior
open. The core uses a sphere of radius 3.6, following the core's breathing scale.
These are gameplay approximations rather than triangle-perfect collisions.
The existing sphere-versus-OBB sweep is conservative around box corners.

Player bullets sweep their previous-to-current position and resolve the first
hit across the armor and core. Closer enemies still intercept the bullet.
The same OBBs and core sphere participate in aiming raycasts.
Armor produces a guard effect without losing HP. Core HP starts at 120;
core hits flash, and at zero HP the actor disappears with an explosion.
Homing lock-on to this actor is not implemented yet.

In Debug, the `Debug Teleport Menu` provides `Teleport to Ice Jellyfish`,
core HP, and `Show Ice Jellyfish collision`. Bell boxes are cyan, tentacle
boxes purple, and the core sphere orange. The visualization defaults to off.

The CPU-only regression tests are in `project/Tests/IceJellyfishCollisionTests.vcxproj`.
Build that project with Debug/x64 and run
`generated/tests/IceJellyfish/IceJellyfishCollisionTests.exe` from the repository root.
They cover hollow-shell entry, armor occlusion, gaps between tentacles, rotated
OBBs, fast bullets, finite sweeps, initial overlap, and aiming target cleanup.

Ice uses the existing opaque lit pipeline with environment reflections;
physical transparency/refraction from the concept illustration is not implemented.

## Regeneration

Run from the repository root in a separate Blender background process:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 4.4/blender.exe' --background --factory-startup --python project/tools/CreateIceJellyfish.py
```

The script recreates these assets and saves the assembled Blender scene and
preview to `generated/IceJellyfish`. It must not run in a live scene with
unsaved user work. The Blender preview shows geometry and material intent;
lighting differs from the engine.
