module;

#include <cstdio>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>

export module VEEngine.Simple;
import std;
export import VEEngine.Simple.Types;
export import :Graph;
export import :Window;
export import :Assets;
export import :RenderSystem;
export import :Gui;

/// @file
/// @brief Small simple runtime facade: SDL windows, input, assets, rendering, and GUI.

export namespace vve::simple {

	/// @brief Educational simple engine shell with SDL windows and lightweight subsystems.
	class Engine {
	public:
		Engine();
		~Engine();
		Engine(const Engine &) = delete;
		Engine(Engine &&) = delete;
		Engine &operator=(const Engine &) = delete;
		Engine &operator=(Engine &&) = delete;

		template <typename... TOptions>
			requires(sizeof...(TOptions) > 0)
		explicit Engine(TOptions &&...options);

		[[nodiscard]] auto versionMajor() const								-> std::uint32_t;
		[[nodiscard]] auto versionName() const									-> std::string_view;
		[[nodiscard]] AssetSystem &assets();
		[[nodiscard]] RenderSystem &renderSystem();
		[[nodiscard]] const RenderSystem &renderSystem() const;
		[[nodiscard]] GuiSystem &gui();
		[[nodiscard]] ECS &ecs();
		[[nodiscard]] WindowSystem &windowSystem();
		[[nodiscard]] const WindowSystem &windowSystem() const;
		[[nodiscard]] auto init()													-> std::expected<void, Error>;
		[[nodiscard]] auto run()													-> std::expected<void, Error>;
		[[nodiscard]] auto step()													-> std::expected<FrameStatus, Error>;
		[[nodiscard]] FrameContext frameContext() const;
		[[nodiscard]] auto renderFrame()										-> std::expected<void, Error>;

	private:
		template <typename TOption> void applyOption(TOption &&);
		[[nodiscard]] auto makeImportedAssetReadAccess()					-> ImportedAssetReadAccess;
		auto applyDefaults()															-> void;

		ApplicationName application_name_{};										///< Name used for default window titles.
		MaxFrames max_frames_{};														///< Optional frame cap.
		Windows windows_{};																///< Startup window descriptors.
		ECS ecs_{};																			///< Entity/component storage owned by the implementation and shared with the facade.
		WindowSystem window_system_{};												///< SDL platform window owner.
		AssetSystem assets_{};															///< Asset and object catalog facade.
		RenderSystem render_system_{makeImportedAssetReadAccess(), &window_system_};	///< Renderer selection and active CPU render scene.
		GuiSystem gui_{};																	///< GUI descriptor facade.
		std::chrono::steady_clock::time_point last_frame_time_{};			///< Timestamp of the previous step().
		std::uint64_t frame_{0};														///< Number of completed step() calls.
		FrameContext frame_context_{};											///< Index and delta of the latest polled frame, shared with the facade.
		bool initialized_{false};														///< True after init() succeeds.
		bool gui_initialization_attempted_{false};							///< Prevents retrying or logging failed GUI initialization each frame.
	};

	/// @brief Creates an engine with default options.
	inline Engine::Engine(){
		applyDefaults();
	}

	/// @brief Releases runtime systems owned by the simple engine.
	inline Engine::~Engine() {
		render_system_.waitIdle();																					///< ImGui pipelines may still be referenced by the last submitted frame.
		gui_.shutdownVulkan();
		render_system_.shutdown();
		gui_.shutdownSDL();
		gui_.shutdownContext();
	}

	/// @brief Creates an engine from typed options such as ApplicationName, MaxFrames, and Windows.
	template <typename... TOptions>
		requires(sizeof...(TOptions) > 0)
	Engine::Engine(TOptions &&...options) {
		(applyOption(std::forward<TOptions>(options)), ...);
		applyDefaults();
	}

	/// @brief Returns the major engine version.
	inline std::uint32_t Engine::versionMajor() const { return 1; }


	/// @brief Returns the printable engine version name.
	inline std::string_view Engine::versionName() const { return "simple"; }

	/// @brief Returns the asset system.
	inline AssetSystem &Engine::assets() { return assets_; }

	/// @brief Returns the render system.
	inline RenderSystem &Engine::renderSystem() { return render_system_; }

	/// @brief Returns the render system.
	inline const RenderSystem &Engine::renderSystem() const { return render_system_; }

	/// @brief Returns the GUI system.
	inline GuiSystem &Engine::gui() { return gui_; }

	/// @brief Returns the entity/component storage shared with the facade.
	inline ECS &Engine::ecs() { return ecs_; }

	/// @brief Returns the implementation window system.
	inline WindowSystem &Engine::windowSystem() { return window_system_; }

	/// @brief Returns the implementation window system.
	inline const WindowSystem &Engine::windowSystem() const {
		return window_system_;
	}

	/// @brief Creates SDL windows.
	inline auto Engine::init()																				-> std::expected<void, Error>{
		if (initialized_) { return {}; }
		// Renderer selection errors are reported by init, before lazy GPU initialization.
		for (const auto &window : windows_.value) {
			if (!RenderSystem::supportsRenderer(window.renderer_id)) { return std::unexpected(Error::invalid_argument); }
		}
		if (const auto result = window_system_.init(windows_); !result) { return result; }
		initialized_ = true;
		return {};
	}

	/// @brief Polls and renders until a window closes, a system fails, or the frame cap is reached.
	inline auto Engine::run()																				-> std::expected<void, Error>{
		if (!initialized_) {
			if (const auto result = init(); !result) { return result; }
		}
		while (true) {
			const auto status = step();
			if (!status) { return std::unexpected(status.error()); }
			// Render the final polled frame before honoring its stop status.
			if (const auto result = renderFrame(); !result) { return result; }
			if (*status == FrameStatus::stopped) { return {}; }
		}
	}

	/// @brief Reports a recovered GUI callback error once, otherwise polls input and advances the frame status.
	inline auto Engine::step()																				-> std::expected<FrameStatus, Error>{
		if (!initialized_) { return std::unexpected(Error::missing_object); }
		if (gui_.takeFrameCallbackError()) { return std::unexpected(Error::platform_error); }
		if (const auto result = window_system_.poll(); !result) { return std::unexpected(result.error()); }

		const auto now = std::chrono::steady_clock::now();
		// Capture timing before advancing the frame cap, even if a later update or render fails.
		frame_context_ = FrameContext{.frame_index = FrameCount{.value = frame_},
			.delta_time = frame_ == 0 ? DeltaTime{} :
				DeltaTime{.seconds = std::chrono::duration<double>{now - last_frame_time_}.count()}};
		last_frame_time_ = now;

		++frame_;
		if (window_system_.anyShouldClose() || (max_frames_.value.value > 0 && frame_ >= max_frames_.value.value)) {
			return FrameStatus::stopped;
		}
		return FrameStatus::running;
	}

	/// @brief Returns the latest polled frame context; the first step uses DeltaTime{}.
	inline FrameContext Engine::frameContext() const { return frame_context_; }

	/// @brief Skips minimized or zero-size windows, waits when none can render, and initializes rendering lazily.
	auto Engine::renderFrame()																	-> std::expected<void, Error>{
		bool drawable = false;
		// Borrow current state to check drawability without copying window descriptions.
		for (const auto &window : window_system_.windows()) {
			const auto &info = window.get().info();
			if (info.should_close || info.renderer_id.value == "none" || info.minimized) { continue; }
			int width{}, height{};
			SDL_GetWindowSizeInPixels(window.get().native(), &width, &height);
			drawable = drawable || (width > 0 && height > 0);
		}
		if (drawable && !render_system_.initialized()) {
			if (const auto result = render_system_.initialize(window_system_); !result) { return result; }
			if (!gui_initialization_attempted_ && !render_system_.forward().targets.empty()) {
				const auto &target = render_system_.forward().targets.front();
				gui_initialization_attempted_ = true;
				gui_.initContext(target.swapchain.guiFormat);
				// Stop at the first failed stage so no uninitialized backend is driven.
				const auto failed_stage = [&]() -> std::string_view {
					if (!gui_.initSDL(target.window)) { return "SDL"; }
					auto info = render_system_.makeGuiInitInfo();
					if (!info) { return "Vulkan init info"; }
					if (!gui_.initVulkan(&*info)) { return "Vulkan"; }
					if (!gui_.buildFonts()) { return "fonts"; }
					return {};
				}();
				if (!failed_stage.empty()) {
					std::println(stderr, "[vve::simple] GUI disabled: {}", failed_stage);
					gui_.shutdownVulkan();
					gui_.shutdownSDL();
					gui_.shutdownContext();
				} else {
					render_system_.setGuiPrepareSink([this]{ return gui_.prepareFrame(); });
					render_system_.setGuiRecordSink([this](VkCommandBuffer cmd){ gui_.record(cmd); });
				}
			}
		}
		// Reset presentation counts and retire closed targets even when every live target is skipped.
		if (render_system_.initialized()) {
			if (const auto result = render_system_.renderFrame(window_system_); !result) { return result; }
		}
		// Leave the wake-up event queued for the next poll, including a restore event.
		if (!drawable) { (void)SDL_WaitEventTimeout(nullptr, 100); }
		return {};
	}

	/// @brief Applies typed engine options; an unknown option type is a compile error, not silently dropped.
	template <typename TOption>
	auto Engine::applyOption(TOption &&option)														-> void{
		using Option = std::remove_cvref_t<TOption>;
		if constexpr (std::same_as<Option, ApplicationName>) {
			application_name_ = std::forward<TOption>(option);
		} else if constexpr (std::same_as<Option, MaxFrames>) {
			max_frames_ = std::forward<TOption>(option);
		} else if constexpr (std::same_as<Option, Windows>) {
			windows_ = std::forward<TOption>(option);
		} else {
			static_assert(!std::same_as<Option, Option>, "simple::Engine: unknown option type (use vve::simple::Windows, not vve::WindowSetups)");
		}
	}

	/// @brief Builds the borrowed asset-read callbacks handed to the render system.
	inline auto Engine::makeImportedAssetReadAccess()						-> ImportedAssetReadAccess{
		return ImportedAssetReadAccess{
			.scene_nodes = [this](SceneHandle scene) { return assets_.sceneNodes(scene); },
			.scene_root_node = [this](SceneHandle scene) { return assets_.sceneRootNode(scene); },
			.scene_node_children = [this](SceneHandle scene, NodeHandle node) { return assets_.sceneNodeChildren(scene, node); },
			.node_transform = [this](NodeHandle node) { return assets_.nodeTransform(node); },
			.node_meshes = [this](NodeHandle node) { return assets_.nodeMeshes(node); },
			.mesh_material = [this](MeshHandle mesh) { return assets_.meshMaterial(mesh); },
			.material_base_color = [this](MaterialHandle material) { return assets_.materialBaseColor(material); },
			.material_texture_sources = [this](MaterialHandle material) { return assets_.materialTextureSources(material); },
			.material_factors = [this](MaterialHandle material) { return assets_.materialFactors(material); },
			.scene_lights = [this](SceneHandle scene) { return assets_.sceneLights(scene); },
			.light_data = [this](LightHandle light) { return assets_.lightData(light); },
			.scene_cameras = [this](SceneHandle scene) { return assets_.sceneCameras(scene); },
			.camera_data = [this](CameraHandle camera) { return assets_.cameraData(camera); },
			.mesh_geometry = [this](MeshHandle mesh) { return assets_.meshGeometry(mesh); },
			.mesh_indices = [this](MeshHandle mesh) { return assets_.meshIndicesView(mesh); }};
	}

	/// @brief Fills small defaults after options have been applied.
	inline auto Engine::applyDefaults()																	-> void{
		window_system_.setGuiEventSink([this](const auto &event) { return gui_.processEvent(event); });	///< Forwards SDL input to the GUI; claimed events skip the game input.
		if (windows_.value.empty()) { windows_.value.push_back(WindowDesc{}); }
		for (auto &window : windows_.value) {
			if (window.title == WindowDesc{}.title && application_name_.value != ApplicationName{}.value) {
				window.title = application_name_.value;
			}
		}
	}

} // namespace vve::simple
