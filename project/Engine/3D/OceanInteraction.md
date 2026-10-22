# Low-altitude water interaction

## FearWorm surface contact

- Player and boss emit through OceanSurface::AddWake. The existing 48 records and 32-byte
  CPU/HLSL layout are retained; directionSize.w selects directional (0) or radial (1) waves.
- Expired records are compacted each update. Boss records are capped at 16, preserving at
  least 32 player slots. Without boss records the player can still use all 48 slots.
  Full boss history rejects new records until an old record expires; player emission only
  replaces player records. Existing boss crests are never removed early to make room.
- After boss movement, each living segment samples natural ocean height, excluding wakes
  to avoid feedback. A sphere/water intersection sets wake strength; width stays fixed per
  segment to avoid changing wave shape during motion. A linear
  swept signed-height check also detects complete crossings between frames.
- Horizontal movement emits directional wakes; vertical center crossings queue radial
  impacts. Analytic radial slopes are shared between CPU sampling and VS normals.
- Velocity and directional strength follow with a 0.15-second time constant. Impact rings
  only rearm after the segment clears its radius plus 0.5 units, preventing surface jitter
  from emitting repeated rings. Queued impacts retain their original birth time and fade in
  over 0.08 seconds when uploaded; radial directionSize.x stores this reveal delay.
- A segment emits at most once per 0.18 seconds, and the boss emits at most once per 0.18
  seconds. Round-robin selection prevents the head monopolizing emission. Pending impacts
  expire after 0.8 seconds. No new particles, draw passes or textures are allocated.
- Pause holds contact history; death, destroyed parts, waiting, out-of-ocean positions and
  movements of 40 units or more reset contact history. Existing waves expire in 2.5 seconds.
- Contact cost is up to ten natural height queries per frame; each query evaluates the
  five natural waves seven times. Fixed history scans add O(48) CPU work. VS cost remains
  proportional to active history count, with radial evaluation selected per record.
- Validate with OceanInteractionTests (swept contact, impact rearming, frame-rate-independent
  smoothing, queued-ring reveal, radial slopes and existing player-wave checks), plus
  OceanWakeHistoryTests for shared-history retention. The history test can link OceanSurface.cpp
  with OCEAN_HISTORY_STANDALONE defined to replace unused rendering dependencies with throwing
  stubs. Compile Ocean VS/PS and build the application. In-game
  appearance and timing still require verification, especially away from the player-focused mesh.

## Player interaction and shared rendering

- OceanSurface records up to 48 world-space wake stamps. They expand and fade over 2.5 seconds;
  old stamps stop being evaluated after the last wake expires. Stamps do not follow the aircraft.
- Wake vertex displacement and slope normals are added in Ocean.VS.hlsl; rings and V-shaped foam are added to Ocean.PS.hlsl.
  Broad bounds reject unrelated water pixels before calculating the detailed shape.
- Clearance uses player center minus 1 world unit (the existing vertical screen bounds).
  Interaction fades between 0.5 and 6 units above the sampled waves, and is scaled by actual XZ speed.
- CPU height queries invert Gerstner horizontal displacement with six iterations. CPU and VS use
  the same OceanWaveLayers.hlsli coefficients. The mesh is coarse, so analytic height can differ
  slightly from the final triangle interpolation.
- OceanWaveShape.hlsli shares the wave evaluation between CPU and VS. Each component has its
  own smooth world-space amplitude envelope (0.40 to 1.35) and phase warp (up to 0.35 radians).
  The fields drift slowly using game time; pausing also holds their motion. Five wave components
  remain, with two four-corner value-noise evaluations per component instead of the common
  two-sine strength modulation. No additional mesh vertices, draw passes or textures are allocated.
- Wave tangents include amplitude/phase derivatives and horizontal Gerstner displacement.
  Wake slopes are transformed through those tangents before computing the surface normal.
  Natural crest foam requires broken noise coverage; crest height alone no longer paints a stripe.
  Foam remains procedural without a persistent history buffer.
- The noise also runs during CPU height reconstruction (seven displacement evaluations per query).
  Actual GPU/CPU timing and appearance require in-game measurement; compilation and analytic
  height/tangent tests alone do not establish visual quality or a performance budget.
- OceanSpray emits pooled one-shot bursts: small falling droplets plus softer expanding mist.
  Spawned particles stay in world space; no attachment is used. Direction and strength are per burst.
  Normal maximum rate is about 12 bursts/second, boost about 18; each burst has at most 20 particles.
- Boost widens wakes and increases the spray rate. Rising/stopping reduces intensity smoothly.
- Ocean and wake time pause with gameplay. Existing sprays follow EffectManager's game-time update.
  Death stops new emission; remaining stamps/particles expire. Scene reload/teleports cannot join distant stamps.

Validation: compile Ocean VS/PS and OceanSpray Emit/Update; compile and run
Tests/OceanInteractionTests.cpp with the project include directory. It checks displaced-coordinate
height reconstruction across the stage, two wave amplitudes plus flat water, stationary cutoff,
altitude fade, and speed scaling. Verify appearance/performance in game at the lower flight bound,
while climbing, boosting, pausing, and changing scenes.

## Warmup pool safety
OceanSpray reserves 14 resource sets, keeping the shipped prewarm total at the EffectManager
pool capacity of 80. Run Tests/EffectWarmUpBudgetTests.py after changing reservations.
WarmUpEffect rejects a full pool before recording DispatchInitialize, so excess reservations
cannot destroy resources referenced by pending GPU commands.
The engine command-line option --verify-ocean-transition runs title -> loading -> STAGE1,
renders gameplay, and exits; its OceanSmoke markers are written to runtime/logs/EngineLog_*.txt.
CreateBufferResource logs the HRESULT and device removal reason before throwing on allocation
failure; Camera also checks Map rather than continuing with an invalid mapped pointer.

## Spray framing
Spray and wake origins lead the aircraft by 0.30 seconds of horizontal travel, clamped to 9-16 world units.
Height is sampled at that projected location. Droplets are split evenly into left/right fans,
with reduced backward velocity and taller mist. Lifetime, particle count, and pool reservation
are unchanged; the camera is unchanged.


## Directional geometric wakes
OceanWakeShape.hlsli is shared by C++ and HLSL. Each stamp uses its saved flight direction.
Two narrow crests open to the sides and trail behind the source as a V; the center mound is removed.
Each lateral profile is a second derivative of a Gaussian, with flanking troughs and zero cross-section
integral before soft saturation. This avoids accumulating a broad positive water mound.
Each arm travels outward at 3.2 units/sec, widen by 0.42 units per unit aft, and fade over 2.5 seconds.
Height gradients include the lateral, longitudinal and front-mask derivatives for consistent normals.
The existing ocean Gerstner waves remain; this directional wake adds vertical displacement only.
GetHeightAt includes the same wake shape. The 48-stamp limit, 1.90 soft height cap and existing
128 x 512 focused mesh remain. This analytic approximation does not simulate collisions/reflections.

Crest foam is enabled with Ocean.PS's kEnableInteractionFoam=true; airborne spray stays disabled
with GamePlayScene's kEnableOceanSpray=false. The vertex shader passes signed interaction height
and slope to the pixel shader. Only raised crests receive patchy, softly antialiased foam, with
world-space noise holes, a maximum blend of 0.50 and a shadow-aware pale blue-white color.
The obsolete circular wake foam is removed; there is no per-pixel loop over wake stamps.
Natural ocean foam, geometric wake displacement, normal calculation and height queries remain active.

The directional profile retains amplitude 0.64 and soft height limit 1.90.


## Smooth heading follow
New stamps follow the actual frame-to-frame horizontal player velocity, including lateral steering.
Yaw follows the shortest arc with a 0.18-second time constant and a 2.5-radian/sec turn-speed limit.
Movement below 0.5 units/sec holds heading to avoid stationary jitter. Pause holds state; death,
teleports and missing movement history reset it to the aircraft flight heading before following again.
The forward spawn offset uses the same smoothed heading. Existing stamps retain their saved direction.


## Near-field speed references
Stage01 adds six small reef clusters alternating left/right at Z=65,145,260,385,535,720.
The existing model is flattened and partially submerged; the new decorative objects have no colliders.
Thin foam streaks use a sparse, jittered 14 x 40 world-space cell pattern with bent lines and broken tips.
The pattern stays anchored to world XZ, so camera travel supplies the visual motion. Blending is capped
at 0.18, with derivative-based antialiasing and distance fade from 80 to 170 units. No new textures,
particles or per-frame CPU objects are needed. Crest foam and the disabled airborne spray are unchanged.
