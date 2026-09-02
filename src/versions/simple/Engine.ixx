module;

#include <vulkan/vulkan_core.h>

#ifndef VVE_ENGINE_IMPLEMENTATION_NAMESPACE
#define VVE_ENGINE_IMPLEMENTATION_NAMESPACE simple
#endif

#define VVE_DETAIL_STRINGIFY_IMPL(value) #value
#define VVE_DETAIL_STRINGIFY(value) VVE_DETAIL_STRINGIFY_IMPL(value)

export module VEEngine.Simple;
import std;
export import VEEngine.Simple.Math;
export import :Graph;
export import :Window;
export import :Assets;
export import :RenderSystem;
export import VEEngine.Simple.Shaders;
export import VEEngine.Simple.Handle;
export import :Gui;

/// @file
/// @brief Small simple runtime facade: SDL windows, input, graphs, and stub subsystems.

export namespace vve::simple {

	struct TaskHandleTag {};								///< simple CPU task graph node handle tag.

	using TaskHandle = TypedHandle<TaskHandleTag>;	///< simple CPU task graph node handle.

	/// @brief User-system task names supplied by the facade for graph dumps.
	struct UserSystemTasks {
		Vector<ObjectName> value{};													///< Task names already formatted for the task graph.
	};

} // namespace vve::simple

export namespace vve::simple {

	using TaskGraph = Graph<TaskHandle>;				///< Generic DAG for CPU frame-task dependencies.

	namespace detail {

#ifndef NDEBUG
		inline constexpr std::int32_t debugDumpGraphHotkey{0x40000042};	///< SDL keycode for F9.
#endif

	} // namespace detail

	/// @brief Educational simple engine shell with SDL windows and lightweight subsystems.
	class Engine {
	public:
		Engine();
		~Engine();
		Engine(const Engine &) = delete;
		Engine(Engine &&) = delete;
		Engine &operator=(const Engine &) = delete;
		Engine &operator=(Engine &&) = delete;
		explicit Engine(EngineConfig config);

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
		[[nodiscard]] auto renderFrame()										-> std::expected<void, Error>;
		[[nodiscard]] std::expected<void, Error>
		writeDebugGraphs(const std::filesystem::path &directory = "graph_dumps") const;

	private:
		template <typename TOption> void applyOption(TOption &&);
		[[nodiscard]] auto makeImportedAssetReadAccess()					-> ImportedAssetReadAccess;
		auto applyDefaults()															-> void;
		[[nodiscard]] auto buildDefaultGraphs()								-> std::expected<void, Error>;
		[[nodiscard]] static auto graphFileStem(std::string_view text)	-> std::string;

		ApplicationName application_name_{};										///< Name used for default window titles.
		MaxFrames max_frames_{};														///< Optional frame cap.
		Windows windows_{};																///< Startup window descriptors.
		ECS ecs_{};																			///< Entity/component storage owned by the implementation and shared with the facade.
		WindowSystem window_system_{};												///< SDL platform window owner.
		AssetSystem assets_{};															///< Asset and object catalog facade.
		RenderSystem render_system_{makeImportedAssetReadAccess()};					///< Renderer selection and active CPU render scene.
		TaskGraph tasks_{};																///< CPU task graph facade.
		RenderGraph render_graph_{};													///< Render pass graph facade.
		ShaderSystem shaders_{};														///< Shader descriptor facade.
		GuiSystem gui_{};																	///< GUI descriptor facade.
		Vector<ObjectName> user_system_tasks_{};									///< User-system task names supplied by the facade.
		std::chrono::steady_clock::time_point last_frame_time_{};			///< Timestamp of the previous step().
		std::uint64_t frame_{0};														///< Number of completed step() calls.
		bool initialized_{false};														///< True after init() succeeds.
		bool gui_sdl_initialized_{false};											///< True after the active SDL window is bound to ImGui.
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

	/// @brief Creates an engine from the compact compatibility config.
	inline Engine::Engine(EngineConfig config){
		applyOption(std::move(config));
		applyDefaults();
	}

	/// @brief Creates an engine from typed options such as ApplicationName, Windows, and UserSystemTasks.
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

	/// @brief Creates SDL windows and default graphs.
	inline auto Engine::init()																				-> std::expected<void, Error>{
		if (initialized_) { return {}; }
		if (const auto result = window_system_.init(windows_); !result) { return result; }
		if (const auto result = buildDefaultGraphs(); !result) { return result; }
		last_frame_time_ = std::chrono::steady_clock::now();
		initialized_ = true;
		return {};
	}

	/// @brief Runs the engine until a window closes, a system fails, or the frame cap is reached.
	inline auto Engine::run()																				-> std::expected<void, Error>{
		if (!initialized_) {
			if (const auto result = init(); !result) { return result; }
		}
		while (true) {
			const auto status = step();
			if (!status) { return std::unexpected(status.error()); }
			if (*status == FrameStatus::stopped) { return {}; }
		}
	}

	/// @brief Polls input and advances the frame status.
	inline auto Engine::step()																				-> std::expected<FrameStatus, Error>{
		if (!initialized_) { return std::unexpected(Error::missing_object); }
		if (const auto result = window_system_.poll(); !result) { return std::unexpected(result.error()); }
#ifndef NDEBUG
		if (window_system_.input().wasKeyPressed(detail::debugDumpGraphHotkey)) {
			if (const auto result = writeDebugGraphs(); !result) { return std::unexpected(result.error()); }
		}
#endif

		const auto now = std::chrono::steady_clock::now();
		last_frame_time_ = now;

		++frame_;
		if (window_system_.anyShouldClose() || (max_frames_.value.value > 0 && frame_ >= max_frames_.value.value)) {
			return FrameStatus::stopped;
		}
		return FrameStatus::running;
	}

	/// @brief Lazily initializes the renderer and binds the GUI SDL backend to its window.
	inline auto Engine::renderFrame()																	-> std::expected<void, Error>{
		if (!render_system_.initialized()) {
			for (auto window : window_system_.windows()) {
				auto *native = window.get().native();
				if (native == nullptr) { continue; }
				if (const auto result = render_system_.initialize(native, window.get().rendererId()); !result) {
					return result;
				}
				if (!gui_sdl_initialized_) {
					gui_.initContext();
					gui_.initSDL(native);
					if (auto info = render_system_.makeGuiInitInfo()) {
						gui_.initVulkan(&*info);
						gui_.buildFonts();
					}
					render_system_.setGuiRecordSink([this](VkCommandBuffer cmd){ gui_.recordFrame(cmd); });
					gui_sdl_initialized_ = true;
				}
				break;
			}
		}
		return render_system_.renderFrame(window_system_);
	}

	/// @brief Applies typed engine options; unknown option types are ignored.
	template <typename TOption>
	auto Engine::applyOption(TOption &&option)														-> void{
		using Option = std::remove_cvref_t<TOption>;
		if constexpr (std::same_as<Option, EngineConfig>) {
			auto config = std::forward<TOption>(option);
			application_name_.value = std::move(config.application_name);
			max_frames_.value = config.max_frames;
		} else if constexpr (std::same_as<Option, ApplicationName>) {
			application_name_ = std::forward<TOption>(option);
		} else if constexpr (std::same_as<Option, MaxFrames>) {
			max_frames_ = std::forward<TOption>(option);
		} else if constexpr (std::same_as<Option, Windows>) {
			windows_ = std::forward<TOption>(option);
		} else if constexpr (std::same_as<Option, UserSystemTasks>) {
			user_system_tasks_ = std::forward<TOption>(option).value;
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
			.material_textures = [this](MaterialHandle material) { return assets_.materialTextures(material); },
			.scene_lights = [this](SceneHandle scene) { return assets_.sceneLights(scene); },
			.light_data = [this](LightHandle light) { return assets_.lightData(light); },
			.scene_cameras = [this](SceneHandle scene) { return assets_.sceneCameras(scene); },
			.camera_data = [this](CameraHandle camera) { return assets_.cameraData(camera); },
			.mesh_positions = [this](MeshHandle mesh) { return assets_.meshPositions(mesh); },
			.mesh_normals = [this](MeshHandle mesh) { return assets_.meshNormals(mesh); },
			.mesh_texcoords = [this](MeshHandle mesh) { return assets_.meshTexcoords(mesh); },
			.mesh_indices = [this](MeshHandle mesh) { return assets_.meshIndices(mesh); }};
	}

	/// @brief Fills small defaults after options have been applied.
	inline auto Engine::applyDefaults()																	-> void{
		render_system_.setGuiSystem(&gui_);																///< Hands engine-owned GUI to renderer for later forward GUI integration.
		window_system_.setGuiEventSink([this](const auto &event) { gui_.processEvent(event); });		///< Forwards SDL input to the engine-owned GUI system.
		if (windows_.value.empty()) { windows_.value.push_back(WindowDesc{}); }
		for (auto &window : windows_.value) {
			if (window.title == WindowDesc{}.title && application_name_.value != ApplicationName{}.value) {
				window.title = application_name_.value;
			}
		}
	}

	/// @brief Builds simple inspectable default graphs for debug dumps and teaching.
	inline auto Engine::buildDefaultGraphs()															-> std::expected<void, Error>{
		tasks_ = {};
		render_graph_ = {};

		const auto begin = tasks_.addNode(ObjectName{.value = "task.frame_begin"});
		const auto poll = tasks_.addNode(ObjectName{.value = "task.poll_window_events"});
		const auto render = tasks_.addNode(ObjectName{.value = "task.render_graph"});
		const auto finish = tasks_.addNode(ObjectName{.value = "task.frame_finished"});
		if (!begin || !poll || !render || !finish) { return std::unexpected(Error::internal_error); }

		tasks_.addEdge(*begin, *poll);
		auto previous = *poll;
		for (const auto &task_name : user_system_tasks_) {
			const auto task = tasks_.addNode(task_name);
			if (!task) { return std::unexpected(task.error()); }
			tasks_.addEdge(previous, *task);
			previous = *task;
		}
		tasks_.addEdge(previous, *render);
		tasks_.addEdge(*render, *finish);

		const auto renderer_id = RendererId{.value = "forward"};
		const auto renderer = render_system_.createRenderer(renderer_id);
		if (!renderer) { return std::unexpected(renderer.error()); }

		const std::array pass_lists{renderer->passes, gui_.passes()};
		const auto graph = render_system_.buildRenderGraph(pass_lists);
		if (!graph) { return std::unexpected(graph.error()); }
		render_graph_ = *graph;
		return {};
	}

	/// @brief Converts a window id into a compact filesystem-safe graph dump stem.
	inline auto Engine::graphFileStem(std::string_view text)										-> std::string{
		std::string result{};
		result.reserve(text.size());
		for (const char ch : text) {
			const bool safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
									(ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
			result.push_back(safe ? ch : '_');
		}
		return result.empty() ? "window" : result;
	}

	/// @brief Writes task and render graph JSON dumps into a debug directory.
	inline auto Engine::writeDebugGraphs(const std::filesystem::path &directory) const	-> std::expected<void, Error>{
#ifndef NDEBUG
		const auto task_path = directory / "task_graph.json";
		const auto render_path = directory / "render_graph.json";
		if (const auto result = tasks_.writeJson(task_path, "task_graph", "simple task graph"); !result) { return result; }
		if (const auto result = render_graph_.writeJson(render_path, "render_graph", "simple render graph"); !result) {
			return result;
		}

		for (const auto &window : window_system_.snapshot()) {
			auto renderer_id = window.renderer_id.value.empty() ? RendererId{.value = "forward"} : window.renderer_id;
			const auto renderer = render_system_.createRenderer(renderer_id);
			if (!renderer) { return std::unexpected(renderer.error()); }

			const std::array pass_lists{renderer->passes, gui_.passes()};
			const auto graph = render_system_.buildRenderGraph(pass_lists);
			if (!graph) { return std::unexpected(graph.error()); }

			const auto file = directory / ("render_graph_" + graphFileStem(window.id) + ".json");
			const auto name = "simple render graph window=" + window.id + " renderer=" + renderer->id.value;
			if (const auto result = graph->writeJson(file, "render_graph", name); !result) { return result; }
		}

		return {};
#else
		(void)directory;
		return {};
#endif
	}

} // namespace vve::simple
