# Game plan: "Ring Racer", an intermediate car game on the simple engine

Hand-off document for building a small lap-racing game in `examples/racer` with the VVE V3 facade. It is
self-contained: read it, then `AGENTS.md`, `src/versions/simple/AGENTS.md`, `CLAUDE.md` and
`examples/hierarchy_car/hierarchy_car.cpp`, then work through section 5 one step at a time. Line counts are
budgets, not targets.

## 1. Goal

One player races three AI cars for three laps on a closed track, or drives alone against the ghost of their best
lap. It is the next step after the crate collector (`examples/game`): where that game is one loop in `main`, this
one shows how a game is structured on the engine.

- Game state lives in the ECS, and logic runs in user systems (`EngineBuilder<TSystems...>`, `init`/`update` hooks).
- Pure game logic (track, vehicle, race rules) is separate from rendering, so it can be tested without a GPU.
- It uses every feature added in the last round: transparency, background colour, skybox, sprites, input, object
  looks, shapes and meshes, picking, fullscreen and HUD images, plus the scene tree from the car example.

Size: about 1,400 game lines and 400 test lines in total.

## 2. Rules

1. Facade only. The game imports `VVEngine` and `VVE.ExampleSupport`, never `VVEngine.Simple`. If the game needs
   something the facade lacks, stop and add it to the engine as its own step (section 4), not as a workaround.
2. Follow the example conventions: `vve::example::assetRoot`, `--frames N`, `--output <png>`, `--verify <txt>`, and a
   capture plus a verification file for bounded runs (see `hierarchy_car`).
3. Deterministic runs: a fixed physics step (1/120 s with an accumulator), a fixed random seed, and `--autopilot`,
   which lets the AI drive the player car. A bounded run must give the same verification file every time.
4. Keep files under 500 lines. Follow `CLAUDE.md` and the documentation rules in `AGENTS.md`.
5. One commit per step, with its tests and README section. Build and test on Linux after every step:
   `bash build_linux.sh debug`.
6. Keep the "left out" items out.

## 3. What the engine already gives (reuse it)

| Need | Engine feature |
| --- | --- |
| Car with steering and spinning wheels | `assets/hierarchy_car/car.gltf` with `keep_hierarchy`; `setSceneInstanceNodeTransform` on `Car`, `MountFront*` and `Wheel*`. Car +X is forward, wheel base 2.6 m, wheel radius 0.4 m. |
| Several cars | Instantiate the same scene several times; recolour each copy's `Body` objects with `setObjectColor`. |
| Road and barriers | `addMesh(MeshDescriptor)` with normals and UVs for the road ribbon; `addCuboid` or `addMesh` for barriers. |
| Ground, sun and shadows | `addTexturedPlane` with `assets/game/plane/grass.jpg`; `setDirectionalLight`. |
| Sky | `setSkybox(panorama)` and `setBackgroundColor` (see gap G1). |
| Ghost car | A further car instance with `setObjectOpacity(part, 0.4)` on every part; transparent objects cast no shadow. |
| Smoke and dust | `addSprite` with a soft-alpha texture; fade with `setObjectOpacity`, grow with the transform scale. |
| Brake lights | Small unlit cuboids hung on the car's instance root with `setObjectParent`; switch with `setObjectColor`. |
| Headlights (night mode) | Two spot lights re-placed each frame (`clearLights`, `addSpotLight`); 10 spot lights at most. |
| Controls | `vve::Key` names, `vve::key('c')`, mouse buttons, `setMouseCaptured` for the photo camera. |
| Fullscreen | `WindowSetup::fullscreen` and `WindowSystem::setFullscreen` (F11). |
| Click on a car | `screenRay(window, mousePosition)` and `raycast`; boxes are enough for cars. |
| HUD | ImGui through `GuiSystem::draw`; logo and gauge face through `loadImage` and `imageId`. |
| Music | `AudioSystem` and the three V2 tracks, as in `examples/game`. |
| Camera range | `Camera::lookAt(..., ClipPlanes{.far_plane = 250})`; the default far plane is 100 m. |

There is no physics or collision engine. The game does arcade physics in the XZ plane itself: circles for cars,
segments for barriers.

## 4. Gaps to decide before starting

- **G1 Sky.** `setSkybox` takes one 2:1 panorama, but `assets/game/sky/cloudy` holds six cube faces. Either add the
  left-out engine feature "skybox from six images, stitched into a panorama on load" (recommended, about 60 engine
  lines plus a test), or add a CC0 panorama to `assets/racer`.
- **G2 Engine sound.** `AudioPlaybackOptions` has volume but no pitch, so the engine note cannot rise with speed.
  Either cross-fade an idle loop and a high-revs loop by volume (no engine change), or add a per-voice pitch call to
  `AudioSystem` (SDL3_mixer can change a track's frequency ratio).
- **G3 Assets.** Needed: engine idle and high loops, tyre skid, crash and countdown beep sounds, a soft smoke texture
  with alpha, and a logo image. Use CC0 sources, put them in `assets/racer` with a `license.txt`, and copy the
  sounds beside the executable as `examples/game` does.
- **G4 Shadow range.** A 250 m far plane spreads the four directional cascades over a larger range, so near shadows
  get softer. Keep the track inside a 120 m radius and check the captures.

## 5. Steps

Each step lists what to build, what is left out, the test, and a size budget (game lines / test lines).

### Step 1: project and track (250 / 80)

- `examples/racer/`: `racer.cpp` (main and user systems), `RacerLogic.ixx` (module `Racer.Logic`: track, vehicle and
  race rules, no engine rendering calls), `CMakeLists.txt`. Register `RacerExample` (`--frames 600 --autopilot
  --output ... --verify ...`, labels `gpu;example`) and `RacerLogicTests` (a CPU test that imports `Racer.Logic`).
- Track: a closed Catmull-Rom spline through about 12 control points, sampled every 2 m. Each sample stores the
  position, tangent, distance along the track and curvature. The road is one `addMesh` ribbon, 12 m wide, with an up
  normal, and UVs running along the length so a dashed centre line can come from the texture or from thin meshes.
  Kerbs are red and white strips; barriers are low walls along both edges.
- Scene: grass plane, sun with shadows, skybox (G1), and a far plane of 250 m.
- Left out: elevation, banking, bridges and a track editor. The track is flat.
- Test: `RacerLogicTests` checks the track length, that the loop is closed, that `nearest(position)` returns the right
  distance and lateral offset, and the curvature sign in left and right bends. `RacerExample` writes the track length
  and sample count and captures the track from above.

### Step 2: car and driving (250 / 70)

- Instantiate the car with `keep_hierarchy` and keep its node handles. ECS components: `Vehicle` (position, yaw,
  speed, steering angle), `Controls` (throttle, brake, steer, handbrake), `CarVisual` (scene instance, node handles,
  part objects).
- Vehicle model (bicycle model): the yaw rate is `speed / wheelBase * tan(steer)`, with acceleration, braking,
  reverse, rolling and air drag, and a grip limit, so the handbrake lets the rear slide out. Steering is rate-limited
  and gets smaller at high speed.
- Visuals each frame: set the `Car` node to the position and yaw, the front mounts to the steering angle, and the
  wheels to a spin angle from the distance driven.
- Cameras: a chase camera (spring-damped, behind and above, looking ahead), a hood camera and a top camera, switched
  with C.
- Controls: W/Up throttle, S/Down brake and reverse, A/D or Left/Right steer, Space handbrake, R puts the car back on
  the track.
- Left out: gears, suspension, weight transfer and tyre temperature.
- Test: `RacerLogicTests` drives the vehicle model with fixed inputs (straight acceleration to the top speed, the
  circle radius at fixed steering, the braking distance) and compares the results with closed-form values.
  `RacerExample --autopilot` writes the car pose every 60 steps.

### Step 3: race rules and HUD (200 / 60)

- Off track: beyond half the road width plus the kerb, the car gets grass drag and a lower grip.
- Barriers: when the car circle crosses a barrier segment, push it back, reflect the velocity component along the
  barrier normal with damping, and record the impact strength (used for the crash sound in step 7).
- Laps: the progress is the distance along the track. A lap counts only when all six checkpoints were passed in
  order; driving the wrong way shows a warning. Lap timer, best lap, three laps per race.
- HUD (ImGui): speed, lap n/3, current, last and best lap time, position, and a minimap (the track polyline and car
  dots drawn with `ImDrawList`).
- Left out: penalties for cutting corners, damage models.
- Test: `RacerLogicTests` checks lap counting, missed checkpoints, wrong-way detection and barrier push-back.
  `RacerExample` writes lap times for the autopilot run.

### Step 4: AI opponents (200 / 50)

- Three more car instances on a starting grid, with their `Body` objects recoloured by `setObjectColor`.
- AI driver: steer towards a look-ahead point on the racing line (pure pursuit); pick a target speed from the
  curvature ahead; each car gets a small seeded skill factor.
- Car-to-car contact: circles push each other apart and exchange some speed.
- Race flow: a 3-2-1 countdown, then racing, positions ordered by laps and progress, a finish screen with results.
- Left out: overtaking tactics, rubber-banding.
- Test: `RacerLogicTests` runs four AI cars headless for 3 laps with a fixed seed: every car finishes, no car leaves
  the track, the finish order is stable. `RacerExample` writes the finish order and times.

### Step 5: ghost car (100 / 40)

- Record the player pose at every physics step. After a lap that beats the best time, its record becomes the ghost.
- The ghost is a further car instance with opacity 0.4 on every part; in time-trial mode it replays in step with the
  lap timer.
- Left out: saving ghosts to disk, ghosts of AI cars.
- Test: `RacerLogicTests` checks recording and the replay pose at a given lap time. `RacerExample` writes the ghost
  pose at fixed times and that its parts report opacity 0.4.

### Step 6: effects and night mode (150 / 30)

- Smoke and dust: a pool of 64 sprites. Spawn them at the rear wheels when the car drifts, brakes hard or drives on
  grass. Each sprite rises, grows and fades over one second and is then reused (`setObjectVisible(false)`).
- Brake lights: two small unlit cuboids hung on the car's instance root with `setObjectParent`; bright red while
  braking, dark otherwise.
- Checkpoint gates: transparent arches (opacity 0.3) across the road.
- Night mode (N): background colour dark blue, sky off, the sun dimmed, and two spot-light headlights per player car.
- Left out: skid marks, rain, reflections.
- Test: `RacerExample --night` captures one frame and checks that the road in front of the car is brighter than the
  road behind it.

### Step 7: sound (80 / 20)

- Music as in `examples/game`. The engine sound follows speed and throttle (G2); skid and crash sounds use the slip
  and impact values from steps 2 and 3; beeps during the countdown.
- Left out: 3D positional sound, Doppler.
- Test: `RacerExample` with `SDL_AUDIO_DRIVER=dummy` and `--require-audio`, as `GameMusic*` does.

### Step 8: menus and polish (170 / 50)

- Main menu (ImGui): Race, Time trial, Options (laps, number of AI cars, AI skill, night mode, music volume,
  fullscreen), Quit. Esc pauses with Resume, Restart and Menu. The logo is drawn with `loadImage` and `imageId`.
- F11 switches fullscreen with `setFullscreen`.
- Photo mode (P): pause the race, orbit a free camera with the right mouse button held (`setMouseCaptured`), and click
  a car (`screenRay` + `raycast`) to show its name, lap and best time.
- Left out: key remapping, gamepads, saving settings.
- Test: `RacerExample --frames 3 --menu` captures the menu with the logo visible. A picking check: the ray through the
  projected centre of each car's `objectBounds` hits that car.

## 6. Order and checks

Do the steps in order; each one leaves a playable game. Decide G1 to G3 before step 1 (sky and assets) and G2 before
step 7. After each step: build, run `ctest --test-dir build/debug-linux --output-on-failure`, play the game for a
minute, look at the capture, and update `examples/racer/README.md` (controls, options, asset licenses) and the
examples list in the top-level README. Stop and ask when a design here turns out not to fit the engine.
