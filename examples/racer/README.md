# Ring Racer

A lap-racing game on the VVE facade: race three computer cars for three laps, or drive a time trial against the
see-through ghost of your best lap. Day races run under a photographed sky through a countryside of trees, bushes and flowers; at night every car drives with
headlights.

## Controls

| Key | Action |
| --- | --- |
| W / Up | Throttle |
| S / Down | Brake, then reverse |
| A, D / Left, Right | Steer |
| Space | Handbrake (slide) |
| C | Camera: chase, hood, top |
| R | Back onto the track |
| N | Night mode |
| P | Photo mode: hold the right mouse button to orbit, wheel to zoom, click a car to see its lap and speed |
| F11 | Fullscreen |
| Esc | Pause menu |

## Options

`racer [--frames N] [--autopilot] [--time-trial] [--night] [--menu] [--camera chase|hood|top] [--output <png>] [--verify <txt>]`

With `--frames N` the game runs N frames of exactly 1/60 s, so runs repeat exactly, then writes a capture and a
verification file. The run fails when a car corner ever got inside a wall, two cars overlapped by more than 5 cm, a car
drove less than 50 m, picking from above missed the player car, the front-left wheel did not point along heading plus
steering angle, or (time trial) no ghost lap was recorded.

## How it is built

- `RacerLogic.ixx` (`Racer.Logic`): the circuit (a closed Catmull-Rom curve), arcade bicycle-model physics and box
  colliders. `RacerRules.ixx` (`Racer.Rules`): laps and checkpoints, the AI driver, the ghost, the grid and one race step.
  Neither renders anything; `racer_logic_tests.cpp` checks both without a GPU (CTest `RacerLogicTests`).
- `RacerScene.ixx` (`Racer.Scene`): the ground (`assets/racer/ground.obj`, normal-mapped grass tiled every 6 m, under a
  see-through layer of large noise patches that hides the repetition), road, kerbs, walls and gates as `addMesh` meshes
  and cuboids; 371 trees, bushes, grass tufts, flowers and rocks scattered beside the track with a fixed seed and
  recoloured from the Nature Kit's mint palette to natural colours; Kenney cars
  instantiated with `keep_hierarchy`, moved and scaled through their instance root, front wheels steered and all wheels
  spun through their nodes; brake lights hung on the instance root with `setObjectParent`; smoke sprites.
- `RacerRace.ixx` (`Racer.Race`): the shared game types and `RaceSystem`, the user system that keeps one `racer::RaceCar`
  per car in the ECS and steps the race at 120 Hz.
- `RacerView.ixx` (`Racer.View`): `ViewSystem`, the user system that mirrors the race into the scene and runs the cameras,
  lights, sky, smoke, HUD (ImGui, minimap, logo through `GuiSystem::loadImage`) and the music.
- `racer.cpp`: options, the frame loop and the checks of bounded runs.

Physics is 2D in the ground plane. Every body has a collider: each car is an oriented box sized from its model's mesh
bounds, the walls follow the track edges, and every checkpoint gate has two square post colliders. Car boxes collide with
each other (separating-axis test, equal masses), with the posts and with the walls; walls are resolved last, so no push
leaves a car inside a wall.

CTest runs `RacerLogicTests` plus four bounded game runs: `RacerExample` (race), `RacerNightExample`,
`RacerGhostExample` (time trial until the ghost drives) and `RacerMenuExample`. Captures and verification files go to
`<build>/examples/racer/verify`.

## Assets

`assets/racer`, all CC0 (see `assets/racer/license.txt`): four cars from Kenney's Car Kit, fifteen plants and rocks from
Kenney's Nature Kit, the ground material using `assets/game/plane/Grass_005`, the Poly Haven sky
"Kloofendal 48d Partly Cloudy (Pure Sky)" scaled to 4096 x 2048, and `smoke.tga`, `logo.tga` and `variation.tga`,
written by `make_textures.sh`. The music is the V2 soundtrack from `assets/sounds`.
