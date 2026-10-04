#include <chrono>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>

import VVEngine;

/// @file
/// @brief A facade-only audio example: no window, Vulkan device or startup script is needed.

/// @brief Plays two overlapping voices, pauses one, resumes it and stops the loop.
/// An optional WAV/OGG path replaces the bundled sound; --verify is the bounded CTest path.
int main(int argc, char **argv) {
    const bool verify = argc == 2 && std::string_view{argv[1]} == "--verify";
    // argv[0] identifies the executable when launched directly from its folder or by absolute path.
    const auto directory = std::filesystem::absolute(argv[0]).parent_path();
    const auto path = argc > 1 && !verify ? std::filesystem::path{argv[1]} : directory / "audio/demo.wav";

    vve::Engine engine;
    auto world = engine.world();
    auto &audio = world.get<vve::AudioSystem>();
    auto failed = [&audio](std::string_view operation) {
        std::cerr << operation << ": " << audio.lastError() << '\n';
        return 1;
    };
    if (!audio.init()) { return failed("Opening audio"); }
    const auto sound = audio.loadSound(path);
    if (!sound) { return failed("Loading sound"); }
    if (!audio.setMasterVolume(vve::AudioVolume{.value = 0.7F})) { return failed("Master volume"); }

    const auto music = audio.play(*sound, vve::AudioPlaybackOptions{.loop = true, .volume = {.value = 0.35F}});
    const auto effect = audio.play(*sound);
    if (!music || !effect) { return failed("Starting playback"); }
    if (!audio.isPlaying(*music) || !audio.isPlaying(*effect)) { return failed("Overlapping playback"); }
    std::cout << "Playing a looping sound and a separate effect.\n";
    const auto delay = verify ? std::chrono::milliseconds{30} : std::chrono::milliseconds{900};
    std::this_thread::sleep_for(delay);

    if (!audio.pause(*music) || !audio.isPaused(*music)) { return failed("Pausing loop"); }
    std::cout << "Loop paused.\n";
    std::this_thread::sleep_for(delay);
    if (!audio.resume(*music) || !audio.setVolume(*music, vve::AudioVolume{.value = 0.5F})) {
        return failed("Resuming loop");
    }
    std::cout << "Loop resumed at a new volume.\n";
    std::this_thread::sleep_for(delay);

    if (!audio.stop(*music) || audio.isPlaying(*music)) { return failed("Stopping loop"); }
    if (!audio.unloadSound(*sound)) { return failed("Unloading sound"); }
    std::cout << "Audio demo passed.\n";
    return 0; // Engine destruction releases the audio device automatically.
}
