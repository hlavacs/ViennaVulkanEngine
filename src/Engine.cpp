module VEEngine;
import std;

/// @file
/// @brief Creates the selected engine implementation from facade startup options.

namespace vve {

	namespace detail {

		/// @brief Converts startup window options and returns the implementation owned by the facade.
		std::unique_ptr<EngineImpl> makeEngineImpl(EngineStartupOptions options) {
			if (options.windows.has_value()) {
				return std::make_unique<EngineImpl>(std::move(options.application_name), options.max_frames,
					WindowsImpl{.value = std::move(*options.windows)});
			}
			return std::make_unique<EngineImpl>(std::move(options.application_name), options.max_frames);
		}

	} // namespace detail

} // namespace vve
