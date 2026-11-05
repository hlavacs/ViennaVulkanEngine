/** @file @brief Shared asset discovery, runtime renderer selection and bounded captures for facade examples. */
export module VVE.ExampleSupport;
import std;
import VVEngine;

export namespace vve::example {

inline constexpr std::array rendererNames{std::string_view{"forward"}, std::string_view{"deferred"},
	std::string_view{"hybrid-gi"}}; ///< Example CLI and diagnostic spellings in RendererBackend order.

/// @brief Finds a root containing the requested file, checking cwd, executable ancestors, then the configured source root.
[[nodiscard]] std::filesystem::path assetRoot(char *argv0, const std::filesystem::path &requiredAsset) {
	const auto containsAsset = [&requiredAsset](const std::filesystem::path &candidate) {
		return std::filesystem::is_regular_file(candidate / requiredAsset);
	};
	if (const auto cwd = std::filesystem::current_path(); containsAsset(cwd)) { return cwd; }
	if (argv0 != nullptr) {
		auto executable = std::filesystem::absolute(std::filesystem::path{argv0});
		if (std::filesystem::exists(executable)) { executable = std::filesystem::weakly_canonical(executable); }
		// Skip partial asset directories belonging to other examples when walking executable ancestors.
		for (auto candidate = executable.parent_path(); !candidate.empty(); candidate = candidate.parent_path()) {
			if (containsAsset(candidate)) { return candidate; }
			if (candidate == candidate.root_path()) { break; }
		}
	}
	return std::filesystem::path{VVE_ASSET_ROOT}; // Out-of-source builds retain the configured asset location.
}

/// @brief Reads a nonnegative --frames value; zero or no valid option means run until closed.
[[nodiscard]] int frameLimit(int argc, char **argv) {
	// Ignore unrelated or invalid options, preserving the examples' command-line parsing.
	for (int index = 1; index + 1 < argc; ++index) {
		if (argv[index] == nullptr || argv[index + 1] == nullptr) { continue; }
		if (std::string_view{argv[index]} != "--frames") { continue; }
		int value{};
		const std::string_view text{argv[index + 1]};
		const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		if (result.ec == std::errc{} && value >= 0) { return value; }
	}
	return 0;
}

/// @brief Selects a public renderer before the first frame; invalid names and unsupported devices fail the launch.
[[nodiscard]] bool selectRenderer(RenderSystem &render, int argc, char **argv) {
	auto backend = RendererBackend::forward; // Ordinary launches preserve the engine's default renderer.
	// Parse the same option in every application, including those with independent game-specific options.
	for (int index = 1; index < argc; ++index) {
		if (argv[index] == nullptr || std::string_view{argv[index]} != "--renderer") { continue; }
		if (++index >= argc || argv[index] == nullptr) {
			std::cerr << "--renderer requires forward, deferred or hybrid-gi\n"; return false;
		}
		const std::string_view name{argv[index]};
		const auto selected = std::ranges::find(rendererNames, name);
		if (selected == rendererNames.end()) {
			std::cerr << "Unknown renderer: " << name << "; expected forward, deferred or hybrid-gi\n"; return false;
		}
		backend = static_cast<RendererBackend>(selected - rendererNames.begin());
	}
	if (const auto result = render.setRendererBackend(backend); !result) {
		std::cerr << "Renderer selection failed: " << errorName(result.error()) << '\n'; return false;
	}
	std::cout << "[renderer] backend=" << rendererNames[std::to_underlying(render.rendererBackend())] << '\n';
	return true;
}

/// @brief Saves a bounded run's last frame; --output overrides the executable-relative verification PNG.
[[nodiscard]] bool captureBounded(RenderSystem &render, int argc, char **argv, std::string_view application) {
	if (frameLimit(argc, argv) == 0) { return true; }
	const auto executable = std::filesystem::absolute(argc > 0 && argv[0] != nullptr ? argv[0] : ".");
	auto path = executable.parent_path().parent_path() / "verify" / (std::string{application} + ".png");
	// A separate output per backend keeps comparison captures from replacing one another.
	for (int index = 1; index + 1 < argc; ++index) {
		if (argv[index] != nullptr && std::string_view{argv[index]} == "--output") { path = argv[index + 1]; }
	}
	if (const auto result = render.captureFrameToPng(path); !result) {
		std::cerr << '[' << application << "] capture failed: " << errorName(result.error()) << '\n'; return false;
	}
	std::cout << '[' << application << "] capture=" << path << " rendered_frames=" << render.renderedFrameCount()
		<< " objects=" << render.sceneInstanceCount() << " meshes=" << render.sceneMeshCount()
		<< " textures=" << render.sceneTextureCount() << '\n';
	return true;
}

} // namespace vve::example
