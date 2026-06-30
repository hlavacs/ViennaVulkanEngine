module VEEngine;
import std;
import VEEngine.Simple;

namespace vve {

	namespace detail {

		struct EngineState {
			template <typename... TOptions>
			explicit EngineState(TOptions &&...options) : impl{std::forward<TOptions>(options)...} {}

			VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Engine impl{};	///< Selected engine implementation owned by the facade.
		};																///< Concrete state hidden from the exported interface.

		/// @brief Converts facade startup windows into selected implementation descriptors.
		[[nodiscard]] auto implementationWindows(const std::vector<EngineWindowSetup> &windows)
			-> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows {
			auto result = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows{};
			result.value.clear();
			result.value.reserve(windows.size());
			for (const auto &window : windows) {
				result.value.push_back(VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowDesc{
					.id = window.id,
					.title = window.title,
					.extent = window.extent,
					.x = window.x,
					.y = window.y,
					.renderer_id = window.renderer_id,
					.resizable = window.resizable,
					.visible = window.visible,
				});
			}
			return result;
		}

		/// @brief Converts facade user-system task names into selected implementation descriptors.
		[[nodiscard]] auto implementationUserSystemTasks(const Vector<ObjectName> &tasks)
			-> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks {
			auto result = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks{};
			for (const auto &task : tasks) { result.value.push_back(task); }
			return result;
		}

		/// @brief Converts selected implementation window snapshots into facade frame data.
		[[nodiscard]] auto facadeWindowFrame(const VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowFrameData &frame)
			-> WindowFrameData {
			auto result = WindowFrameData{};
			result.windows.reserve(frame.windows.size());
			for (const auto &window : frame.windows) {
				result.windows.push_back(WindowFrameInfo{.handle = window.handle,
																	 .id = window.id,
																	 .title = window.title,
																	 .extent = window.extent,
																	 .renderer_id = window.renderer_id,
																	 .camera = window.camera,
																	 .focused = window.focused,
																	 .minimized = window.minimized,
																	 .should_close = window.should_close});
			}
			return result;
		}

		/// @brief Deletes the opaque engine state where the selected implementation is complete.
		void EngineStateDeleter::operator()(EngineState *state) const noexcept { delete state; }

		/// @brief Creates the selected engine implementation from facade-owned startup options.
		EngineStateHandle makeEngineState(EngineStartupOptions options) {
			const auto tasks = implementationUserSystemTasks(options.user_system_tasks);
			if (options.windows.has_value()) {
				return EngineStateHandle{
					new EngineState{std::move(options.config), implementationWindows(*options.windows), tasks},
					EngineStateDeleter{}};
			}
			return EngineStateHandle{new EngineState{std::move(options.config), tasks}, EngineStateDeleter{}};
		}

		/// @brief Returns the selected implementation major version.
		std::uint32_t engineVersionMajor(const EngineState &state) { return state.impl.versionMajor(); }

		/// @brief Returns the selected implementation major version through the compatibility API.
		std::expected<int, Error> engineGetVersionMajor(const EngineState &state) noexcept {
			return state.impl.getVersionMajor();
		}

		/// @brief Returns the selected implementation version name.
		std::string_view engineVersionName(const EngineState &state) { return state.impl.versionName(); }

		/// @brief Returns an erased pointer to the selected asset system.
		void *engineAssets(EngineState &state) { return std::addressof(state.impl.assets()); }

		/// @brief Returns an erased pointer to the selected GUI system.
		void *engineGui(EngineState &state) { return std::addressof(state.impl.gui()); }

		/// @brief Returns an erased pointer to the selected window system.
		void *engineWindowSystem(EngineState &state) { return std::addressof(state.impl.windowSystem()); }

		/// @brief Returns an erased pointer to the selected render system.
		void *engineRenderSystem(EngineState &state) { return std::addressof(state.impl.renderSystem()); }

		/// @brief Initializes the selected engine implementation.
		std::expected<void, Error> engineInit(EngineState &state) { return state.impl.init(); }

		/// @brief Steps the selected engine implementation.
		std::expected<FrameStatus, Error> engineStep(EngineState &state) { return state.impl.step(); }

		/// @brief Captures facade window frame data from the selected implementation.
		WindowFrameData engineWindowFrame(EngineState &state) {
			return facadeWindowFrame(VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowFrameData{
				.windows = state.impl.windowSystem().snapshot()});
		}

		/// @brief Renders one frame through the selected implementation.
		std::expected<void, Error> engineRenderFrame(EngineState &state) {
			auto &render_system = state.impl.renderSystem();
			auto &window_system = state.impl.windowSystem();
			if (!render_system.initialized()) {
				for (auto window : window_system.windows()) {
					auto *native = window.get().native();
					if (native == nullptr) { continue; }
					if (const auto result = render_system.initialize(native, window.get().rendererId()); !result) { return result; }
					break;
				}
			}
			return render_system.renderFrame(window_system);
		}

		/// @brief Writes selected implementation debug graphs.
		std::expected<void, Error>
		engineWriteDebugGraphs(const EngineState &state, const std::filesystem::path &directory) {
			return state.impl.writeDebugGraphs(directory);
		}

	} // namespace detail

} // namespace vve
