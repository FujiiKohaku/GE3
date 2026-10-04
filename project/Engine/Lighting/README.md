# Screen-space global illumination

`ScreenSpaceGlobalIllumination` adds one approximate diffuse light bounce from
visible surfaces. It defaults to ON, strength 0.65, four cosine-weighted rays,
16 steps per ray, a 12-world-unit range, temporal history and edge-aware blur.
It is separate from the existing SSR and ambient-only SSAO.

## Frame order

1. Forward rendering produces linear HDR color, normal/depth metadata, ambient
   color and surface material information.
2. SSGI reads the original forward color before SSGI, SSR, fog and bloom. It never
   feeds its own previous output back into the source radiance.
3. Half-resolution tracing, temporal reprojection, two separable geometry-aware
   blur passes and full-resolution geometry-aware upsampling add diffuse light.
4. SSAO removes only occluded ambient lighting. Fog, SSR, DLAA, particles, bloom,
   tone mapping and UI follow the existing render order.

The material target is linear RGBA8_UNORM: RGB is surface base color, alpha zero
is invalid/unlit, and alpha `(1 + metallic * 254) / 255` marks lit surfaces.
Opaque and depth-writing transparent materials write material metadata without
blending. Jellyfish's receiving base color includes its opacity. Materials that
do not write matching depth/normal metadata are rejected.

## Hi-Z and history

The renderer prepares SSR's min/max depth pyramid once before SSGI. SSR consumes
that prepared pyramid later in the same frame. SSGI uses coarse depth bounds to
reject candidates before fine depth tests and four-step hit refinement. SSR's
pyramid excludes reflective floors; SSGI tests those pixels directly so floor
radiance remains available without changing SSR traversal behavior.

The cosine-weighted directions rotate over eight frames when temporal history
and motion input are available. Current-minus-previous motion vectors include an
explicit correction for projection jitter. History is rejected on receiver depth,
world normal or reprojection mismatch, and clamped to current neighborhood color.
Disabling SSGI, changing settings, changing camera/history identity or scene
revision, and missing motion invalidate history. A missing material or camera
returns the original color input. Strength zero also bypasses processing.

## Controls and cost

Debug ImGui exposes `Screen space global illumination`; the Development Web panel
exposes `間接光 SSGI`. Controls include enable, strength, distance, thickness,
rays, steps, history weight, radiance limit, blur, shared Hi-Z and history reset.
`Indirect light` and `Raw indirect light` views bypass fog/SSAO/SSR so the
contribution can be inspected independently. The existing direct/ambient source
views bypass SSGI as well.

At 1280x720 the additional texture payload is about 24.6 MiB: eight half-resolution
RGBA16F images, one full-resolution RGBA16F composition image and one RGBA8 material
target. Descriptor, allocation alignment and driver overhead are separate. Shared
Hi-Z is an existing SSR allocation. SSGI timing includes tracing, temporal resolve,
blur and composition; shared Hi-Z preparation and the forward material write are
outside that timer. Quality settings change GPU cost and do not reallocate images.

`ShadowMapTests.exe --ssr` checks SSGI colored bounce, OFF/zero/missing-material
equivalence, metallic receivers, Hi-Z result parity, temporal noise reduction,
removed objects, camera/scene history resets and D3D12 validation, alongside SSR
and SSAO regression checks. `--dlss` checks the actual stage, SSGI controls,
ON/OFF/re-enable, diagnostic views and SSR/DLAA integration. Captures and timing
reports are under `runtime/captures/SsrValidation` and `ShadowMapTests`.

## Limits

Only visible depth surfaces are available. Off-screen and occluded surfaces do
not contribute; ambient/sky lighting remains the fallback. View-dependent source
color is an approximation to diffuse outgoing radiance. This is a one-bounce
screen-space effect, not multi-bounce GI, ray tracing or a replacement for probes.
Radiance clamping limits bright outliers. Low ray counts may still show noise,
especially while moving. Moving light emitters can require several frames to
settle. Particles rendered after temporal resolve do not illuminate the scene.
The current targets follow the engine's fixed 1280x720 resolution.
