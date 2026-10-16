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
steering angle, (time trial) no ghost lap was recorded, or fewer than 300 plants, 20 countryside pieces or 150
spectators were placed.

## How it is built

- `RacerLogic.ixx` (`Racer.Logic`): the circuit (a closed Catmull-Rom curve), arcade bicycle-model physics and box
  colliders. `RacerRules.ixx` (`Racer.Rules`): laps and checkpoints, the AI driver, the ghost, the grid and one race step.
  Neither renders anything; `racer_logic_tests.cpp` checks both without a GPU (CTest `RacerLogicTests`).
- `RacerScene.ixx` (`Racer.Scene`): the ground (`assets/racer/ground.obj`, normal-mapped Poly Haven grass tiled every
  4 m, under a see-through layer of large noise patches that hides the repetition), the asphalt (every 2 m), white edge
  lines, red and white concrete kerbs, concrete walls and gates as `addMesh` meshes with texture coordinates in metres; about 600 trees, bushes, grass tufts, flowers
  and rocks scattered beside the track and in a sparser belt further out, with a fixed seed, kept out of the areas the
  countryside uses and recoloured from the Nature Kit's mint palette to natural colours; Kenney cars instantiated with
  `keep_hierarchy`, moved and scaled through their instance root, front wheels steered and all wheels spun through their
  nodes, the body node leaning out of bends and pitching under acceleration and braking (`leanBody`, eased towards
  targets from the sideways and forward acceleration); brake lights hung on the instance root with `setObjectParent`;
  smoke sprites.
- `RacerWorld.ixx` (`Racer.World`): the countryside. Kenney kit models: the inflatable finish arch over the start line,
  pit garages, a lit paddock road, tents and trucks in the infield, and a town of houses along a lit street with a fountain
  square east of the circuit. A covered grandstand outside the start straight holds about 200 spectators: Kenney Mini
  Characters, each posed on its own (the joints of `assets/racer/crowd` turned like Kenney's sit animation; about one in
  five cheers with raised arms or waves, some stand in the top row, three wheelchair users have places between the front
  benches), all merged into one mesh with their shared colormap. Without `assets/racer/crowd`, box people sit there.
  Built from boxes, gabled roofs and cylinders: the grandstand itself, a farm (barn, farmhouse, silo, shed with straw)
  west of the circuit with fenced yard, pasture and hay meadow, wooden fences, round hay bales, and tyre stacks behind
  the outer wall of every sharp bend.
  Textured fields (`wheat.tga`, `plowed.tga`, `crops.tga`) lie north, south and inside the circuit, next to a pond. The
  procedural parts carry Poly Haven textures tinted by their colour (red weathered planks for the barn, white plaster
  and clay roof tiles for the farmhouse, corrugated iron for roofs and the silo, concrete for the grandstand) and are
  merged into one mesh per look, so the countryside costs about 80 draw calls.
- `RacerRace.ixx` (`Racer.Race`): the shared game types and `RaceSystem`, the user system that keeps one `racer::RaceCar`
  per car in the ECS, steps the race at 120 Hz and reports the strongest impact of each car in the frame.
- `RacerView.ixx` (`Racer.View`): `ViewSystem`, the user system that builds the world, mirrors the race into the scene
  and runs the cameras, lights, sky, smoke and HUD (ImGui, minimap, logo through `GuiSystem::loadImage`). At night the
  skybox switches to `night_sky.jpg`: dark clouds, stars and the moon where the day photo has its sun.
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
Kenney's Nature Kit, the Poly Haven sky "Kloofendal 48d Partly Cloudy (Pure Sky)" scaled to 4096 x 2048, the night sky
`night_sky.jpg` made from it by `make_night_sky.py` (Pillow and NumPy), the textures `smoke.tga`, `logo.tga`,
`variation.tga`, `wheat.tga`, `plowed.tga` and `crops.tga`, written by `make_textures.sh`, and the race signals and crowd
in `assets/racer/sounds`, written by `make_sounds.sh`. The music is the V2 soundtrack from `assets/sounds`.

`assets/racer/textures` holds ten 1024 x 1024 JPGs made from Poly Haven textures (CC0) by `prepare_textures.py`, which
levels their brightness and colour so that the game can tint them. To make them again, download the 1k JPG maps below
from `https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/<asset>/<file>` into one folder, for example
`assets/polyhaven` (not part of the repository), and run `python examples/racer/prepare_textures.py assets/polyhaven`:

| Texture | Poly Haven asset | Downloaded file | Used for |
| --- | --- | --- | --- |
| `asphalt_track.jpg` | [asphalt_track](https://polyhaven.com/a/asphalt_track) | `asphalt_track_diff_1k.jpg` | road |
| `leafy_grass.jpg` | [leafy_grass](https://polyhaven.com/a/leafy_grass) | `leafy_grass_diff_1k.jpg` | ground (`ground.mtl`) |
| `leafy_grass_normal.jpg` | [leafy_grass](https://polyhaven.com/a/leafy_grass) | `leafy_grass_nor_gl_1k.jpg` | ground normal map |
| `concrete_wall.jpg` | [concrete_wall_008](https://polyhaven.com/a/concrete_wall_008) | `concrete_wall_008_diff_1k.jpg` | track walls |
| `concrete_kerb.jpg` | [concrete_floor_01](https://polyhaven.com/a/concrete_floor_01) | `concrete_floor_01_diff_1k.jpg` | kerbs |
| `concrete_floor.jpg` | [concrete_floor_02](https://polyhaven.com/a/concrete_floor_02) | `concrete_floor_02_diff_1k.jpg` | grandstand |
| `weathered_planks.jpg` | [weathered_planks](https://polyhaven.com/a/weathered_planks) | `weathered_planks_diff_1k.jpg` | barn, doors, fences, shed |
| `white_plaster.jpg` | [white_plaster_02](https://polyhaven.com/a/white_plaster_02) | `white_plaster_02_diff_1k.jpg` | farmhouse |
| `clay_roof_tiles.jpg` | [clay_roof_tiles_02](https://polyhaven.com/a/clay_roof_tiles_02) | `clay_roof_tiles_02_diff_1k.jpg` | farmhouse roof |
| `corrugated_iron.jpg` | [corrugated_iron](https://polyhaven.com/a/corrugated_iron) | `corrugated_iron_diff_1k.jpg` | barn, shed and grandstand roofs, silo |

`assets/racer/kenney` holds files from two of Kenney's starter kits on GitHub (models and sounds CC0), copied with their
folder structure, because every model finds its `Textures/colormap.png` next to it:

| Folder | From | Files |
| --- | --- | --- |
| `kenney/racing/` | [Starter-Kit-Racing](https://github.com/KenneyNL/Starter-Kit-Racing) | `audio/engine.ogg`, `audio/skid.ogg`, `audio/impact.ogg`, `models/Textures/colormap.png`, `models/track-finish.glb`, `models/decoration-tents.glb`, `models/vehicle-truck-{green,red,yellow,purple}.glb` |
| `kenney/city/` | [Starter-Kit-City-Builder](https://github.com/KenneyNL/Starter-Kit-City-Builder) | `models/Textures/colormap.png`, `models/building-small-{a,b,c,d}.glb`, `models/building-garage.glb`, `models/road-straight-lightposts.glb`, `models/pavement.glb`, `models/pavement-fountain.glb`, `sounds/ambience.ogg`, `sounds/placement-a.ogg` |

A missing kit file only leaves its piece out (the start line then gets the yellow see-through gate); the game prints how
many kit models are missing.

`assets/racer/crowd` holds the twelve characters and four wheelchairs of Kenney's
[Mini Characters](https://kenney.nl/assets/mini-characters) (CC0) and their `colormap.png`. The download's characters
are skinned, which the engine does not draw, so `split_characters.py` (NumPy) writes each one again as a plain node tree
with the same joint names (root, legs, torso, arms, head), every node carrying the triangles of its body part, and
flattens the wheelchairs into one mesh. To make them again, unpack the download, for example into
`assets/kenney_mini-characters` (not part of the repository), and run
`python examples/racer/split_characters.py assets/kenney_mini-characters`.
