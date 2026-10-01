# Kohaku Model Preview

A separate DirectX 12 model viewer. It shares the engine's Object3D shaders but
does not initialize the game scenes, audio, collision, or particle effects.

## Run

Choose **ModelPreview** as the Visual Studio startup project and build Release x64,
or run from the repository:

```powershell
powershell -ExecutionPolicy Bypass -File project/Tools/StartModelPreview.ps1 -Build
```

The executable is `generated/outputs/Release/ModelPreview.exe`. It finds the
repository resources when launched directly. It can also run beside a `resources`
directory. Keep `dxcompiler.dll` and `dxil.dll` beside the executable.

## Controls

- Open a project model from the filtered list, or use **Open model...** for an external file.
- OBJ, glTF/GLB, FBX, DAE, and PLY triangle meshes are supported by the bundled Assimp importer.
- The entire model hierarchy is baked into a static pose, including all meshes and textures.
- **Add** keeps existing models. Select an object to change its transform, material, tint, and transparency.
- Right drag orbits, middle drag pans, the wheel zooms, and **F** fits all loaded objects.
- **Tab** hides the panel; **F12** or **Save PNG** saves a full-color 1280×720 screenshot.
- **Record GIF** exports at 640×360 with 256 colors; choose 5–30 fps and 1–10 seconds.
- **One full orbit** produces a complete turntable loop. Disable it to record shader motion from a fixed view.
- **Stop GIF** saves the frames recorded so far. The panel and mouse cursor are excluded from captures.
- Output files have unique timestamp names under `project/captures/ModelPreview`.
- **Reload shaders** rebuilds material pipelines; existing DXIL cache invalidation recompiles modified sources/includes.

**Game rendering (Stage03)** is enabled by default. It uses the same shared
lighting preset and material selection as GamePlayScene, the Stage03 skybox from
StageCatalog, and the engine's full depth-outline/fog composition path. Normal
gameplay does not apply boost blur or bloom, so the preview does not enable them.
The normal game camera FOV (0.45 radians), near plane (0.1), and far plane (1000)
are used; orbit/fit position still serves the model viewer rather than replaying
the rail camera.

Ice models found in the Stage03 layout use their first active instance's position,
rotation and non-uniform scale, read with the game's LevelDataLoader. Ice object
highlights are calculated in the pixel shader without environment-map scenery;
the floor uses a separate matte shader. A model absent from that layout uses its
asset size. **Stage scale** displays the imported scale; the Scale control is an
additional multiplier. Multiple objects and their camera fit retain their world
positions.

Disable Game rendering for a plain background and manually adjustable lighting.
Standard and Toon use the adjustable light; Ice and Archive use the fixed lighting
in their existing shaders.
The Jellyfish material uses the existing vertex deformation and animation time.
The viewer does not reconstruct gameplay assemblies or play imported skeletal clips.
It displays isolated models, so surrounding stage geometry, gameplay particles
and transient effects are not part of the preview.
Alpha blending between separate transparent objects is sorted by their centers;
transparent faces within one mesh use the engine's existing approximation.

GIF encoding streams each frame using a fixed dithered palette and bounded LZW
dictionary. Memory does not grow with recording length. File size and capture
time depend on the image. PNG preserves full color; GIF is necessarily quantized.

## Validation

```powershell
generated/outputs/Release/ModelPreview.exe --smoke-test
```

This runs the viewer's actual rendering path in a hidden window, exports a PNG and
eight-frame GIF of the ice boulder, verifies PNG exports for all six ice models,
glTF and the jellyfish bell, then checks multiple objects, shader reload and
invalid-file recovery. It logs the result and exits. It does not launch the game.
Normal startup opens the viewer window.
