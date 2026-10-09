# Directional shadow maps

Stage01 through Stage04 opt in through their stage `settings.json` (`shadows`).
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
Generic transparent and skeletal shadow casting is outside this version;
Jellyfish has an explicit opaque, vertex-deformed shadow path.
ShadowToon receives shadows for title props, retaining ambient light in shadow.
The title's frozen room uses the same StageIce materials as the game. The ocean
keeps its separate wave renderer and receives shadows on the displaced surface,
but does not cast them. Enabled stages static level meshes use ShadowToon to cast and
receive shadows; emissive laser hazards keep their existing material.
Legacy Ice remains compatible with the skinning root signature and does not
sample shadows.

## Rendering

ShadowMapRenderer creates a 2048-square R32_TYPELESS resource, D32_FLOAT DSV and
R32_FLOAT SRV. A separate depth-only root signature/PSO uses world matrices as
root constants and a shared light view/projection CBV. It does not overwrite the
camera transform CBV. Opaque Mesh DrawDepth binds geometry without texture/material
root bindings. Alpha-masked Object3d uses a separate pixel shader, material CBV and
per-primitive texture binding with the same UV transform/cutoff as regular rendering
and DXR (see [alpha masks](../Raytracing/AlphaMasks.md)). Custom depth deformation
does not yet support masks. The light map transitions from shader resource to depth write and
back before regular offscreen rendering restores the viewport and targets.

ShadowCamera fits a rotation-stable sphere around the camera frustum up to the
configured distance, with extra depth range toward possible offscreen casters.
Its light-space X/Y origin is snapped to texels. Conservative transformed model
bounds exclude casters outside the light volume, rather than the visible camera
frustum. Visibility fades out near the volume boundary. The map uses depth bias,
receiver normal offset and comparison-sampler weighted 5x5 PCF. Blue ambient lighting is
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
`resources/Graphics/visual-presets.json` owns the Stage03 light and fog presets.
The game, development-light reset controls and model preview share the light
preset. Stage03 distance fog starts at 450 and reaches full density at 1000;
other stages retain their previous fog distances. Scene changes reset the fog
preset, while development distance sliders remain editable between changes.

## Cost

One depth-only draw per in-range caster, up to 25 comparison samples per
receiving pixel, and 16 MiB for the 2048-square depth texture (plus small descriptor
and constant buffers). Model bounds are computed at load time. The light camera
and bounds tests run on the CPU each frame. GPU time depends on scene coverage.

## Validation

Build the game Release x64 through KohakuEngine.sln. Optional rendering tests:

```powershell
MSBuild project/Tests/Projects/ShadowMapTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:SolutionDir=C:\Projects\KohakuEngine\project\
```

Run `generated/outputs/Release/ShadowMapTests.exe` with working directory `project`.
It exports ON/OFF/CastShadow-OFF/ReceiveShadow-OFF PNGs under
`runtime/captures/ShadowMapTests`, compares actual GPU pixels and checks D3D12 warnings
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

## Dynamic actors

Player, normal enemies, pirate mid-boss, FearWorm segments, AngerBlock parts,
and opaque IceJellyfish core/crystals/pillars cast shadows through the same
culled depth pass. Destroyed parts and player blink visibility follow normal
rendering. Toon and Standard receivers select shadow-aware materials while
unlit effects remain unlit. Point lights, ambient and environment reflections
are retained by ShadowStandard. Jellyfish bell and living tentacles cast opaque shadows with a dedicated depth VS using the same deformation and animation parameters as normal rendering. Generic transparent casting and skinning remain unsupported. Weighted 5x5 PCF softens edges without increasing
the 2048-square map allocation.

`--stage01`, `--stage02`, and `--stage` capture the normal scene and its boss
with simulation frozen for shadow ON/OFF pixel comparisons.
Jellyfish shadow geometry is double-sided and caster bounds include wave displacement.

Tentacles use a smooth 16-side, 12-strip link mesh (384 triangles per link).
CPU poses filter direction more slowly toward the tip, overlap adjacent links,
and apply a traveling stretch pulse with inverse square-root width compensation.
The shared VS tapers each link, corrects its normals and uses the same geometry
for opaque shadows. Jellyfish surface data adds broad moving wet highlights.
Tentacle normal-buffer alpha encodes -(2 + depth); DepthOutline decodes this tag
to soften internal normal edges while retaining depth silhouettes and stale-depth
validation. Boss render tests advance actual time for a second animation pose.
