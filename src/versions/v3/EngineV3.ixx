module;

#if defined(_WIN32)
#if defined(VVE_ENGINE_BUILD)
#define VVE_API __declspec(dllexport)
#else
#define VVE_API __declspec(dllimport)
#endif
#else
#define VVE_API
#endif

export module VEEngine.V3;
import VEEngine;
export import VEEngine.V3.Types;
export import VEEngine.V3.Systems;
import :Internal;
import std;

namespace vve::v3 {

namespace detail {

template <typename TSystem>
[[nodiscard]] std::expected<void, vve::Error> invokeUserSystemInit(
    TSystem& system,
    vve::World& world) {
    // C++26 reflection is not available in the current toolchain, so hook
    // detection falls back to constrained lookup in this single helper seam.
    if constexpr (requires { system.init(world); }) {
        return system.init(world);
    } else {
        return {};
    }
}

template <typename TSystem>
[[nodiscard]] std::expected<void, vve::Error> invokeUserSystemUpdate(
    TSystem& system,
    vve::World& world,
    const FrameContext& frame_context) {
    if constexpr (requires { system.update(world, frame_context); }) {
        return system.update(world, frame_context);
    } else {
        return {};
    }
}

template <typename... TSystems>
[[nodiscard]] std::expected<void, vve::Error> initUserSystems(
    std::tuple<TSystems...>& systems,
    vve::World& world) {
    std::expected<void, vve::Error> result{};
    std::apply(
        [&](auto&... system) {
            ((result = invokeUserSystemInit(system, world), result ? void() : void()), ...);
        },
        systems);
    return result;
}

template <typename... TSystems>
[[nodiscard]] std::expected<void, vve::Error> updateUserSystems(
    std::tuple<TSystems...>& systems,
    vve::World& world,
    const FrameContext& frame_context) {
    std::expected<void, vve::Error> result{};
    std::apply(
        [&](auto&... system) {
            ((result = invokeUserSystemUpdate(system, world, frame_context), result ? void() : void()), ...);
        },
        systems);
    return result;
}

} // namespace detail

export inline [[nodiscard]] std::expected<void, vve::Error> executeTaskGraph(
    const TaskGraph& task_graph,
    const TaskExecutionContext& execution_context) {
    std::unordered_map<vve::Handle::value_type, std::size_t> node_indices{};
    node_indices.reserve(task_graph.nodes.size());

    for (std::size_t index = 0; index < task_graph.nodes.size(); ++index) {
        const auto handle_value = task_graph.nodes[index].handle.value.value();
        if (!node_indices.emplace(handle_value, index).second) {
            return std::unexpected(vve::Error::invalid_argument);
        }
    }

    std::vector<std::vector<std::size_t>> dependents(task_graph.nodes.size());
    std::vector<std::size_t> remaining_dependencies(task_graph.nodes.size(), 0);
    for (std::size_t index = 0; index < task_graph.nodes.size(); ++index) {
        const auto& node = task_graph.nodes[index];
        remaining_dependencies[index] = node.depends_on.size();

        for (const auto& dependency : node.depends_on) {
            const auto dependency_it = node_indices.find(dependency.value.value());
            if (dependency_it == node_indices.end()) {
                return std::unexpected(vve::Error::invalid_argument);
            }

            dependents[dependency_it->second].push_back(index);
        }
    }

    std::vector<std::size_t> ready_nodes{};
    ready_nodes.reserve(task_graph.nodes.size());
    for (std::size_t index = 0; index < task_graph.nodes.size(); ++index) {
        if (remaining_dependencies[index] == 0) {
            ready_nodes.push_back(index);
        }
    }

    std::size_t completed_nodes = 0;
    while (!ready_nodes.empty()) {
        const auto node_index = ready_nodes.front();
        ready_nodes.erase(ready_nodes.begin());

        const auto& node = task_graph.nodes[node_index];
        if (node.callback) {
            if (auto callback_result = node.callback(execution_context); !callback_result) {
                return callback_result;
            }
        }

        ++completed_nodes;
        for (const auto dependent_index : dependents[node_index]) {
            auto& dependency_count = remaining_dependencies[dependent_index];
            if (dependency_count == 0) {
                return std::unexpected(vve::Error::internal_error);
            }

            --dependency_count;
            if (dependency_count == 0) {
                ready_nodes.push_back(dependent_index);
            }
        }
    }

    if (completed_nodes != task_graph.nodes.size()) {
        return std::unexpected(vve::Error::invalid_argument);
    }

    return {};
}

export template <typename... TUserSystems>
class VVE_API BasicEngineImplementation {
public:
    explicit BasicEngineImplementation(const vve::EngineConfig& config);

    [[nodiscard]] std::expected<void, vve::Error> init();
    [[nodiscard]] std::expected<void, vve::Error> run();
    [[nodiscard]] std::expected<vve::FrameStatus, vve::Error> step();
    [[nodiscard]] std::expected<bool, vve::Error> isInitialized() const noexcept;
    [[nodiscard]] std::expected<int, vve::Error> getVersionMajor() const noexcept;
    [[nodiscard]] std::expected<void, vve::Error> loadFile(
        const std::filesystem::path& file_path);

private:
    [[nodiscard]] std::expected<void, vve::Error> rebuildTaskGraph();

    std::string application_name_;
    bool validation_enabled_{false};
    bool initialized_{false};
    bool running_{false};
    std::uint64_t frame_index_{0};
    EngineRuntimeDesc runtime_desc_{};
    detail::Runtime runtime_{};
    std::filesystem::path loaded_file_path_{};
    std::optional<SceneData> scene_{};
    std::optional<TaskGraph> task_graph_{};
    bool task_graph_dirty_{true};
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds>
        last_time_{};
    vve::ECS<> ecs_{};
    std::tuple<TUserSystems...> user_systems_{};
};

export using EngineImplementation = BasicEngineImplementation<>;
export using Engine = vve::Engine<EngineImplementation>;

template <typename... TUserSystems>
BasicEngineImplementation<TUserSystems...>::BasicEngineImplementation(const vve::EngineConfig& config)
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

    if constexpr (sizeof...(TUserSystems) > 0) {
        if (const auto user_systems = config.tryGet<vve::UserSystems<TUserSystems...>>()) {
            user_systems_ = user_systems->value;
        }
    }
}

template <typename... TUserSystems>
std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::init() {
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

    vve::World world{ecs_};
    if (auto user_system_result = detail::initUserSystems(user_systems_, world); !user_system_result) {
        return user_system_result;
    }

    return {};
}

template <typename... TUserSystems>
std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::run() {
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

template <typename... TUserSystems>
std::expected<vve::FrameStatus, vve::Error> BasicEngineImplementation<TUserSystems...>::step() {
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

    vve::World world{ecs_};
    if (auto user_system_result = detail::updateUserSystems(user_systems_, world, frame_context);
        !user_system_result) {
        return std::unexpected(user_system_result.error());
    }

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

    if (std::ranges::any_of(
            runtime_.window_frame->windows,
            [](const WindowState& window) {
                return window.should_close;
            })) {
        running_ = false;
        return vve::FrameStatus::should_close;
    }

    return vve::FrameStatus::continue_running;
}

template <typename... TUserSystems>
std::expected<bool, vve::Error> BasicEngineImplementation<TUserSystems...>::isInitialized() const noexcept {
    return initialized_;
}

template <typename... TUserSystems>
std::expected<int, vve::Error> BasicEngineImplementation<TUserSystems...>::getVersionMajor() const noexcept {
    return 3;
}

template <typename... TUserSystems>
std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::loadFile(
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

template <typename... TUserSystems>
std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::rebuildTaskGraph() {
    if (!scene_) {
        return std::unexpected(vve::Error::invalid_argument);
    }

    std::vector<ITaskSystem*> task_systems{};
    task_systems.reserve(runtime_.task_systems.size());
    for (const auto& task_system : runtime_.task_systems) {
        if (task_system != nullptr) {
            task_systems.push_back(task_system.get());
        }
    }

    auto task_graph = runtime_.task_graph_system->build(
        *scene_,
        task_systems,
        *runtime_.window_system,
        *runtime_.graphics_backend,
        *runtime_.resource_system,
        *runtime_.scene_system,
        *runtime_.render_system,
        runtime_.render_pipelines);
    task_graph_ = std::move(task_graph);
    task_graph_dirty_ = false;
    return {};
}

} // namespace vve::v3
