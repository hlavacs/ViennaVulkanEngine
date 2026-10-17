# Island Air

A C++23 VVE game with **256 randomly scattered named islands**. Start on
North Haven's runway with cargo and an assigned destination, follow the
direction arrow on the small map, land, taxi into the
yellow unloading area and remain stopped for two seconds. Then collect the
next cargo and continue from that island with the same aircraft. Flying over
the destination never completes a delivery. The default
window is **1920 × 1080**, with four-sample antialiasing, physically based
materials, a photographed sky, atmospheric haze, animated ocean, island
airfields and a downloaded textured twin-prop aircraft.

The game uses only `import VVEngine` and its public facade. VVE supplies Vulkan
rendering and audio; there are no direct Vulkan, SDL or implementation imports.
The aircraft's authoritative `Flight` component lives in VVE's ECS. The
fixed 120 Hz flight model is deliberately approachable, with rotation speed,
stall descent, bank/rudder turning, tight nose-wheel steering, wheel brakes and
hard-landing failures. The procedural archipelago spans approximately 44 km
in each direction. A fixed random seed keeps navigation and tests reproducible;
island sites are sampled across the region with at least 2.2 km separation.
Only the nearest nine islands within four kilometres own
scenery resources; distant islands use simpler geometry, nearby airfields get
full detail, and departed scenery is released. The same downloaded asset set
serves the whole archipelago; 45 ocean tiles follow the aircraft.

## Build and run

Use the repository's normal build script, or an already configured build:

```sh
./build_linux.sh release
# Incremental build after the normal VVE configuration:
cmake --build build/release-linux --target island_delivery
bin/release/exe/island_delivery
```

On Windows use `build_windows.cmd release`, then
`bin\release\exe\island_delivery.exe`. The existing macOS script and presets
also include the example. The game locates assets beside its executable, so it
can be launched from another working directory.

## Configure-time assets

Enabling examples downloads the aircraft, original Poly Haven photographs and
eight recorded sounds during CMake configuration. Every file has a pinned
SHA256; the model and its license also have a pinned upstream commit.
External assets stay in `<build>/_assets/island_delivery`, and the game build
copies them to `bin/<variant>/exe/assets/island_delivery`. **No external asset
binaries are added to this repository.** The manifest is
[cmake/IslandDeliveryAssets.cmake](../../cmake/IslandDeliveryAssets.cmake).

An intact cache is reused without downloads. Share a cache or configure offline:

```sh
cmake -S . -B build/release-linux \
  -DVVE_ISLAND_ASSET_DIR=/absolute/path/to/island_delivery \
  -DVVE_ISLAND_DOWNLOAD_ASSETS=OFF
```

Offline configuration fails clearly if a required file is absent or its hash
has changed. `--asset-dir DIRECTORY` selects an existing asset directory at
runtime. Credits and selected licenses are in [ASSET_LICENSES.txt](ASSET_LICENSES.txt)
and [AUDIO_LICENSE.txt](AUDIO_LICENSE.txt); both accompany packaged assets.

## Controls

| Input | Action |
| --- | --- |
| W / S | Increase / decrease throttle; the lever retains its setting |
| Down / Up | Nose up / nose down |
| Left / Right | Bank left / right |
| A / D | Rudder; steer the nose wheel while taxiing |
| Space | Wheel brakes |
| M | Open/close the optional top-down archipelago chart; flight pauses while it is open |
| C | Chase, pilot-eye instruments, aircraft showcase camera |
| P | Toggle autopilot through the same physics as manual flight |
| R | Retry the current job from its departure island |
| Escape | Pause and sound volume menu |
| F11 | Toggle desktop fullscreen |

The game opens directly on the starting island with a cargo assignment shown
in the flight panel. After unloading, the destination airport supplies the next
cargo and a nearby destination automatically. The aircraft stays where you
stopped, ready to turn around and depart again. Completed deliveries accumulate.
The route and manifest remain assigned until delivery; an intermediate landing
cannot change them. M opens the large chart when you want to inspect the region.

The regional navigation inset stays visible during flight. It shows the actual
aircraft heading, a short gold direction arrow and bearing to the destination, nearby
islands and their runway outlines. It zooms in near the destination so the
700 m strip, **18/36** runway ends and unloading bay are visible before landing.
The direction arrow acts as a compass cue, without drawing a route across the
small map. On the large chart, use the mouse wheel to zoom and drag with the
right mouse button to pan; hover islands for their names. Both maps are
north-up, with north along -Z.

Take off above 27 m/s (about 53 knots). Keep touchdown below 47 m/s, sink rate
below 4 m/s, wings near level and heading aligned with the strip. Water and
off-runway touchdowns end the flight. At taxi speed, A/D gives a turn radius
of about 5.6 m, allowing a full turnaround within the strip; keep the throttle
low and use Space to brake. The autopilot aligns on the ground before adding
departure power, then demonstrates departure, navigation, an aligned approach,
landing, ground roll and stationary delivery.

Audio crossfades recorded propeller layers with throttle, changes playback
pitch with power, blends a separate cabin recording in pilot-eye view, and
adds airflow, coastal ambience, wheels, touchdown and unloading. Positional
sources and Doppler use VVE's `AudioSystem`; pause also pauses playback cursors.
`--mute` skips opening an audio device; a missing physical output device is
reported in telemetry and does not prevent flight.

## Deterministic verification

Bounded runs emit a PNG and CSV flight telemetry, then return nonzero for a
crash, incomplete run, failed capture or required delivery that did not finish:

```sh
bin/release/exe/island_delivery --autopilot --frames 60 \
  --steps-per-frame 240 --require-delivery \
  --output build/delivery.png --verify build/delivery.csv
# Two completed jobs, with new cargo collected without resetting the aircraft:
bin/release/exe/island_delivery --autopilot --deliveries 2 --require-delivery \
  --frames 60 --steps-per-frame 1200 --output build/two_jobs.png --verify build/two_jobs.csv
# The same gameplay at the opposite end of the archipelago:
bin/release/exe/island_delivery --start-island 254 --autopilot \
  --frames 60 --steps-per-frame 1200 --require-delivery
ctest --test-dir build/release-linux --output-on-failure -R IslandDelivery
```

Each bounded frame advances two fixed 120 Hz physics steps by default.
`--steps-per-frame 240` samples two simulated seconds per rendered frame while
preserving every physics step, making a complete mission practical in tests.
The autopilot never teleports or bypasses landing checks. `--width`, `--height`,
`--hidden`, `--no-msaa` and `--camera chase|cockpit|showcase` support repeatable
captures. `--help` opens no window. `IslandDeliveryLogicTests` checks flight,
all 256 island sites, safe/unsafe landings, unloading gates, cargo assignment,
runway turnarounds, reverse/lateral routes and repeated deliveries without a GPU.
`IslandDeliveryMapTests` checks the assigned route, all island coastlines,
compass arrow directions and runway guidance at Full HD and 640 × 360.
`IslandDeliverySoundTests` checks mixing telemetry and decodes/plays every
download through VVE with SDL's dummy audio device. GPU tests capture the
departure, pilot-eye view, the map, repeated deliveries and a remote airport.
`IslandDeliverySceneTests` checks the imported aircraft's wheel contact, nose
alignment, propeller animation, automatic airport assignments, bounded scenery residency and resource release
across distant islands without opening a window. CSV verification includes
departure/target IDs, completed deliveries and resident-island counts.

## Presentation scope

This playable example establishes the requested photorealistic direction using
actual aircraft geometry, photographs and field recordings. The current VVE
ocean and environment lighting remain approximations; the compact aircraft has
no detailed cockpit interior, and the generated terrain and palms still need
art refinement to reach production hyperrealism. Performance at Full HD must
be measured on the intended GPU; software Vulkan captures establish correctness.
