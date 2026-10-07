/** @file @brief Shared asset discovery and frame limits for the facade examples. */
export module VVE.ExampleSupport;
import std;

export namespace vve::example {

/// @brief Finds assets in the cwd, then executable ancestors, then the configured source root.
[[nodiscard]] std::filesystem::path assetRoot(char *argv0) {
	auto containsAssets = [](const std::filesystem::path &candidate) {
		return std::filesystem::is_directory(candidate / "assets");
	};
	if (const auto cwd = std::filesystem::current_path(); containsAssets(cwd)) { return cwd; }
	if (argv0 != nullptr) {
		auto executable = std::filesystem::absolute(std::filesystem::path{argv0});
		if (std::filesystem::exists(executable)) { executable = std::filesystem::weakly_canonical(executable); }
		// Support launches from the executable directory or any other working directory.
		for (auto candidate = executable.parent_path(); !candidate.empty(); candidate = candidate.parent_path()) {
			if (containsAssets(candidate)) { return candidate; }
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

} // namespace vve::example
