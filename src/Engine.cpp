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
			if (options.windows.has_value()) {
				return EngineStateHandle{
					new EngineState{std::move(options.config), implementationWindows(*options.windows)},
					EngineStateDeleter{}};
			}
			return EngineStateHandle{new EngineState{std::move(options.config)}, EngineStateDeleter{}};
		}

		/// @brief Returns the selected implementation major version.
		std::uint32_t engineVersionMajor(const EngineState &state) { return state.impl.versionMajor(); }


		/// @brief Returns the selected implementation version name.
		std::string_view engineVersionName(const EngineState &state) { return state.impl.versionName(); }

		/// @brief Returns the entity/component storage owned by the selected implementation.
		ECS &engineEcs(EngineState &state) { return state.impl.ecs(); }

		/// @brief Returns the selected asset system.
		VVE_ENGINE_IMPLEMENTATION_NAMESPACE::AssetSystem &engineAssets(EngineState &state) { return state.impl.assets(); }

		/// @brief Returns the selected GUI system.
		VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiSystem &engineGui(EngineState &state) { return state.impl.gui(); }

		/// @brief Returns the selected window system.
		VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowSystem &engineWindowSystem(EngineState &state) { return state.impl.windowSystem(); }

		/// @brief Returns the selected render system.
		VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem &engineRenderSystem(EngineState &state) { return state.impl.renderSystem(); }

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
			return state.impl.renderFrame();
		}

	} // namespace detail

} // namespace vve
