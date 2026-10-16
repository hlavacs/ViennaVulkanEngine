# Ring Racer

A lap-racing game on the VVE facade: race three computer cars for three laps, or drive a time trial against the
see-through ghost of your best lap. Day races run under a photographed sky through a countryside with a grandstand full of
spectators, a paddock, a small town, a farm, fields, a pond and fences; at night every car drives with headlights. You
hear the engines rise and fall with the speed, the tyres squeal, crashes, the countdown, lap chimes, birds and the crowd.

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
steering angle, (time trial) no ghost lap was recorded, or fewer than 300 plants or 20 countryside pieces were placed.

## How it is built

- `RacerLogic.ixx` (`Racer.Logic`): the circuit (a closed Catmull-Rom curve), arcade bicycle-model physics and box
  colliders. `RacerRules.ixx` (`Racer.Rules`): laps and checkpoints, the AI driver, the ghost, the grid and one race step.
  Neither renders anything; `racer_logic_tests.cpp` checks both without a GPU (CTest `RacerLogicTests`).
- `RacerScene.ixx` (`Racer.Scene`): the ground (`assets/racer/ground.obj`, normal-mapped grass tiled every 6 m, under a
  see-through layer of large noise patches that hides the repetition), the asphalt (`asphalt.tga`, repeated every 3 m),
  white edge lines, kerbs, walls and gates as `addMesh` meshes and cuboids; about 600 trees, bushes, grass tufts, flowers
  and rocks scattered beside the track and in a sparser belt further out, with a fixed seed, kept out of the areas the
  countryside uses and recoloured from the Nature Kit's mint palette to natural colours; Kenney cars instantiated with
  `keep_hierarchy`, moved and scaled through their instance root, front wheels steered and all wheels spun through their
  nodes; brake lights hung on the instance root with `setObjectParent`; smoke sprites.
- `RacerWorld.ixx` (`Racer.World`): the countryside. Kenney kit models: the inflatable finish arch over the start line,
  pit garages, a lit paddock road, tents and trucks in the infield, and a town of houses along a lit street with a fountain
  square east of the circuit. Built from boxes, gabled roofs and cylinders: a covered grandstand with about 200
  spectators outside the start straight, a farm (barn, farmhouse, silo, shed with straw) west of the circuit with fenced
  yard, pasture and hay meadow, wooden fences, round hay bales, and tyre stacks behind the outer wall of every sharp bend.
  Textured fields (`wheat.tga`, `plowed.tga`, `crops.tga`) lie north, south and inside the circuit, next to a pond. The
  procedural parts are merged into one mesh per colour, so the countryside costs about 80 draw calls.
- `RacerRace.ixx` (`Racer.Race`): the shared game types and `RaceSystem`, the user system that keeps one `racer::RaceCar`
  per car in the ECS, steps the race at 120 Hz and reports the strongest impact of each car in the frame.
- `RacerView.ixx` (`Racer.View`): `ViewSystem`, the user system that builds the world, mirrors the race into the scene
  and runs the cameras, lights, sky, smoke and HUD (ImGui, minimap, logo through `GuiSystem::loadImage`).
- `RacerSound.ixx` (`Racer.Sound`): `SoundSystem`, the user system for everything heard. Each car has a looping engine
  whose pitch (`AudioSystem::setPitch`) follows its speed through five gears; the player's tyres squeal while sliding or
  braking hard; crashes play with their strength; the countdown beeps, the start tone, a chime per lap (higher for a best
  lap) and the finish fanfare mark the race; birdsong plays everywhere and the crowd near the grandstand. Every sound but
  the music fades with the distance from the camera. Menu buttons click.
- `racer.cpp`: options, the frame loop and the checks of bounded runs.

Physics is 2D in the ground plane. Every body has a collider: each car is an oriented box sized from its model's mesh
bounds, the walls follow the track edges, and every checkpoint gate has two square post colliders. Car boxes collide with
each other (separating-axis test, equal masses), with the posts and with the walls; walls are resolved last, so no push
leaves a car inside a wall. The countryside has no colliders; it all lies behind the walls.

## Tests

`RacerLogicTests` runs with every build. The four bounded game runs, `RacerExample` (race), `RacerNightExample`,
`RacerGhostExample` (time trial until the ghost drives) and `RacerMenuExample`, take a while, so they carry the CTest
label `racer` and the build scripts skip them (`ctest -LE racer`). Run them on request:

```
ctest --test-dir build/release-windows -L racer --output-on-failure
```

Captures and verification files go to `<build>/examples/racer/verify`.

## Assets

`assets/racer`, all CC0 (see `assets/racer/license.txt`): four cars from Kenney's Car Kit, fifteen plants and rocks from
Kenney's Nature Kit, the ground material using `assets/game/plane/Grass_005`, the Poly Haven sky
"Kloofendal 48d Partly Cloudy (Pure Sky)" scaled to 4096 x 2048, the textures `smoke.tga`, `logo.tga`, `variation.tga`,
`asphalt.tga`, `wheat.tga`, `plowed.tga` and `crops.tga`, written by `make_textures.sh`, and the race signals and crowd
in `assets/racer/sounds`, written by `make_sounds.sh`. The music is the V2 soundtrack from `assets/sounds`.

`assets/racer/kenney` holds files from two of Kenney's starter kits on GitHub (models and sounds CC0), copied with their
folder structure, because every model finds its `Textures/colormap.png` next to it:

| Folder | From | Files |
| --- | --- | --- |
| `kenney/racing/` | [Starter-Kit-Racing](https://github.com/KenneyNL/Starter-Kit-Racing) | `audio/engine.ogg`, `audio/skid.ogg`, `audio/impact.ogg`, `models/Textures/colormap.png`, `models/track-finish.glb`, `models/decoration-tents.glb`, `models/vehicle-truck-{green,red,yellow,purple}.glb` |
| `kenney/city/` | [Starter-Kit-City-Builder](https://github.com/KenneyNL/Starter-Kit-City-Builder) | `models/Textures/colormap.png`, `models/building-small-{a,b,c,d}.glb`, `models/building-garage.glb`, `models/road-straight-lightposts.glb`, `models/pavement.glb`, `models/pavement-fountain.glb`, `sounds/ambience.ogg`, `sounds/placement-a.ogg` |

A missing kit file only leaves its piece out (the start line then gets the yellow see-through gate); the game prints how
many kit models are missing.
