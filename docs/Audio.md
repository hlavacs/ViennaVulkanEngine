# Audio in VVE

VVE's `AudioSystem` facade uses SDL3_mixer. Import `VVEngine` and obtain audio from
`engine.world()`, just like the renderer or asset system. Games do not include SDL
headers or call SDL themselves.

## Build and run

The normal VVE build scripts install `sdl3-mixer` through vcpkg, compile the audio
implementation and copy Windows runtime DLLs alongside the executables. VVE keeps
its original dependency baseline; `vcpkg-configuration.json` pins only SDL3_mixer
to a newer registry snapshot. No new build or launch scripts are needed.

```text
build_windows.cmd debug
build_windows.cmd release
./build_linux.sh debug
./build_macos.sh release
```

Run `bin/debug/exe/audio_demo.exe` or `bin/release/exe/audio_demo.exe` on Windows;
on Linux/macOS the executable is named `audio_demo`. It plays a bundled original
sound, demonstrates overlapping playback, pause/resume and volume, then exits.
An optional command-line audio filename replaces the bundled sound. CMake copies
the default WAV into the executable's `audio` directory.

A program links only `ViennaVulkanEngine::ViennaVulkanEngine`; VVE links its mixer.
If your program manages its own vcpkg manifest, include `sdl3-mixer` there too and
use a baseline containing the port (or VVE's package-specific registry pin).
An already-built VVE also needs its matching SDL3 and SDL3_mixer runtime libraries.

## First sound

```cpp
#include <filesystem>
#include <iostream>
import VVEngine;

// After constructing your engine (inside a user system, use the supplied world):
auto world = engine.world();
auto &audio = world.get<vve::AudioSystem>();
if (const auto result = audio.init(); !result) {
    std::cerr << audio.lastError() << '\n';
    return 1;
}
const auto sound = audio.loadSound("assets/jump.wav");
if (!sound) { return 1; }
const auto voice = audio.play(*sound);
if (!voice) { return 1; }
// Keep the engine alive while audio plays; return to your normal game loop.
```

Audio is optional: engine construction and `engine.init()` do not open an audio
device. Call `audio.init()` in your game's initialization and handle its result.
If there is no working device, it returns `Error::platform_error`; graphics can
still run if the game elects to continue silently. Repeating audio initialization
is harmless. Every fallible operation returns `std::expected<..., vve::Error>`;
`lastError()` provides the latest failure description, including SDL's diagnostic.

## Sounds and voices

`loadSound(path)` returns a `SoundHandle`: an engine-owned reusable resource.
Default `AudioLoadMode::decoded` converts the file to PCM up front, useful for
short effects. `AudioLoadMode::on_demand` retains encoded data and decodes while
playing; it can reduce memory usage for longer compressed audio but is not disk
streaming. WAV, OGG Vorbis and MP3 work with VVE's decoder build. The manifest
enables SDL3_mixer's `mpg123` feature so V2's original MP3 soundtrack plays. Other
formats depend on which SDL3_mixer decoder features are installed.

Each `play(sound)` returns a different `AudioPlaybackHandle`, even for the same
sound. Two calls can play simultaneously; one voice's volume or pause state does
not change the other. The initial voice volume is one and looping is disabled.

```cpp
const auto music = audio.play(*sound, vve::AudioPlaybackOptions{
    .loop = true, .volume = vve::AudioVolume{.value = 0.4F}});
if (!music) { return 1; }
// Check the expected result from each control call in production code:
auto quieter = audio.setMasterVolume(vve::AudioVolume{.value = 0.7F});
auto paused = audio.pause(*music);
auto resumed = audio.resume(*music);
auto stopped = audio.stop(*music);
```

Volumes are finite linear gains in `[0, 1]`: zero is silent, one is full volume.
The effective gain is voice volume multiplied by master volume. NaN, infinity,
negative values and values above one return `Error::invalid_argument`.
Overlapping loud sources can still clip; leave headroom with the master volume.
`volume(voice)` reads an individual gain; `masterVolume()` reads the master gain.

`pause(voice)` preserves the playback cursor. `resume(voice)` continues from it.
`isPlaying(voice)` is false while paused; `isPaused(voice)` distinguishes that
state. Both queries return false for unknown or expired handles.

## Ownership and cleanup

`stop(voice)` immediately stops and releases that voice. `stopAll()` releases
all voices while keeping sounds loaded. `unloadSound(sound)` stops every voice
using that sound before releasing it. `shutdown()` releases everything, closes
the device and is safe to repeat. Calling `init()` afterwards creates a fresh
audio system: old handles stay invalid and the master gain returns to one.

Finished voices are collected during `engine.step()` and before the next
`play()`. Their control handles then return `Error::invalid_handle`. Paused
voices are retained until resumed or explicitly stopped. Loaded sounds remain
available until unloaded or shutdown. Handles belong to their originating engine;
passing them to another engine is an error.

All facade calls belong on the game's main thread. SDL3_mixer's audio thread
performs decoding and mixing independently; you do not push samples each frame.
Keep the engine alive during playback. Engine destruction cleans up automatically;
Worlds and facade copies are non-owning views and must not outlive the engine.
This API provides ordinary output and volume controls, without positional audio.

## Verification

`AudioSystemTests` authors WAV fixtures in C++ and checks overlap, looping,
pause/resume, gain validation, bad files, handle ownership, cleanup and reinitialization.
`AudioDemo` exercises the bundled runnable example. Both use SDL's dummy audio
driver during CTest, so no speakers or audio hardware are required.

VVE's Crate Collector and Relay Siege also reuse V2's original Dance, Ophelia and
Never Get Out tracks. Both provide a music selector, a volume slider (zero mutes)
and a music pause toggle. Crate Collector's files are staged in `audio/v2` beside
`game`; Relay Siege's copies are in its executable's `assets/sounds` directory.
The original `license.txt` and `V2_MUSIC.md` credits travel with the music.
`LegacyMusicTests` decodes every original track in both load modes; each game's
three music smoke tests initialize its real startup path with a dummy audio device.

```text
ctest --test-dir build/debug-windows --output-on-failure -L audio
```
