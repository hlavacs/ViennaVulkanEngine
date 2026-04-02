module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

namespace {

[[nodiscard]] std::vector<ITaskSystem*> makeTaskSystemView(
    const std::vector<std::shared_ptr<ITaskSystem>>& task_systems) {
    std::vector<ITaskSystem*> view{};
    view.reserve(task_systems.size());

    for (const auto& task_system : task_systems) {
        if (task_system != nullptr) {
            view.push_back(task_system.get());
        }
    }

    return view;
}

[[nodiscard]] std::expected<TaskGraph, vve::Error> buildTaskGraph(
    detail::Runtime& runtime,
    const SceneData& scene) {
    auto task_systems = makeTaskSystemView(runtime.task_systems);
    return runtime.task_graph_system->build(
        scene,
        task_systems,
        *runtime.window_system,
        *runtime.graphics_backend,
        *runtime.resource_system,
        *runtime.scene_system,
        *runtime.render_system,
        runtime.render_pipelines);
}

[[nodiscard]] bool anyWindowShouldClose(const std::shared_ptr<const WindowFrameData>& window_frame) {
    if (window_frame == nullptr) {
        return false;
    }

    return std::ranges::any_of(
        window_frame->windows,
        [](const WindowState& window) {
            return window.should_close;
        });
}

} // namespace

EngineImplementation::EngineImplementation(const vve::EngineConfig& config)
    : application_name_("ViennaVulkanEngine"),
      validation_enabled_(false) {
    if (const auto application_name = config.tryGet<vve::ApplicationName>()) {
        application_name_ = application_name->value;
    }

    if (const auto enable_validation = config.tryGet<vve::EnableValidation>()) {
        validation_enabled_ = enable_validation->value;
    }

    if (const auto graphics_api = config.tryGet<vve::PreferredGraphicsApi>()) {
        runtime_desc_.graphics_api = graphics_api->value;
    }

    if (const auto renderer = config.tryGet<vve::PreferredRenderer>()) {
        runtime_desc_.renderer = renderer->value;
    }

    if (const auto shadow = config.tryGet<vve::PreferredShadow>()) {
        runtime_desc_.shadow = shadow->value;
    }

    if (const auto imgui = config.tryGet<vve::EnableImGui>()) {
        runtime_desc_.imgui_enabled = imgui->value;
    }

    if (const auto windows = config.tryGet<vve::Windows>()) {
        runtime_desc_.windows = windows->value;
    }

    if (const auto task_systems = config.tryGet<vve::v3::TaskSystems>()) {
        runtime_desc_.task_systems = task_systems->value;
    }
}

std::expected<void, vve::Error> EngineImplementation::init() {
    if (*isInitialized()) {
        return std::unexpected(vve::Error::already_initialized);
    }

    auto runtime = detail::createRuntime(runtime_desc_);
    if (!runtime) {
        return std::unexpected(runtime.error());
    }

    runtime_ = std::move(*runtime);
    if (auto backend_result = runtime_.graphics_backend->init(); !backend_result) {
        return backend_result;
    }

    if (runtime_.gui_system != nullptr) {
        if (auto gui_result = runtime_.gui_system->init(*runtime_.graphics_backend); !gui_result) {
            return gui_result;
        }
    }

    loaded_file_path_.clear();
    scene_ = SceneData{};
    task_graph_.reset();
    initialized_ = true;
    running_ = false;
    task_graph_dirty_ = true;
    last_time_ = std::chrono::time_point_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now());
    return {};
}

std::expected<void, vve::Error> EngineImplementation::run() {
    if (!*isInitialized()) {
        if (auto init_result = init(); !init_result) {
            return init_result;
        }
    }

    running_ = true;
    while (running_) {
        if (auto step_result = step(); !step_result) {
            running_ = false;
            return std::unexpected(step_result.error());
        } else if (*step_result == vve::FrameStatus::should_close) {
            running_ = false;
        }
    }

    return {};
}

std::expected<vve::FrameStatus, vve::Error> EngineImplementation::step() {
    if (!*isInitialized()) {
        return std::unexpected(vve::Error::not_initialized);
    }

    if (!scene_) {
        return std::unexpected(vve::Error::invalid_argument);
    }

    const auto current_time = std::chrono::time_point_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now());
    const double seconds_elapsed =
        std::chrono::duration<double>(current_time - last_time_).count();
    last_time_ = current_time;

    const FrameContext frame_context{
        .frame_index = frame_index_++,
        .delta_seconds = seconds_elapsed
    };

    if (task_graph_dirty_ || !task_graph_) {
        if (auto task_graph_result = rebuildTaskGraph(); !task_graph_result) {
            return std::unexpected(task_graph_result.error());
        }
    }

    const TaskExecutionContext execution_context{
        .frame_context = &frame_context,
        .scene = &*scene_,
        .window_frame = runtime_.window_frame
    };
    if (auto execute_result = executeTaskGraph(*task_graph_, execution_context); !execute_result) {
        return std::unexpected(execute_result.error());
    }

    if (anyWindowShouldClose(runtime_.window_frame)) {
        running_ = false;
        return vve::FrameStatus::should_close;
    }

    return vve::FrameStatus::continue_running;
}

std::expected<bool, vve::Error> EngineImplementation::isInitialized() const noexcept {
    return initialized_;
}

std::expected<int, vve::Error> EngineImplementation::getVersionMajor() const noexcept {
    return 3;
}

std::expected<void, vve::Error> EngineImplementation::loadFile(
    const std::filesystem::path& file_path) {
    if (!*isInitialized()) {
        return std::unexpected(vve::Error::not_initialized);
    }

    if (file_path.empty()) {
        return std::unexpected(vve::Error::invalid_argument);
    }

    const auto imported_scene = runtime_.asset_system->importScene(file_path);
    if (!imported_scene) {
        return std::unexpected(imported_scene.error());
    }

    if (auto register_result = runtime_.resource_system->registerImportedScene(
            *imported_scene, file_path); !register_result) {
        return register_result;
    }

    const auto scene = runtime_.scene_system->instantiate(*imported_scene);
    if (!scene) {
        return std::unexpected(scene.error());
    }

    loaded_file_path_ = file_path;
    scene_ = std::move(*scene);
    task_graph_dirty_ = true;
    return rebuildTaskGraph();
}

std::expected<void, vve::Error> EngineImplementation::rebuildTaskGraph() {
    if (!scene_) {
        return std::unexpected(vve::Error::invalid_argument);
    }

    auto task_graph = buildTaskGraph(runtime_, *scene_);
    if (!task_graph) {
        return std::unexpected(task_graph.error());
    }

    task_graph_ = std::move(*task_graph);
    task_graph_dirty_ = false;
    return {};
}

} // namespace vve::v3
