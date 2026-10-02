# Directional shadow maps

Stage01 and Stage03 opt in through their stage `settings.json` (`shadows`).
TitleScene also enables shadows for its static base, aircraft and visible room
props. Other scenes default to disabled. The game Renderer owns the shadow resources;
model preview does not run a shadow pass or add a floor.

## Object controls

```cpp
ice.SetCastShadow(true);
ice.SetReceiveShadow(true);
floor.SetCastShadow(false);
floor.SetReceiveShadow(true);
```

Both flags default to false. CastShadow draws static opaque Object3d meshes into
the light depth map. ReceiveShadow is implemented by StageIceFloor and the
StageIce object pixel shaders. Other materials need to opt in to
ShadowSampling.hlsli before the receive flag changes their lighting.
Transparent, skeletal and vertex-deformed shadow casting is outside this version.
ShadowToon receives shadows for title props, retaining ambient light in shadow.
The title's frozen room uses the same StageIce materials as the game. The ocean
keeps its separate wave renderer and receives shadows on the displaced surface,
but does not cast them. Stage01 static level meshes use ShadowToon to cast and
receive shadows; emissive laser hazards keep their existing material.
Legacy Ice remains compatible with the skinning root signature and does not
sample shadows.

## Rendering

ShadowMapRenderer creates a 2048-square R32_TYPELESS resource, D32_FLOAT DSV and
R32_FLOAT SRV. A separate depth-only root signature/PSO uses world matrices as
root constants and a shared light view/projection CBV. It does not overwrite the
camera transform CBV. Mesh DrawDepth binds geometry without texture/material
root bindings. The light map transitions from shader resource to depth write and
back before regular offscreen rendering restores the viewport and targets.

ShadowCamera fits a rotation-stable sphere around the camera frustum up to the
configured distance, with extra depth range toward possible offscreen casters.
Its light-space X/Y origin is snapped to texels. Conservative transformed model
bounds exclude casters outside the light volume, rather than the visible camera
frustum. Visibility fades out near the volume boundary. The map uses depth bias,
receiver normal offset and comparison-sampler 3x3 PCF. Blue ambient lighting is
retained while direct lighting and specular highlights are attenuated.

The floor remains matte. Ice highlights use pixel-shader math without scenery
reflections. Normal/SRV outputs remain compatible with outline and fog passes.
Disabled scenes and model preview bind valid disabled constants and a null SRV.
The shadow root slots are appended after the existing static-object VS slots;
shared skinning slot indices do not change.

## Ice lighting and atmosphere

`resources/Shaders/Object3D/StageIceLighting.hlsli` uses the existing directional
light (b1) and ambient light (b5) for ice diffuse shading and the matte floor.
Ambient color/intensity also controls the ice glaze and rim; these are not
emissive. Shadow visibility attenuates only the direct contribution.
`App/Game/Stage/GameplayVisualPreset.h` owns the Stage03 light and fog presets.
The game, development-light reset controls and model preview share the light
preset. Stage03 distance fog starts at 450 and reaches full density at 1000;
other stages retain their previous fog distances. Scene changes reset the fog
preset, while development distance sliders remain editable between changes.

## Cost

One depth-only draw per in-range caster, up to nine comparison samples per
receiving pixel, and 16 MiB for the 2048-square depth texture (plus small descriptor
and constant buffers). Model bounds are computed at load time. The light camera
and bounds tests run on the CPU each frame. GPU time depends on scene coverage.

## Validation

Build the game Release x64 through KohakuEngine.sln. Optional rendering tests:

```powershell
MSBuild project/ShadowMapTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:SolutionDir=C:\Projects\KohakuEngine\project\
```

Run `generated/outputs/Release/ShadowMapTests.exe` with working directory `project`.
It exports ON/OFF/CastShadow-OFF/ReceiveShadow-OFF PNGs under
`captures/ShadowMapTests`, compares actual GPU pixels and checks D3D12 warnings
and errors when the debug layer is available. `--stage` follows the normal
Title -> Loading -> Stage03 path, renders the actual game scene with shadows
ON/OFF, exports PNGs and checks D3D12 errors. This is a separate validation binary;
normal game startup and model-preview behavior do not change.
The fixture also isolates the floor and ice to check directional intensity,
ambient intensity and ambient color using actual GPU pixels.
`--title` renders the title overview, hangar interior, room swap, frozen hangar
and return to overview. It compares frozen shadow ON/OFF frames and checks D3D12
errors. Private transition access is enabled only in this validation project;
normal game builds do not expose test controls.
`--stage01` follows the normal loading path for Stage01, freezes simulation for
the shadow ON/OFF comparison and requires a visible pixel difference.
